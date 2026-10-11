qt_add_executable(fusecad
    main.cpp
    BrowserPanel.cpp BrowserPanel.h
    CanvasOverlays.cpp CanvasOverlays.h
    Commands.cpp Commands.h
    DocumentController.cpp DocumentController.h
    FeatureDialogs.cpp FeatureDialogs.h
    Icons.cpp Icons.h
    MainWindow.cpp MainWindow.h
    ParametersPanel.cpp ParametersPanel.h
    Ribbon.cpp Ribbon.h
    SceneBuilder.cpp SceneBuilder.h
    Theme.cpp Theme.h
    TimelineBar.cpp TimelineBar.h
    ViewCube.cpp ViewCube.h
    ViewState.h
    Viewport.cpp Viewport.h
    Workbench.cpp Workbench.h
    resources/fusecad.icns)

# Dock and Finder icon (macOS bundle); the window icon below covers other platforms.
set_source_files_properties(resources/fusecad.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
qt_add_resources(fusecad "branding" PREFIX "/" BASE resources FILES resources/fusecad.png)

target_link_libraries(fusecad PRIVATE
    fuse::model fuse::occt fuse::project_options
    Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

target_compile_definitions(fusecad PRIVATE FUSE_VERSION_STRING="${PROJECT_VERSION}")

set_target_properties(fusecad PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "FuseCAD"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.fusecad.FuseCAD"
    MACOSX_BUNDLE_ICON_FILE fusecad.icns
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE ON)
