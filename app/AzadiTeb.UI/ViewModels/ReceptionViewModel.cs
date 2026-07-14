using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using AzadiTeb.UI.Models;
using AzadiTeb.UI.Services;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace AzadiTeb.UI.ViewModels;

/// <summary>
/// The پذیرش بیمار (patient admission) form. All I/O is async so the UI never
/// blocks; billing is recomputed live off the authoritative backend.
/// </summary>
public partial class ReceptionViewModel : ViewModelBase
{
    private readonly IReceptionBackend _backend;
    private readonly INotificationService _toast;
    private CancellationTokenSource? _billCts;

    public ReceptionViewModel() : this(new LocalReceptionBackend(), new NotificationService()) { }

    public ReceptionViewModel(IReceptionBackend backend, INotificationService toast)
    {
        _backend = backend;
        _toast = toast;
        Services.CollectionChanged += (_, _) => _ = RecomputeBillAsync();
        _ = LoadReferenceAsync();
    }

    // ---------------- Patient identity ----------------
    [ObservableProperty] private string _firstName = "";
    [ObservableProperty] private string _lastName = "";
    [ObservableProperty] private string _nationalId = "";
    [ObservableProperty] private string _fatherName = "";
    [ObservableProperty] private string _birthDate = "";
    [ObservableProperty] private string _mobile = "";
    [ObservableProperty] private string _phone = "";
    [ObservableProperty] private string _address = "";

    // ---------------- Reference data (combos) ----------------
    public ObservableCollection<string> Genders { get; } = new();
    public ObservableCollection<string> Shifts { get; } = new();
    public ObservableCollection<PatientKind> PatientKinds { get; } = new();
    public ObservableCollection<VisitKind> VisitKinds { get; } = new();
    public ObservableCollection<InsurancePlan> Insurances { get; } = new();

    [ObservableProperty] private string? _selectedGender;
    [ObservableProperty] private string? _selectedShift;
    [ObservableProperty] private PatientKind? _selectedPatientKind;
    [ObservableProperty] private VisitKind? _selectedVisitKind;
    [ObservableProperty] private InsurancePlan? _selectedInsurance;
    [ObservableProperty] private InsurancePlan? _selectedSupplementary;

    [ObservableProperty] private bool _hasInsurance = true;
    [ObservableProperty] private double _supplementaryPercent;
    [ObservableProperty] private string _insuranceStatus = "حالت دارای بیمه — کد ملی را وارد و Enter بزنید.";

    // ---------------- Services grid ----------------
    public ObservableCollection<ServiceRowViewModel> Services { get; } = new();

    // New-service entry fields
    [ObservableProperty] private string _newServiceName = "";
    [ObservableProperty] private string _newServiceDoctor = "";
    [ObservableProperty] private string _newServicePerformer = "";
    [ObservableProperty] private long _newServiceAmount;

    // ---------------- Bill summary ----------------
    [ObservableProperty] private long _billTotal;
    [ObservableProperty] private long _billInsuranceShare;
    [ObservableProperty] private long _billSupplementaryShare;
    [ObservableProperty] private long _billDiscount;
    [ObservableProperty] private long _billPatientPayable;

    public string BillTotalText => PersianTools.Rial(BillTotal);
    public string BillInsuranceText => PersianTools.Rial(BillInsuranceShare);
    public string BillSupplementaryText => PersianTools.Rial(BillSupplementaryShare);
    public string BillDiscountText => PersianTools.Rial(BillDiscount);
    public string BillPayableText => PersianTools.Rial(BillPatientPayable);

    // ---------------- Reception queue ----------------
    public ObservableCollection<QueueEntry> Queue { get; } = new();
    public bool QueueEmpty => Queue.Count == 0;

    [ObservableProperty] private bool _isBusy;

    partial void OnBillTotalChanged(long value) => OnPropertyChanged(nameof(BillTotalText));
    partial void OnBillInsuranceShareChanged(long value) => OnPropertyChanged(nameof(BillInsuranceText));
    partial void OnBillSupplementaryShareChanged(long value) => OnPropertyChanged(nameof(BillSupplementaryText));
    partial void OnBillDiscountChanged(long value) => OnPropertyChanged(nameof(BillDiscountText));
    partial void OnBillPatientPayableChanged(long value) => OnPropertyChanged(nameof(BillPayableText));

    partial void OnHasInsuranceChanged(bool value)
    {
        InsuranceStatus = value
            ? "حالت دارای بیمه — کد ملی را وارد و Enter بزنید."
            : "حالت بدون بیمه — بیمه را به‌صورت دستی انتخاب کنید.";
        if (!value) SelectedInsurance = Insurances.FirstOrDefault(i => i.Code == "none");
        _ = RecomputeBillAsync();
    }

