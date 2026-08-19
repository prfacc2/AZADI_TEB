#pragma once
#include <vector>

// One vertical fragment of a logical service row. `offset` and `height` are in
// measured device pixels inside the full wrapped row.
struct PdServicesRowFragment {
    int row;
    int offset;
    int height;
    PdServicesRowFragment(int r=0,int o=0,int h=0):row(r),offset(o),height(h){}
};

// Fragments rendered on one physical page under the repeated services header.
struct PdServicesPageSlice {
    std::vector<PdServicesRowFragment> rows;
};

inline int pdPrintableDataHeight(int frameHeight,int requestedHeaderHeight,
                                 int minimumDataHeight,int* appliedHeaderHeight){
    if(frameHeight<1) frameHeight=1;
    if(minimumDataHeight<1) minimumDataHeight=1;
    int reservedData=minimumDataHeight<frameHeight?minimumDataHeight:frameHeight;
    int maxHeader=frameHeight-reservedData;
    int header=requestedHeaderHeight;
    if(header<0) header=0;
    if(header>maxHeader) header=maxHeader;
    if(appliedHeaderHeight) *appliedHeaderHeight=header;
    return frameHeight-header;
}

struct PdServicesFrame {
    int top;
    int bottom;
    PdServicesFrame(int t=0,int b=0):top(t),bottom(b){}
};
inline PdServicesFrame pdEnsureServicesFrame(int top,int bottom,int pageTop,
                                             int pageBottom,int minimumHeight){
    if(minimumHeight<1) minimumHeight=1;
    if(pageBottom<=pageTop) pageBottom=pageTop+minimumHeight;
    if(bottom-top>=minimumHeight) return PdServicesFrame(top,bottom);
    bottom=top+minimumHeight;
    if(bottom>pageBottom){ bottom=pageBottom; top=bottom-minimumHeight; }
    if(top<pageTop){ top=pageTop; bottom=top+minimumHeight; }
    if(bottom>pageBottom) bottom=pageBottom;
    if(bottom<=top) bottom=top+1;
    return PdServicesFrame(top,bottom);
}

// Split measured rows into physical pages without clipping. Normal rows move to
// the next page intact when they fit there; a row taller than a whole data frame
// is split into contiguous vertical fragments until its complete measured height
// has been emitted. `available` is normalized to at least one pixel so even a
// header-consuming custom geometry always advances and never drops a row.
inline std::vector<PdServicesPageSlice> pdSliceServiceRows(
        const std::vector<int>& rowHeights,int available,
        int splitQuantum=1,int firstInset=0){
    std::vector<PdServicesPageSlice> pages;
    if(rowHeights.empty()){
        pages.push_back(PdServicesPageSlice());
        return pages;
    }
    if(available<1) available=1;
    if(splitQuantum<1) splitQuantum=1;
    if(firstInset<0) firstInset=0;
    if(firstInset>=splitQuantum) firstInset%=splitQuantum;
    PdServicesPageSlice page;
    int pageUsed=0;
    for(int row=0;row<(int)rowHeights.size();++row){
        int total=rowHeights[row]>0?rowHeights[row]:1;
        int offset=0;
        while(offset<total){
            int pageLeft=available-pageUsed;
            int remaining=total-offset;
            // Every logical row starts on a fresh page when it cannot fit in
            // the current remainder. Oversized rows then split from the top of
            // a clean data frame, yielding deterministic full-frame fragments.
            if(offset==0 && pageUsed>0 && remaining>pageLeft){
                pages.push_back(page); page=PdServicesPageSlice(); pageUsed=0;
                pageLeft=available;
            }
            int take=remaining<pageLeft?remaining:pageLeft;
            bool alignedBreak=false;
            if(remaining>pageLeft && splitQuantum>1){
                int boundary=offset+take;
                int aligned=boundary-((boundary-firstInset)%splitQuantum+splitQuantum)%splitQuantum;
                if(aligned>offset){ take=aligned-offset; alignedBreak=take<pageLeft; }
            }
            if(take<1){
                pages.push_back(page); page=PdServicesPageSlice(); pageUsed=0;
                continue;
            }
            page.rows.push_back(PdServicesRowFragment(row,offset,take));
            offset+=take; pageUsed+=take;
            if((alignedBreak||pageUsed>=available) &&
               (offset<total || row+1<(int)rowHeights.size())){
                pages.push_back(page); page=PdServicesPageSlice(); pageUsed=0;
            }
        }
    }
    if(!page.rows.empty()) pages.push_back(page);
    return pages;
}
