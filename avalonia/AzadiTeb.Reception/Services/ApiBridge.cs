using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace AzadiTeb.Reception.Services;

/// <summary>
/// Talks to the C++ host over the EXACT SAME loopback bridge the retired HTML
/// admission page used:
///   * POST http://127.0.0.1:&lt;port&gt;/api/&lt;verb&gt;  with a JSON body → JSON reply
///   * POST /api/poll  → { "events":[ {event,data}, ... ] } (C++ → UI push)
///
/// Because the C++ side routes every /api verb through the very same
/// admissionApi(verb, body) function it always used, the whole reception stays
/// fully synced with the C++ data layer (patients, services, insurance,
/// billing, queue, printing). NOTHING in the C++ core had to change — we only
/// swapped the presentation engine from an embedded browser to Avalonia.
/// </summary>
public sealed class ApiBridge : IDisposable
{
    private readonly HttpClient _http;
    private readonly string _base;
    private readonly Dictionary<string, List<Action<JsonElement>>> _listeners = new();
    private CancellationTokenSource? _pollCts;

    public bool Connected => _base.Length > 0;

    public ApiBridge(int port)
    {
        _base = port > 0 ? $"http://127.0.0.1:{port}" : "";
        _http = new HttpClient
        {
            Timeout = TimeSpan.FromSeconds(15),
            BaseAddress = _base.Length > 0 ? new Uri(_base) : null
        };
        // The C++ host namespaces verbs / correlates logs by this header.
        _http.DefaultRequestHeaders.TryAddWithoutValidation("X-Az-Page", "admission");
    }

    /// <summary>Call a verb; returns the parsed JSON reply (or an empty object).</summary>
    public async Task<JsonElement> CallAsync(string verb, object? payload = null, CancellationToken ct = default)
    {
        if (!Connected) return EmptyObject();
        try
        {
            var body = payload is null ? "{}" : JsonSerializer.Serialize(payload);
            using var content = new StringContent(body, Encoding.UTF8, "application/json");
            using var resp = await _http.PostAsync("/api/" + verb, content, ct).ConfigureAwait(false);
            var txt = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
            if (string.IsNullOrWhiteSpace(txt)) return EmptyObject();
            using var doc = JsonDocument.Parse(txt);
            return doc.RootElement.Clone();
        }
        catch
        {
            return EmptyObject();
        }
    }

    /// <summary>Subscribe to a C++ → UI push event (catalog.update, patient.load, …).</summary>
    public void On(string evt, Action<JsonElement> handler)
    {
        if (!_listeners.TryGetValue(evt, out var list))
        {
            list = new List<Action<JsonElement>>();
            _listeners[evt] = list;
        }
        list.Add(handler);
    }

    /// <summary>Start long-polling /api/poll for pushed events (fires On handlers).</summary>
    public void StartPolling(Func<Action, Task> uiPost)
    {
        if (!Connected) return;
        _pollCts = new CancellationTokenSource();
        _ = PollLoopAsync(_pollCts.Token, uiPost);
    }

    private async Task PollLoopAsync(CancellationToken ct, Func<Action, Task> uiPost)
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                var res = await CallAsync("poll", null, ct).ConfigureAwait(false);
                if (res.ValueKind == JsonValueKind.Object &&
                    res.TryGetProperty("events", out var evts) &&
                    evts.ValueKind == JsonValueKind.Array)
                {
                    foreach (var e in evts.EnumerateArray())
                    {
                        if (!e.TryGetProperty("event", out var nameEl)) continue;
                        var name = nameEl.GetString() ?? "";
                        var data = e.TryGetProperty("data", out var d) ? d.Clone() : EmptyObject();
                        if (_listeners.TryGetValue(name, out var handlers))
                        {
                            var snapshot = handlers.ToArray();
                            await uiPost(() =>
                            {
                                foreach (var h in snapshot) h(data);
                            }).ConfigureAwait(false);
                        }
                    }
                }
                await Task.Delay(900, ct).ConfigureAwait(false);
            }
            catch (OperationCanceledException) { break; }
            catch { await Task.Delay(1500, ct).ConfigureAwait(false); }
        }
    }

    private static JsonElement EmptyObject()
    {
        using var doc = JsonDocument.Parse("{}");
        return doc.RootElement.Clone();
    }

    public void Dispose()
    {
        _pollCts?.Cancel();
        _http.Dispose();
    }
}
