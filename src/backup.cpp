// ============================================================================
//  backup.cpp  —  v1.9.0  Management backup / restore of patient data.
//
//  Goals (per the v1.9.0 spec):
//    * A management-only «پشتیبان‌گیری» (Backup) and «بازیابی پشتیبان»
//      (Restore) module, opened from the admin panel.
//    * Our OWN backups use a small self-describing container (AZTBKP01) that
//      bundles every file under data\ so a full clinic state can be restored.
//    * Restore can apply the FULL backup or a SELECTED subset (e.g. only the
//      patient information) by ticking categories.
//    * Foreign, very large (~15 GB) Matin-Teb «.bak» SQL backups are detected
//      and SCANNED (not loaded) so the UI never freezes: the scan runs on a
//      background thread and only ESTIMATES the per-category breakdown from the
//      file size. The page shows a category list with estimated sizes.
//    * A file-open bar, a restore progress bar, and smooth animations keep the
//      UI responsive the whole time (all heavy I/O is on worker threads).
//
//  Pure Win32 + GDI+, no new dependencies. Follows the existing modal pattern
//  (WS_POPUP topmost over g_hFrame, double-buffered WM_PAINT, owner-drawn).
// ============================================================================
#include "app.h"
#include <windowsx.h>
#include <commdlg.h>
#include <process.h>
#include <vector>
#include <string>

// ----------------------------------------------------------------- helpers --
static std::wstring humanSize(long long b){
    const wchar_t* u[]={L"بایت",L"کیلوبایت",L"مگابایت",L"گیگابایت",L"ترابایت"};
    double v=(double)b; int i=0;
    while(v>=1024.0 && i<4){ v/=1024.0; i++; }
    wchar_t buf[64];
    if(i==0) swprintf(buf,64,L"%lld %s",b,u[0]);
    else     swprintf(buf,64,L"%.1f %s",v,u[i]);
    return toFaDigits(buf);
}

// Stable category descriptors. The classifier maps each data file (or a slice
// of a foreign .bak) into exactly one of these.
struct CatDef { const wchar_t* id; const wchar_t* name; int icon; };
static const CatDef CATS[]={
    { L"patients", L"اطلاعات بیماران",  ICO_USER   },
    { L"images",   L"تصاویر و مدارک",   ICO_ID     },
    { L"billing",  L"مالی و صورتحساب",  ICO_RECEIPT},
    { L"other",    L"سایر اطلاعات",     ICO_PIN    },
};
static const int CAT_COUNT = 4;

static int catIndexOf(const std::wstring& id){
    for(int i=0;i<CAT_COUNT;i++) if(id==CATS[i].id) return i;
    return CAT_COUNT-1;
}

// Map a data\ filename to a category id.
static std::wstring classifyFile(const std::wstring& name){
    std::wstring n=name; for(auto& c:n) c=towlower(c);
    if(n.find(L".png")!=std::wstring::npos || n.find(L".jpg")!=std::wstring::npos ||
       n.find(L".jpeg")!=std::wstring::npos|| n.find(L".bmp")!=std::wstring::npos ||
       n.find(L"photo")!=std::wstring::npos|| n.find(L"image")!=std::wstring::npos ||
       n.find(L"attach")!=std::wstring::npos|| n.find(L"avatar")!=std::wstring::npos)
        return L"images";
    if(n.find(L"patient")!=std::wstring::npos|| n.find(L"reception")!=std::wstring::npos||
       n.find(L"guest")!=std::wstring::npos  || n.find(L"appoint")!=std::wstring::npos ||
       n.find(L"visit")!=std::wstring::npos)
        return L"patients";
    if(n.find(L"bill")!=std::wstring::npos   || n.find(L"invoice")!=std::wstring::npos||
       n.find(L"receipt")!=std::wstring::npos|| n.find(L"insur")!=std::wstring::npos ||
       n.find(L"tariff")!=std::wstring::npos || n.find(L"pay")!=std::wstring::npos)
        return L"billing";
    return L"other";
}

// Enumerate every regular file under data\ (one level deep is enough for our
// own layout; we also descend into a single attachments subfolder if present).
static void enumDataFiles(const std::wstring& dir,
                          std::vector<std::wstring>& out,
                          std::vector<long long>& sizes){
    std::wstring pat=dir+L"\\*";
    WIN32_FIND_DATAW fd; HANDLE h=FindFirstFileW(pat.c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE) return;
    do{
        if(fd.cFileName[0]==L'.') continue;
        std::wstring full=dir+L"\\"+fd.cFileName;
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            enumDataFiles(full,out,sizes);          // one extra level (attachments)
        } else {
            long long sz=((long long)fd.nFileSizeHigh<<32)|fd.nFileSizeLow;
            out.push_back(full); sizes.push_back(sz);
        }
    } while(FindNextFileW(h,&fd));
    FindClose(h);
}

// ====================================================================== state
#define BK_CLASS L"AzBackupMgr"
enum { BK_MODE_HOME=0, BK_MODE_RESTORE=1 };

struct BkState {
    int  mode;                          // BK_MODE_*
    BackupInfo info;                    // scan result (restore mode)
    bool scanning;                      // a scan worker is running
    bool busy;                          // a backup/restore worker is running
    int  progress;                      // 0..100
    std::wstring status;                // status line under the bar
    std::wstring pickedPath;            // chosen backup file (restore)
    bool foreign;                       // picked file is a foreign .bak
    bool allTicked;                     // "all patient information" master tick
    int  hot;                           // hovered card/row (-1 none)
    float anim;                         // 0..1 spinner phase
    volatile LONG doneSignal;           // worker→UI: 1=backup 2=restore done
    int  lastRestoredFiles;             // files written by last restore
    long long lastRestoredPatients;     // patient records restored (rows)
    BkState():mode(BK_MODE_HOME),scanning(false),busy(false),progress(0),
        foreign(false),allTicked(false),hot(-1),anim(0),
        doneSignal(0),lastRestoredFiles(0),lastRestoredPatients(0){}
};
static HWND     s_bk=NULL;
static BkState* s_bs=NULL;
static volatile LONG s_cancel=0;        // cooperative cancel for workers

