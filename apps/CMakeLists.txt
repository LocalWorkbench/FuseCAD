if(OC_WITH_OCCT)
    add_subdirectory(oc-cli)
endif()

if(OC_BUILD_APP)
    add_subdirectory(opencircuit)
endif()
