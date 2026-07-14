using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using AzadiTeb.UI.Models;

namespace AzadiTeb.UI.Services;

/// <summary>
/// Contract for the reception backend. The production implementation talks to
/// the C++ Core engine over REST/WebSocket; a local implementation provides a
/// deterministic offline fallback so the UI is always responsive.
///
/// Every call is async + cancellable => no UI thread is ever blocked.
/// </summary>
public interface IReceptionBackend
{
    /// <summary>Reference data used to populate the form combos.</summary>
    Task<ReferenceData> GetReferenceDataAsync(CancellationToken ct = default);

    /// <summary>Online national-ID lookup (ثبت احوال / سامانه بیمه).</summary>
    Task<Patient?> LookupCitizenAsync(string nationalId, CancellationToken ct = default);

    /// <summary>Authoritative bill computation (tariff + insurance math).</summary>
    Task<BillResult> ComputeBillAsync(BillRequest request, CancellationToken ct = default);

    /// <summary>Persist the admission and return the assigned ticket number.</summary>
    Task<int> SubmitAdmissionAsync(AdmissionRequest request, CancellationToken ct = default);

    /// <summary>True when the remote C++ backend is reachable.</summary>
    Task<bool> PingAsync(CancellationToken ct = default);
}

public sealed class ReferenceData
{
    public List<InsurancePlan> Insurances { get; set; } = new();
    public List<PatientKind> PatientKinds { get; set; } = new();
    public List<VisitKind> VisitKinds { get; set; } = new();
    public List<string> Genders { get; set; } = new();
    public List<string> Shifts { get; set; } = new();
}

public sealed class BillRequest
{
    public string PatientKindCode { get; set; } = "";
    public string VisitKindCode { get; set; } = "";
    public string InsuranceCode { get; set; } = "";
    public string SupplementaryCode { get; set; } = "";
    public double SupplementaryPercent { get; set; }
    public bool HasInsurance { get; set; }
    public List<ServiceItem> Services { get; set; } = new();
}

public sealed class AdmissionRequest
{
    public Patient Patient { get; set; } = new();
    public BillRequest Bill { get; set; } = new();
    public string Shift { get; set; } = "";
    public bool Pay { get; set; } = true;   // false => صندوق نرفته‌ها
}
