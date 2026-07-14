using System;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using AzadiTeb.UI.Models;

namespace AzadiTeb.UI.Services;

/// <summary>
/// Talks to the C++ Core engine over REST. Every method degrades gracefully:
/// on any network error / timeout it transparently falls back to the local
/// engine so the operator is never blocked. Backend stays UI-agnostic — the
/// only contract is JSON over HTTP.
///
///   Avalonia  ──POST /api/patient/search──▶  C++ Backend  ──▶  SQL Server
/// </summary>
public sealed class RestReceptionBackend : IReceptionBackend
{
    private readonly HttpClient _http;
    private readonly LocalReceptionBackend _fallback = new();
    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public RestReceptionBackend(string baseUrl)
    {
        _http = new HttpClient
        {
            BaseAddress = new Uri(baseUrl),
            Timeout = TimeSpan.FromSeconds(4),   // fail fast → fall back, never hang
        };
    }

    public async Task<bool> PingAsync(CancellationToken ct = default)
    {
        try
        {
            using var r = await _http.GetAsync("/api/ping", ct).ConfigureAwait(false);
            return r.IsSuccessStatusCode;
        }
        catch { return false; }
    }

    public async Task<ReferenceData> GetReferenceDataAsync(CancellationToken ct = default)
    {
        try
        {
            var data = await _http.GetFromJsonAsync<ReferenceData>(
                "/api/reference", JsonOpts, ct).ConfigureAwait(false);
            if (data != null) return data;
        }
        catch { /* fall through */ }
        return await _fallback.GetReferenceDataAsync(ct).ConfigureAwait(false);
    }

    public async Task<Patient?> LookupCitizenAsync(string nationalId, CancellationToken ct = default)
    {
        try
        {
            using var resp = await _http.PostAsJsonAsync(
                "/api/patient/search", new { nationalId }, JsonOpts, ct).ConfigureAwait(false);
            if (resp.IsSuccessStatusCode)
                return await resp.Content.ReadFromJsonAsync<Patient>(JsonOpts, ct).ConfigureAwait(false);
        }
        catch { /* fall through */ }
        return await _fallback.LookupCitizenAsync(nationalId, ct).ConfigureAwait(false);
    }

    public async Task<BillResult> ComputeBillAsync(BillRequest request, CancellationToken ct = default)
    {
        try
        {
            using var resp = await _http.PostAsJsonAsync(
                "/api/bill/compute", request, JsonOpts, ct).ConfigureAwait(false);
            if (resp.IsSuccessStatusCode)
            {
                var r = await resp.Content.ReadFromJsonAsync<BillResult>(JsonOpts, ct).ConfigureAwait(false);
                if (r != null) return r;
            }
        }
        catch { /* fall through */ }
        return await _fallback.ComputeBillAsync(request, ct).ConfigureAwait(false);
    }

    public async Task<int> SubmitAdmissionAsync(AdmissionRequest request, CancellationToken ct = default)
    {
        try
        {
            using var resp = await _http.PostAsJsonAsync(
                "/api/admission", request, JsonOpts, ct).ConfigureAwait(false);
            if (resp.IsSuccessStatusCode)
            {
                var doc = await resp.Content.ReadFromJsonAsync<JsonElement>(JsonOpts, ct).ConfigureAwait(false);
                if (doc.TryGetProperty("ticketNo", out var t))
                    return t.GetInt32();
            }
        }
        catch { /* fall through */ }
        return await _fallback.SubmitAdmissionAsync(request, ct).ConfigureAwait(false);
    }
}
