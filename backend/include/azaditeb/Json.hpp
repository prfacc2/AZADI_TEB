// ============================================================================
//  Json.hpp — nlohmann::json (de)serialization for the domain models.
//  Property names are camelCase to match the Avalonia System.Text.Json client.
// ============================================================================
#pragma once
#include "Models.hpp"
#include "../../third_party/json.hpp"

namespace azaditeb {

inline void to_json(nlohmann::json& j, const InsurancePlan& p) {
    j = {{"code", p.code}, {"name", p.name}, {"coverPercent", p.coverPercent}};
}
inline void to_json(nlohmann::json& j, const NamedMultiplier& m) {
    j = {{"code", m.code}, {"name", m.name}, {"multiplier", m.multiplier}};
}
inline void to_json(nlohmann::json& j, const ReferenceData& d) {
    j = {
        {"insurances", d.insurances},
        {"patientKinds", d.patientKinds},
        {"visitKinds", d.visitKinds},
        {"genders", d.genders},
        {"shifts", d.shifts},
    };
}
inline void to_json(nlohmann::json& j, const ServiceItem& s) {
    j = {
        {"name", s.name}, {"doctor", s.doctor}, {"performer", s.performer},
        {"amount", s.amount}, {"insuranceShare", s.insuranceShare},
        {"insuranceDiscount", s.insuranceDiscount},
    };
}
inline void to_json(nlohmann::json& j, const BillResult& r) {
    j = {
        {"total", r.total}, {"insuranceShare", r.insuranceShare},
        {"supplementaryShare", r.supplementaryShare}, {"discount", r.discount},
        {"patientPayable", r.patientPayable}, {"lines", r.lines},
    };
}
inline void to_json(nlohmann::json& j, const Patient& p) {
    j = {
        {"firstName", p.firstName}, {"lastName", p.lastName},
        {"nationalId", p.nationalId}, {"fatherName", p.fatherName},
        {"birthDate", p.birthDate}, {"gender", p.gender},
        {"mobile", p.mobile}, {"phone", p.phone}, {"address", p.address},
    };
}

inline std::string js(const nlohmann::json& j, const char* k) {
    if (j.contains(k) && j[k].is_string()) return j[k].get<std::string>();
    return "";
}
inline std::int64_t ji(const nlohmann::json& j, const char* k) {
    if (j.contains(k) && j[k].is_number()) return j[k].get<std::int64_t>();
    return 0;
}
inline double jd(const nlohmann::json& j, const char* k) {
    if (j.contains(k) && j[k].is_number()) return j[k].get<double>();
    return 0.0;
}
inline bool jb(const nlohmann::json& j, const char* k, bool def) {
    if (j.contains(k) && j[k].is_boolean()) return j[k].get<bool>();
    return def;
}

inline ServiceItem parseService(const nlohmann::json& j) {
    ServiceItem s;
    s.name = js(j, "name"); s.doctor = js(j, "doctor"); s.performer = js(j, "performer");
    s.amount = ji(j, "amount"); s.insuranceShare = ji(j, "insuranceShare");
    s.insuranceDiscount = ji(j, "insuranceDiscount");
    return s;
}

inline BillRequest parseBillRequest(const nlohmann::json& j) {
    BillRequest r;
    r.patientKindCode = js(j, "patientKindCode");
    r.visitKindCode = js(j, "visitKindCode");
    r.insuranceCode = js(j, "insuranceCode");
    r.supplementaryCode = js(j, "supplementaryCode");
    r.supplementaryPercent = jd(j, "supplementaryPercent");
    r.hasInsurance = jb(j, "hasInsurance", true);
    if (j.contains("services") && j["services"].is_array())
        for (const auto& s : j["services"]) r.services.push_back(parseService(s));
    return r;
}

inline Patient parsePatient(const nlohmann::json& j) {
    Patient p;
    p.firstName = js(j, "firstName"); p.lastName = js(j, "lastName");
    p.nationalId = js(j, "nationalId"); p.fatherName = js(j, "fatherName");
    p.birthDate = js(j, "birthDate"); p.gender = js(j, "gender");
    p.mobile = js(j, "mobile"); p.phone = js(j, "phone"); p.address = js(j, "address");
    return p;
}

inline AdmissionRequest parseAdmission(const nlohmann::json& j) {
    AdmissionRequest a;
    if (j.contains("patient")) a.patient = parsePatient(j["patient"]);
    if (j.contains("bill")) a.bill = parseBillRequest(j["bill"]);
    a.shift = js(j, "shift");
    a.pay = jb(j, "pay", true);
    return a;
}

} // namespace azaditeb
