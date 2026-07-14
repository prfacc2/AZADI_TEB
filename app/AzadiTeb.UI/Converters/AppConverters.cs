using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media;
using AzadiTeb.UI.Services;

namespace AzadiTeb.UI.Converters;

/// <summary>Maps a toast kind to an accent brush.</summary>
public sealed class ToastKindToBrushConverter : IValueConverter
{
    public static readonly ToastKindToBrushConverter Instance = new();

    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        => value is ToastKind k
            ? new SolidColorBrush(k switch
            {
                ToastKind.Success => Color.Parse("#16A34A"),
                ToastKind.Warning => Color.Parse("#D97706"),
                ToastKind.Error => Color.Parse("#E5484D"),
                _ => Color.Parse("#1F6FEB"),
            })
            : Brushes.SlateGray;

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}

/// <summary>Maps a toast kind to a leading glyph.</summary>
public sealed class ToastKindToIconConverter : IValueConverter
{
    public static readonly ToastKindToIconConverter Instance = new();

    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        => value is ToastKind k
            ? k switch
            {
                ToastKind.Success => "\u2714",  // ✔
                ToastKind.Warning => "\u26A0",  // ⚠
                ToastKind.Error => "\u2716",     // ✖
                _ => "\u2139",                    // ℹ
            }
            : "\u2139";

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}
