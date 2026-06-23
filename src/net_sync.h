// ============================================================================
//  net_sync.h — thin LAN-sync layer (release 1.4.0, §9)
//  Prefers HTTP to the configured admin_host; on failure falls back to a
//  file-based inbox/outbox under a SMB share_path. Failures never pop modal
//  dialogs — callers do silent retries + an unobtrusive toast.
// ============================================================================
#pragma once
#include <string>

// Settings live in data\settings.ini under [net_sync] (flattened keys:
// net_sync.enabled / net_sync.admin_host / net_sync.share_path).
struct NetSyncCfg {
    bool         enabled;
    std::wstring admin_host;   // e.g. http://192.168.1.10:8080
    std::wstring share_path;   // e.g. \\NAS01\AzadiTeb_Share
    NetSyncCfg():enabled(false){}
};
NetSyncCfg NetSync_Config();

// POST a UTF-8 JSON body to <admin_host><path>. Falls back to writing a file
// in share_path\AzadiTeb\outbox when the host is unreachable. Returns true if
// the data was delivered OR durably queued.
bool NetSync_PostJson(const wchar_t* path, const std::string& json);

// GET JSON from <admin_host><path> into out (UTF-8). Falls back to reading the
// next file from share_path\AzadiTeb\inbox. Returns true if something was read.
bool NetSync_GetJson(const wchar_t* path, std::string& out);

// HEAD probe (true if the admin host answered 2xx/3xx). Used to decide HTTP vs
// file fallback. Two failures within ~2s are treated as "unreachable".
bool NetSync_HeadOk(const wchar_t* path);

// Convenience: is the admin host currently reachable (cached briefly)?
bool NetSync_HostReachable();
