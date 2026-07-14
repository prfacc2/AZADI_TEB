// ============================================================================
//  BillingService.hpp — authoritative tariff + insurance math (business logic)
//  This is the single source of billing truth; the Avalonia UI never fabricates
//  a bill — it POSTs /api/bill/compute and renders whatever this returns.
// ============================================================================
#pragma once
#include "Models.hpp"
#include "IRepository.hpp"
#include <memory>

namespace azaditeb {

class BillingService {
public:
    explicit BillingService(std::shared_ptr<IReferenceRepository> refRepo);

    // Compute the full bill for an admission request.
    BillResult compute(const BillRequest& req) const;

private:
    std::shared_ptr<IReferenceRepository> ref_;

    double patientMultiplier(const std::string& code) const;
    double visitMultiplier(const std::string& code) const;
    double coverPercent(const std::string& code, bool hasInsurance) const;
};

} // namespace azaditeb
