add_executable(fusecad-cli main.cpp)
target_link_libraries(fusecad-cli PRIVATE fuse::model fuse::occt fuse::project_options)

if(FUSE_BUILD_TESTS)
    add_test(NAME cli_smoke
        COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:fusecad-cli>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.cmake)
endif()
