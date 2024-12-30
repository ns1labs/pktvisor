add_executable(pktvisord main.cpp)

find_package(docopt REQUIRED)

if(NOT WIN32)
    set(ADDITIONAL_LIBS resolv)
endif()

if(NOT CRASHPAD_NOT_SUPPORTED)
    find_package(crashpad REQUIRED)
    set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} crashpad::handler)
endif()

target_link_libraries(pktvisord
        PRIVATE
        timer
        ${ADDITIONAL_LIBS}
        docopt_s
        Visor::Core
        ${VISOR_STATIC_PLUGINS}
        )

#copy conan crashpad built binary to binaries folder
if(NOT CRASHPAD_NOT_SUPPORTED)
    if(WIN32)
        configure_file("${CONAN_BIN_DIRS_SENTRY-CRASHPAD}/crashpad_handler.exe" "${PROJECT_BINARY_DIR}/bin/crashpad_handler.exe" COPYONLY)
    else()
        configure_file("${CONAN_BIN_DIRS_SENTRY-CRASHPAD}/crashpad_handler" "${PROJECT_BINARY_DIR}/bin/crashpad_handler" COPYONLY)
    endif()
endif()