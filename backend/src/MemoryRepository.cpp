// ============================================================================
//  MemoryRepository.cpp — offline / test implementation of the repositories.
//  Production would swap this for a SQL Server-backed repository behind the
//  same IRepository interfaces (no service/UI change needed).
// ============================================================================
#include "azaditeb/MemoryRepository.hpp"
#include <atomic>

namespace azaditeb {

ReferenceData MemoryReferenceRepository::load() {
    ReferenceData d;
    d.genders = {"مرد", "زن"};
    d.shifts  = {"صبح (۶ تا ۱۴:۳۰)", "عصر (۱۴:۳۰ تا ۲۲:۳۰)", "شب (۲۲:۳۰ تا ۶)"};
    d.patientKinds = {
        {"normal",     "عادی",   1.00},
        {"outpatient", "سرپایی", 1.15},
        {"inpatient",  "بستری",  1.60},
    };
    d.visitKinds = {
        {"normal",   "عادی",      1.00},
        {"vip",      "VIP",       1.50},
        {"discount", "تخفیف‌دار", 0.80},
    };
    d.insurances = {
        {"none",    "بدون بیمه",             0},
        {"tamin",   "تأمین اجتماعی",         70},
        {"salamat", "بیمه سلامت",            70},
        {"armed",   "نیروهای مسلح",          80},
        {"bank",    "بانک‌ها (آتیه‌سازان)",  75},
        {"oil",     "صنعت نفت",              85},
        {"dana",    "بیمه دانا",             65},
        {"iran",    "بیمه ایران",            65},
        {"asia",    "بیمه آسیا",             65},
        {"razi",    "بیمه رازی",             60},
    };
    return d;
}

std::optional<Patient> MemoryPatientRepository::findByNationalId(const std::string&) {
    // Offline: no civil-registry connection available.
    return std::nullopt;
}

int MemoryPatientRepository::saveAdmission(const AdmissionRequest&) {
    static std::atomic<int> ticket{100};
    return ++ticket;
}

} // namespace azaditeb
