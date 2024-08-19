message(STATUS "Visor Lib Utils")

find_package(pcapplusplus REQUIRED)
find_package(fmt REQUIRED)
find_package(Catch2 REQUIRED)

add_library(VisorLibUtils utils.cpp)

add_library(Visor::Lib::Utils ALIAS VisorLibUtils)

target_include_directories(VisorLibUtils
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        )

target_link_libraries(VisorLibUtils
        PUBLIC
        pcapplusplus::pcapplusplus
        fmt::fmt
        )

## TEST SUITE
add_executable(unit-tests-visor-utils test_utils.cpp)

target_link_libraries(unit-tests-visor-utils
        PRIVATE
        Visor::Lib::Utils
        catch2::catch2_with_main)

add_test(NAME unit-tests-visor-utils
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/libs
        COMMAND unit-tests-visor-utils)