// ============================================================================
//  backup_mtf.cpp  — vendored MTF descriptor reader (release 1.11.0 §C).
//  Pure C++17, no external dependency, static-link compatible. The heavy
//  lifting lives header-only in backup_mtf.h; this translation unit provides
//  the file-bound entry point that reads ONLY the leading descriptor region
//  (capped, never the whole multi-GB file) and parses it.
// ============================================================================
#include "backup_mtf.h"
#include <cstdio>
#include <cwchar>

namespace mtf {

//  Read at most `cap` bytes from the front of `path` and parse the MTF
//  descriptor region. `bytesRead` (out, optional) receives how many bytes were
//  actually pulled in. Never loads more than `cap` bytes. Returns a Descriptor
//  whose `isMtf` is false when the file is not a recognisable MTF stream.
Descriptor parseFile(const wchar_t* path, size_t cap, size_t* bytesRead){
    if(bytesRead) *bytesRead = 0;
    if(!path || !*path) return Descriptor();
    // MTF descriptors live in the first few MiB; the work order caps reads at
    // ~4 MiB. Honour the caller's cap but never exceed a sane ceiling.
    if(cap == 0)            cap = 4u * 1024u * 1024u;
    if(cap > 16u*1024u*1024u) cap = 16u * 1024u * 1024u;

    FILE* f = _wfopen(path, L"rb");
    if(!f) return Descriptor();
    std::vector<uint8_t> buf(cap);
    size_t got = std::fread(buf.data(), 1, cap, f);
    std::fclose(f);
    buf.resize(got);
    if(bytesRead) *bytesRead = got;
    return parse(buf.data(), buf.size());
}

} // namespace mtf
