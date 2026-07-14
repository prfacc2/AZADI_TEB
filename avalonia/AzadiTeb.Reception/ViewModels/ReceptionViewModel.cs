using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using AzadiTeb.Reception.Services;

namespace AzadiTeb.Reception.ViewModels;

/// <summary>
/// The whole «پذیرش بیمار» surface as an MVVM view-model. It is a THIN
/// presentation layer over the C++ /api bridge: every price, identity,
/// insurance rate, bill amount and queue entry comes from the C++ data layer.
/// The layout mirrors the retired HTML admission page 1:1 (right column =
/// profile/search/insurance/doctor, center = patient fields + services + queue,
/// left = invoice summary + primary actions).
/// </summary>
public partial class ReceptionViewModel : ObservableObject
{
    private readonly ApiBridge _api;
    private bool _loaded;

    public ReceptionViewModel(ApiBridge api)
    {
        _api = api;
        Services.CollectionChanged += (_, _) => RecomputeAsync();
    }

    // ---------------------------------------------------------------- header
    [ObservableProperty] private string _headerDate = "—";
    [ObservableProperty] private string _headerTime = "—";
    [ObservableProperty] private string _statusText = "در حال اتصال به هسته…";
    [ObservableProperty] private bool _busy = true;

    // --------------------------------------------------------------- profile
    [ObservableProperty] private string _profileName = "بیمار جدید";
    [ObservableProperty] private string _fileCode = "----";
    [ObservableProperty] private string _rxElectronic = "۰";
    [ObservableProperty] private string _rxSerial = "۰";
    [ObservableProperty] private string _psPaid = "۰";
    [ObservableProperty] private string _psCash = "۰";

    // ------------------------------------------------------------ quick search
    [ObservableProperty] private string _qsNid = "";
    [ObservableProperty] private string _qsFile = "";
    public ObservableCollection<SuggestRow> PatientResults { get; } = new();

    // --------------------------------------------------------------- insurance
    [ObservableProperty] private bool _hasInsurance = true;
    [ObservableProperty] private string _insBooklet = "";
    [ObservableProperty] private string _insValid = "";
    [ObservableProperty] private string _rxDate = "";
    public ObservableCollection<string> InsuranceMain { get; } = new();
    public ObservableCollection<string> InsuranceSupp { get; } = new();

    [ObservableProperty] private int _insMainIndex;
    [ObservableProperty] private int _insSuppIndex;
    [ObservableProperty] private string _insSuppPct = "۰";

    partial void OnHasInsuranceChanged(bool value) => RecomputeAsync();
    partial void OnInsMainIndexChanged(int value) => RecomputeAsync();
    partial void OnInsSuppIndexChanged(int value) => RecomputeAsync();

    // ----------------------------------------------------------- doctor search
    [ObservableProperty] private string _docCode = "";
    [ObservableProperty] private string _docSearch = "";
    public ObservableCollection<SuggestRow> DoctorResults { get; } = new();

    // --------------------------------------------------------- patient fields
    [ObservableProperty] private string _first = "";
    [ObservableProperty] private string _last = "";
    [ObservableProperty] private string _nid = "";
    [ObservableProperty] private string _father = "";
    [ObservableProperty] private string _birth = "";
    [ObservableProperty] private string _mobile = "";
    [ObservableProperty] private string _phone = "";
    [ObservableProperty] private string _addr = "";
    public ObservableCollection<string> Genders { get; } = new() { "مرد", "زن" };
    [ObservableProperty] private int _genderIndex;

    public ObservableCollection<string> PatientTypes { get; } = new() { "عادی", "سرپایی", "بستری" };
    public ObservableCollection<string> VisitTypes { get; } = new() { "عادی", "اورژانس", "پرسنلی" };
    public ObservableCollection<string> Shifts { get; } = new() { "صبح", "عصر", "شب" };
    [ObservableProperty] private int _pTypeIndex;
    [ObservableProperty] private int _nTypeIndex;
    [ObservableProperty] private int _shiftIndex;
    [ObservableProperty] private string _apptDate = "";

