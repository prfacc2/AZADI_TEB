using System;
using System.IO;
using System.Text.Json;

namespace AzadiTeb.UI.Services;

public interface IServiceLocator
{
    IReceptionBackend Backend { get; }
    INotificationService Notifications { get; }
    AppConfig Config { get; }
}

/// <summary>App configuration loaded from data/settings.json (optional).</summary>
public sealed class AppConfig
{
    public string BackendBaseUrl { get; set; } = "";   // empty => local-only
    public bool DarkTheme { get; set; }
}

/// <summary>
/// Minimal composition root. Chooses the REST backend when a URL is
/// configured, otherwise the offline local engine. Either way the UI code
/// only ever sees <see cref="IReceptionBackend"/>.
/// </summary>
public sealed class ServiceLocator : IServiceLocator
{
    public IReceptionBackend Backend { get; }
    public INotificationService Notifications { get; }
    public AppConfig Config { get; }

    public ServiceLocator()
    {
        Config = LoadConfig();
        Backend = string.IsNullOrWhiteSpace(Config.BackendBaseUrl)
            ? new LocalReceptionBackend()
            : new RestReceptionBackend(Config.BackendBaseUrl);
        Notifications = new NotificationService();
    }

    private static AppConfig LoadConfig()
    {
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "data", "settings.json");
            if (File.Exists(path))
            {
                var cfg = JsonSerializer.Deserialize<AppConfig>(
                    File.ReadAllText(path),
                    new JsonSerializerOptions { PropertyNameCaseInsensitive = true });
                if (cfg != null) return cfg;
            }
        }
        catch { /* use defaults */ }
        return new AppConfig();
    }
}
