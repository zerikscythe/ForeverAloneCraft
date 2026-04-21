#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
CollectSourceFiles(
        ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE_SOURCES
)

include_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/mocks"
)

add_executable(
        unit_tests
        ${PRIVATE_SOURCES}
)

if(MSVC)
    # Keep MSVC test compiles from contending on the shared program database.
    target_compile_options(unit_tests PRIVATE /FS)
endif()

target_link_libraries(
        unit_tests
        game
        gtest_main
        gmock_main
        game-interface
)

# Add module test sources if any modules registered tests
get_property(MODULE_TEST_SOURCES GLOBAL PROPERTY ACORE_MODULE_TEST_SOURCES)
get_property(MODULE_TEST_INCLUDES GLOBAL PROPERTY ACORE_MODULE_TEST_INCLUDES)

if(MODULE_TEST_SOURCES)
    target_sources(unit_tests PRIVATE ${MODULE_TEST_SOURCES})
    message(STATUS "Added module tests to unit_tests target")
endif()

if(MODULE_TEST_INCLUDES)
    list(REMOVE_DUPLICATES MODULE_TEST_INCLUDES)
    target_include_directories(unit_tests PRIVATE ${MODULE_TEST_INCLUDES})
    message(STATUS "Added module test includes to unit_tests target")
endif()

# Link modules library to tests so module code is available
if(TARGET modules)
    target_link_libraries(unit_tests modules)
endif()

if(WIN32)
    set(UNIT_TEST_RUNTIME_PATHS
            "$<TARGET_FILE_DIR:unit_tests>"
    )

    if(DEFINED Boost_ROOT)
        list(APPEND UNIT_TEST_RUNTIME_PATHS
                "${Boost_ROOT}/bin"
                "${Boost_ROOT}/debug/bin"
        )
    endif()

    if(DEFINED OPENSSL_ROOT_DIR)
        list(APPEND UNIT_TEST_RUNTIME_PATHS
                "${OPENSSL_ROOT_DIR}/bin"
                "${OPENSSL_ROOT_DIR}/debug/bin"
        )
    endif()

    if(DEFINED MYSQL_ROOT_DIR)
        list(APPEND UNIT_TEST_RUNTIME_PATHS
                "${MYSQL_ROOT_DIR}/bin"
                "${MYSQL_ROOT_DIR}/debug/bin"
        )
    endif()

    list(REMOVE_DUPLICATES UNIT_TEST_RUNTIME_PATHS)
    list(JOIN UNIT_TEST_RUNTIME_PATHS ";" UNIT_TEST_RUNTIME_PATH)

    add_test(
            NAME
            unit
            COMMAND
            ${CMAKE_COMMAND} -E env "PATH=${UNIT_TEST_RUNTIME_PATH};$ENV{PATH}" $<TARGET_FILE:unit_tests>
    )
else()
    add_test(
            NAME
            unit
            COMMAND
            $<TARGET_FILE:unit_tests>
    )
endif()
