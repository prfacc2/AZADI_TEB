using System;
using System.Globalization;
using System.Linq;
using System.Text;

namespace AzadiTeb.UI.Services;

/// <summary>
/// Persian locale helpers: Jalali calendar, Persian digits, Rial formatting.
/// Pure/stateless — safe to call from any thread.
/// </summary>
public static class PersianTools
{
    private static readonly PersianCalendar Pc = new();
    private const string FaDigits = "۰۱۲۳۴۵۶۷۸۹";

    /// <summary>Today's Jalali date as YYYY/MM/DD.</summary>
    public static string TodayJalali()
    {
        var now = DateTime.Now;
        return $"{Pc.GetYear(now):0000}/{Pc.GetMonth(now):00}/{Pc.GetDayOfMonth(now):00}";
    }

    /// <summary>Live Iran time (UTC+3:30) as HH:mm:ss.</summary>
    public static string IranTime()
    {
        var iran = DateTime.UtcNow.AddHours(3.5);
        return iran.ToString("HH:mm:ss", CultureInfo.InvariantCulture);
    }

    /// <summary>Full Jalali date label, e.g. «شنبه ۱۴۰۳/۰۵/۱۲».</summary>
    public static string TodayJalaliLabel()
    {
        var now = DateTime.Now;
        string[] days = { "یکشنبه", "دوشنبه", "سه‌شنبه", "چهارشنبه", "پنجشنبه", "جمعه", "شنبه" };
        var dow = days[(int)now.DayOfWeek];
        return $"{dow} {ToFa(TodayJalali())}";
    }

    /// <summary>Convert ASCII digits in a string to Persian digits.</summary>
    public static string ToFa(string s)
    {
        if (string.IsNullOrEmpty(s)) return s;
        var sb = new StringBuilder(s.Length);
        foreach (var c in s)
            sb.Append(c is >= '0' and <= '9' ? FaDigits[c - '0'] : c);
        return sb.ToString();
    }

    /// <summary>Convert Persian/Arabic digits to ASCII.</summary>
    public static string ToEn(string s)
    {
        if (string.IsNullOrEmpty(s)) return s;
        var sb = new StringBuilder(s.Length);
        foreach (var c in s)
        {
            int idx = FaDigits.IndexOf(c);
            if (idx >= 0) { sb.Append((char)('0' + idx)); continue; }
            // Arabic-Indic ٠..٩
            if (c is >= '\u0660' and <= '\u0669') { sb.Append((char)('0' + (c - '\u0660'))); continue; }
            sb.Append(c);
        }
        return sb.ToString();
    }

    /// <summary>Format a Rial amount with Persian digits + thousands separators.</summary>
    public static string Rial(long amount)
    {
        var s = amount.ToString("#,0", CultureInfo.InvariantCulture);
        return ToFa(s) + " ریال";
    }

    /// <summary>Format a Rial amount, digits only (no suffix).</summary>
    public static string Money(long amount)
        => ToFa(amount.ToString("#,0", CultureInfo.InvariantCulture));

    /// <summary>
    /// Smart birth-date entry: digits typed in are auto-slashed into a Jalali
    /// YYYY/MM/DD as the user types (matches the native form behaviour).
    /// </summary>
    public static string SmartJalali(string raw)
    {
        var d = ToEn(raw ?? "").Where(char.IsDigit).ToArray();
        var digits = new string(d);
        if (digits.Length > 8) digits = digits[..8];
        var sb = new StringBuilder();
        for (int i = 0; i < digits.Length; i++)
        {
            if (i == 4 || i == 6) sb.Append('/');
            sb.Append(digits[i]);
        }
        return ToFa(sb.ToString());
    }

    /// <summary>Parse a possibly-Persian numeric string to long (0 on fail).</summary>
    public static long ParseLong(string? s)
    {
        if (string.IsNullOrWhiteSpace(s)) return 0;
        var clean = new string(ToEn(s).Where(c => char.IsDigit(c)).ToArray());
        return long.TryParse(clean, out var v) ? v : 0;
    }
}
