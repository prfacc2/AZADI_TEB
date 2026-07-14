using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using AzadiTeb.Reception.Services;
using AzadiTeb.Reception.ViewModels;
using AzadiTeb.Reception.Views;

namespace AzadiTeb.Reception;

public partial class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var bridge = new ApiBridge(Program.ApiPort);
            var vm = new ReceptionViewModel(bridge);
            desktop.MainWindow = new MainWindow(vm, Program.ParentHwnd);
        }
        base.OnFrameworkInitializationCompleted();
    }
}
