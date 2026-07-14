// ============================================================================
//  IRepository.hpp — Repository pattern abstraction (persistence port)
//  Production: SQL Server.  Tests/offline: SQLite / in-memory.
//  The service layer depends only on this interface (Dependency Inversion).
// ============================================================================
#pragma once
#include "Models.hpp"
#include <optional>
#include <string>

namespace azaditeb {

class IPatientRepository {
public:
    virtual ~IPatientRepository() = default;

    // Online national-ID lookup (ثبت احوال / سامانه بیمه). nullopt => not found.
    virtual std::optional<Patient> findByNationalId(const std::string& nid) = 0;

    // Persist an admission and return the assigned ticket number.
    virtual int saveAdmission(const AdmissionRequest& req) = 0;
};

class IReferenceRepository {
public:
    virtual ~IReferenceRepository() = default;
    virtual ReferenceData load() = 0;
};

} // namespace azaditeb
