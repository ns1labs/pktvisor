from conan import ConanFile


class Pktvisor(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("catch2/3.5.0")
        self.requires("corrade/2020.06")
        self.requires("cpp-httplib/0.14.3")
        self.requires("docopt.cpp/0.6.3")
        self.requires("fast-cpp-csv-parser/cci.20211104")
        self.requires("json-schema-validator/2.3.0")
        self.requires("libmaxminddb/1.8.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("openssl/3.2.2")
        if self.settings.os != "Windows":
            self.requires("libpcap/1.10.4", force=True)
        self.requires("opentelemetry-proto/1.0.0")
        self.requires("pcapplusplus/23.09")
        self.requires("protobuf/3.21.12")
        self.requires("sigslot/1.2.2")
        self.requires("spdlog/1.12.0")
        self.requires("uvw/3.2.0")
        self.requires("yaml-cpp/0.8.0")
        self.requires("robin-hood-hashing/3.11.5")
        self.requires("libcurl/8.5.0")
        self.requires("crashpad/cci.20220219")
        self.requires("zlib/[>=1.2.10 <1.3]")
        

    def build_requirements(self):
        self.tool_requires("corrade/2020.06")
        self.tool_requires("protobuf/3.21.12")
