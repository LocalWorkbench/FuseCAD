add_executable(oc-cli main.cpp)
target_link_libraries(oc-cli PRIVATE oc::model oc::occt oc::project_options)
install(TARGETS oc-cli RUNTIME DESTINATION bin)

if(OC_BUILD_TESTS)
    add_test(NAME cli_smoke
        COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:oc-cli>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.cmake)
endif()
