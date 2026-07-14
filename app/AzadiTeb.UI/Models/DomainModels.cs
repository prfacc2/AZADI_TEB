using System;
using System.Collections.Generic;

namespace AzadiTeb.UI.Models;

/// <summary>Patient identity + demographics captured on the reception form.</summary>
public sealed class Patient
{
    public string FirstName { get; set; } = "";
    public string LastName { get; set; } = "";
    public string NationalId { get; set; } = "";
    public string FatherName { get; set; } = "";
    public string BirthDate { get; set; } = "";   // Jalali YYYY/MM/DD
    public string Gender { get; set; } = "";       // مرد / زن
    public string Mobile { get; set; } = "";
    public string Phone { get; set; } = "";
    public string Address { get; set; } = "";

    public string FullName => $"{FirstName} {LastName}".Trim();
}

/// <summary>A single clinical service/visit line added to the admission.</summary>
public sealed class ServiceItem
{
    public string Name { get; set; } = "";
    public string Doctor { get; set; } = "";
    public string Performer { get; set; } = "";
    public long Amount { get; set; }              // Rial
    public long InsuranceShare { get; set; }      // Rial
    public long InsuranceDiscount { get; set; }   // Rial

    public long PatientShare => Math.Max(0, Amount - InsuranceShare - InsuranceDiscount);
}

/// <summary>Insurance plan option (real Iranian insurers).</summary>
public sealed class InsurancePlan
{
    public string Code { get; set; } = "";
    public string Name { get; set; } = "";
    public double CoverPercent { get; set; }   // 0..100 base coverage

    public override string ToString() => Name;
}

/// <summary>Patient category — affects base tariff.</summary>
public sealed class PatientKind
{
    public string Code { get; set; } = "";
    public string Name { get; set; } = "";
    public double Multiplier { get; set; } = 1.0;
    public override string ToString() => Name;
}

/// <summary>Appointment / visit kind — VIP, discount, normal.</summary>
public sealed class VisitKind
{
    public string Code { get; set; } = "";
    public string Name { get; set; } = "";
    public double Multiplier { get; set; } = 1.0;
    public override string ToString() => Name;
}

/// <summary>Computed bill returned by the (C++) billing engine.</summary>
public sealed class BillResult
{
    public long Total { get; set; }
    public long InsuranceShare { get; set; }
    public long SupplementaryShare { get; set; }
    public long Discount { get; set; }
    public long PatientPayable { get; set; }
    public List<ServiceItem> Lines { get; set; } = new();
}

/// <summary>An entry in the reception queue (صف پذیرش).</summary>
public sealed class QueueEntry
{
    public int TicketNo { get; set; }
    public string PatientName { get; set; } = "";
    public string NationalId { get; set; } = "";
    public string Kind { get; set; } = "";
    public string Insurance { get; set; } = "";
    public long Payable { get; set; }
    public string Time { get; set; } = "";
    public string Status { get; set; } = "در انتظار";
}
