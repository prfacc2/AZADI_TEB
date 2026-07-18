// ============================================================================
//  web_designer.cpp — loopback HTTP host for the HTML/CSS/JS print designer
//  (release 1.19.1).
//
//  Design goals:
//   * NO external dependency — uses Winsock (already linked) only.
//   * Crash/hang-proof — the designer page is opened in the operator's DEFAULT
//     BROWSER (ShellExecute on the loopback URL). The fragile embedded Trident
//     control (blank screen / "not responding") was removed in 1.19.1.
//   * Light & stable — the HTTP host is a single blocking-accept thread (no
//     polling/busy loop); the page itself is a tiny static app.
//   * Fully synced with C++ — the page persists through /api/save which writes
//     the section design and posts WM_APP_DESIGN_PUSHED so reception picks the
//     new layout up immediately.
//   * Safe fallback — if the loopback host can't start, WebDesigner_Open
//     returns false and the caller opens the proven native GDI designer.
// ============================================================================
#include "app.h"
#include "web_designer.h"
#include "print_designer.h"
#include "sections.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>

// ----------------------------------------------------------------------------
//  small utils
// ----------------------------------------------------------------------------
static std::string w2u8(const std::wstring& w){
    if(w.empty()) return "";
    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),NULL,0,NULL,NULL);
    std::string s(n,0); WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&s[0],n,NULL,NULL);
    return s;
}
static std::wstring u82w(const std::string& s){
    if(s.empty()) return L"";
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}

// load an embedded RCDATA asset as raw bytes
static bool loadRes(int id, std::string& out){
    HRSRC h=FindResourceW(g_hInst,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!h) return false;
    HGLOBAL g=LoadResource(g_hInst,h); if(!g) return false;
    void* p=LockResource(g); DWORD sz=SizeofResource(g_hInst,h);
    if(!p||!sz) return false;
    out.assign((const char*)p,(size_t)sz); return true;
}

// ----------------------------------------------------------------------------
//  PrintDesign  <->  designer JSON (the JS shape uses string types & #hex)
// ----------------------------------------------------------------------------
//  The JS engine uses type strings ("label","field",…) and "#rrggbb" colours.
//  The C++ PrintDesign uses int enums and 0x00RRGGBB. We bridge here so a saved
//  web design is identical to a native one and prints through the same path.
// §1.53.0 FIX (Bug A): this array MUST have one entry per PrintItemType enum
// member and be in the EXACT enum order, because jsTypeToInt/intToJsType index
// straight into it. Before this fix "services" (index 12 = PIT_SERVICES) was
// MISSING, so a saved PIT_SERVICES item was silently downgraded to a plain
// label on load, and the browser designer had no way to author a live services
// list. Order below == PIT_LABEL,PIT_FIELD,PIT_HLINE,PIT_VLINE,PIT_RECT,
// PIT_FRAME,PIT_IMAGE,PIT_LOGO,PIT_QR,PIT_PHOTO,PIT_APPTNO,PIT_TABLE,PIT_SERVICES.
static const char* JS_TYPES[] = {
    "label","field","hline","vline","rect","frame","image","logo","qr","photo","apptno","table","services" };
static int jsTypeToInt(const std::string& t){
    for(int i=0;i<(int)(sizeof(JS_TYPES)/sizeof(JS_TYPES[0]));++i)
        if(t==JS_TYPES[i]) return i;
    return PIT_LABEL;
}
static std::string intToJsType(int t){
    if(t<0||t>=(int)(sizeof(JS_TYPES)/sizeof(JS_TYPES[0]))) return "label";
    return JS_TYPES[t];
}
static std::string hexColor(unsigned int c){
    char b[8]; sprintf(b,"#%02x%02x%02x",(c>>16)&0xFF,(c>>8)&0xFF,c&0xFF);
    return b;
}
static unsigned int parseHexColor(const std::string& s){
    std::string h=s; if(!h.empty()&&h[0]=='#') h=h.substr(1);
    if(h.size()>=6){ unsigned int v=(unsigned)strtoul(h.c_str(),NULL,16); return v&0xFFFFFF; }
    return 0;
}
static std::string jbool(bool b){ return b?"true":"false"; }
static std::string jnum(double v){ char b[40]; snprintf(b,40,"%g",v); return b; }
static std::string jint(long long v){ char b[40]; snprintf(b,40,"%lld",v); return b; }
static std::string jstr(const std::wstring& w){
    std::string u=w2u8(w), o="\"";
    for(char c:u){ switch(c){ case '"':o+="\\\"";break; case '\\':o+="\\\\";break;
        case '\n':o+="\\n";break; case '\r':o+="\\r";break; case '\t':o+="\\t";break;
        default: if((unsigned char)c<0x20){ char b[8]; sprintf(b,"\\u%04x",c); o+=b; } else o+=c; } }
    o+="\""; return o;
}

