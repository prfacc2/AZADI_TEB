// ============================================================================
//  sections.cpp — implementation of the Sections registry (release 1.4.0, §2).
//  Storage: data\sections.dat, one record per line, '|' separated:
//      id|code|name_fa|kind|is_active|created_at|updated_at
//  Lines are escaped so the separator never appears inside a field.
// ============================================================================
#include "app.h"
#include "sections.h"
#include "ui_kit.h"
#include <algorithm>

static std::wstring sec_path(){ return dataDir()+L"\\sections.dat"; }

static std::wstring sec_esc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::vector<std::wstring> sec_split(const std::wstring& s, wchar_t sep){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(sep,pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}
static std::wstring sec_now(){
    SYSTEMTIME st=iranNow(); wchar_t b[40];
    swprintf(b,40,L"%04d-%02d-%02dT%02d:%02d:%02d",
             st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    return b;
}

static std::vector<Section> sec_readAll(){
    std::vector<Section> v;
    std::wstring all = readFileUtf8(sec_path());
    size_t pos=0;
    while(pos < all.size()){
        size_t e = all.find(L'\n', pos);
        if(e==std::wstring::npos) e=all.size();
        std::wstring line = all.substr(pos, e-pos);
        pos = e+1;
        while(!line.empty() && (line.back()==L'\r'||line.back()==L'\n')) line.pop_back();
        if(line.empty()) continue;
        auto f = sec_split(line, L'|');
        if(f.size() < 7) continue;
        Section s;
        s.id        = _wtoi(f[0].c_str());
        s.code      = f[1];
        s.name_fa   = f[2];
        s.kind      = f[3];
        s.is_active = _wtoi(f[4].c_str());
        s.created_at= f[5];
        s.updated_at= f[6];
        // §7 (1.14.0): optional 8th field = net_meta. Older files have only 7
        // fields and load unchanged (net_meta stays empty).
        if(f.size() >= 8) s.net_meta = f[7];
        v.push_back(s);
    }
    return v;
}

static bool sec_writeAll(const std::vector<Section>& v){
    std::wstring out;
    for(const auto& s : v){
        wchar_t idb[16]; swprintf(idb,16,L"%d",s.id);
        wchar_t act[8];  swprintf(act,8,L"%d",s.is_active);
        out += idb; out += L'|';
        out += sec_esc(s.code);    out += L'|';
        out += sec_esc(s.name_fa); out += L'|';
        out += sec_esc(s.kind);    out += L'|';
        out += act;                out += L'|';
        out += sec_esc(s.created_at);out += L'|';
        out += sec_esc(s.updated_at);out += L'|';
        out += sec_esc(s.net_meta); out += L"\r\n";
    }
    return writeFileUtf8(sec_path(), out, false);
}

void Sections_Init(){
    std::vector<Section> v = sec_readAll();
    if(!v.empty()) return;     // already seeded
    struct Seed { const wchar_t* code; const wchar_t* name; const wchar_t* kind; };
    // §7 (1.14.0): seeds use the stable category-code scheme
    // (REC/APR/LAB/INJ/PHR/BIL/RAD/PHY). Codes are durable routing/sync keys;
    // the Persian names are display-only and may be renamed later without
    // breaking any binding.
    static const Seed seeds[] = {
        { L"REC01", L"\u067e\u0630\u06cc\u0631\u0634 \u0645\u0631\u06a9\u0632\u06cc", L"reception" },
        { L"REC02", L"\u067e\u0630\u06cc\u0631\u0634 \u0637\u0628\u0642\u0647 \u06f1", L"reception" },
        { L"APR01", L"\u0646\u0648\u0628\u062a\u200c\u062f\u0647\u06cc", L"appointment" },
        { L"INJ01", L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0648 \u067e\u0627\u0646\u0633\u0645\u0627\u0646", L"injection" },
        { L"LAB01", L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647", L"lab" },
        { L"PHR01", L"\u062f\u0627\u0631\u0648\u062e\u0627\u0646\u0647", L"pharmacy" },
        { L"BIL01", L"\u0635\u0646\u062f\u0648\u0642 / \u062d\u0633\u0627\u0628\u062f\u0627\u0631\u06cc", L"billing" },
        { L"RAD01", L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc", L"radiology" },
        { L"PHY01", L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc", L"physio" },
    };
    std::wstring now = sec_now();
    int id=1;
    for(const auto& s : seeds){
        Section x; x.id=id++; x.code=s.code; x.name_fa=s.name; x.kind=s.kind;
        x.is_active=1; x.created_at=now; x.updated_at=now;
        v.push_back(x);
    }
    sec_writeAll(v);
}

int Sections_All(std::vector<Section>& out){
    out = sec_readAll();
    std::sort(out.begin(), out.end(), [](const Section&a,const Section&b){return a.id<b.id;});
    return (int)out.size();
}

int Sections_Find(const std::wstring& query, std::vector<Section>& out){
    out.clear();
    std::vector<Section> all = sec_readAll();
    std::sort(all.begin(), all.end(), [](const Section&a,const Section&b){return a.id<b.id;});
    std::wstring q = uikit::NormalizeFa(query);
    if(q.empty()){ out = all; return (int)out.size(); }
    for(const auto& s : all){
        std::wstring code = uikit::NormalizeFa(s.code);
        std::wstring name = uikit::NormalizeFa(s.name_fa);
        if(code.find(q)!=std::wstring::npos || name.find(q)!=std::wstring::npos)
            out.push_back(s);
    }
    return (int)out.size();
}

int Sections_Upsert(const Section& s){
    std::vector<Section> v = sec_readAll();
    std::wstring now = sec_now();
    if(s.id > 0){
        for(auto& x : v){
            if(x.id==s.id){
                std::wstring created = x.created_at;
                x = s; x.created_at = created; x.updated_at = now;
                sec_writeAll(v); return x.id;
            }
        }
    }
    // insert
    int maxId=0; for(const auto& x : v) maxId = (x.id>maxId)?x.id:maxId;
    Section x = s; x.id = maxId+1;
    if(x.created_at.empty()) x.created_at = now;
    x.updated_at = now;
    v.push_back(x);
    sec_writeAll(v);
    return x.id;
}

int Sections_Delete(int id){
    std::vector<Section> v = sec_readAll();
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(),
            [id](const Section&x){return x.id==id;}), v.end());
    if(v.size()==before) return 0;
    sec_writeAll(v);
    return 1;
}

const wchar_t* Sections_KindLabel(const std::wstring& kind){
    if(kind==L"reception")   return L"\u067e\u0630\u06cc\u0631\u0634";
    if(kind==L"appointment") return L"\u0646\u0648\u0628\u062a\u200c\u062f\u0647\u06cc";
    if(kind==L"injection")   return L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a";
    if(kind==L"lab")         return L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647";
    if(kind==L"pharmacy")    return L"\u062f\u0627\u0631\u0648\u062e\u0627\u0646\u0647";
    if(kind==L"billing")     return L"\u0635\u0646\u062f\u0648\u0642";
    if(kind==L"radiology")   return L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc";
    if(kind==L"physio")      return L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc";
    return L"\u0633\u0627\u06cc\u0631";
}

// §7 (1.14.0): stable, durable category code for a section `kind`. These short
// prefixes are the canonical routing/sync keys and never change with display
// names. Unknown kinds collapse to a stable "GEN" (general) so a code is always
// produced.
const wchar_t* Sections_CategoryCode(const std::wstring& kind){
    if(kind==L"reception")   return L"REC";
    if(kind==L"appointment") return L"APR";
    if(kind==L"lab")         return L"LAB";
    if(kind==L"injection")   return L"INJ";
    if(kind==L"pharmacy")    return L"PHR";
    if(kind==L"billing")     return L"BIL";
    if(kind==L"radiology")   return L"RAD";
    if(kind==L"physio")      return L"PHY";
    return L"GEN";
}

std::wstring Sections_CodePrefix(const Section& s){
    // Leading alpha run of the stored code, e.g. "REC01" -> "REC".
    std::wstring p;
    for(wchar_t c : s.code){
        if((c>=L'A'&&c<=L'Z') || (c>=L'a'&&c<=L'z')) p += (wchar_t)towupper(c);
        else break;
    }
    if(!p.empty()) return p;
    // No alpha prefix in the code — derive a durable one from the kind.
    return Sections_CategoryCode(s.kind);
}