    partial void OnSelectedInsuranceChanged(InsurancePlan? value) => _ = RecomputeBillAsync();
    partial void OnSelectedPatientKindChanged(PatientKind? value) => _ = RecomputeBillAsync();
    partial void OnSelectedVisitKindChanged(VisitKind? value) => _ = RecomputeBillAsync();
    partial void OnSupplementaryPercentChanged(double value) => _ = RecomputeBillAsync();

    private async Task LoadReferenceAsync()
    {
        try
        {
            var data = await _backend.GetReferenceDataAsync().ConfigureAwait(true);
            Genders.Clear(); foreach (var g in data.Genders) Genders.Add(g);
            Shifts.Clear(); foreach (var s in data.Shifts) Shifts.Add(s);
            PatientKinds.Clear(); foreach (var p in data.PatientKinds) PatientKinds.Add(p);
            VisitKinds.Clear(); foreach (var v in data.VisitKinds) VisitKinds.Add(v);
            Insurances.Clear(); foreach (var i in data.Insurances) Insurances.Add(i);

            SelectedGender ??= Genders.FirstOrDefault();
            SelectedShift ??= Shifts.FirstOrDefault();
            SelectedPatientKind ??= PatientKinds.FirstOrDefault();
            SelectedVisitKind ??= VisitKinds.FirstOrDefault();
            SelectedInsurance ??= Insurances.FirstOrDefault(i => i.Code == "tamin") ?? Insurances.FirstOrDefault();
            SelectedSupplementary ??= Insurances.FirstOrDefault(i => i.Code == "none");
        }
        catch (Exception ex)
        {
            _toast.Error("بارگذاری اطلاعات پایه ناموفق بود: " + ex.Message);
        }
    }

    // ---------------- Commands ----------------

    [RelayCommand]
    private async Task LookupCitizenAsync()
    {
        if (!HasInsurance)
        {
            _toast.Warning("برای استعلام، گزینه «دارای بیمه» را فعال کنید.");
            return;
        }
        var nid = PersianTools.ToEn(NationalId).Trim();
        if (nid.Length < 8)
        {
            _toast.Warning("کد ملی معتبر وارد کنید.");
            return;
        }

        IsBusy = true;
        try
        {
            var p = await _backend.LookupCitizenAsync(nid).ConfigureAwait(true);
            if (p != null)
            {
                FirstName = p.FirstName; LastName = p.LastName;
                FatherName = p.FatherName; BirthDate = p.BirthDate;
                SelectedGender = string.IsNullOrEmpty(p.Gender) ? SelectedGender : p.Gender;
                Mobile = p.Mobile; Phone = p.Phone; Address = p.Address;
                _toast.Success("مشخصات بیمار از استعلام دریافت شد.");
            }
            else
            {
                _toast.Info("استعلام برخط ناموفق بود. مشخصات را دستی وارد و بررسی کنید.");
            }
        }
        catch (Exception ex) { _toast.Error("خطای استعلام: " + ex.Message); }
        finally { IsBusy = false; }
    }

    [RelayCommand]
    private async Task AddServiceAsync()
    {
        if (string.IsNullOrWhiteSpace(NewServiceName))
        {
            _toast.Warning("نام خدمت را وارد کنید.");
            return;
        }
        if (NewServiceAmount <= 0)
        {
            _toast.Warning("مبلغ ویزیت/خدمت را وارد کنید.");
            return;
        }
        Services.Add(new ServiceRowViewModel
        {
            Name = NewServiceName.Trim(),
            Doctor = NewServiceDoctor.Trim(),
            Performer = NewServicePerformer.Trim(),
            Amount = NewServiceAmount,
        });
        NewServiceName = ""; NewServiceDoctor = ""; NewServicePerformer = ""; NewServiceAmount = 0;
        _toast.Success("خدمت اضافه شد.");
        await RecomputeBillAsync().ConfigureAwait(true);
    }

    [RelayCommand]
    private async Task RemoveServiceAsync(ServiceRowViewModel? row)
    {
        if (row != null && Services.Remove(row))
        {
            _toast.Info("خدمت حذف شد.");
            await RecomputeBillAsync().ConfigureAwait(true);
        }
    }

