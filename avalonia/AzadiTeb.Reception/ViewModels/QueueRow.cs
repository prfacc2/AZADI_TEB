using AzadiTeb.Reception.Services;

namespace AzadiTeb.Reception.ViewModels;

/// <summary>One row in the «صندوق نرفته‌ها» unpaid queue table.</summary>
public sealed class QueueRow
{
    public int RowIndex { get; set; }
    public string Id { get; set; } = "";
    public string PatientName { get; set; } = "";
    public string Barcode { get; set; } = "";
    public string JDate { get; set; } = "";
    public string Time { get; set; } = "";
    public long MinutesAgo { get; set; }
    public long Amount { get; set; }

    public string RowIndexDisplay => PersianTools.ToFa(RowIndex.ToString());
    public string JDateDisplay => PersianTools.ToFa(JDate);
    public string TimeDisplay => PersianTools.ToFa(Time);
    public string MinutesAgoDisplay => PersianTools.ToFa(MinutesAgo.ToString());
    public string AmountDisplay => PersianTools.Money(Amount);
}
