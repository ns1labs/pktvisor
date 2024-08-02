message(STATUS "Input Module: Net Probe")

find_package(uvw REQUIRED)

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

corrade_add_static_plugin(VisorInputNetProbe
        ${CMAKE_CURRENT_BINARY_DIR}
        NetProbeInput.conf
        NetProbeInputModulePlugin.cpp
        NetProbeInputStream.cpp
        PingProbe.cpp
        TcpProbe.cpp
        )
add_library(Visor::Input::NetProbe ALIAS VisorInputNetProbe)

target_include_directories(VisorInputNetProbe
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        )

target_link_libraries(VisorInputNetProbe
        PUBLIC
        Visor::Core
        Visor::Lib::Tcp
        uvw::uvw
        ${CONAN_LIBS_LIBUV}
        ${CONAN_LIBS_UVW}
        ${CONAN_LIBS_PCAPPLUSPLUS}
        ${CONAN_LIBS_PTHREADS4W}
        ${CONAN_LIBS_LIBPCAP}
        ${CONAN_LIBS_NPCAP}
        )

set(VISOR_STATIC_PLUGINS ${VISOR_STATIC_PLUGINS} Visor::Input::NetProbe PARENT_SCOPE)

## TEST SUITE
add_executable(unit-tests-input-netprobe test_netprobe.cpp)

target_link_libraries(unit-tests-input-netprobe
        PRIVATE
        Visor::Input::NetProbe
        Visor::Lib::Test)

add_test(NAME unit-tests-input-netprobe
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src
        COMMAND unit-tests-input-netprobe)