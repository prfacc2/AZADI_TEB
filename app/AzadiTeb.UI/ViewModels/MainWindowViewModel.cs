using System;
using Avalonia.Threading;
using AzadiTeb.UI.Services;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace AzadiTeb.UI.ViewModels;

/// <summary>
/// Application shell: top bar (brand + theme + clock), the active work page
/// (reception), and the toast host. Owns the live Iran clock timer.
/// </summary>
public partial class MainWindowViewModel : ViewModelBase
{
    private readonly IServiceLocator _services;
    private readonly DispatcherTimer _clock;

    [ObservableProperty] private string _clockText = "";
    [ObservableProperty] private string _dateText = "";
    [ObservableProperty] private bool _isDark;
    [ObservableProperty] private ReceptionViewModel _reception;

    // ----- Toast (non-blocking banner) -----
    [ObservableProperty] private bool _toastVisible;
    [ObservableProperty] private string _toastMessage = "";
    [ObservableProperty] private ToastKind _toastKind = ToastKind.Info;
    private DispatcherTimer? _toastTimer;

    public string BackendStatus { get; }

    public MainWindowViewModel() : this(new ServiceLocator()) { }

    public MainWindowViewModel(IServiceLocator services)
    {
        _services = services;
        _reception = new ReceptionViewModel(services.Backend, services.Notifications);
        _isDark = services.Config.DarkTheme;
        App.SetTheme(_isDark);

        BackendStatus = string.IsNullOrWhiteSpace(services.Config.BackendBaseUrl)
            ? "حالت محلی (Offline)"
            : "متصل به هستهٔ C++";

        services.Notifications.Toasted += OnToast;

        _clock = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        _clock.Tick += (_, _) => Tick();
        _clock.Start();
        Tick();
    }

    private void Tick()
    {
        ClockText = PersianTools.ToFa(PersianTools.IranTime());
        DateText = PersianTools.TodayJalaliLabel();
    }

    partial void OnIsDarkChanged(bool value) => App.SetTheme(value);

    [RelayCommand]
    private void ToggleTheme() => IsDark = !IsDark;

    private void OnToast(object? sender, ToastEventArgs e)
    {
        Dispatcher.UIThread.Post(() =>
        {
            ToastMessage = e.Message;
            ToastKind = e.Kind;
            ToastVisible = true;
            _toastTimer?.Stop();
            _toastTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(3.2) };
            _toastTimer.Tick += (_, _) => { ToastVisible = false; _toastTimer?.Stop(); };
            _toastTimer.Start();
        });
    }
}
