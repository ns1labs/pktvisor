add_executable(pktvisor-reader main.cpp)

find_package(docopt REQUIRED)

target_link_libraries(pktvisor-reader
        PRIVATE
        docopt::docopt
        ${VISOR_STATIC_PLUGINS}
        )