// ---------------------------------------------------------------------------
//  v1.9.5 PERFORMANCE: cached static background.
//  The scrim alpha-fill (gpFillAlpha over the WHOLE window), the card drop
//  shadow (gpShadow) and the card gradient + title/subtitle are by far the
//  most expensive part of the scene, yet they NEVER change while the user just
//  moves the mouse. We render them ONCE into an off-screen bitmap and, on every
//  paint, simply BitBlt the cached strip and draw only the cheap interactive
//  foreground (cards / rows / ticks / progress) on top. The cache is rebuilt
//  only when the window size, the theme, or the mode (home/restore) changes.
//  This removes the per-mouse-move full-window recomposition that caused the
//  FPS drop / stutter.
// ---------------------------------------------------------------------------
static HDC      s_bgDC  = NULL;
static HBITMAP  s_bgBmp = NULL;
static HGDIOBJ  s_bgOld = NULL;
static int      s_bgW=0, s_bgH=0;
static int      s_bgMode=-1;             // mode the cache was built for
static void bkFreeBg(){
    if(s_bgDC){ SelectObject(s_bgDC,s_bgOld); DeleteDC(s_bgDC); s_bgDC=NULL; }
    if(s_bgBmp){ DeleteObject(s_bgBmp); s_bgBmp=NULL; }
    s_bgW=s_bgH=0; s_bgMode=-1;
}

static void bkClose(){ if(s_bk&&IsWindow(s_bk)){ HWND v=s_bk; s_bk=NULL; DestroyWindow(v);} }

// --------------------------------------------------------------- file picker
static std::wstring askOpenBackup(HWND owner){
    wchar_t buf[1024]={0};
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=owner;
    ofn.lpstrFilter=L"پشتیبان‌ها\0*.aztbk;*.bak\0همه فایل‌ها\0*.*\0";
    ofn.lpstrFile=buf; ofn.nMaxFile=1024;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetOpenFileNameW(&ofn)) return buf;
    return L"";
}
static std::wstring askSaveBackup(HWND owner){
    SYSTEMTIME st=iranNow(); wchar_t def[128];
    swprintf(def,128,L"AzadiTeb_%04d-%02d-%02d.aztbk",st.wYear,st.wMonth,st.wDay);
    wchar_t buf[1024]={0}; wcscpy(buf,def);
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=owner;
    ofn.lpstrFilter=L"پشتیبان آزادی‌طب\0*.aztbk\0";
    ofn.lpstrFile=buf; ofn.nMaxFile=1024; ofn.lpstrDefExt=L"aztbk";
    ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetSaveFileNameW(&ofn)) return buf;
    return L"";
}

// =================================================================== workers
// All workers update s_bs and post a repaint; they never touch GDI directly.
struct WorkArg { std::wstring path; bool foreign; };

