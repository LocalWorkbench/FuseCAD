find_package(doctest 2.4 QUIET CONFIG)
if(NOT doctest_FOUND)
    include(FetchContent)
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(doctest)
endif()

# oc_add_test(<name> SOURCES ... DEPS ...)
function(oc_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
    add_executable(${name} test_main.cpp ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_DEPS} doctest::doctest oc::project_options)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

oc_add_test(core_tests
    SOURCES test_expression.cpp test_parameters.cpp
    DEPS oc::core)

oc_add_test(sketch_tests
    SOURCES test_solver.cpp test_profiles.cpp
    DEPS oc::sketch)

oc_add_test(model_tests
    SOURCES test_document.cpp
    DEPS oc::model)

if(OC_WITH_OCCT)
    oc_add_test(occt_tests
        SOURCES test_occt.cpp
        DEPS oc::model oc::occt)
    set_tests_properties(occt_tests PROPERTIES
        ENVIRONMENT "OC_TEST_TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endif()
