using CommunityToolkit.Mvvm.ComponentModel;
using AzadiTeb.Reception.Services;

namespace AzadiTeb.Reception.ViewModels;

/// <summary>
/// One line in the services table (خدمات). Prices/codes always originate from
/// the Management catalog via the C++ /api service.* verbs — the UI never
/// invents them.
/// </summary>
public partial class ServiceRow : ObservableObject
{
    public ServiceRow(int index) => RowIndex = index;

    [ObservableProperty] private int _rowIndex;
    [ObservableProperty] private string _code = "";
    [ObservableProperty] private string _name = "";

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(GrossDisplay), nameof(PatientShareDisplay), nameof(InsShareDisplay))]
    private long _price;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(GrossDisplay), nameof(PatientShareDisplay), nameof(InsShareDisplay))]
    private int _qty = 1;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(GrossDisplay), nameof(PatientShareDisplay), nameof(InsShareDisplay))]
    private long _discount;

    // Server-authoritative shares, refreshed after each bill.compute.
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(InsShareDisplay))]
    private long _insShare;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(PatientShareDisplay))]
    private long _patientShare;

    public long Gross => Price * (Qty > 0 ? Qty : 1);

    public string QtyDisplay => PersianTools.ToFa(Qty.ToString());
    public string PriceDisplay => PersianTools.Money(Price);
    public string DiscountDisplay => PersianTools.Money(Discount);
    public string GrossDisplay => PersianTools.Money(Gross);
    public string InsShareDisplay => PersianTools.Money(InsShare);
    public string PatientShareDisplay => PersianTools.Money(PatientShare);
    public string RowIndexDisplay => PersianTools.ToFa(RowIndex.ToString());
}
