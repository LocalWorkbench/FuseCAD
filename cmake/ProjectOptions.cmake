option(OC_WITH_OCCT "Build the OpenCASCADE geometry kernel backend" ON)
option(OC_BUILD_APP "Build the Qt 6 desktop application" ON)
option(OC_BUILD_TESTS "Build unit and integration tests" ON)
option(OC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(OC_SANITIZERS "" CACHE STRING "Semicolon-separated sanitizers, e.g. address;undefined")

# Interface target carrying the project-wide warning and sanitizer flags.
# Every first-party target links it; third-party code does not.
add_library(oc_project_options INTERFACE)
add_library(oc::project_options ALIAS oc_project_options)

if(MSVC)
    target_compile_options(oc_project_options INTERFACE /W4 /permissive- /utf-8)
    if(OC_WARNINGS_AS_ERRORS)
        target_compile_options(oc_project_options INTERFACE /WX)
    endif()
else()
    target_compile_options(oc_project_options INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
        -Wimplicit-fallthrough -Wmissing-declarations
        # Aggregates such as sketch::Constraint rely on default member
        # initialisers; partial brace-initialisation is intentional.
        -Wno-missing-field-initializers)
    if(OC_WARNINGS_AS_ERRORS)
        target_compile_options(oc_project_options INTERFACE -Werror)
    endif()
endif()

if(OC_SANITIZERS)
    list(JOIN OC_SANITIZERS "," _oc_san)
    target_compile_options(oc_project_options INTERFACE -fsanitize=${_oc_san} -fno-omit-frame-pointer)
    target_link_options(oc_project_options INTERFACE -fsanitize=${_oc_san})
endif()

# oc_add_library(<name> SOURCES ... DEPS ...)
# Creates oc_<name> with alias oc::<name>, rooted at src/ for includes.
function(oc_add_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    add_library(oc_${name} ${ARG_SOURCES})
    add_library(oc::${name} ALIAS oc_${name})
    target_include_directories(oc_${name} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>)
    target_link_libraries(oc_${name}
        PUBLIC ${ARG_PUBLIC_DEPS}
        PRIVATE oc::project_options ${ARG_PRIVATE_DEPS})
    target_compile_features(oc_${name} PUBLIC cxx_std_20)
endfunction()