    // ---------------------------------------------------------- doctor cards
    public ObservableCollection<string> Doctors { get; } = new();
    [ObservableProperty] private string _doc2Code = "";
    [ObservableProperty] private int _doc2Index = -1;
    [ObservableProperty] private string _perfCode = "";
    [ObservableProperty] private int _perfIndex = -1;

    // -------------------------------------------------------------- services
    public ObservableCollection<ServiceRow> Services { get; } = new();
    [ObservableProperty] private string _svcSearch = "";
    public ObservableCollection<SuggestRow> ServiceSuggest { get; } = new();

    // ----------------------------------------------------------- invoice sums
    [ObservableProperty] private string _sumGross = "۰";
    [ObservableProperty] private string _sumDiscount = "۰";
    [ObservableProperty] private string _sumInsShare = "۰";
    [ObservableProperty] private string _sumPatShare = "۰";
    [ObservableProperty] private string _invTotal = "۰";
    [ObservableProperty] private string _invPatient = "۰";
    [ObservableProperty] private string _invOrg = "۰";
    [ObservableProperty] private string _invFinal = "۰";
    [ObservableProperty] private bool _noPay;

    partial void OnNoPayChanged(bool value) => RecomputeAsync();

    // -------------------------------------------------------------- queue
    public ObservableCollection<QueueRow> Queue { get; } = new();
    [ObservableProperty] private string _queueCount = "۰";
    [ObservableProperty] private string _qSearch = "";

    // =====================================================================
    //  Lifecycle
    // =====================================================================
    public async Task InitAsync()
    {
        if (_loaded) return;
        _loaded = true;

        _api.On("catalog.update", _ => { });
        _api.On("insurance.update", data => Dispatcher.UIThread.Post(() => ApplyInsurance(data)));
        _api.On("patient.load", data => Dispatcher.UIThread.Post(() => ApplyPatient(data)));
        _api.On("ps.update", data => Dispatcher.UIThread.Post(() => ApplyPs(data)));
        _api.StartPolling(action => { Dispatcher.UIThread.Post(action); return Task.CompletedTask; });

        var init = await _api.CallAsync("init");
        if (init.ValueKind == JsonValueKind.Object && init.TryGetProperty("ok", out var ok) && ok.ValueKind == JsonValueKind.True)
        {
            ApplyInsurance(init);
            if (init.TryGetProperty("date", out var d)) HeaderDate = PersianTools.ToFa(d.GetString() ?? "");
            if (init.TryGetProperty("time", out var t)) HeaderTime = PersianTools.ToFa(t.GetString() ?? "");
            ApplyPs(init.TryGetProperty("ps", out var ps) ? ps : init);
            StatusText = "متصل به هسته";
        }
        else
        {
            StatusText = _api.Connected ? "پاسخی از هسته دریافت نشد" : "اجرای مستقل (بدون هسته)";
        }
        Busy = false;

        await LoadDoctorsAsync();
        await RefreshQueueAsync();
        _ = ClockLoopAsync();
    }

    private async Task ClockLoopAsync()
    {
        while (true)
        {
            await Task.Delay(1000);
            var c = await _api.CallAsync("clock");
            if (c.ValueKind == JsonValueKind.Object)
            {
                if (c.TryGetProperty("time", out var t)) HeaderTime = PersianTools.ToFa(t.GetString() ?? HeaderTime);
                if (c.TryGetProperty("date", out var d)) HeaderDate = PersianTools.ToFa(d.GetString() ?? HeaderDate);
            }
        }
    }

