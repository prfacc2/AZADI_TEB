#pragma once
#include <string>
#include <vector>
#include "service_identity.h"

inline std::wstring servicePreferredText(const std::wstring& a,
                                         const std::wstring& b){
    if(a.empty()) return b;
    if(b.empty()) return a;
    std::wstring ak=serviceIdentityKey(a), bk=serviceIdentityKey(b);
    if(ak<bk) return a;
    if(bk<ak) return b;
    return b<a?b:a;
}

// Merge one already-priced line into the canonical native row set. `Line` is
// intentionally structural (code/name/category/desc/price/qty/discount) so the
// exact production algorithm can also run in the platform-independent harness.
template<class Line>
inline void serviceCanonicalAdd(std::vector<Line>& lines,
        std::vector<std::wstring>& identityCodes,
        std::vector<bool>& identityFreeRates,const Line& incoming,
        const std::wstring& incomingCodeKey,bool freeRate){
    std::wstring nameKey=serviceIdentityKey(incoming.name);
    std::size_t match=lines.size();
    for(std::size_t i=0;i<lines.size();++i){
        if(serviceVariantMatches(incomingCodeKey,nameKey,freeRate,incoming.price,
                identityCodes[i],serviceIdentityKey(lines[i].name),
                identityFreeRates[i],lines[i].price)){
            match=i;
            break;
        }
    }
    if(match==lines.size()){
        lines.push_back(incoming);
        identityCodes.push_back(incomingCodeKey);
        identityFreeRates.push_back(freeRate);
        return;
    }

    Line& dst=lines[match];
    bool dstCoded=!identityCodes[match].empty();
    bool incomingCoded=!incomingCodeKey.empty();
    if(dst.qty<=999-incoming.qty) dst.qty+=incoming.qty;
    else dst.qty=999;
    dst.discount+=incoming.discount;

    // A coded catalogue row deterministically owns metadata when name fallback
    // joins it to an uncoded row. Otherwise choose text independently of input
    // order and retain the already-equal effective unit price.
    if(!dstCoded&&incomingCoded){
        dst.code=incoming.code;
        dst.name=incoming.name;
        dst.category=incoming.category;
        dst.desc=incoming.desc;
        dst.price=incoming.price;
        identityCodes[match]=incomingCodeKey;
    } else if(!(dstCoded&&!incomingCoded)){
        dst.code=servicePreferredText(dst.code,incoming.code);
        dst.name=servicePreferredText(dst.name,incoming.name);
        dst.category=servicePreferredText(dst.category,incoming.category);
        dst.desc=servicePreferredText(dst.desc,incoming.desc);
    }
    long long cap=dst.price*(long long)dst.qty;
    if(dst.discount>cap) dst.discount=cap;
}

template<class Line>
inline void serviceCanonicalSort(std::vector<Line>& lines,
        std::vector<std::wstring>& identityCodes,
        std::vector<bool>& identityFreeRates){
    for(std::size_t i=1;i<lines.size();++i){
        Line line=lines[i];
        bool freeFlag=identityFreeRates[i];
        std::wstring code=identityCodes[i];
        std::size_t j=i;
        while(j>0&&serviceVariantCompare(
                code,serviceIdentityKey(line.name),freeFlag,line.price,
                serviceIdentityKey(line.desc),identityCodes[j-1],
                serviceIdentityKey(lines[j-1].name),identityFreeRates[j-1],
                lines[j-1].price,serviceIdentityKey(lines[j-1].desc))<0){
            lines[j]=lines[j-1];
            identityCodes[j]=identityCodes[j-1];
            identityFreeRates[j]=identityFreeRates[j-1];
            --j;
        }
        lines[j]=line;
        identityCodes[j]=code;
        identityFreeRates[j]=freeFlag;
    }
}
