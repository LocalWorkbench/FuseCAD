# Layering (each library may depend only on those above it):
#
#   core    math, units, expressions, parameters      (no dependencies)
#   sketch  2D geometry, constraint solver, profiles  (core)
#   kernel  abstract geometry-kernel interface        (sketch)
#   model   features, timeline, document, file I/O    (kernel, nlohmann_json private)
#   occt    OpenCASCADE implementation of Kernel      (kernel, OCCT private)

oc_add_library(core
    SOURCES
        oc/core/Expression.cpp
        oc/core/Parameters.cpp
        oc/core/Units.cpp)

oc_add_library(sketch
    SOURCES
        oc/sketch/Profile.cpp
        oc/sketch/Sketch.cpp
        oc/sketch/Solver.cpp
    PUBLIC_DEPS oc::core)

add_library(oc_kernel INTERFACE)
add_library(oc::kernel ALIAS oc_kernel)
target_link_libraries(oc_kernel INTERFACE oc::sketch)

oc_add_library(model
    SOURCES
        oc/model/Builders.cpp
        oc/model/Document.cpp
        oc/model/Features.cpp
        oc/model/Samples.cpp
        oc/model/Serialization.cpp
    PUBLIC_DEPS oc::kernel
    PRIVATE_DEPS nlohmann_json::nlohmann_json)

if(OC_WITH_OCCT)
    oc_add_library(occt
        SOURCES oc/occt/OcctKernel.cpp
        PUBLIC_DEPS oc::kernel
        PRIVATE_DEPS
            TKernel TKMath TKG2d TKG3d TKGeomBase TKGeomAlgo TKBRep TKTopAlgo
            TKPrim TKBO TKBool TKFillet TKMesh TKShHealing
            TKXSBase TKDE TKDESTEP TKDESTL)
    # OCCT headers trip our strict warnings; treat them as system headers.
    target_include_directories(oc_occt SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})
endif()
