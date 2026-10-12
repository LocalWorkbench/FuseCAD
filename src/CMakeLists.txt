# Layering (each library may depend only on those above it):
#
#   core    math, units, expressions, parameters      (no dependencies)
#   sketch  2D geometry, constraint solver, profiles  (core)
#   kernel  abstract geometry-kernel interface        (sketch)
#   model   features, timeline, document, file I/O    (kernel, nlohmann_json private)
#   occt    OpenCASCADE implementation of Kernel      (kernel, OCCT private)
#   command user actions as typed commands, sessions  (model, nlohmann_json public: JSON is its interface)

fuse_add_library(core
    SOURCES
        fuse/core/Expression.cpp
        fuse/core/Parameters.cpp
        fuse/core/Units.cpp)

fuse_add_library(sketch
    SOURCES
        fuse/sketch/Profile.cpp
        fuse/sketch/Sketch.cpp
        fuse/sketch/Solver.cpp
    PUBLIC_DEPS fuse::core)

add_library(fuse_kernel INTERFACE)
add_library(fuse::kernel ALIAS fuse_kernel)
target_link_libraries(fuse_kernel INTERFACE fuse::sketch)

fuse_add_library(model
    SOURCES
        fuse/model/Builders.cpp
        fuse/model/Document.cpp
        fuse/model/Features.cpp
        fuse/model/Samples.cpp
        fuse/model/Serialization.cpp
    PUBLIC_DEPS fuse::kernel
    PRIVATE_DEPS nlohmann_json::nlohmann_json)

fuse_add_library(command
    SOURCES
        fuse/command/Builtins.cpp
        fuse/command/DocumentCommands.cpp
        fuse/command/FeatureCommands.cpp
        fuse/command/ParameterCommands.cpp
        fuse/command/Registry.cpp
        fuse/command/Session.cpp
        fuse/command/SketchCommands.cpp
    PUBLIC_DEPS fuse::model nlohmann_json::nlohmann_json)

if(FUSE_WITH_OCCT)
    # OCCT's imported targets carry their include directory as a system path,
    # so its headers are exempt from our warning flags.
    fuse_add_library(occt
        SOURCES fuse/occt/OcctKernel.cpp
        PUBLIC_DEPS fuse::kernel
        PRIVATE_DEPS
            TKernel TKMath TKG3d TKGeomBase TKBRep TKTopAlgo TKPrim TKBO TKFillet TKMesh TKShHealing
            TKXSBase TKDESTEP TKDESTL)
endif()
