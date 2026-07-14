using System;
using Avalonia;

namespace AzadiTeb.Reception;

/// <summary>
/// Entry point for the Avalonia Patient-Reception surface («پذیرش بیمار»).
///
/// This process is spawned by the native C++ host (AzadiTeb.exe) when the
/// reception tab opens. It is embedded INSIDE that tab: the C++ side reparents
/// this window's HWND as a child of the reception page and keeps it sized to
/// fill. All data flows over the SAME loopback /api bridge the old HTML surface
/// used, so C++ stays the single source of truth (patients, services,
/// insurance, billing, queue, printing) — nothing in the C++ core changed.
///
/// Command line:
///   --port &lt;n&gt;      loopback API port served by the C++ host (required)
///   --parent &lt;hwnd&gt;  parent HWND (decimal) to embed into (optional; when
///                     present we co-operate with the host reparent + resize)
/// </summary>
internal static class Program
{
    public static int ApiPort;
    public static IntPtr ParentHwnd;

    [STAThread]
    public static void Main(string[] args)
    {
        ParseArgs(args);
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    private static void ParseArgs(string[] args)
    {
        for (int i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--port" when i + 1 < args.Length:
                    int.TryParse(args[++i], out ApiPort);
                    break;
                case "--parent" when i + 1 < args.Length:
                    if (long.TryParse(args[++i], out var h)) ParentHwnd = new IntPtr(h);
                    break;
            }
        }
        // Env-var fallback (used when the host prefers not to pass argv).
        if (ApiPort == 0 && int.TryParse(Environment.GetEnvironmentVariable("AZ_API_PORT"), out var p))
            ApiPort = p;
        if (ParentHwnd == IntPtr.Zero && long.TryParse(Environment.GetEnvironmentVariable("AZ_PARENT_HWND"), out var ph))
            ParentHwnd = new IntPtr(ph);
    }

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
}