// Serialise a PrintDesign into the JS-shaped JSON the page expects.
static std::string designToWebJson(const PrintDesign& d){
    std::string o="{";
    o+="\"id\":"+jint(d.id)+",";
    o+="\"name\":"+jstr(d.name)+",";
    o+="\"kind\":"+jstr(d.kind)+",";
    o+="\"paper\":"+jstr(d.paper)+",";
    o+="\"orientation\":"+jint(d.orientation)+",";
    o+="\"paperW\":"+jnum(d.paperW)+",";
    o+="\"paperH\":"+jnum(d.paperH)+",";
    o+="\"items\":[";
    for(size_t i=0;i<d.items.size();++i){
        const PrintItem& it=d.items[i]; if(i)o+=",";
        o+="{";
        o+="\"id\":"+jint(it.id)+",";
        o+="\"type\":\""+intToJsType(it.type)+"\",";
        o+="\"x\":"+jnum(it.x)+",\"y\":"+jnum(it.y)+",\"w\":"+jnum(it.w)+",\"h\":"+jnum(it.h)+",";
        o+="\"rot\":"+jnum(it.rot)+",\"z\":"+jint(it.z)+",";
        o+="\"locked\":"+jbool(it.locked)+",\"isFrame\":"+jbool(it.is_frame)+",";
        o+="\"text\":"+jstr(it.text)+",\"field\":"+jstr(it.field)+",";
        o+="\"prefix\":"+jstr(it.prefix)+",\"suffix\":"+jstr(it.suffix)+",";
        o+="\"font\":"+jstr(it.fontName)+",\"pt\":"+jnum(it.fontPt)+",";
        o+="\"bold\":"+jbool(it.bold)+",\"italic\":"+jbool(it.italic)+",";
        o+="\"align\":"+jint(it.align)+",\"dir\":"+jint(it.dir)+",\"valign\":"+jint(it.valign)+",\"lineSpacing\":"+jnum(it.lineSpacing)+",";
        o+="\"objectFit\":\""+std::string(it.objectFit==1?"cover":it.objectFit==2?"fill":"contain")+"\",";
        o+="\"textColor\":\""+hexColor(it.textColor)+"\",";
        o+="\"fillColor\":\""+hexColor(it.fillColor)+"\",";
        o+="\"fillTransparent\":"+jbool(it.fillTransparent)+",";
        o+="\"borderColor\":\""+hexColor(it.borderColor)+"\",";
        o+="\"borderWidth\":"+jnum(it.borderWidth)+",";
        o+="\"borderStyle\":0,";
        o+="\"corner\":"+jnum(it.corner)+",\"padding\":"+jnum(it.padding)+",";
        o+="\"opacity\":"+jnum(it.opacity)+",\"visibility\":"+jint(it.visibility)+",";
        o+="\"startValue\":"+jint(it.startValue)+",\"step\":"+jint(it.step)+",";
        o+="\"imgPath\":"+jstr(it.imgPath);
        o+="}";
    }
    o+="]}";
    return o;
}

