using System;

namespace AzadiTeb.UI.Services;

public enum ToastKind { Info, Success, Warning, Error }

public sealed class ToastEventArgs : EventArgs
{
    public string Message { get; init; } = "";
    public ToastKind Kind { get; init; } = ToastKind.Info;
}

public interface INotificationService
{
    event EventHandler<ToastEventArgs>? Toasted;
    void Info(string message);
    void Success(string message);
    void Warning(string message);
    void Error(string message);
}

/// <summary>
/// Lightweight in-app toast bus. The shell subscribes and renders a
/// non-blocking banner — no modal dialogs that could freeze the operator.
/// </summary>
public sealed class NotificationService : INotificationService
{
    public event EventHandler<ToastEventArgs>? Toasted;

    private void Raise(string m, ToastKind k)
        => Toasted?.Invoke(this, new ToastEventArgs { Message = m, Kind = k });

    public void Info(string message) => Raise(message, ToastKind.Info);
    public void Success(string message) => Raise(message, ToastKind.Success);
    public void Warning(string message) => Raise(message, ToastKind.Warning);
    public void Error(string message) => Raise(message, ToastKind.Error);
}
