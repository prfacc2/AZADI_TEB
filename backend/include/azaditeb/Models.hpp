// ============================================================================
//  Models.hpp — Azadi-Teb Core domain models (C++20)
//  Pure data structures shared by the business logic and the REST API layer.
//  The backend is UI-agnostic: these serialize to/from JSON only.
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace azaditeb {

struct Patient {
    std::string firstName;
    std::string lastName;
    std::string nationalId;
    std::string fatherName;
    std::string birthDate;   // Jalali YYYY/MM/DD
    std::string gender;
    std::string mobile;
    std::string phone;
    std::string address;
};

struct ServiceItem {
    std::string name;
    std::string doctor;
    std::string performer;
    std::int64_t amount            = 0;   // Rial (base tariff before multipliers)
    std::int64_t insuranceShare    = 0;
    std::int64_t insuranceDiscount = 0;
};

struct InsurancePlan {
    std::string code;
    std::string name;
    double      coverPercent = 0.0;   // 0..100
};

struct NamedMultiplier {
    std::string code;
    std::string name;
    double      multiplier = 1.0;
};

struct ReferenceData {
    std::vector<InsurancePlan>   insurances;
    std::vector<NamedMultiplier> patientKinds;
    std::vector<NamedMultiplier> visitKinds;
    std::vector<std::string>     genders;
    std::vector<std::string>     shifts;
};

struct BillRequest {
    std::string  patientKindCode;
    std::string  visitKindCode;
    std::string  insuranceCode;
    std::string  supplementaryCode;
    double       supplementaryPercent = 0.0;
    bool         hasInsurance         = true;
    std::vector<ServiceItem> services;
};

struct BillResult {
    std::int64_t total              = 0;
    std::int64_t insuranceShare     = 0;
    std::int64_t supplementaryShare = 0;
    std::int64_t discount           = 0;
    std::int64_t patientPayable     = 0;
    std::vector<ServiceItem> lines;
};

struct AdmissionRequest {
    Patient     patient;
    BillRequest bill;
    std::string shift;
    bool        pay = true;
};

} // namespace azaditeb