    private void ApplyInsurance(JsonElement data)
    {
        // init sends "insurances"/"supp"; the live push sends "main"/"supp".
        JsonElement mainArr = default, suppArr = default;
        if (data.TryGetProperty("insurances", out var i1)) mainArr = i1;
        else if (data.TryGetProperty("main", out var i2)) mainArr = i2;
        if (data.TryGetProperty("supp", out var s1)) suppArr = s1;

        if (mainArr.ValueKind == JsonValueKind.Array)
        {
            var keep = InsMainIndex;
            InsuranceMain.Clear();
            foreach (var e in mainArr.EnumerateArray())
                InsuranceMain.Add(e.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "");
            if (InsuranceMain.Count > 0) InsMainIndex = Math.Clamp(keep, 0, InsuranceMain.Count - 1);
        }
        if (suppArr.ValueKind == JsonValueKind.Array)
        {
            var keep = InsSuppIndex;
            InsuranceSupp.Clear();
            foreach (var e in suppArr.EnumerateArray())
                InsuranceSupp.Add(e.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "");
            if (InsuranceSupp.Count > 0) InsSuppIndex = Math.Clamp(keep, 0, InsuranceSupp.Count - 1);
        }
    }

    private void ApplyPs(JsonElement ps)
    {
        if (ps.ValueKind != JsonValueKind.Object) return;
        if (ps.TryGetProperty("P", out var p)) PsPaid = PersianTools.ToFa(p.ToString());
        if (ps.TryGetProperty("S", out var s)) PsCash = PersianTools.ToFa(s.ToString());
    }

    // =====================================================================
    //  Quick search + patient lookup
    // =====================================================================
    [RelayCommand]
    private async Task LookupNidAsync()
    {
        var nid = PersianTools.ToEn(string.IsNullOrWhiteSpace(QsNid) ? Nid : QsNid).Trim();
        if (nid.Length == 0) return;
        StatusText = "استعلام کد ملی…";
        var r = await _api.CallAsync("patient.lookup", new { nid });
        if (r.TryGetProperty("found", out var f) && f.ValueKind == JsonValueKind.True &&
            r.TryGetProperty("patient", out var p))
        {
            ApplyPatient(p);
            StatusText = "بیمار یافت شد";
        }
        else
        {
            Nid = nid;
            StatusText = "بیمار جدید — اطلاعات را وارد کنید";
        }
    }

    [RelayCommand]
    private async Task SearchPatientAsync()
    {
        var q = string.IsNullOrWhiteSpace(QsFile) ? QsNid : QsFile;
        var r = await _api.CallAsync("patient.search", new { q = q ?? "" });
        PatientResults.Clear();
        if (r.TryGetProperty("rows", out var rows) && rows.ValueKind == JsonValueKind.Array)
        {
            foreach (var p in rows.EnumerateArray())
                PatientResults.Add(PatientToSuggest(p));
        }
    }

    private static SuggestRow PatientToSuggest(JsonElement p)
    {
        string Get(string k) => p.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";
        int GetI(string k) => p.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetInt32() : -1;
        return new SuggestRow
        {
            Primary = $"{Get("first")} {Get("last")}".Trim(),
            Secondary = PersianTools.ToFa(Get("nid")) + "  •  " + PersianTools.ToFa(Get("mobile")),
            Nid = Get("nid"), First = Get("first"), Last = Get("last"), Father = Get("father"),
            Gender = Get("gender"), Birth = Get("birth"), Mobile = Get("mobile"),
            Phone = Get("phone"), Addr = Get("addr"), SuppIdx = GetI("suppIdx"),
        };
    }

    [RelayCommand]
    private void PickPatient(SuggestRow? row)
    {
        if (row is null) return;
        First = row.First; Last = row.Last; Nid = PersianTools.ToFa(row.Nid);
        Father = row.Father; Birth = PersianTools.ToFa(row.Birth);
        Mobile = PersianTools.ToFa(row.Mobile); Phone = PersianTools.ToFa(row.Phone);
        Addr = row.Addr;
        GenderIndex = row.Gender == "زن" ? 1 : 0;
        if (row.SuppIdx >= 0 && row.SuppIdx < InsuranceSupp.Count) InsSuppIndex = row.SuppIdx;
        ProfileName = $"{row.First} {row.Last}".Trim();
        if (ProfileName.Length == 0) ProfileName = "بیمار جدید";
        PatientResults.Clear();
    }

