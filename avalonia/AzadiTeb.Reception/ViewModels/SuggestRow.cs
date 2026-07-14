using AzadiTeb.Reception.Services;

namespace AzadiTeb.Reception.ViewModels;

/// <summary>Generic suggestion / result row (patient, service, doctor lists).</summary>
public sealed class SuggestRow
{
    public string Primary { get; set; } = "";
    public string Secondary { get; set; } = "";
    public string Code { get; set; } = "";
    public string Name { get; set; } = "";
    public long Price { get; set; }

    // Extra fields carried for patient rows so a click can auto-fill the form.
    public string Nid { get; set; } = "";
    public string First { get; set; } = "";
    public string Last { get; set; } = "";
    public string Father { get; set; } = "";
    public string Gender { get; set; } = "";
    public string Birth { get; set; } = "";
    public string Mobile { get; set; } = "";
    public string Phone { get; set; } = "";
    public string Addr { get; set; } = "";
    public int SuppIdx { get; set; } = -1;

    public string PriceDisplay => Price > 0 ? PersianTools.Money(Price) + " ریال" : "";
}
