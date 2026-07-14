using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using AzadiTeb.UI.Models;

namespace AzadiTeb.UI.Services;

/// <summary>
/// Deterministic in-process backend that mirrors the C++ business rules.
/// It runs off the UI thread, so the form stays perfectly responsive even
/// when the remote C++ REST engine is offline (air-gapped clinics).
/// </summary>
public sealed class LocalReceptionBackend : IReceptionBackend
{
    private int _ticket = 100;

    public Task<ReferenceData> GetReferenceDataAsync(CancellationToken ct = default)
    {
        var data = new ReferenceData
        {
            Genders = new() { "مرد", "زن" },
            Shifts = new() { "صبح (۶ تا ۱۴:۳۰)", "عصر (۱۴:۳۰ تا ۲۲:۳۰)", "شب (۲۲:۳۰ تا ۶)" },
            PatientKinds = new()
            {
                new() { Code = "normal",     Name = "عادی",   Multiplier = 1.00 },
                new() { Code = "outpatient", Name = "سرپایی", Multiplier = 1.15 },
                new() { Code = "inpatient",  Name = "بستری",  Multiplier = 1.60 },
            },
            VisitKinds = new()
            {
                new() { Code = "normal",   Name = "عادی",       Multiplier = 1.00 },
                new() { Code = "vip",      Name = "VIP",        Multiplier = 1.50 },
                new() { Code = "discount", Name = "تخفیف‌دار",  Multiplier = 0.80 },
            },
            // Real Iranian insurers with representative base coverage.
            Insurances = new()
            {
                new() { Code = "none",   Name = "بدون بیمه",              CoverPercent = 0 },
                new() { Code = "tamin",  Name = "تأمین اجتماعی",          CoverPercent = 70 },
                new() { Code = "salamat",Name = "بیمه سلامت",             CoverPercent = 70 },
                new() { Code = "armed",  Name = "نیروهای مسلح",           CoverPercent = 80 },
                new() { Code = "bank",   Name = "بانک‌ها (آتیه‌سازان)",   CoverPercent = 75 },
                new() { Code = "oil",    Name = "صنعت نفت",               CoverPercent = 85 },
                new() { Code = "dana",   Name = "بیمه دانا",              CoverPercent = 65 },
                new() { Code = "iran",   Name = "بیمه ایران",             CoverPercent = 65 },
                new() { Code = "asia",   Name = "بیمه آسیا",              CoverPercent = 65 },
                new() { Code = "razi",   Name = "بیمه رازی",              CoverPercent = 60 },
            },
        };
        return Task.FromResult(data);
    }

    public Task<Patient?> LookupCitizenAsync(string nationalId, CancellationToken ct = default)
    {
        // Offline: we cannot really query ثبت احوال; return null so the UI shows
        // the "please enter manually" message — exactly like the native form.
        return Task.FromResult<Patient?>(null);
    }

    public Task<bool> PingAsync(CancellationToken ct = default) => Task.FromResult(false);

    public Task<BillResult> ComputeBillAsync(BillRequest req, CancellationToken ct = default)
        => Task.FromResult(Compute(req));

    public Task<int> SubmitAdmissionAsync(AdmissionRequest req, CancellationToken ct = default)
    {
        var no = Interlocked.Increment(ref _ticket);
        return Task.FromResult(no);
    }

    /// <summary>Pure tariff+insurance math — the single source of billing truth.</summary>
    public static BillResult Compute(BillRequest req)
    {
        var result = new BillResult();
        var refData = new LocalReceptionBackend();
        var kinds = refData.GetReferenceDataAsync().Result;

        double patientMult = kinds.PatientKinds
            .FirstOrDefault(k => k.Code == req.PatientKindCode)?.Multiplier ?? 1.0;
        double visitMult = kinds.VisitKinds
            .FirstOrDefault(v => v.Code == req.VisitKindCode)?.Multiplier ?? 1.0;
        double cover = req.HasInsurance
            ? kinds.Insurances.FirstOrDefault(i => i.Code == req.InsuranceCode)?.CoverPercent ?? 0
            : 0;

        foreach (var s in req.Services)
        {
            long baseAmount = (long)Math.Round(s.Amount * patientMult * visitMult);
            long insShare = req.HasInsurance
                ? (long)Math.Round(baseAmount * cover / 100.0)
                : 0;

            // Supplementary insurance covers a percent of the remaining patient share.
            long remaining = baseAmount - insShare;
            long supShare = req.SupplementaryPercent > 0
                ? (long)Math.Round(remaining * Math.Clamp(req.SupplementaryPercent, 0, 100) / 100.0)
                : 0;

            long discount = s.InsuranceDiscount;
            long line = new ServiceItem
            {
                Name = s.Name, Doctor = s.Doctor, Performer = s.Performer,
                Amount = baseAmount, InsuranceShare = insShare + supShare,
                InsuranceDiscount = discount,
            }.PatientShare;

            result.Total += baseAmount;
            result.InsuranceShare += insShare;
            result.SupplementaryShare += supShare;
            result.Discount += discount;
            result.PatientPayable += line;
            result.Lines.Add(new ServiceItem
            {
                Name = s.Name, Doctor = s.Doctor, Performer = s.Performer,
                Amount = baseAmount, InsuranceShare = insShare + supShare,
                InsuranceDiscount = discount,
            });
        }
        return result;
    }
}