    private void ApplyPatient(JsonElement p)
    {
        string Get(string k) => p.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";
        First = Get("first"); Last = Get("last"); Nid = PersianTools.ToFa(Get("nid"));
        Father = Get("father"); Birth = PersianTools.ToFa(Get("birth"));
        Mobile = PersianTools.ToFa(Get("mobile")); Phone = PersianTools.ToFa(Get("phone"));
        Addr = Get("addr");
        GenderIndex = Get("gender") == "زن" ? 1 : 0;
        if (p.TryGetProperty("suppIdx", out var si) && si.ValueKind == JsonValueKind.Number)
        {
            var idx = si.GetInt32();
            if (idx >= 0 && idx < InsuranceSupp.Count) InsSuppIndex = idx;
        }
        ProfileName = $"{First} {Last}".Trim();
        if (ProfileName.Length == 0) ProfileName = "بیمار جدید";
    }

    // =====================================================================
    //  Doctor search
    // =====================================================================
    private async Task LoadDoctorsAsync()
    {
        var r = await _api.CallAsync("doctor.search", new { q = "" });
        Doctors.Clear();
        if (r.TryGetProperty("rows", out var rows) && rows.ValueKind == JsonValueKind.Array)
        {
            foreach (var d in rows.EnumerateArray())
                Doctors.Add(d.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "");
        }
    }

    [RelayCommand]
    private async Task SearchDoctorAsync()
    {
        var r = await _api.CallAsync("doctor.search",
            string.IsNullOrWhiteSpace(DocCode) ? new { q = DocSearch ?? "" } : new { q = DocCode });
        DoctorResults.Clear();
        if (r.TryGetProperty("rows", out var rows) && rows.ValueKind == JsonValueKind.Array)
        {
            foreach (var d in rows.EnumerateArray())
            {
                string Get(string k) => d.TryGetProperty(k, out var v) ? v.GetString() ?? "" : "";
                DoctorResults.Add(new SuggestRow
                {
                    Primary = Get("name"),
                    Secondary = Get("specialty"),
                    Code = Get("code"),
                    Name = Get("name"),
                });
            }
        }
    }

    [RelayCommand]
    private void PickDoctor(SuggestRow? row)
    {
        if (row is null) return;
        var idx = Doctors.IndexOf(row.Name);
        if (idx < 0) { Doctors.Add(row.Name); idx = Doctors.Count - 1; }
        Doc2Index = idx;
        Doc2Code = PersianTools.ToFa(row.Code);
        DoctorResults.Clear();
    }

    // =====================================================================
    //  Services
    // =====================================================================
    [RelayCommand]
    private async Task SearchServiceAsync()
    {
        var r = await _api.CallAsync("service.search", new { q = SvcSearch ?? "" });
        ServiceSuggest.Clear();
        if (r.TryGetProperty("rows", out var rows) && rows.ValueKind == JsonValueKind.Array)
        {
            foreach (var s in rows.EnumerateArray())
            {
                string Get(string k) => s.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";
                long price = s.TryGetProperty("price", out var pr) && pr.ValueKind == JsonValueKind.Number ? pr.GetInt64() : 0;
                ServiceSuggest.Add(new SuggestRow
                {
                    Primary = Get("name"),
                    Secondary = PersianTools.ToFa(Get("code")),
                    Code = Get("code"), Name = Get("name"), Price = price,
                });
            }
        }
    }

    [RelayCommand]
    private async Task AddServiceAsync()
    {
        // If the search box holds a code, resolve it authoritatively first.
        if (!string.IsNullOrWhiteSpace(SvcSearch))
        {
            var res = await _api.CallAsync("service.resolve", new { code = PersianTools.ToEn(SvcSearch) });
            if (res.TryGetProperty("found", out var f) && f.ValueKind == JsonValueKind.True &&
                res.TryGetProperty("service", out var s))
            {
                AddServiceRow(s.TryGetProperty("code", out var c) ? c.GetString() ?? "" : "",
                              s.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "",
                              s.TryGetProperty("price", out var p) && p.ValueKind == JsonValueKind.Number ? p.GetInt64() : 0);
                SvcSearch = "";
                ServiceSuggest.Clear();
                return;
            }
        }
        // else: first suggestion, if any.
        if (ServiceSuggest.Count > 0) PickService(ServiceSuggest[0]);
    }

