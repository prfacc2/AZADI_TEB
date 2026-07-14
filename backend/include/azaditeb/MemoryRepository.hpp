#pragma once
#include "IRepository.hpp"

namespace azaditeb {

class MemoryReferenceRepository : public IReferenceRepository {
public:
    ReferenceData load() override;
};

class MemoryPatientRepository : public IPatientRepository {
public:
    std::optional<Patient> findByNationalId(const std::string& nid) override;
    int saveAdmission(const AdmissionRequest& req) override;
};

} // namespace azaditeb
