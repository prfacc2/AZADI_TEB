using System;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using AzadiTeb.Reception.ViewModels;

namespace AzadiTeb.Reception.Views;

public partial class MainWindow : Window
{
    private readonly ReceptionViewModel _vm;
    private readonly IntPtr _parentHwnd;

    // Parameterless ctor for the XAML previewer / designer only.
    public MainWindow() : this(new ReceptionViewModel(new Services.ApiBridge(0)), IntPtr.Zero) { }

    public MainWindow(ReceptionViewModel vm, IntPtr parentHwnd)
    {
        _vm = vm;
        _parentHwnd = parentHwnd;
        InitializeComponent();
        DataContext = _vm;

        Opened += OnOpened;
    }

    private async void OnOpened(object? sender, EventArgs e)
    {
        // When launched embedded, reparent our top-level HWND into the C++
        // reception tab so the Avalonia surface fills that tab exactly like the
        // old WebView2 child did. The C++ host keeps us sized via MoveWindow.
        if (_parentHwnd != IntPtr.Zero && OperatingSystem.IsWindows())
            TryEmbed(_parentHwnd);

        await _vm.InitAsync();
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    // ---------------------------------------------------------------- Win32
    private const int GWL_STYLE = -16;
    private const long WS_CHILD = 0x40000000L;
    private const long WS_POPUP = 0x80000000L;
    private const long WS_CAPTION = 0x00C00000L;
    private const long WS_THICKFRAME = 0x00040000L;
    private const uint SWP_NOZORDER = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;
    private const uint SWP_FRAMECHANGED = 0x0020;
    private const uint SWP_SHOWWINDOW = 0x0040;

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetParent(IntPtr hWndChild, IntPtr hWndNewParent);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern long GetWindowLongPtr(IntPtr hWnd, int nIndex);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern long SetWindowLongPtr(IntPtr hWnd, int nIndex, long dwNewLong);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hAfter, int x, int y, int cx, int cy, uint flags);

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int left, top, right, bottom; }

    private void TryEmbed(IntPtr parent)
    {
        try
        {
            var handle = TryGetPlatformHandle();
            if (handle is null || handle.Handle == IntPtr.Zero) return;
            var hwnd = handle.Handle;

            // Turn our window into a borderless child of the reception tab.
            long style = GetWindowLongPtr(hwnd, GWL_STYLE);
            style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME);
            style |= WS_CHILD;
            SetWindowLongPtr(hwnd, GWL_STYLE, style);

            SetParent(hwnd, parent);

            if (GetClientRect(parent, out var rc))
            {
                SetWindowPos(hwnd, IntPtr.Zero, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            }
        }
        catch
        {
            // If embedding fails, fall back to a normal top-level window so the
            // operator still gets the reception surface (never lose the feature).
        }
    }
}
