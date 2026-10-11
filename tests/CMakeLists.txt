find_package(doctest 2.4 CONFIG REQUIRED)

# fuse_add_test(<name> SOURCES ... DEPS ...)
function(fuse_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
    add_executable(${name} test_main.cpp ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_DEPS} doctest::doctest fuse::project_options)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

fuse_add_test(core_tests
    SOURCES test_expression.cpp test_parameters.cpp
    DEPS fuse::core)

fuse_add_test(sketch_tests
    SOURCES test_solver.cpp test_profiles.cpp
    DEPS fuse::sketch)

fuse_add_test(model_tests
    SOURCES test_document.cpp
    DEPS fuse::model)

if(FUSE_WITH_OCCT)
    fuse_add_test(occt_tests
        SOURCES test_occt.cpp
        DEPS fuse::model fuse::occt)
    set_tests_properties(occt_tests PROPERTIES
        ENVIRONMENT "FUSE_TEST_TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endif()
