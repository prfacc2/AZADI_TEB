// Minimal self-contained tests for the authoritative billing math.
#include "azaditeb/BillingService.hpp"
#include "azaditeb/MemoryRepository.hpp"
#include <cassert>
#include <iostream>
#include <memory>

using namespace azaditeb;

static int failures = 0;
#define CHECK(cond) do { if(!(cond)){ std::cerr << "FAIL: " #cond " @ line " << __LINE__ << "\n"; ++failures; } } while(0)

int main() {
    auto ref = std::make_shared<MemoryReferenceRepository>();
    BillingService billing(ref);

    // 1) No insurance: patient pays the full multiplied tariff.
    {
        BillRequest r;
        r.hasInsurance = false;
        r.patientKindCode = "normal";  // x1.0
        r.visitKindCode = "normal";    // x1.0
        r.insuranceCode = "none";
        r.services.push_back({"ویزیت","","",1'000'000,0,0});
        auto b = billing.compute(r);
        CHECK(b.total == 1'000'000);
        CHECK(b.insuranceShare == 0);
        CHECK(b.patientPayable == 1'000'000);
    }

    // 2) Tamin (70%) on a normal visit.
    {
        BillRequest r;
        r.hasInsurance = true;
        r.patientKindCode = "normal";
        r.visitKindCode = "normal";
        r.insuranceCode = "tamin";     // 70%
        r.services.push_back({"ویزیت","","",1'000'000,0,0});
        auto b = billing.compute(r);
        CHECK(b.total == 1'000'000);
        CHECK(b.insuranceShare == 700'000);
        CHECK(b.patientPayable == 300'000);
    }

    // 3) VIP multiplier (x1.5) + inpatient (x1.6) with armed forces (80%).
    {
        BillRequest r;
        r.hasInsurance = true;
        r.patientKindCode = "inpatient"; // x1.6
        r.visitKindCode = "vip";         // x1.5
        r.insuranceCode = "armed";       // 80%
        r.services.push_back({"جراحی","","",1'000'000,0,0});
        auto b = billing.compute(r);
        // base = 1,000,000 * 1.6 * 1.5 = 2,400,000
        CHECK(b.total == 2'400'000);
        CHECK(b.insuranceShare == 1'920'000);   // 80%
        CHECK(b.patientPayable == 480'000);
    }

    // 4) Supplementary insurance covers part of the remaining share.
    {
        BillRequest r;
        r.hasInsurance = true;
        r.patientKindCode = "normal";
        r.visitKindCode = "normal";
        r.insuranceCode = "tamin";           // 70% -> patient 300,000
        r.supplementaryPercent = 50;         // covers 50% of remaining 300,000
        r.services.push_back({"ویزیت","","",1'000'000,0,0});
        auto b = billing.compute(r);
        CHECK(b.insuranceShare == 700'000);
        CHECK(b.supplementaryShare == 150'000);
        CHECK(b.patientPayable == 150'000);
    }

    if (failures == 0) { std::cout << "All billing tests passed.\n"; return 0; }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
