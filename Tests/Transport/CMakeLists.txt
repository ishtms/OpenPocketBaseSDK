cmake_minimum_required(VERSION 3.18)
project(OpenPocketBaseTransportTests LANGUAGES CXX)
find_package(CURL 7.68 REQUIRED)
find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(transport_tests
    transport_tests.cpp
    ../../Source/OpenPocketBaseSDK/Private/Transport/OpenPocketBaseCurlLoop.cpp)
target_include_directories(transport_tests PRIVATE ../../Source/OpenPocketBaseSDK/Private)
target_compile_features(transport_tests PRIVATE cxx_std_17)
target_link_libraries(transport_tests PRIVATE CURL::libcurl Threads::Threads)
enable_testing()
add_test(NAME transport_contract
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/transport_fixture.py" "$<TARGET_FILE:transport_tests>")
