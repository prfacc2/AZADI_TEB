using System;
using System.Globalization;
using System.Linq;
using System.Text;

namespace AzadiTeb.Reception.Services;

/// <summary>
/// Persian locale helpers: Jalali digits, Rial formatting, smart date entry.
/// Pure/stateless. The C++ host owns the authoritative Jalali *date* (sent via
/// the /api init + clock verbs), so we only format/convert digits here.
/// </summary>
public static class PersianTools
{
    private const string FaDigits = "۰۱۲۳۴۵۶۷۸۹";

    /// <summary>ASCII digits → Persian digits.</summary>
    public static string ToFa(string? s)
    {
        if (string.IsNullOrEmpty(s)) return s ?? "";
        var sb = new StringBuilder(s.Length);
        foreach (var c in s)
            sb.Append(c is >= '0' and <= '9' ? FaDigits[c - '0'] : c);
        return sb.ToString();
    }

    /// <summary>Persian / Arabic-Indic digits → ASCII.</summary>
    public static string ToEn(string? s)
    {
        if (string.IsNullOrEmpty(s)) return s ?? "";
        var sb = new StringBuilder(s.Length);
        foreach (var c in s)
        {
            int idx = FaDigits.IndexOf(c);
            if (idx >= 0) { sb.Append((char)('0' + idx)); continue; }
            if (c is >= '\u0660' and <= '\u0669') { sb.Append((char)('0' + (c - '\u0660'))); continue; }
            sb.Append(c);
        }
        return sb.ToString();
    }

    /// <summary>Format a Rial amount with Persian digits + thousands separators.</summary>
    public static string Money(long amount)
        => ToFa(amount.ToString("#,0", CultureInfo.InvariantCulture));

    public static string Rial(long amount) => Money(amount) + " ریال";

    /// <summary>Smart Jalali entry: auto-insert slashes as YYYY/MM/DD.</summary>
    public static string SmartJalali(string? raw)
    {
        var digits = new string(ToEn(raw ?? "").Where(char.IsDigit).ToArray());
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
        var clean = new string(ToEn(s).Where(char.IsDigit).ToArray());
        return long.TryParse(clean, out var v) ? v : 0;
    }
}