// --- tiny tolerant JSON reader (only what the bridge needs) -----------------
namespace {
struct WJ {
    const std::string& s; size_t p; bool ok;
    WJ(const std::string& x):s(x),p(0),ok(true){}
    void ws(){ while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))++p; }
    bool eat(char c){ ws(); if(p<s.size()&&s[p]==c){++p;return true;} return false; }
    std::string str(){ ws(); std::string o; if(p>=s.size()||s[p]!='"'){ok=false;return o;} ++p;
        while(p<s.size()&&s[p]!='"'){ char c=s[p++]; if(c=='\\'&&p<s.size()){ char e=s[p++];
            switch(e){case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;
                case '"':o+='"';break;case '\\':o+='\\';break;case '/':o+='/';break;
                case 'u':{ if(p+4<=s.size()){ unsigned v=(unsigned)strtoul(s.substr(p,4).c_str(),NULL,16); p+=4;
                    if(v<0x80)o+=(char)v; else if(v<0x800){o+=(char)(0xC0|(v>>6));o+=(char)(0x80|(v&0x3F));}
                    else{o+=(char)(0xE0|(v>>12));o+=(char)(0x80|((v>>6)&0x3F));o+=(char)(0x80|(v&0x3F));} } break; }
                default:o+=e; } } else o+=c; }
        if(p<s.size()&&s[p]=='"')++p; else ok=false; return o; }
    double dbl(){ ws(); size_t st=p; while(p<s.size()&&(isdigit((unsigned char)s[p])||s[p]=='-'||s[p]=='+'||s[p]=='.'||s[p]=='e'||s[p]=='E'))++p;
        if(p==st){ok=false;return 0;} return atof(s.substr(st,p-st).c_str()); }
    bool boolean(){ ws(); if(s.compare(p,4,"true")==0){p+=4;return true;} if(s.compare(p,5,"false")==0){p+=5;return false;} dbl(); return false; }
    void skip(){ ws(); if(p>=s.size())return; char c=s[p];
        if(c=='"')str(); else if(c=='{')skipObj(); else if(c=='[')skipArr();
        else while(p<s.size()&&s[p]!=','&&s[p]!='}'&&s[p]!=']')++p; }
    void skipObj(){ eat('{'); while(true){ ws(); if(eat('}'))break; str(); eat(':'); skip(); if(!eat(',')){eat('}');break;} if(!ok)break; } }
    void skipArr(){ eat('['); while(true){ ws(); if(eat(']'))break; skip(); if(!eat(',')){eat(']');break;} if(!ok)break; } }
};
}

// Parse a JS-shaped design (the `design` object) into a PrintDesign.
static bool webJsonToDesign(const std::string& json, PrintDesign& out){
    WJ j(json); out=PrintDesign();
    if(!j.eat('{')) return false;
    while(true){ j.ws(); if(j.eat('}'))break; std::string k=j.str(); j.eat(':');
        if(k=="id") out.id=(int)j.dbl();
        else if(k=="name") out.name=u82w(j.str());
        else if(k=="kind") out.kind=u82w(j.str());
        else if(k=="paper") out.paper=u82w(j.str());
        else if(k=="orientation") out.orientation=(int)j.dbl();
        else if(k=="paperW") out.paperW=j.dbl();
        else if(k=="paperH") out.paperH=j.dbl();
        else if(k=="items"){ j.eat('[');
            while(true){ j.ws(); if(j.eat(']'))break; PrintItem it;
                if(!j.eat('{'))return false;
                while(true){ j.ws(); if(j.eat('}'))break; std::string ik=j.str(); j.eat(':');
                    if(ik=="id") it.id=(int)j.dbl();
                    else if(ik=="type") it.type=jsTypeToInt(j.str());
                    else if(ik=="x") it.x=j.dbl(); else if(ik=="y") it.y=j.dbl();
                    else if(ik=="w") it.w=j.dbl(); else if(ik=="h") it.h=j.dbl();
                    else if(ik=="rot") it.rot=j.dbl(); else if(ik=="z") it.z=(int)j.dbl();
                    else if(ik=="locked") it.locked=j.boolean();
                    else if(ik=="isFrame") it.is_frame=j.boolean();
                    else if(ik=="text") it.text=u82w(j.str());
                    else if(ik=="field") it.field=u82w(j.str());
                    else if(ik=="prefix") it.prefix=u82w(j.str());
                    else if(ik=="suffix") it.suffix=u82w(j.str());
                    else if(ik=="font") it.fontName=u82w(j.str());
                    else if(ik=="pt") it.fontPt=j.dbl();
                    else if(ik=="bold") it.bold=j.boolean();
                    else if(ik=="italic") it.italic=j.boolean();
                    else if(ik=="align") it.align=(int)j.dbl();
                    else if(ik=="dir") it.dir=(int)j.dbl();
                    else if(ik=="valign") it.valign=(int)j.dbl();
                    else if(ik=="objectFit"){
                        // accept either a CSS string ("contain"/"cover"/"fill")
                        // or a plain integer, for forward/backward compatibility.
                        j.ws();
                        if(j.p<j.s.size() && j.s[j.p]=='"'){ std::string v=j.str();
                            it.objectFit=(v=="cover")?1:(v=="fill")?2:0; }
                        else it.objectFit=(int)j.dbl();
                    }
                    else if(ik=="lineSpacing") it.lineSpacing=j.dbl();
                    else if(ik=="textColor") it.textColor=parseHexColor(j.str());
                    else if(ik=="fillColor") it.fillColor=parseHexColor(j.str());
                    else if(ik=="fillTransparent") it.fillTransparent=j.boolean();
                    else if(ik=="borderColor") it.borderColor=parseHexColor(j.str());
                    else if(ik=="borderWidth") it.borderWidth=j.dbl();
                    else if(ik=="corner") it.corner=j.dbl();
                    else if(ik=="padding") it.padding=j.dbl();
                    else if(ik=="opacity") it.opacity=j.dbl();
                    else if(ik=="visibility") it.visibility=(int)j.dbl();
                    else if(ik=="startValue") it.startValue=(int)j.dbl();
                    else if(ik=="step") it.step=(int)j.dbl();
                    else if(ik=="imgPath") it.imgPath=u82w(j.str());
                    else j.skip();
                    if(!j.eat(',')){ j.eat('}'); break; } if(!j.ok)break; }
                if(it.fontName.empty()) it.fontName=L"Vazirmatn";
                if(it.fontPt<=0) it.fontPt=10;
                out.items.push_back(it);
                if(!j.eat(',')){ j.eat(']'); break; } if(!j.ok)break; }
        } else j.skip();
        if(!j.eat(',')){ j.eat('}'); break; } if(!j.ok)break; }
    double pw,ph; if(Paper_Dims(out.paper,pw,ph)){
        out.paperW=pw; out.paperH=ph; if(out.orientation==1) std::swap(out.paperW,out.paperH); }
    return j.ok;
}

