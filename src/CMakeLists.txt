# Layering (each library may depend only on those above it):
#
#   core    math, units, expressions, parameters      (no dependencies)
#   sketch  2D geometry, constraint solver, profiles  (core)
#   kernel  abstract geometry-kernel interface        (sketch)
#   model   features, timeline, document, file I/O    (kernel, nlohmann_json private)
#   occt    OpenCASCADE implementation of Kernel      (kernel, OCCT private)
#   command user actions as typed commands, sessions  (model, nlohmann_json public: JSON is its interface)

age_add_library(core
    SOURCES
        age/core/Expression.cpp
        age/core/Parameters.cpp
        age/core/Units.cpp)

age_add_library(sketch
    SOURCES
        age/sketch/Profile.cpp
        age/sketch/Sketch.cpp
        age/sketch/Solver.cpp
    PUBLIC_DEPS age::core)

add_library(age_kernel INTERFACE)
add_library(age::kernel ALIAS age_kernel)
target_link_libraries(age_kernel INTERFACE age::sketch)

age_add_library(model
    SOURCES
        age/model/Builders.cpp
        age/model/Document.cpp
        age/model/Features.cpp
        age/model/Samples.cpp
        age/model/Serialization.cpp
    PUBLIC_DEPS age::kernel
    PRIVATE_DEPS nlohmann_json::nlohmann_json)

age_add_library(command
    SOURCES
        age/command/Builtins.cpp
        age/command/DocumentCommands.cpp
        age/command/FeatureCommands.cpp
        age/command/ParameterCommands.cpp
        age/command/Registry.cpp
        age/command/Session.cpp
        age/command/SketchCommands.cpp
    PUBLIC_DEPS age::model nlohmann_json::nlohmann_json)

if(AGE_WITH_OCCT)
    # OCCT's imported targets carry their include directory as a system path,
    # so its headers are exempt from our warning flags.
    age_add_library(occt
        SOURCES age/occt/OcctKernel.cpp
        PUBLIC_DEPS age::kernel
        PRIVATE_DEPS
            TKernel TKMath TKG3d TKGeomBase TKBRep TKTopAlgo TKPrim TKBO TKFillet TKMesh TKShHealing
            TKXSBase TKDESTEP TKDESTL)
endif()
