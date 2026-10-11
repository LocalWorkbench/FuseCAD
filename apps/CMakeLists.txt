if(FUSE_WITH_OCCT)
    add_subdirectory(fusecad-cli)
endif()

if(FUSE_BUILD_APP)
    # Found here rather than at the top level so that qt_standard_project_setup()'s
    # AUTOMOC/AUTOUIC defaults apply to the desktop app only.
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
    qt_standard_project_setup()
    add_subdirectory(fusecad)
endif()