// v1.21.1: public wrappers so the native UI (Print Settings import/export) can
// round-trip the very files the browser designer downloads.
std::string Design_ToWebJson(const PrintDesign& d){ return designToWebJson(d); }
bool Design_FromWebJson(const std::string& json, PrintDesign& out){ return webJsonToDesign(json,out); }

// extract a string value of `key` from a flat JSON object (top-level only)
static bool jsonGetObject(const std::string& json, const std::string& key, std::string& out){
    // find "key" then capture the following {...} balanced block
    std::string pat="\""+key+"\"";
    size_t k=json.find(pat); if(k==std::string::npos) return false;
    size_t c=json.find(':',k+pat.size()); if(c==std::string::npos) return false;
    size_t p=c+1; while(p<json.size()&&(json[p]==' '||json[p]=='\t'||json[p]=='\n'||json[p]=='\r'))++p;
    if(p>=json.size()||json[p]!='{') return false;
    int depth=0; bool instr=false; size_t st=p;
    for(; p<json.size(); ++p){ char ch=json[p];
        if(instr){ if(ch=='\\'){++p;continue;} if(ch=='"')instr=false; continue; }
        if(ch=='"') instr=true; else if(ch=='{')++depth; else if(ch=='}'){ if(--depth==0){ out=json.substr(st,p-st+1); return true; } } }
    return false;
}

// v1.22.0: extract a top-level numeric value: "key": 123  (or 123.4)
static bool jsonGetNumber(const std::string& json, const std::string& key, double& out){
    std::string pat="\""+key+"\"";
    size_t k=json.find(pat); if(k==std::string::npos) return false;
    size_t c=json.find(':',k+pat.size()); if(c==std::string::npos) return false;
    size_t p=c+1; while(p<json.size()&&(json[p]==' '||json[p]=='\t'||json[p]=='\n'||json[p]=='\r'))++p;
    size_t st=p;
    while(p<json.size()&&(isdigit((unsigned char)json[p])||json[p]=='-'||json[p]=='+'||json[p]=='.'||json[p]=='e'||json[p]=='E'))++p;
    if(p==st) return false;
    out=atof(json.substr(st,p-st).c_str());
    return true;
}

#include "web_designer_http.inc"
#include "web_designer_host.inc"
