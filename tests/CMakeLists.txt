find_package(doctest 2.4 CONFIG REQUIRED)

# age_add_test(<name> SOURCES ... DEPS ...)
function(age_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
    add_executable(${name} test_main.cpp ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_DEPS} doctest::doctest age::project_options)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

age_add_test(core_tests
    SOURCES test_expression.cpp test_parameters.cpp
    DEPS age::core)

age_add_test(sketch_tests
    SOURCES test_solver.cpp test_profiles.cpp
    DEPS age::sketch)

age_add_test(model_tests
    SOURCES test_document.cpp
    DEPS age::model)

age_add_test(command_tests
    SOURCES test_commands.cpp
    DEPS age::command)
set_tests_properties(command_tests PROPERTIES
    ENVIRONMENT "AGE_TEST_TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")

if(AGE_WITH_OCCT)
    age_add_test(occt_tests
        SOURCES test_occt.cpp
        DEPS age::command age::occt)
    set_tests_properties(occt_tests PROPERTIES
        ENVIRONMENT "AGE_TEST_TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endif()