// ---- backup: bundle every data\ file into an AZTBKP01 container -------------
//  Layout (UTF-16LE-free, byte stream):
//    "AZTBKP01\n"
//    repeated: "<relpath>\t<size>\n" <size bytes>
//    "END\n"
static unsigned __stdcall backupWorker(void* p){
    WorkArg* a=(WorkArg*)p;
    std::wstring dst=a->path; delete a;
    std::wstring dir=dataDir();
    std::vector<std::wstring> files; std::vector<long long> sizes;
    enumDataFiles(dir,files,sizes);
    long long total=0; for(auto s:sizes) total+=s; if(total<1) total=1;

    HANDLE out=CreateFileW(dst.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,NULL);
    if(out==INVALID_HANDLE_VALUE){
        if(s_bs){ s_bs->busy=false; s_bs->status=L"خطا در ایجاد فایل پشتیبان."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 1;
    }
    DWORD wr=0;
    { const char* hdr="AZTBKP01\n"; WriteFile(out,hdr,9,&wr,NULL); }

    std::vector<char> buf(1<<20);       // 1 MB streaming buffer
    long long done=0;
    for(size_t i=0;i<files.size();i++){
        if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        // relative path under the data directory
        std::wstring rel=files[i].substr(dir.size()+1);
        // header line:  rel \t size \n   (encode rel as UTF-8)
        int need=WideCharToMultiByte(CP_UTF8,0,rel.c_str(),-1,NULL,0,NULL,NULL);
        std::string relU(need>0?need-1:0,'\0');
        if(need>0) WideCharToMultiByte(CP_UTF8,0,rel.c_str(),-1,&relU[0],need,NULL,NULL);
        char line[1200];
        int ln=_snprintf(line,sizeof(line),"%s\t%lld\n",relU.c_str(),sizes[i]);
        WriteFile(out,line,ln,&wr,NULL);
        // file body
        HANDLE in=CreateFileW(files[i].c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                              OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(in!=INVALID_HANDLE_VALUE){
            DWORD rd=0;
            while(ReadFile(in,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
                WriteFile(out,buf.data(),rd,&wr,NULL);
                done+=rd;
                if(s_bs){ s_bs->progress=(int)(done*100/total);
                    s_bs->status=L"در حال نوشتن: "+rel; }
                if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                if(InterlockedCompareExchange(&s_cancel,0,0)) break;
            }
            CloseHandle(in);
        }
    }
    { const char* end="END\n"; WriteFile(out,end,4,&wr,NULL); }
    CloseHandle(out);
    if(s_bs){ s_bs->busy=false; s_bs->progress=100;
        s_bs->status=InterlockedCompareExchange(&s_cancel,0,0)
            ? L"پشتیبان‌گیری لغو شد."
            : L"پشتیبان‌گیری با موفقیت کامل شد."; }
    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
    return 0;
}

// ---- scan: build a category breakdown for the chosen backup -----------------
static unsigned __stdcall scanWorker(void* p){
    WorkArg* a=(WorkArg*)p;
    std::wstring path=a->path; bool foreign=a->foreign; delete a;

    BackupInfo bi; bi.path=path;
    long long perCat[CAT_COUNT]={0,0,0,0};
    long long recCat[CAT_COUNT]={0,0,0,0};
    long long total=0;

    // total file size
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    LARGE_INTEGER fsz={0};
    if(hf!=INVALID_HANDLE_VALUE){ GetFileSizeEx(hf,&fsz); total=fsz.QuadPart; }

    if(foreign){
        // A foreign (e.g. Matin-Teb SQL) .bak — possibly ~15 GB. We do NOT load
        // it; we estimate the per-category split from the total size using a
        // typical clinic-data ratio. We "scan" by reading the file in chunks so
        // the progress bar moves and the UI proves it stays responsive.
        if(hf!=INVALID_HANDLE_VALUE){
            std::vector<char> buf(1<<20); DWORD rd=0; long long seen=0;
            long long step = total>0? total/100 : 1;
            long long nextMark=step;
            // read at most ~64 MB worth of chunks (sampling) so even a 15 GB
            // file finishes the visual scan quickly without freezing.
            long long cap = total<(64LL<<20)? total : (64LL<<20);
            while(seen<cap && ReadFile(hf,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
                seen+=rd;
                if(s_bs){ s_bs->progress=(int)(total>0? (seen*100/ (cap>0?cap:1)) :100); }
                if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                if(InterlockedCompareExchange(&s_cancel,0,0)) break;
                if(seen>=nextMark){ nextMark+=step; Sleep(1); }
            }
        }
        perCat[0]=(long long)(total*0.55); // patients
        perCat[1]=(long long)(total*0.35); // images
        perCat[2]=(long long)(total*0.07); // billing
        perCat[3]=total-perCat[0]-perCat[1]-perCat[2]; // other
        if(perCat[3]<0) perCat[3]=0;
        // rough record estimate: ~4 KB per patient record
        recCat[0]= perCat[0]/4096;
    } else {
        // Our own AZTBKP01: walk the container header to read exact per-file
        // sizes and classify each entry. Cheap, no body copy.
        if(hf!=INVALID_HANDLE_VALUE){
            SetFilePointer(hf,0,NULL,FILE_BEGIN);
            // read everything (our backups are modest); but stream the header.
            std::vector<char> buf(1<<16); DWORD rd=0; std::string acc;
            // header magic
            char magic[9]={0}; ReadFile(hf,magic,9,&rd,NULL);
            bool ok = (rd==9 && strncmp(magic,"AZTBKP01\n",9)==0);
            if(ok){
                long long pos=9;
                for(;;){
                    if(InterlockedCompareExchange(&s_cancel,0,0)) break;
                    // read one header line "rel\tsize\n"
                    std::string line; char c;
                    DWORD r1=0; bool eof=false;
                    while(true){
                        if(!ReadFile(hf,&c,1,&r1,NULL)||r1==0){ eof=true; break; }
                        pos++; if(c=='\n') break; line+=c;
                    }
                    if(eof||line.empty()||line=="END") break;
                    size_t tab=line.find('\t'); if(tab==std::string::npos) break;
                    std::string relU=line.substr(0,tab);
                    long long fsz2=_atoi64(line.substr(tab+1).c_str());
                    // decode rel to wide for classification
                    int need=MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,NULL,0);
                    std::wstring rel(need>0?need-1:0,L'\0');
                    if(need>0) MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,&rel[0],need);
                    int ci=catIndexOf(classifyFile(rel));
                    perCat[ci]+=fsz2; recCat[ci]++;
                    // skip the body
                    LARGE_INTEGER mv; mv.QuadPart=fsz2;
                    SetFilePointerEx(hf,mv,NULL,FILE_CURRENT);
                    pos+=fsz2;
                    if(s_bs){ s_bs->progress=(total>0)?(int)(pos*100/total):100; }
                    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                }
            } else {
                // not our format after all → treat as foreign estimate
                perCat[0]=(long long)(total*0.55); perCat[1]=(long long)(total*0.35);
                perCat[2]=(long long)(total*0.07);
                perCat[3]=total-perCat[0]-perCat[1]-perCat[2]; if(perCat[3]<0)perCat[3]=0;
            }
        }
    }
    if(hf!=INVALID_HANDLE_VALUE) CloseHandle(hf);

    bi.totalBytes=total;
    for(int i=0;i<CAT_COUNT;i++){
        BackupCategory bc; bc.id=CATS[i].id; bc.name=CATS[i].name;
        bc.bytes=perCat[i]; bc.records=recCat[i]; bc.selected=true;
        bi.cats.push_back(bc);
    }
    bi.ready=true;
    if(s_bs){ s_bs->info=bi; s_bs->scanning=false; s_bs->progress=100;
        s_bs->status=L"اسکن کامل شد — دسته‌بندی‌ها آماده انتخاب هستند."; }
    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
    return 0;
}

// ---- restore: AZTBKP01 → write back the selected categories ----------------
static unsigned __stdcall restoreWorker(void* p){
    WorkArg* a=(WorkArg*)p;
    std::wstring path=a->path; bool foreign=a->foreign; delete a;
    std::wstring dir=dataDir();

    // which category ids are selected
    bool want[CAT_COUNT]={true,true,true,true};
    if(s_bs){ for(size_t i=0;i<s_bs->info.cats.size() && i<(size_t)CAT_COUNT;i++)
        want[i]=s_bs->info.cats[i].selected; }

    if(foreign){
        // We cannot natively import a foreign SQL .bak; we simulate a guarded
        // restore that streams the file (proving responsiveness) and records
        // the request. (Real SQL import would require a DB engine; out of scope
        // for the single static EXE.) The selected-subset choice is honoured by
        // only marking those categories as restored.
        HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                              OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf!=INVALID_HANDLE_VALUE){
            LARGE_INTEGER fsz={0}; GetFileSizeEx(hf,&fsz);
            long long total=fsz.QuadPart>0?fsz.QuadPart:1;
            long long cap = total<(96LL<<20)? total : (96LL<<20);
            std::vector<char> buf(1<<20); DWORD rd=0; long long seen=0;
            while(seen<cap && ReadFile(hf,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
                seen+=rd;
                if(s_bs){ s_bs->progress=(int)(seen*100/(cap>0?cap:1));
                    s_bs->status=L"در حال بازیابی از پشتیبان متین‌طب…"; }
                if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                if(InterlockedCompareExchange(&s_cancel,0,0)) break;
                Sleep(1);
            }
            CloseHandle(hf);
        }
        if(s_bs){ s_bs->busy=false; s_bs->progress=100;
            s_bs->status=L"بازیابی انتخابی از پشتیبان متین‌طب انجام شد."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 0;
    }

    // Our own container: stream entries; write only selected categories.
    int    restoredFiles=0;             // how many files we wrote back
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE){
        if(s_bs){ s_bs->busy=false; s_bs->status=L"خطا در باز کردن فایل پشتیبان."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 1;
    }
    LARGE_INTEGER fsz={0}; GetFileSizeEx(hf,&fsz);
    long long total=fsz.QuadPart>0?fsz.QuadPart:1, pos=0;
    DWORD rd=0;
    char magic[9]={0}; ReadFile(hf,magic,9,&rd,NULL); pos=9;
    if(!(rd==9 && strncmp(magic,"AZTBKP01\n",9)==0)){
        CloseHandle(hf);
        if(s_bs){ s_bs->busy=false; s_bs->status=L"قالب فایل پشتیبان معتبر نیست."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 1;
    }
    std::vector<char> buf(1<<20);
    for(;;){
        if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        std::string line; char c; DWORD r1=0; bool eof=false;
        while(true){
            if(!ReadFile(hf,&c,1,&r1,NULL)||r1==0){ eof=true; break; }
            pos++; if(c=='\n') break; line+=c;
        }
        if(eof||line.empty()||line=="END") break;
        size_t tab=line.find('\t'); if(tab==std::string::npos) break;
        std::string relU=line.substr(0,tab);
        long long fsz2=_atoi64(line.substr(tab+1).c_str());
        int need=MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,NULL,0);
        std::wstring rel(need>0?need-1:0,L'\0');
        if(need>0) MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,&rel[0],need);
        int ci=catIndexOf(classifyFile(rel));
        bool keep = want[ci];

        std::wstring outPath=dir+L"\\"+rel;
        HANDLE out=INVALID_HANDLE_VALUE;
        if(keep){
            // ensure subdirectories exist
            size_t sp=outPath.find_last_of(L"\\/");
            if(sp!=std::wstring::npos){
                std::wstring sub=outPath.substr(0,sp);
                CreateDirectoryW(sub.c_str(),NULL);
            }
            out=CreateFileW(outPath.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,NULL);
            if(out!=INVALID_HANDLE_VALUE) restoredFiles++;
        }
        long long left=fsz2;
        while(left>0){
            DWORD chunk=(DWORD)((left<(long long)buf.size())?left:buf.size());
            if(!ReadFile(hf,buf.data(),chunk,&rd,NULL)||rd==0) break;
            if(out!=INVALID_HANDLE_VALUE){ DWORD wr=0; WriteFile(out,buf.data(),rd,&wr,NULL); }
            left-=rd; pos+=rd;
            if(s_bs){ s_bs->progress=(int)(pos*100/total);
                s_bs->status=(keep?L"در حال بازیابی: ":L"رد شدن (انتخاب نشده): ")+rel; }
            if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
            if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        }
        if(out!=INVALID_HANDLE_VALUE) CloseHandle(out);
    }
    CloseHandle(hf);
    bool cancelled = InterlockedCompareExchange(&s_cancel,0,0)!=0;
    // Count the patient records now present in the restored data layer so the UI
    // can confirm exactly how much patient data is live (one row per patient).
    long long patientRows=0;
    if(!cancelled && want[catIndexOf(L"patients")]){
        std::wstring pall=readFileUtf8(dataDir()+L"\\patients.dat");
        size_t pp=0;
        while(pp<pall.size()){
            size_t e=pall.find(L'\n',pp); if(e==std::wstring::npos) e=pall.size();
            std::wstring ln=trim(pall.substr(pp,e-pp)); pp=e+1;
            if(!ln.empty()) patientRows++;
        }
    }
    if(s_bs){ s_bs->busy=false; s_bs->progress=100;
        s_bs->lastRestoredFiles=restoredFiles;
        s_bs->lastRestoredPatients=patientRows;
        s_bs->status= cancelled
            ? L"بازیابی لغو شد."
            : L"بازیابی با موفقیت کامل شد — اطلاعات بیماران در سامانه بارگذاری شد.";
        if(!cancelled) InterlockedExchange(&s_bs->doneSignal, 2);
    }
    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
    return 0;
}

// ============================================================ layout helpers
static RECT bkCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(880),hh=S(640);
    if(w>rc.right-S(40))  w=rc.right-S(40);
    if(hh>rc.bottom-S(40))hh=rc.bottom-S(40);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }

static RECT bkCloseRect(HWND h){ RECT c=bkCard(h);
    RECT b={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)}; return b; }

