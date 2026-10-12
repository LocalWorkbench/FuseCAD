option(AGE_WITH_OCCT "Build the OpenCASCADE geometry kernel backend" ON)
option(AGE_BUILD_TESTS "Build unit and integration tests" ON)
option(AGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(AGE_SANITIZERS "" CACHE STRING "Semicolon-separated sanitizers, e.g. address;undefined")

# Interface target carrying the project-wide warning and sanitizer flags.
# Every first-party target links it; third-party code does not.
add_library(age_project_options INTERFACE)
add_library(age::project_options ALIAS age_project_options)

if(MSVC)
    target_compile_options(age_project_options INTERFACE /W4 /permissive- /utf-8)
    if(AGE_WARNINGS_AS_ERRORS)
        target_compile_options(age_project_options INTERFACE /WX)
    endif()
else()
    target_compile_options(age_project_options INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
        -Wimplicit-fallthrough -Wmissing-declarations
        # Aggregates such as sketch::Constraint rely on default member
        # initialisers; partial brace-initialisation is intentional.
        -Wno-missing-field-initializers)
    if(AGE_WARNINGS_AS_ERRORS)
        target_compile_options(age_project_options INTERFACE -Werror)
    endif()
endif()

if(AGE_SANITIZERS)
    list(JOIN AGE_SANITIZERS "," _age_san)
    target_compile_options(age_project_options INTERFACE -fsanitize=${_age_san} -fno-omit-frame-pointer)
    target_link_options(age_project_options INTERFACE -fsanitize=${_age_san})
endif()

# age_add_library(<name> SOURCES ... DEPS ...)
# Creates age_<name> with alias age::<name>, rooted at src/ for includes.
function(age_add_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    add_library(age_${name} ${ARG_SOURCES})
    add_library(age::${name} ALIAS age_${name})
    target_include_directories(age_${name} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(age_${name}
        PUBLIC ${ARG_PUBLIC_DEPS}
        PRIVATE age::project_options ${ARG_PRIVATE_DEPS})
    target_compile_features(age_${name} PUBLIC cxx_std_20)
endfunction()
