#pragma once
#include <string>

inline bool serviceIdentitySeparator(wchar_t c){
    return c==L' '||c==L'\t'||c==L'\n'||c==L'\r'||c==L'\f'||c==L'\v'||
           c==0x00A0||c==0x1680||(c>=0x2000&&c<=0x200A)||c==0x2028||
           c==0x2029||c==0x202F||c==0x205F||c==0x3000||
           c==0x200C||c==0x200E||c==0x200F;
}

// One normalization contract for service code/name identity on both native and
// browser paths: Persian/Arabic letters are unified, ASCII/Persian/Arabic-Indic
// digits become ASCII, and the explicit Unicode separator set above collapses
// to one interior space while disappearing at boundaries. ASCII case folding is
// deliberate so C++ and ES5 do not depend on different Unicode locale tables.
inline std::wstring serviceIdentityKey(const std::wstring& in){
    std::wstring out;
    out.reserve(in.size());
    bool pendingSpace=false;
    for(wchar_t c:in){
        if(c>=0x06F0&&c<=0x06F9) c=(wchar_t)(L'0'+(c-0x06F0));
        else if(c>=0x0660&&c<=0x0669) c=(wchar_t)(L'0'+(c-0x0660));
        else if(c==L'ي') c=L'ی';
        else if(c==L'ك') c=L'ک';
        if(serviceIdentitySeparator(c)){
            if(!out.empty()) pendingSpace=true;
            continue;
        }
        if(pendingSpace){ out+=L' '; pendingSpace=false; }
        if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
        out+=c;
    }
    return out;
}

// Codes are primary identity. Display-name fallback is allowed only when at
// least one code is absent.
inline bool serviceIdentityMatches(const std::wstring& codeKey,
        const std::wstring& nameKey,const std::wstring& existingCodeKey,
        const std::wstring& existingNameKey){
    bool bothCoded=!codeKey.empty()&&!existingCodeKey.empty();
    if(bothCoded) return codeKey==existingCodeKey;
    return !nameKey.empty()&&!existingNameKey.empty()&&nameKey==existingNameKey;
}

// Identity alone is insufficient for merging: normal and manual/free rates, or
// two distinct effective unit prices, stay separate so totals are independent
// of submission order.
inline bool serviceVariantMatches(const std::wstring& codeKey,
        const std::wstring& nameKey,bool freeRate,long long unitPrice,
        const std::wstring& existingCodeKey,const std::wstring& existingNameKey,
        bool existingFreeRate,long long existingUnitPrice){
    return serviceIdentityMatches(codeKey,nameKey,existingCodeKey,existingNameKey)&&
           freeRate==existingFreeRate&&unitPrice==existingUnitPrice;
}

inline int serviceVariantCompare(const std::wstring& codeKey,
        const std::wstring& nameKey,bool freeRate,long long unitPrice,
        const std::wstring& descKey,const std::wstring& otherCodeKey,
        const std::wstring& otherNameKey,bool otherFreeRate,
        long long otherUnitPrice,const std::wstring& otherDescKey){
    std::wstring key=codeKey.empty()?nameKey:codeKey;
    std::wstring otherKey=otherCodeKey.empty()?otherNameKey:otherCodeKey;
    if(key<otherKey) return -1;
    if(otherKey<key) return 1;
    if(freeRate!=otherFreeRate) return freeRate?1:-1;
    if(unitPrice<otherUnitPrice) return -1;
    if(otherUnitPrice<unitPrice) return 1;
    if(descKey<otherDescKey) return -1;
    if(otherDescKey<descKey) return 1;
    return 0;
}