    private async Task RecomputeBillAsync()
    {
        // Debounce & cancel any in-flight compute so rapid edits stay smooth.
        _billCts?.Cancel();
        _billCts = new CancellationTokenSource();
        var ct = _billCts.Token;
        try
        {
            var req = BuildBillRequest();
            var result = await _backend.ComputeBillAsync(req, ct).ConfigureAwait(true);
            if (ct.IsCancellationRequested) return;
            BillTotal = result.Total;
            BillInsuranceShare = result.InsuranceShare;
            BillSupplementaryShare = result.SupplementaryShare;
            BillDiscount = result.Discount;
            BillPatientPayable = result.PatientPayable;
        }
        catch (OperationCanceledException) { /* superseded */ }
        catch (Exception ex) { _toast.Error("محاسبه قبض ناموفق بود: " + ex.Message); }
    }

    private BillRequest BuildBillRequest() => new()
    {
        HasInsurance = HasInsurance,
        InsuranceCode = HasInsurance ? (SelectedInsurance?.Code ?? "none") : "none",
        SupplementaryCode = SelectedSupplementary?.Code ?? "none",
        SupplementaryPercent = SupplementaryPercent,
        PatientKindCode = SelectedPatientKind?.Code ?? "normal",
        VisitKindCode = SelectedVisitKind?.Code ?? "normal",
        Services = Services.Select(s => s.ToModel()).ToList(),
    };

    [RelayCommand]
    private Task SubmitAsync() => SubmitCore(true);

    [RelayCommand]
    private Task SubmitNoPayAsync() => SubmitCore(false);

    private async Task SubmitCore(bool pay)
    {
        if (string.IsNullOrWhiteSpace(FirstName) || string.IsNullOrWhiteSpace(LastName))
        {
            _toast.Warning("نام و نام خانوادگی بیمار الزامی است."); return;
        }
        if (string.IsNullOrWhiteSpace(NationalId))
        {
            _toast.Warning("کد ملی الزامی است."); return;
        }
        if (Services.Count == 0)
        {
            _toast.Warning("حداقل یک خدمت به پذیرش اضافه کنید."); return;
        }

        IsBusy = true;
        try
        {
            var req = new AdmissionRequest
            {
                Patient = BuildPatient(),
                Bill = BuildBillRequest(),
                Shift = SelectedShift ?? "",
                Pay = pay,
            };
            int ticket = await _backend.SubmitAdmissionAsync(req).ConfigureAwait(true);

            Queue.Insert(0, new QueueEntry
            {
                TicketNo = ticket,
                PatientName = $"{FirstName} {LastName}".Trim(),
                NationalId = PersianTools.ToEn(NationalId),
                Kind = SelectedPatientKind?.Name ?? "",
                Insurance = HasInsurance ? (SelectedInsurance?.Name ?? "") : "بدون بیمه",
                Payable = BillPatientPayable,
                Time = PersianTools.IranTime(),
                Status = pay ? "پرداخت‌شده" : "صندوق نرفته",
            });
            OnPropertyChanged(nameof(QueueEmpty));

            _toast.Success(pay
                ? $"به «صف پذیرش» افزوده شد — نوبت {PersianTools.ToFa(ticket.ToString())}."
                : $"به «صندوق نرفته‌ها» افزوده شد — نوبت {PersianTools.ToFa(ticket.ToString())} (بدون پرداخت).");
            NewAdmission();
        }
        catch (Exception ex) { _toast.Error("ثبت پذیرش ناموفق بود: " + ex.Message); }
        finally { IsBusy = false; }
    }

    private Patient BuildPatient() => new()
    {
        FirstName = FirstName.Trim(), LastName = LastName.Trim(),
        NationalId = PersianTools.ToEn(NationalId).Trim(), FatherName = FatherName.Trim(),
        BirthDate = BirthDate.Trim(), Gender = SelectedGender ?? "",
        Mobile = PersianTools.ToEn(Mobile).Trim(), Phone = PersianTools.ToEn(Phone).Trim(),
        Address = Address.Trim(),
    };

    [RelayCommand]
    private void NewAdmission()
    {
        FirstName = LastName = NationalId = FatherName = BirthDate = "";
        Mobile = Phone = Address = "";
        SelectedGender = Genders.FirstOrDefault();
        Services.Clear();
        NewServiceName = NewServiceDoctor = NewServicePerformer = "";
        NewServiceAmount = 0;
        SupplementaryPercent = 0;
        BillTotal = BillInsuranceShare = BillSupplementaryShare = BillDiscount = BillPatientPayable = 0;
    }

    [RelayCommand]
    private void RemoveFromQueue(QueueEntry? e)
    {
        if (e != null && Queue.Remove(e))
        {
            OnPropertyChanged(nameof(QueueEmpty));
            _toast.Info("از صف پذیرش حذف شد.");
        }
    }
}