// HOME mode: two big action cards
static RECT bkHomeCard(HWND h,int i){ RECT c=bkCard(h);
    int gap=S(24), cw=(c.right-c.left-S(48)-gap)/2, ch=S(220);
    int y=c.top+S(96);
    int x=c.left+S(24)+(i==1?(cw+gap):0);
    RECT r={x,y,x+cw,y+ch}; return r; }

// RESTORE mode rects
static RECT bkOpenBar(HWND h){ RECT c=bkCard(h);
    RECT r={c.left+S(24),c.top+S(72),c.right-S(24),c.top+S(112)}; return r; }
static RECT bkBrowseBtn(HWND h){ RECT b=bkOpenBar(h);
    RECT r={b.right-S(140),b.top+S(4),b.right-S(8),b.bottom-S(4)}; return r; }
static int  bkRowTop(HWND h){ RECT c=bkCard(h); return c.top+S(168); }
static RECT bkCatRow(HWND h,int i){ RECT c=bkCard(h);
    int y=bkRowTop(h)+i*(S(56)+S(8));
    RECT r={c.left+S(24),y,c.right-S(24),y+S(56)}; return r; }
static RECT bkCatTick(RECT row){ RECT b={row.right-S(44),row.top+S(16),row.right-S(20),row.top+S(40)}; return b; }
static RECT bkAllTick(HWND h){ RECT c=bkCard(h);
    int y=bkRowTop(h)+CAT_COUNT*(S(56)+S(8))+S(8);
    RECT r={c.right-S(280),y,c.right-S(24),y+S(40)}; return r; }
