option(FUSE_WITH_OCCT "Build the OpenCASCADE geometry kernel backend" ON)
option(FUSE_BUILD_TESTS "Build unit and integration tests" ON)
option(FUSE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(FUSE_SANITIZERS "" CACHE STRING "Semicolon-separated sanitizers, e.g. address;undefined")

# Interface target carrying the project-wide warning and sanitizer flags.
# Every first-party target links it; third-party code does not.
add_library(fuse_project_options INTERFACE)
add_library(fuse::project_options ALIAS fuse_project_options)

if(MSVC)
    target_compile_options(fuse_project_options INTERFACE /W4 /permissive- /utf-8)
    if(FUSE_WARNINGS_AS_ERRORS)
        target_compile_options(fuse_project_options INTERFACE /WX)
    endif()
else()
    target_compile_options(fuse_project_options INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
        -Wimplicit-fallthrough -Wmissing-declarations
        # Aggregates such as sketch::Constraint rely on default member
        # initialisers; partial brace-initialisation is intentional.
        -Wno-missing-field-initializers)
    if(FUSE_WARNINGS_AS_ERRORS)
        target_compile_options(fuse_project_options INTERFACE -Werror)
    endif()
endif()

if(FUSE_SANITIZERS)
    list(JOIN FUSE_SANITIZERS "," _fuse_san)
    target_compile_options(fuse_project_options INTERFACE -fsanitize=${_fuse_san} -fno-omit-frame-pointer)
    target_link_options(fuse_project_options INTERFACE -fsanitize=${_fuse_san})
endif()

# fuse_add_library(<name> SOURCES ... DEPS ...)
# Creates fuse_<name> with alias fuse::<name>, rooted at src/ for includes.
function(fuse_add_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    add_library(fuse_${name} ${ARG_SOURCES})
    add_library(fuse::${name} ALIAS fuse_${name})
    target_include_directories(fuse_${name} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(fuse_${name}
        PUBLIC ${ARG_PUBLIC_DEPS}
        PRIVATE fuse::project_options ${ARG_PRIVATE_DEPS})
    target_compile_features(fuse_${name} PUBLIC cxx_std_20)
endfunction()
