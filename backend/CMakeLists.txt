# ============================================================================
#  Azadi-Teb Core — C++20 REST backend engine
#  Build: cmake -B build && cmake --build build
#  Output: AzadiTebCore(.exe) — the business-logic REST server.
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(AzadiTebCore VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- Core business-logic library (UI-agnostic, testable) ---
add_library(azaditeb_core STATIC
    src/BillingService.cpp
    src/MemoryRepository.cpp
)
target_include_directories(azaditeb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# --- REST server executable ---
add_executable(AzadiTebCore src/main.cpp)
target_link_libraries(AzadiTebCore PRIVATE azaditeb_core)

if(WIN32)
    target_link_libraries(AzadiTebCore PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(AzadiTebCore PRIVATE Threads::Threads)
endif()

# --- Unit test (billing math) ---
enable_testing()
add_executable(billing_tests tests/billing_tests.cpp)
target_link_libraries(billing_tests PRIVATE azaditeb_core)
add_test(NAME billing_tests COMMAND billing_tests)
