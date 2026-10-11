qt_add_executable(opencircuit
    main.cpp
    BrowserPanel.cpp BrowserPanel.h
    CanvasOverlays.cpp CanvasOverlays.h
    Commands.cpp Commands.h
    FeatureDialogs.cpp FeatureDialogs.h
    Icons.cpp Icons.h
    MainWindow.cpp MainWindow.h
    ParametersPanel.cpp ParametersPanel.h
    Ribbon.cpp Ribbon.h
    Theme.cpp Theme.h
    TimelineBar.cpp TimelineBar.h
    ViewCube.cpp ViewCube.h
    ViewState.h
    Viewport.cpp Viewport.h)

target_link_libraries(opencircuit PRIVATE
    oc::model oc::occt oc::project_options
    Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

target_compile_definitions(opencircuit PRIVATE OC_VERSION_STRING="${PROJECT_VERSION}")

set_target_properties(opencircuit PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "OpenCircuit"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.opencircuit.OpenCircuit"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE ON)

install(TARGETS opencircuit BUNDLE DESTINATION . RUNTIME DESTINATION bin)
