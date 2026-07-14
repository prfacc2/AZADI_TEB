// ============================================================================
//  main.cpp — Azadi-Teb C++ Core REST server (cpp-httplib)
//  Exposes the reception/billing business logic over REST so the Avalonia UI
//  stays 100% presentation-only. Backend has ZERO knowledge of the UI.
//
//    Avalonia  ──POST /api/patient/search──▶  C++ Backend  ──▶  SQL Server
//
//  Endpoints:
//    GET  /api/ping                → { "status": "ok", "version": "2.0.0" }
//    GET  /api/reference           → ReferenceData
//    POST /api/patient/search      → Patient | 404
//    POST /api/bill/compute        → BillResult
//    POST /api/admission           → { "ticketNo": N }
// ============================================================================
#include "../third_party/httplib.h"
#include "azaditeb/Json.hpp"
#include "azaditeb/BillingService.hpp"
#include "azaditeb/MemoryRepository.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

using namespace azaditeb;
using nlohmann::json;

int main(int argc, char** argv) {
    int port = 8787;
    std::string host = "127.0.0.1";
    if (argc > 1) port = std::atoi(argv[1]);
    if (const char* p = std::getenv("AZADITEB_PORT")) port = std::atoi(p);

    // --- Composition root (Dependency Injection) ---
    auto refRepo     = std::make_shared<MemoryReferenceRepository>();
    auto patientRepo = std::make_shared<MemoryPatientRepository>();
    auto billing     = std::make_shared<BillingService>(refRepo);

    httplib::Server srv;

    // CORS + JSON defaults so the desktop client (and future web client) can call.
    srv.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
    });
    srv.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    auto sendJson = [](httplib::Response& res, const json& j, int status = 200) {
        res.status = status;
        res.set_content(j.dump(), "application/json; charset=utf-8");
    };

    srv.Get("/api/ping", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json{{"status", "ok"}, {"version", "2.0.0"}});
    });

    srv.Get("/api/reference", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json(refRepo->load()));
    });

    srv.Post("/api/patient/search", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body, nullptr, false);
            std::string nid = body.is_discarded() ? "" : js(body, "nationalId");
            auto p = patientRepo->findByNationalId(nid);
            if (p) sendJson(res, json(*p));
            else   sendJson(res, json{{"error", "not_found"}}, 404);
        } catch (const std::exception& e) {
            sendJson(res, json{{"error", e.what()}}, 400);
        }
    });

    srv.Post("/api/bill/compute", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            BillResult r = billing->compute(parseBillRequest(body));
            sendJson(res, json(r));
        } catch (const std::exception& e) {
            sendJson(res, json{{"error", e.what()}}, 400);
        }
    });

    srv.Post("/api/admission", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int ticket = patientRepo->saveAdmission(parseAdmission(body));
            sendJson(res, json{{"ticketNo", ticket}});
        } catch (const std::exception& e) {
            sendJson(res, json{{"error", e.what()}}, 400);
        }
    });

    std::cout << "AzadiTeb Core REST engine listening on http://"
              << host << ":" << port << "\n";
    if (!srv.listen(host.c_str(), port)) {
        std::cerr << "FATAL: could not bind " << host << ":" << port << "\n";
        return 1;
    }
    return 0;
}
