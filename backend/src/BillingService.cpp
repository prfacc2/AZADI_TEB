#include "azaditeb/BillingService.hpp"
#include <algorithm>
#include <cmath>

namespace azaditeb {

BillingService::BillingService(std::shared_ptr<IReferenceRepository> refRepo)
    : ref_(std::move(refRepo)) {}

double BillingService::patientMultiplier(const std::string& code) const {
    auto ref = ref_->load();
    for (const auto& k : ref.patientKinds)
        if (k.code == code) return k.multiplier;
    return 1.0;
}

double BillingService::visitMultiplier(const std::string& code) const {
    auto ref = ref_->load();
    for (const auto& v : ref.visitKinds)
        if (v.code == code) return v.multiplier;
    return 1.0;
}

double BillingService::coverPercent(const std::string& code, bool hasInsurance) const {
    if (!hasInsurance) return 0.0;
    auto ref = ref_->load();
    for (const auto& i : ref.insurances)
        if (i.code == code) return i.coverPercent;
    return 0.0;
}

BillResult BillingService::compute(const BillRequest& req) const {
    BillResult r;
    const double pMult = patientMultiplier(req.patientKindCode);
    const double vMult = visitMultiplier(req.visitKindCode);
    const double cover = coverPercent(req.insuranceCode, req.hasInsurance);
    const double supPct = std::clamp(req.supplementaryPercent, 0.0, 100.0);

    for (const auto& s : req.services) {
        const std::int64_t base =
            static_cast<std::int64_t>(std::llround(s.amount * pMult * vMult));
        const std::int64_t insShare = req.hasInsurance
            ? static_cast<std::int64_t>(std::llround(base * cover / 100.0))
            : 0;
        const std::int64_t remaining = base - insShare;
        const std::int64_t supShare = supPct > 0
            ? static_cast<std::int64_t>(std::llround(remaining * supPct / 100.0))
            : 0;
        const std::int64_t discount = s.insuranceDiscount;
        const std::int64_t patient =
            std::max<std::int64_t>(0, base - insShare - supShare - discount);

        r.total              += base;
        r.insuranceShare     += insShare;
        r.supplementaryShare += supShare;
        r.discount           += discount;
        r.patientPayable     += patient;

        ServiceItem line = s;
        line.amount         = base;
        line.insuranceShare = insShare + supShare;
        line.insuranceDiscount = discount;
        r.lines.push_back(line);
    }
    return r;
}

} // namespace azaditeb
