add_executable(pktvisor-reader main.cpp)

find_package(docopt REQUIRED)

target_link_libraries(pktvisor-reader
        PRIVATE
        docopt_s
        ${VISOR_STATIC_PLUGINS}
        )

set_target_properties(pktvisor-reader
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
        )