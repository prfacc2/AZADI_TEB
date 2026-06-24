// ============================================================================
//  backup_mtf.h  — vendored, dependency-free Microsoft Tape Format (MTF)
//  descriptor reader for SQL Server native «.bak» backups (release 1.11.0 §C).
//
//  SQL Server native backups are stored in Microsoft Tape Format (MTF). Every
//  MTF stream begins with a TAPE descriptor DBLK; the backup-set (SSET),
//  volume (VOLB) and database-file (DBDB/MSCI) descriptors that follow it carry
//  the database name, server/machine name, backup type, timestamps and the
//  embedded database file list.
//
//  This reader ONLY parses the leading descriptor region (the first few MiB) —
//  it NEVER loads a multi-GB .bak wholesale. It is pure C++17, header-only
//  friendly and static-link compatible: no external dependency whatsoever.
//
//  Every accessor is BOUNDS-CHECKED against the supplied buffer span, so a
//  truncated / corrupt descriptor block can never read out of bounds. Callers
//  layer their own SEH/longjmp containment on top (see backup_analyzer.cpp).
// ============================================================================
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace mtf {

//  Recognisable MTF DBLK types we surface. (4-char ASCII tags.)
//    "TAPE" media header, "SSET" backup set, "VOLB" volume,
//    "DBDB"/"FILE" database file, "ESET"/"EOTM" end markers, "SPAD" pad.

//  Result of reading the leading descriptor region of an MTF stream.
struct Descriptor {
    bool         isMtf      = false;   // leading signature == "TAPE"
    bool         unicode    = false;   // string-storage format (TAPE+14 == 2)

    std::wstring databaseName;         // DB name (from SSET / DBDB)
    std::wstring serverName;           // machine / server name (VOLB / SSET)
    std::wstring vendor;               // backup software vendor
    std::wstring software;             // backup software description (TAPE)
    std::wstring mediaName;            // media / tape label (TAPE)
    std::wstring setName;              // backup-set description (SSET)
    std::wstring backupType;           // "Full" / "Differential" / "Log"
    std::wstring startTime;            // backup start  (ISO-ish, may be empty)
    std::wstring finishTime;           // backup finish (ISO-ish, may be empty)
    int          compatLevel = 0;      // DB compatibility level (0 = unknown)

    int          sets    = 0;          // # of SSET descriptors seen
    int          volumes = 0;          // # of VOLB descriptors seen
    int          dbFiles = 0;          // # of DBDB/FILE descriptors seen
    std::vector<std::wstring> fileNames;   // first ≤32 DB-file names

    uint16_t     blockSize = 1024;     // resolved MTF logical block size
};

//  A bounds-checked little-endian reader over a flat byte span. Every read
//  validates the requested range against the span length and returns 0 / empty
//  on out-of-range — there is NO raw pointer arithmetic that can escape `buf`.
class SpanReader {
public:
    SpanReader(const uint8_t* buf, size_t size)
        : data_(buf), size_(buf ? size : 0) {}

    bool   valid() const { return data_ != nullptr && size_ > 0; }
    size_t size()  const { return size_; }

    uint16_t u16(size_t off) const {
        if(!data_ || off + 2 > size_) return 0;
        return (uint16_t)(data_[off] | (data_[off + 1] << 8));
    }
    uint32_t u32(size_t off) const {
        if(!data_ || off + 4 > size_) return 0;
        return (uint32_t)(data_[off]       | (data_[off + 1] << 8) |
                         (data_[off + 2] << 16) | (data_[off + 3] << 24));
    }
    uint64_t u64(size_t off) const {
        if(!data_ || off + 8 > size_) return 0;
        uint64_t lo = u32(off), hi = u32(off + 4);
        return lo | (hi << 32);
    }
    //  Does the 4-byte tag at `off` equal `tag` (a 4-char string)?
    bool tagAt(size_t off, const char* tag) const {
        return data_ && off + 4 <= size_ && std::memcmp(data_ + off, tag, 4) == 0;
    }
    //  Raw byte access (bounds-checked; returns 0 when out of range).
    uint8_t at(size_t off) const {
        return (data_ && off < size_) ? data_[off] : (uint8_t)0;
    }
    const uint8_t* ptr(size_t off, size_t need) const {
        if(!data_ || off + need > size_) return nullptr;
        return data_ + off;
    }

    //  Read an MTF (uint16 size, uint16 offset-from-DBLK-start) string field
    //  located at `base + sizeFieldOff`. Fully bounds-checked. `unicode`
    //  selects UTF-16LE vs ANSI decoding.
    std::wstring mtfString(size_t base, size_t sizeFieldOff, bool unicode) const {
        uint16_t sz  = u16(base + sizeFieldOff);
        uint16_t off = u16(base + sizeFieldOff + 2);
        if(sz == 0 || off == 0) return std::wstring();
        size_t s = base + off;
        if(s >= size_) return std::wstring();
        size_t avail = size_ - s;
        if(sz > avail) sz = (uint16_t)avail;
        if(unicode){
            std::wstring w; w.reserve(sz / 2);
            for(size_t i = 0; i + 1 < (size_t)sz; i += 2)
                w.push_back((wchar_t)(data_[s + i] | (data_[s + i + 1] << 8)));
            return trimNul(w);
        }
        // ANSI → wide (Latin-1 fallback; callers may re-decode with CP_ACP).
        std::wstring w; w.reserve(sz);
        for(size_t i = 0; i < (size_t)sz; ++i) w.push_back((wchar_t)data_[s + i]);
        return trimNul(w);
    }

private:
    static std::wstring trimNul(std::wstring w){
        size_t n = w.find(L'\0');
        if(n != std::wstring::npos) w.resize(n);
        return w;
    }
    const uint8_t* data_;
    size_t         size_;
};

//  Parse the leading descriptor region `buf[0..size)` of an MTF stream.
//  `size` should be the bytes actually read from the front of the file
//  (the caller caps this, e.g. ≤ 4 MiB). Returns a fully-populated Descriptor;
//  `isMtf` is false (and the rest left at defaults) if the buffer does not
//  begin with a valid "TAPE" signature. NEVER reads outside `buf`.
inline Descriptor parse(const uint8_t* buf, size_t size){
    Descriptor R;
    SpanReader rd(buf, size);
    if(!rd.valid() || size < 64 || !rd.tagAt(0, "TAPE")) return R;
    R.isMtf   = true;
    R.unicode = (rd.u16(14) == 2);
    const bool uni = R.unicode;

    // Resolve the MTF logical block size (TAPE "format logical block size"
    // hint at +38); fall back to 1024 when the hint is implausible.
    uint16_t hint = rd.u16(38);
    if(hint == 512 || hint == 1024 || hint == 2048 || hint == 4096) R.blockSize = hint;
    const size_t blk = R.blockSize;
    auto alignUp = [&](size_t v, size_t a){ return ((v + a - 1) / a) * a; };

    size_t pos = 0, guard = 0;
    while(pos + 4 <= size && guard < 4096){
        ++guard;
        if(rd.tagAt(pos, "TAPE")){
            if(R.mediaName.empty()) R.mediaName = rd.mtfString(pos, 52, uni);
            if(R.software.empty())  R.software  = rd.mtfString(pos, 60, uni);
        } else if(rd.tagAt(pos, "SSET")){
            R.sets++;
            if(R.setName.empty())      R.setName      = rd.mtfString(pos, 52, uni);
            if(R.vendor.empty())       R.vendor       = rd.mtfString(pos, 56, uni);
            if(R.serverName.empty())   R.serverName   = rd.mtfString(pos, 72, uni);
            if(R.databaseName.empty()) R.databaseName = rd.mtfString(pos, 80, uni);
            // SQL Server stores its backup-type code in the SSET "physical
            // block address" / flags region. We surface a best-effort label
            // from the SSET attributes word at +4 (0x0..=full by convention).
            if(R.backupType.empty()){
                uint32_t attr = rd.u32(pos + 4);
                // Heuristic mapping (documented MTF SSET attribute bits + the
                // SQL Server convention). Default to "Full".
                if(attr & 0x00000010u)      R.backupType = L"Differential";
                else if(attr & 0x00000020u) R.backupType = L"Log";
                else                        R.backupType = L"Full";
            }
        } else if(rd.tagAt(pos, "VOLB")){
            R.volumes++;
            if(R.serverName.empty()) R.serverName = rd.mtfString(pos, 52, uni);
        } else if(rd.tagAt(pos, "DBDB") || rd.tagAt(pos, "FILE") || rd.tagAt(pos, "MSCI")){
            R.dbFiles++;
            std::wstring nm = rd.mtfString(pos, 52, uni);
            if(!nm.empty() && (int)R.fileNames.size() < 32) R.fileNames.push_back(nm);
            if(R.databaseName.empty() && rd.tagAt(pos, "DBDB")) R.databaseName = nm;
        } else if(rd.tagAt(pos, "ESET") || rd.tagAt(pos, "EOTM")){
            break;   // end of set / end of media
        }
        // Advance: prefer the DBLK "displayable size" at +24 when plausible,
        // else step one block; always align to the block boundary.
        uint32_t dsize = rd.u32(pos + 24);
        size_t step = (dsize >= blk && dsize < size) ? dsize : blk;
        size_t next = alignUp(pos + step, blk);
        if(next <= pos) next = pos + blk;
        pos = next;
    }
    if(R.backupType.empty()) R.backupType = L"Full";
    return R;
}

//  Read at most `cap` bytes (default 4 MiB) from the front of `path` and parse
//  the MTF descriptor region. Implemented in backup_mtf.cpp. Never loads the
//  whole file. `bytesRead` (optional) receives the byte count actually read.
Descriptor parseFile(const wchar_t* path, size_t cap = 4u * 1024u * 1024u,
                     size_t* bytesRead = nullptr);

} // namespace mtf
