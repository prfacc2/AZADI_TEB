// ============================================================================
//  users.cpp — user store (data\users.dat), FNV-salted hash, login checks
//  Hidden admin: prf / prf123  (Ctrl+P+N on home screen)
// ============================================================================
#include "app.h"
#include <stdio.h>

//  Format (UTF-8, one per line):  username|fullname|dept|role|hash
static std::wstring usersPath(){ return dataDir()+L"\\users.dat"; }

std::wstring hashPassword(const std::wstring& p){
    // FNV-1a 64-bit, double pass with salt — adequate for offline local store.
    const wchar_t* SALT = L"AzadiTeb#2025!";
    std::wstring s = SALT + p + SALT;
    unsigned long long h = 14695981039346656037ULL;
    for(wchar_t c : s){ h ^= (unsigned long long)c; h *= 1099511628211ULL; }
    std::wstring r = s; // second pass over reversed
    for(int i=(int)r.size()-1;i>=0;i--){ h ^= (unsigned long long)r[i]*31ULL; h *= 1099511628211ULL; }
    wchar_t buf[24]; swprintf(buf,24,L"%016llX",h);
    return buf;
}

static std::vector<std::wstring> split(const std::wstring& s, wchar_t sep){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){
        size_t e = s.find(sep,pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1;
    }
    return out;
}

std::vector<User> loadUsers(){
    std::vector<User> out;
    std::wstring all = readFileUtf8(usersPath());
    size_t pos=0;
    while(pos < all.size()){
        size_t e = all.find(L'\n',pos);
        if(e==std::wstring::npos) e=all.size();
        std::wstring line = trim(all.substr(pos,e-pos));
        pos = e+1;
        if(line.empty()) continue;
        auto f = split(line, L'|');
        if(f.size() < 5) continue;
        User u; u.username=f[0]; u.fullname=f[1]; u.dept=f[2];
        u.role=_wtoi(f[3].c_str()); u.hash=f[4];
        // §H: preserve any extra columns a newer version may have appended so a
        // round-trip save never drops forward-compatible data.
        for(size_t i=5;i<f.size();i++){ u.extra+=L"|"; u.extra+=f[i]; }
        out.push_back(u);
    }
    return out;
}
static void saveUsers(const std::vector<User>& us){
    std::wstring out;
    for(auto& u : us){
        wchar_t role[4]; swprintf(role,4,L"%d",u.role);
        // §H: re-emit the canonical 5 columns + any preserved forward-compat
        // trailing columns (u.extra already begins with its own '|' separators).
        out += u.username+L"|"+u.fullname+L"|"+u.dept+L"|"+role+L"|"+u.hash+u.extra+L"\r\n";
    }
    writeFileUtf8(usersPath(), out, false);
}

bool addUser(const User& u, std::wstring& err){
    if(trim(u.username).empty() || trim(u.fullname).empty()){
        err = L"نام و نام کاربری نمی‌تواند خالی باشد."; return false;
    }
    if(u.username == L"prf"){
        err = L"این نام کاربری رزرو شده است."; return false;
    }
    auto us = loadUsers();
    for(auto& e : us)
        if(e.username == u.username){
            err = L"این نام کاربری قبلاً ساخته شده است."; return false;
        }
    us.push_back(u);
    saveUsers(us);
    logLine(L"user added: "+u.username);
    return true;
}
bool removeUser(const std::wstring& username){
    auto us = loadUsers();
    for(size_t i=0;i<us.size();i++)
        if(us[i].username==username){
            us.erase(us.begin()+i); saveUsers(us);
            logLine(L"user removed: "+username);
            return true;
        }
    return false;
}

//  §5: apply an admin-approved profile-name change. Updates the user record in
//  users.dat AND records a display-name override so a stale cached login also
//  reflects the new name. Returns false if the username is unknown.
bool setUserFullName(const std::wstring& username, const std::wstring& fullname){
    if(trim(username).empty() || trim(fullname).empty()) return false;
    auto us = loadUsers();
    bool found=false;
    for(auto& u : us)
        if(u.username==username){ u.fullname=fullname; found=true; break; }
    if(found) saveUsers(us);
    // Always set the override so the change is honoured even for prf/cached cases.
    setSetting(L"name_override_"+username, fullname);
    logLine(L"profile name approved: "+username+L" -> "+fullname);
    return found;
}

//  wantRole: 0 پذیرش / 1 مدیریت / 2 hidden admin
bool verifyLogin(const std::wstring& uname, const std::wstring& pass,
                 int wantRole, User& out, std::wstring& err){
    if(wantRole == 2){
        if(uname==L"prf" && pass==L"prf123"){
            out.username=L"prf"; out.fullname=L"مدیر سیستم";
            out.dept=L"ادمین"; out.role=2;
            logLine(L"admin login ok");
            return true;
        }
        err = L"نام کاربری یا رمز عبور اشتباه است.";
        logLine(L"admin login FAILED for: "+uname);
        return false;
    }
    auto us = loadUsers();
    for(auto& u : us){
        if(u.username == uname){
            if(u.hash != hashPassword(pass)){
                err = L"نام کاربری یا رمز عبور اشتباه است.";
                logLine(L"login wrong password: "+uname);
                return false;
            }
            if(u.role != wantRole){
                err = (wantRole==0)
                    ? L"این کاربر به بخش پذیرش دسترسی ندارد."
                    : L"این کاربر به پنل مدیریت دسترسی ندارد.";
                logLine(L"login wrong role: "+uname);
                return false;
            }
            out = u;
            // apply any management-approved display-name override
            { std::wstring ov=getSetting(L"name_override_"+u.username,L"");
              if(!ov.empty()) out.fullname=ov; }
            logLine(L"login ok: "+uname);
            return true;
        }
    }
    err = L"نام کاربری یا رمز عبور اشتباه است.";
    logLine(L"login unknown user: "+uname);
    return false;
}
