add_executable(agecad-cli main.cpp)
target_link_libraries(agecad-cli PRIVATE age::command age::occt age::project_options)

if(AGE_BUILD_TESTS)
    add_test(NAME cli_smoke
        COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:agecad-cli>
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.cmake)
endif()