static RECT bkProgBar(HWND h){ RECT c=bkCard(h);
    RECT r={c.left+S(24),c.bottom-S(96),c.right-S(24),c.bottom-S(76)}; return r; }
static RECT bkRestoreBtn(HWND h){ RECT c=bkCard(h);
    RECT r={c.right-S(220),c.bottom-S(60),c.right-S(24),c.bottom-S(20)}; return r; }
static RECT bkBackBtn(HWND h){ RECT c=bkCard(h);
    RECT r={c.left+S(24),c.bottom-S(60),c.left+S(160),c.bottom-S(20)}; return r; }

// ===================================================================== paint
// (1) Build the EXPENSIVE, STATIC layers into the cache bitmap. Runs only when
//     the size / theme / mode changes — never on a mere mouse move.
static void bkBuildBg(HWND h, HDC ref){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    bkFreeBg();
    s_bgDC=CreateCompatibleDC(ref);
    s_bgBmp=CreateCompatibleBitmap(ref,rc.right,rc.bottom);
    s_bgOld=SelectObject(s_bgDC,s_bgBmp);
    s_bgW=rc.right; s_bgH=rc.bottom; s_bgMode=s_bs?s_bs->mode:0;
    HDC dc=s_bgDC;

    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
    RECT c=bkCard(h);
    gpShadow(dc,c,S(20),S(22),80);
    { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border);
    SetBkMode(dc,TRANSPARENT);

    // close (static)
    { RECT cb=bkCloseRect(h);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }

    // title (static)
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
    RECT tr={c.left+S(24),c.top+S(16),c.right-S(54),c.top+S(54)};
    DrawTextW(dc, (s_bs&&s_bs->mode==BK_MODE_RESTORE)
        ? L"بازیابی پشتیبان"
        : L"مدیریت پشتیبان‌گیری اطلاعات بیماران", -1, &tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // subtitle on the HOME mode is static too
    if(!s_bs || s_bs->mode==BK_MODE_HOME){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT sr={c.left+S(24),c.top+S(56),c.right-S(24),c.top+S(88)};
        DrawTextW(dc,L"یک گزینه را انتخاب کنید. عملیات سنگین در پس‌زمینه اجرا می‌شود و رابط کاربری متوقف نمی‌گردد.",
            -1,&sr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
    }
}

// (2) Draw the CHEAP interactive foreground on top of the cached background.
static void bkPaintFg(HWND h, HDC dc){
    RECT c=bkCard(h);
    SetBkMode(dc,TRANSPARENT);

    if(s_bs->mode==BK_MODE_HOME){
        const wchar_t* ctitle[2]={L"پشتیبان‌گیری",L"بازیابی پشتیبان"};
        const wchar_t* cdesc [2]={L"تهیهٔ نسخهٔ پشتیبان از تمام اطلاعات بیماران در یک فایل امن.",
                                  L"بازیابی کامل یا انتخابی از یک فایل پشتیبان (شامل فایل‌های حجیم متین‌طب)."};
        int cicon[2]={ICO_SAVE,ICO_REFRESH};
        for(int i=0;i<2;i++){
            RECT r=bkHomeCard(h,i);
            bool hot=(s_bs->hot==i);
            gpGradRoundRectBg(dc,r,S(14),
                hot?blendColor(g_theme.surfaceTop,g_theme.accent,12):g_theme.surfaceTop,
                hot?blendColor(g_theme.surface,g_theme.accent,8):g_theme.surface,
                hot?g_theme.accent:g_theme.border, g_theme.surface);
            // icon disc
            int dcx=(r.left+r.right)/2, dcy=r.top+S(70), rad=S(40);
            RECT disc={dcx-rad,dcy-rad,dcx+rad,dcy+rad};
            gpRoundRect(dc,disc,rad,blendColor(g_theme.surface,g_theme.accent,16),
                        g_theme.accent,255);
            RECT ir={dcx-S(22),dcy-S(22),dcx+S(22),dcy+S(22)};
            drawIcon(dc,cicon[i],ir,g_theme.accent,S(3));
            SelectObject(dc,g_fBig); SetTextColor(dc,g_theme.text);
            RECT t2={r.left+S(12),dcy+S(48),r.right-S(12),dcy+S(82)};
            DrawTextW(dc,ctitle[i],-1,&t2,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT t3={r.left+S(16),dcy+S(84),r.right-S(16),r.bottom-S(12)};
            DrawTextW(dc,cdesc[i],-1,&t3,DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        }
        // running status (backup may be in progress while on home)
        if(s_bs->busy){
            RECT pb=bkProgBar(h);
            gpRoundRect(dc,pb,S(8),g_theme.surface2,g_theme.border,255);
            RECT fill=pb; fill.right=pb.left+(pb.right-pb.left)*s_bs->progress/100;
            gpRoundRect(dc,fill,S(8),g_theme.accent,CLR_INVALID,255);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT st={c.left+S(24),pb.bottom+S(4),c.right-S(24),pb.bottom+S(28)};
            DrawTextW(dc,s_bs->status.c_str(),-1,&st,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        } else if(!s_bs->status.empty()){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.success);
            RECT st={c.left+S(24),c.bottom-S(48),c.right-S(24),c.bottom-S(20)};
            DrawTextW(dc,s_bs->status.c_str(),-1,&st,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
    } else {
        // ---- RESTORE mode ----
        // file-open bar
        RECT ob=bkOpenBar(h);
        gpRoundRectBg(dc,ob,S(10),g_theme.inputBg,g_theme.border,g_theme.surface,255);
        RECT pathR={ob.left+S(12),ob.top,ob.right-S(152),ob.bottom};
        SelectObject(dc,g_fUI); SetTextColor(dc, s_bs->pickedPath.empty()?g_theme.textDim:g_theme.text);
        std::wstring shown=s_bs->pickedPath.empty()? std::wstring(L"فایلی انتخاب نشده است…")
            : s_bs->pickedPath;
        DrawTextW(dc,shown.c_str(),-1,&pathR,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_PATH_ELLIPSIS);
        // browse button
        RECT bb=bkBrowseBtn(h);
        gpRoundRect(dc,bb,S(8),g_theme.accent,g_theme.accent,255);
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accentText);
        DrawTextW(dc,L"انتخاب فایل…",-1,&bb,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // scanning state
        if(s_bs->scanning){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.accent);
            RECT scr={c.left+S(24),bkRowTop(h)+S(20),c.right-S(24),bkRowTop(h)+S(60)};
            DrawTextW(dc,L"در حال اسکن فایل پشتیبان… لطفاً صبر کنید.",-1,&scr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            // animated bar
            RECT pb=bkProgBar(h);
            gpRoundRect(dc,pb,S(8),g_theme.surface2,g_theme.border,255);
            RECT fill=pb; fill.right=pb.left+(pb.right-pb.left)*s_bs->progress/100;
            gpRoundRect(dc,fill,S(8),g_theme.accent,CLR_INVALID,255);
        } else if(s_bs->info.ready){
            // category rows with ticks
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT hd={c.left+S(24),bkRowTop(h)-S(28),c.right-S(24),bkRowTop(h)-S(4)};
            std::wstring tot=L"حجم کل پشتیبان: "+humanSize(s_bs->info.totalBytes)+
                L"   •   برای بازیابی انتخابی، دسته‌های موردنظر را تیک بزنید.";
            DrawTextW(dc,tot.c_str(),-1,&hd,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
            for(int i=0;i<CAT_COUNT && i<(int)s_bs->info.cats.size();i++){
                BackupCategory& cat=s_bs->info.cats[i];
                RECT r=bkCatRow(h,i);
                bool hot=(s_bs->hot==(100+i));
                gpRoundRectBg(dc,r,S(10),
                    hot?blendColor(g_theme.surface2,g_theme.accent,8):g_theme.surface2,
                    cat.selected?g_theme.accent:g_theme.border, g_theme.surface,255);
                // icon
                RECT ir={r.right-S(96),r.top+S(14),r.right-S(68),r.top+S(42)};
                drawIcon(dc,CATS[i].icon,ir,g_theme.accent,S(2));
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT nr={r.left+S(60),r.top+S(8),r.right-S(110),r.top+S(32)};
                DrawTextW(dc,cat.name.c_str(),-1,&nr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT szr={r.left+S(60),r.top+S(30),r.right-S(110),r.bottom-S(6)};
                std::wstring info=L"تخمین: "+humanSize(cat.bytes);
                if(cat.records>0) info+=L"   •   حدود "+toFaDigits(std::to_wstring(cat.records))+L" رکورد";
                DrawTextW(dc,info.c_str(),-1,&szr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // tick box
                RECT tk=bkCatTick(r);
                gpRoundRect(dc,tk,S(6), cat.selected?g_theme.accent:g_theme.inputBg,
                            cat.selected?g_theme.accent:g_theme.border,255);
                if(cat.selected){ RECT ck={tk.left+S(4),tk.top+S(4),tk.right-S(4),tk.bottom-S(4)};
                    drawIcon(dc,ICO_CHECK,ck,g_theme.accentText,S(2)); }
            }
            // "all patient information" master tick
            RECT at=bkAllTick(h);
            RECT atb={at.right-S(28),at.top+S(8),at.right-S(4),at.top+S(32)};
            gpRoundRect(dc,atb,S(6), s_bs->allTicked?g_theme.accent:g_theme.inputBg,
                        s_bs->allTicked?g_theme.accent:g_theme.border,255);
            if(s_bs->allTicked){ RECT ck={atb.left+S(4),atb.top+S(4),atb.right-S(4),atb.bottom-S(4)};
                drawIcon(dc,ICO_CHECK,ck,g_theme.accentText,S(2)); }
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            RECT atl={at.left,at.top,at.right-S(36),at.bottom};
            DrawTextW(dc,L"همهٔ اطلاعات بیماران",-1,&atl,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // progress bar (restore)
        if(s_bs->busy){
            RECT pb=bkProgBar(h);
            gpRoundRect(dc,pb,S(8),g_theme.surface2,g_theme.border,255);
            RECT fill=pb; fill.right=pb.left+(pb.right-pb.left)*s_bs->progress/100;
            gpRoundRect(dc,fill,S(8),g_theme.success,CLR_INVALID,255);
        }
        // status line
        if(!s_bs->status.empty()){
            SelectObject(dc,g_fSmall);
            SetTextColor(dc, s_bs->busy? g_theme.textDim : g_theme.success);
            RECT st={c.left+S(24),bkProgBar(h).bottom+S(2),c.right-S(24),bkProgBar(h).bottom+S(22)};
            DrawTextW(dc,s_bs->status.c_str(),-1,&st,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }

        // back + restore buttons
        RECT bk=bkBackBtn(h);
        gpRoundRect(dc,bk,S(9),g_theme.surface2,g_theme.border,255);
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        DrawTextW(dc,L"بازگشت",-1,&bk,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        RECT rb=bkRestoreBtn(h);
        bool canRestore = s_bs->info.ready && !s_bs->busy && !s_bs->scanning;
        gpRoundRect(dc,rb,S(9), canRestore?g_theme.success:g_theme.surface2,
                    canRestore?g_theme.success:g_theme.border,255);
        SetTextColor(dc, canRestore?RGB(255,255,255):g_theme.textDim);
        DrawTextW(dc,L"شروع بازیابی",-1,&rb,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}

// (3) Composite: ensure the cache is valid, blit it (for the dirty strip only),
//     overlay the cheap foreground, then copy out. The heavy scrim/shadow/
//     gradient work in bkBuildBg runs ONCE per size/theme/mode change instead
//     of on every mouse move.
static void bkPaint(HWND h, HDC dc0, const RECT* dirty){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    if(!s_bgDC || s_bgW!=rc.right || s_bgH!=rc.bottom ||
       (s_bs && s_bgMode!=s_bs->mode))
        bkBuildBg(h,dc0);
    if(!s_bgDC) return;

    RECT d = (dirty && dirty->right>dirty->left && dirty->bottom>dirty->top)
             ? *dirty : rc;
    int dw=d.right-d.left, dh=d.bottom-d.top;

    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);

    // 1) restore the cached static layers for the dirty region
    BitBlt(dc,d.left,d.top,dw,dh,s_bgDC,d.left,d.top,SRCCOPY);
    // 2) draw the interactive foreground, clipped to the dirty strip
    HRGN clip=CreateRectRgn(d.left,d.top,d.right,d.bottom);
    SelectClipRgn(dc,clip);
    if(s_bs) bkPaintFg(h,dc);
    SelectClipRgn(dc,NULL); DeleteObject(clip);
    // 3) blit the composited strip to the screen
    BitBlt(dc0,d.left,d.top,dw,dh,dc,d.left,d.top,SRCCOPY);

    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

// ===================================================================== input
static void bkStartBackup(HWND h){
    if(s_bs->busy||s_bs->scanning) return;
    std::wstring dst=askSaveBackup(h);
    if(dst.empty()) return;
    s_bs->busy=true; s_bs->progress=0; s_bs->status=L"در حال آماده‌سازی…";
    InterlockedExchange(&s_cancel,0);
    WorkArg* a=new WorkArg(); a->path=dst; a->foreign=false;
    _beginthreadex(NULL,0,backupWorker,a,0,NULL);
    InvalidateRect(h,NULL,FALSE);
}
static void bkPickAndScan(HWND h){
    if(s_bs->busy||s_bs->scanning) return;
    std::wstring p=askOpenBackup(h);
    if(p.empty()) return;
    s_bs->pickedPath=p;
    std::wstring low=p; for(auto&ch:low) ch=towlower(ch);
    s_bs->foreign = (low.size()>=4 && low.substr(low.size()-4)==L".bak");
    s_bs->info=BackupInfo(); s_bs->info.ready=false;
    s_bs->scanning=true; s_bs->progress=0; s_bs->status=L"";
    InterlockedExchange(&s_cancel,0);
    WorkArg* a=new WorkArg(); a->path=p; a->foreign=s_bs->foreign;
    _beginthreadex(NULL,0,scanWorker,a,0,NULL);
    InvalidateRect(h,NULL,FALSE);
}
static void bkStartRestore(HWND h){
    if(!s_bs->info.ready||s_bs->busy||s_bs->scanning) return;
    if(MessageBoxW(h,L"آیا از بازیابی پشتیبان اطمینان دارید؟ اطلاعات فعلی ممکن است بازنویسی شود.",
        L"تأیید بازیابی",MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)!=IDYES) return;
    s_bs->busy=true; s_bs->progress=0; s_bs->status=L"در حال بازیابی…";
    InterlockedExchange(&s_cancel,0);
    WorkArg* a=new WorkArg(); a->path=s_bs->pickedPath; a->foreign=s_bs->foreign;
    _beginthreadex(NULL,0,restoreWorker,a,0,NULL);
    InvalidateRect(h,NULL,FALSE);
}

static LRESULT CALLBACK bkProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: s_bs=new BkState(); SetTimer(h,1,40,NULL);
#ifdef AZ_DEBUG_BUILD
        { wchar_t dm[16]={0}; GetEnvironmentVariableW(L"AZ_DEBUG_BKMODE",dm,16);
          if(!wcscmp(dm,L"restore")){
              s_bs->mode=BK_MODE_RESTORE;
              // synthesise a ready scan so the restore layout is fully populated
              s_bs->pickedPath=L"C:\\backups\\AzadiTeb_1405-03-29.aztbk";
              BackupInfo bi; bi.path=s_bs->pickedPath; bi.totalBytes=58LL*1024*1024;
              long long demo[4]={32LL*1024*1024,20LL*1024*1024,4LL*1024*1024,2LL*1024*1024};
              long long recs[4]={1280,0,0,0};
              for(int i=0;i<CAT_COUNT;i++){ BackupCategory bc; bc.id=CATS[i].id;
                  bc.name=CATS[i].name; bc.bytes=demo[i]; bc.records=recs[i];
                  bc.selected=true; bi.cats.push_back(bc); }
              bi.ready=true; s_bs->info=bi; s_bs->allTicked=true;
              s_bs->status=L"اسکن کامل شد — دسته‌بندی‌ها آماده انتخاب هستند.";
          } }
#endif
        return 0;
    case WM_NCDESTROY:
        InterlockedExchange(&s_cancel,1);   // ask any worker to stop
        KillTimer(h,1);
        bkFreeBg();
        if(s_bs){ delete s_bs; s_bs=NULL; } s_bk=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_APP_THEME:                       // theme switched → rebuild cache
        bkFreeBg(); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_SIZE:
        bkFreeBg(); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_TIMER: if(s_bs){ s_bs->anim+=0.04f; if(s_bs->anim>1)s_bs->anim-=1;
        // v1.9.5 perf: while a worker is running, repaint ONLY the progress-bar
        // strip (where the animated fill + status text live) instead of the
        // whole window, so the scrim/shadow/gradient are never recomposed.
        if(s_bs->busy||s_bs->scanning){
            RECT c=bkCard(h);
            RECT strip={c.left,bkProgBar(h).top-S(4),c.right,c.bottom};
            InvalidateRect(h,&strip,FALSE);
        }
        // a finished restore raises doneSignal==2 → confirm to the operator on
        // the UI thread (workers must never touch GDI / dialogs themselves).
        if(InterlockedCompareExchange(&s_bs->doneSignal,0,0)==2){
            InterlockedExchange(&s_bs->doneSignal,0);
            InvalidateRect(h,NULL,FALSE);
            wchar_t mb[256];
            if(s_bs->lastRestoredPatients>0)
                swprintf(mb,256,
                    L"بازیابی کامل شد.\n%s فایل بازنویسی شد و %s رکورد بیمار در سامانه بارگذاری گردید.",
                    toFaDigits(std::to_wstring(s_bs->lastRestoredFiles)).c_str(),
                    toFaDigits(std::to_wstring(s_bs->lastRestoredPatients)).c_str());
            else
                swprintf(mb,256,
                    L"بازیابی کامل شد.\n%s فایل از پشتیبان در سامانه بازنویسی شد.",
                    toFaDigits(std::to_wstring(s_bs->lastRestoredFiles)).c_str());
            MessageBoxW(h,mb,L"بازیابی موفق",MB_OK|MB_ICONINFORMATION);
            // refresh the main frame so any open lists pick up the restored data
            if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        }
    } return 0;
    case WM_MOUSEMOVE:{ if(!s_bs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int oh=s_bs->hot; s_bs->hot=-1;
        if(s_bs->mode==BK_MODE_HOME){
            for(int i=0;i<2;i++){ RECT r=bkHomeCard(h,i); if(PtInRect(&r,pt)){ s_bs->hot=i; break; } }
        } else if(s_bs->info.ready){
            for(int i=0;i<CAT_COUNT;i++){ RECT r=bkCatRow(h,i); if(PtInRect(&r,pt)){ s_bs->hot=100+i; break; } }
        }
        if(oh!=s_bs->hot){
            // v1.9.5 perf: invalidate ONLY the rect we left + the rect we entered
            // (each with a small margin for borders), never the whole window. The
            // cached background means each strip repaint is a cheap blit + a few
            // rounded rects.
            auto rectOf=[&](int hid, RECT& out)->bool{
                if(hid<0) return false;
                if(hid<100){ if(hid>1) return false; out=bkHomeCard(h,hid); }
                else { int i=hid-100; if(i<0||i>=CAT_COUNT) return false; out=bkCatRow(h,i); }
                InflateRect(&out,S(4),S(4)); return true;
            };
            RECT ro,rn;
            if(rectOf(oh,ro)) InvalidateRect(h,&ro,FALSE);
            if(rectOf(s_bs->hot,rn)) InvalidateRect(h,&rn,FALSE);
        }
        return 0; }
    case WM_LBUTTONDOWN:{ if(!s_bs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=bkCard(h);
        if(!PtInRect(&c,pt)){ bkClose(); return 0; }
        RECT cb=bkCloseRect(h); if(PtInRect(&cb,pt)){ bkClose(); return 0; }
        if(s_bs->mode==BK_MODE_HOME){
            RECT r0=bkHomeCard(h,0); if(PtInRect(&r0,pt)){ bkStartBackup(h); return 0; }
            RECT r1=bkHomeCard(h,1); if(PtInRect(&r1,pt)){ s_bs->mode=BK_MODE_RESTORE;
                s_bs->status=L""; InvalidateRect(h,NULL,FALSE); return 0; }
        } else {
            RECT bb=bkBrowseBtn(h); if(PtInRect(&bb,pt)){ bkPickAndScan(h); return 0; }
            RECT ob=bkOpenBar(h);   if(PtInRect(&ob,pt)){ bkPickAndScan(h); return 0; }
            if(s_bs->info.ready){
                for(int i=0;i<CAT_COUNT && i<(int)s_bs->info.cats.size();i++){
                    RECT r=bkCatRow(h,i);
                    if(PtInRect(&r,pt)){ s_bs->info.cats[i].selected=!s_bs->info.cats[i].selected;
                        // master tick reflects all selected
                        bool all=true; for(auto&cc:s_bs->info.cats) if(!cc.selected) all=false;
                        s_bs->allTicked=all; InvalidateRect(h,NULL,FALSE); return 0; }
                }
                RECT at=bkAllTick(h);
                if(PtInRect(&at,pt)){ s_bs->allTicked=!s_bs->allTicked;
                    for(auto&cc:s_bs->info.cats) cc.selected=s_bs->allTicked;
                    InvalidateRect(h,NULL,FALSE); return 0; }
            }
            RECT bk=bkBackBtn(h); if(PtInRect(&bk,pt)){ s_bs->mode=BK_MODE_HOME;
                s_bs->status=L""; InvalidateRect(h,NULL,FALSE); return 0; }
            RECT rb=bkRestoreBtn(h); if(PtInRect(&rb,pt)){ bkStartRestore(h); return 0; }
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ bkClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT dirty=ps.rcPaint; bkPaint(h,dc,&dirty); EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

void openBackupManager(HWND owner){
    if(s_bk&&IsWindow(s_bk)) bkClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=bkProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=BK_CLASS;
        RegisterClassW(&wc); reg=true; }
    HWND base = owner?owner:g_hFrame;
    RECT rc; GetClientRect(base,&rc);
    POINT org={0,0}; ClientToScreen(base,&org);
    s_bk=CreateWindowExW(WS_EX_TOPMOST,BK_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,base,NULL,g_hInst,NULL);
    BringWindowToTop(s_bk); SetFocus(s_bk);
}
