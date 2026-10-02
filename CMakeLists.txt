cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()
project(encoder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_CLIENT "Build client" ON)
option(BUILD_SERVER "Build server" ON)
option(BUILD_GUI "Build Qt GUI client" ON)
option(BUILD_ADMIN "Build admin console" ON)
option(BUILD_UI_TESTS "Build isolated GUI regression checks" OFF)
if(BUILD_UI_TESTS)
  enable_testing()
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(common)

if(BUILD_SERVER)
  add_subdirectory(server)
endif()

if(BUILD_CLIENT)
  add_subdirectory(client)
endif()

if(BUILD_ADMIN)
  add_subdirectory(admin)
endif()

if(BUILD_UI_TESTS AND BUILD_CLIENT AND BUILD_ADMIN)
  add_executable(encoder-tls-clients-tests
    client/tests/tls_clients_checks.cpp client/src/client_core.cpp admin/src/admin_client.cpp)
  target_include_directories(encoder-tls-clients-tests PRIVATE client/src admin/src)
  target_link_libraries(encoder-tls-clients-tests PRIVATE encoder_common)
  if(BUILD_SERVER)
    find_package(Python3 COMPONENTS Interpreter)
    find_program(ENCODER_TEST_OPENSSL openssl HINTS "${OPENSSL_ROOT_DIR}/bin")
    if(Python3_FOUND AND ENCODER_TEST_OPENSSL)
      add_test(NAME tls-client-admin-roundtrip COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/client/tests/tls_clients_check.py"
        --server $<TARGET_FILE:encoder-server> --probe $<TARGET_FILE:encoder-tls-clients-tests>
        --openssl "${ENCODER_TEST_OPENSSL}")
      set_tests_properties(tls-client-admin-roundtrip PROPERTIES TIMEOUT 45)
      add_test(NAME tls-client-admin-ip-roundtrip COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/client/tests/tls_clients_check.py"
        --server $<TARGET_FILE:encoder-server> --probe $<TARGET_FILE:encoder-tls-clients-tests>
        --openssl "${ENCODER_TEST_OPENSSL}" --ip-san)
      set_tests_properties(tls-client-admin-ip-roundtrip PROPERTIES TIMEOUT 45)
    endif()
  endif()
endif()

if(BUILD_CLIENT)
  include(${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake)
endif()
