using AzadiTeb.UI.Models;
using AzadiTeb.UI.Services;
using CommunityToolkit.Mvvm.ComponentModel;

namespace AzadiTeb.UI.ViewModels;

/// <summary>A single editable service line inside the admission services grid.</summary>
public partial class ServiceRowViewModel : ViewModelBase
{
    [ObservableProperty] private string _name = "";
    [ObservableProperty] private string _doctor = "";
    [ObservableProperty] private string _performer = "";
    [ObservableProperty] private long _amount;
    [ObservableProperty] private long _insuranceShare;
    [ObservableProperty] private long _insuranceDiscount;

    public long PatientShare => System.Math.Max(0, Amount - InsuranceShare - InsuranceDiscount);

    public string AmountText => PersianTools.Money(Amount);
    public string InsuranceShareText => PersianTools.Money(InsuranceShare);
    public string DiscountText => PersianTools.Money(InsuranceDiscount);
    public string PatientShareText => PersianTools.Money(PatientShare);

    partial void OnAmountChanged(long value) => RefreshComputed();
    partial void OnInsuranceShareChanged(long value) => RefreshComputed();
    partial void OnInsuranceDiscountChanged(long value) => RefreshComputed();

    private void RefreshComputed()
    {
        OnPropertyChanged(nameof(PatientShare));
        OnPropertyChanged(nameof(AmountText));
        OnPropertyChanged(nameof(InsuranceShareText));
        OnPropertyChanged(nameof(DiscountText));
        OnPropertyChanged(nameof(PatientShareText));
    }

    public ServiceItem ToModel() => new()
    {
        Name = Name, Doctor = Doctor, Performer = Performer,
        Amount = Amount, InsuranceShare = InsuranceShare, InsuranceDiscount = InsuranceDiscount,
    };
}