    [RelayCommand]
    private void PickService(SuggestRow? row)
    {
        if (row is null) return;
        AddServiceRow(row.Code, row.Name, row.Price);
        SvcSearch = "";
        ServiceSuggest.Clear();
    }

    private void AddServiceRow(string code, string name, long price)
    {
        var row = new ServiceRow(Services.Count + 1) { Code = code, Name = name, Price = price, Qty = 1 };
        row.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName is nameof(ServiceRow.Qty) or nameof(ServiceRow.Discount) or nameof(ServiceRow.Price))
                RecomputeAsync();
        };
        Services.Add(row);
    }

    [RelayCommand]
    private void RemoveService(ServiceRow? row)
    {
        if (row is null) return;
        Services.Remove(row);
        for (int i = 0; i < Services.Count; i++) Services[i].RowIndex = i + 1;
        RecomputeAsync();
    }

    // =====================================================================
    //  Bill compute (server-authoritative)
    // =====================================================================
    private bool _recomputing;
    private async void RecomputeAsync()
    {
        if (_recomputing) return;
        _recomputing = true;
        try
        {
            var body = BuildBody();
            var r = await _api.CallAsync("bill.compute", body);
            if (r.ValueKind != JsonValueKind.Object) return;
            long G(string k) => r.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetInt64() : 0;
            long gross = G("gross"), disc = G("disc"), org = G("org"), supp = G("supp"), pat = G("pat");
            SumGross = PersianTools.Money(gross);
            SumDiscount = PersianTools.Money(disc);
            SumInsShare = PersianTools.Money(org + supp);
            SumPatShare = PersianTools.Money(pat);
            InvTotal = PersianTools.Money(gross);
            InvOrg = PersianTools.Money(org + supp);
            InvPatient = PersianTools.Money(pat);
            InvFinal = PersianTools.Money(gross);
            // distribute shares over rows proportionally for display
            long totalNet = Services.Sum(s => Math.Max(0, s.Gross - s.Discount));
            foreach (var s in Services)
            {
                long net = Math.Max(0, s.Gross - s.Discount);
                double frac = totalNet > 0 ? (double)net / totalNet : 0;
                s.InsShare = (long)Math.Round((org + supp) * frac);
                s.PatientShare = (long)Math.Round(pat * frac);
            }
        }
        finally { _recomputing = false; }
    }

    // =====================================================================
    //  Save + queue + new
    // =====================================================================
    [RelayCommand]
    private async Task SaveAsync()
    {
        if (string.IsNullOrWhiteSpace(Nid))
        {
            StatusText = "کد ملی خالی است";
            return;
        }
        StatusText = "در حال ثبت و صدور قبض…";
        var r = await _api.CallAsync("admission.save", BuildBody());
        if (r.TryGetProperty("ok", out var ok) && ok.ValueKind == JsonValueKind.True)
        {
            StatusText = "پذیرش ثبت شد";
            await RefreshQueueAsync();
            NewAdmission();
        }
        else
        {
            StatusText = r.TryGetProperty("err", out var e) ? (e.GetString() ?? "خطا در ثبت") : "خطا در ثبت";
        }
    }

    [RelayCommand]
    private async Task AddToQueueAsync()
    {
        if (string.IsNullOrWhiteSpace(Nid)) { StatusText = "کد ملی خالی است"; return; }
        var r = await _api.CallAsync("queue.add", BuildBody());
        if (r.TryGetProperty("ok", out var ok) && ok.ValueKind == JsonValueKind.True)
        {
            StatusText = "به صندوق نرفته‌ها افزوده شد";
            await RefreshQueueAsync();
        }
    }

    [RelayCommand]
    private async Task RemoveQueueAsync(QueueRow? row)
    {
        if (row is null) return;
        await _api.CallAsync("queue.remove", new { id = row.Id });
        await RefreshQueueAsync();
    }

    private async Task RefreshQueueAsync()
    {
        var r = await _api.CallAsync("queue.list");
        Queue.Clear();
        if (r.TryGetProperty("rows", out var rows) && rows.ValueKind == JsonValueKind.Array)
        {
            int i = 1;
            foreach (var q in rows.EnumerateArray())
            {
                string Get(string k) => q.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";
                long GetL(string k) => q.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetInt64() : 0;
                Queue.Add(new QueueRow
                {
                    RowIndex = i++,
                    Id = Get("id"),
                    PatientName = $"{Get("first")} {Get("last")}".Trim(),
                    Barcode = Get("barcode"),
                    JDate = Get("jdate"),
                    Time = Get("time"),
                    MinutesAgo = GetL("minutesAgo"),
                    Amount = GetL("amount"),
                });
            }
        }
        QueueCount = PersianTools.ToFa(Queue.Count.ToString());
    }

    [RelayCommand]
    private void NewAdmission()
    {
        First = Last = Nid = Father = Birth = Mobile = Phone = Addr = "";
        QsNid = QsFile = InsBooklet = InsValid = RxDate = "";
        DocCode = DocSearch = SvcSearch = "";
        GenderIndex = PTypeIndex = NTypeIndex = ShiftIndex = 0;
        Doc2Index = PerfIndex = -1; Doc2Code = PerfCode = "";
        InsSuppPct = "۰"; NoPay = false; HasInsurance = true;
        ProfileName = "بیمار جدید"; FileCode = "----";
        Services.Clear(); PatientResults.Clear(); DoctorResults.Clear(); ServiceSuggest.Clear();
        RecomputeAsync();
        StatusText = "پذیرش جدید";
    }

    [RelayCommand]
    private async Task PrintLastAsync() => await _api.CallAsync("print.last");
    [RelayCommand]
    private async Task ElectronicRxAsync() => await _api.CallAsync("rx.electronic");

    // =====================================================================
    //  Build the shared request body (identical shape to the HTML page).
    // =====================================================================
    private object BuildBody()
    {
        var svc = Services.Select(s => new
        {
            code = PersianTools.ToEn(s.Code),
            name = s.Name,
            price = s.Price,
            qty = s.Qty,
            discount = s.Discount,
        }).ToArray();

        return new
        {
            patient = new
            {
                nid = PersianTools.ToEn(Nid),
                first = First,
                last = Last,
                father = Father,
                gender = GenderIndex == 1 ? "زن" : "مرد",
                birth = PersianTools.ToEn(Birth),
                mobile = PersianTools.ToEn(Mobile),
                phone = PersianTools.ToEn(Phone),
                addr = Addr,
            },
            hasIns = HasInsurance,
            insMain = InsMainIndex,
            insSupp = InsSuppIndex,
            insSuppPct = PersianTools.ParseLong(InsSuppPct),
            ptype = PatientTypes.Count > PTypeIndex && PTypeIndex >= 0 ? PatientTypes[PTypeIndex] : "عادی",
            ntype = VisitTypes.Count > NTypeIndex && NTypeIndex >= 0 ? VisitTypes[NTypeIndex] : "عادی",
            apptShift = Shifts.Count > ShiftIndex && ShiftIndex >= 0 ? Shifts[ShiftIndex] : "صبح",
            apptDate = PersianTools.ToEn(ApptDate),
            doc2name = Doc2Index >= 0 && Doc2Index < Doctors.Count ? Doctors[Doc2Index] : "",
            perfname = PerfIndex >= 0 && PerfIndex < Doctors.Count ? Doctors[PerfIndex] : "",
            insBooklet = PersianTools.ToEn(InsBooklet),
            noPay = NoPay,
            services = svc,
        };
    }
}
