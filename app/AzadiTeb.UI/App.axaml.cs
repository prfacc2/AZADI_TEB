using System;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Styling;
using AzadiTeb.UI.Services;
using AzadiTeb.UI.ViewModels;
using AzadiTeb.UI.Views;

namespace AzadiTeb.UI;

public partial class App : Application
{
    public static IServiceLocator Services { get; private set; } = null!;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);

        // Vazirmatn as the global UI font — embedded in resources, so no
        // system-font dependency (identical rendering on every clinic PC).
        try
        {
            var vazir = new FontFamily(
                "avares://AzadiTeb/Assets/Fonts#Vazirmatn");
            Resources["AppFont"] = vazir;
        }
        catch { /* fall back to system font */ }
    }

    public override void OnFrameworkInitializationCompleted()
    {
        // Composition root — a tiny service locator (no external DI needed for
        // the UI; the C++ backend owns the heavy business logic over REST).
        Services = new ServiceLocator();

        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = new MainWindow
            {
                DataContext = new MainWindowViewModel(Services)
            };
        }

        base.OnFrameworkInitializationCompleted();
    }

    /// <summary>Toggle Light/Dark at runtime.</summary>
    public static void SetTheme(bool dark)
    {
        if (Current is not null)
            Current.RequestedThemeVariant = dark ? ThemeVariant.Dark : ThemeVariant.Light;
    }
}
