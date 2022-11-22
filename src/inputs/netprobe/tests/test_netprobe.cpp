#include "NetProbeInputStream.h"
#include <catch2/catch.hpp>

using namespace visor::input::netprobe;
using namespace std::chrono;

TEST_CASE("NetProbe Configs", "[netprobe][ping]")
{
    NetProbeInputStream stream{"net-probe-test"};
    stream.config_set("test_type", "ping");
    stream.config_set<uint64_t>("interval_msec", 2000);
    stream.config_set<uint64_t>("timeout_msec", 1000);
    stream.config_set<uint64_t>("packets_interval_msec", 25);
    stream.config_set<uint64_t>("packets_per_test", 2);
    stream.config_set<uint64_t>("packet_payload_size", 56);
    auto targets = std::make_shared<visor::Configurable>();
    auto target = std::make_shared<visor::Configurable>();
    target->config_set("target", "localhost");
    targets->config_set<std::shared_ptr<visor::Configurable>>("my_target", target);
    stream.config_set<std::shared_ptr<visor::Configurable>>("targets", targets);

    CHECK_NOTHROW(stream.start());
    std::this_thread::sleep_for(1s);
    CHECK_NOTHROW(stream.stop());

    nlohmann::json j;
    stream.info_json(j);
    CHECK(j["module"]["config"]["test_type"] == "ping");
}

TEST_CASE("NetProbe TCP config", "[netprobe][tcp]")
{
    NetProbeInputStream stream{"net-probe-test"};
    stream.config_set("test_type", "tcp");
    stream.config_set<uint64_t>("interval_msec", 500);
    stream.config_set<uint64_t>("timeout_msec", 200);
    auto targets = std::make_shared<visor::Configurable>();
    auto target = std::make_shared<visor::Configurable>();
    target->config_set("target", "example.com");
    target->config_set<uint64_t>("port", 80);
    targets->config_set<std::shared_ptr<visor::Configurable>>("my_target", target);
    stream.config_set<std::shared_ptr<visor::Configurable>>("targets", targets);

    CHECK_NOTHROW(stream.start());
    std::this_thread::sleep_for(1s);
    CHECK_NOTHROW(stream.stop());

    nlohmann::json j;
    stream.info_json(j);
    CHECK(j["module"]["config"]["test_type"] == "tcp");
}

TEST_CASE("NetProbe Boundaries", "[netprobe]")
{
    NetProbeInputStream stream{"net-probe-test"};
    stream.config_set("test_type", "ping");
    auto targets = std::make_shared<visor::Configurable>();
    auto target = std::make_shared<visor::Configurable>();
    target->config_set("target", "localhost");
    targets->config_set<std::shared_ptr<visor::Configurable>>("my_target", target);
    stream.config_set<std::shared_ptr<visor::Configurable>>("targets", targets);

    SECTION("timeout greater than interval")
    {
        stream.config_set<uint64_t>("interval_msec", 2000);
        stream.config_set<uint64_t>("timeout_msec", 5000);
        CHECK_THROWS_WITH(stream.start(), "timeout_msec [5000] cannot be greater than interval_msec [2000]");
    }

    SECTION("huge payload size")
    {
        stream.config_set<uint64_t>("packet_payload_size", 50000000000);
        CHECK_THROWS_WITH(stream.start(), "packet_payload_size was set to 50000000000 but max supported size is 65500");
    }

    SECTION("num packets times packets interval greater than interval")
    {
        stream.config_set<uint64_t>("interval_msec", 2000);
        stream.config_set<uint64_t>("packets_interval_msec", 100);
        stream.config_set<uint64_t>("packets_per_test", 25);
        CHECK_THROWS_WITH(stream.start(), "packets_per_test [25] times packets_interval_msec [100] cannot be greater than packets_interval_msec [2000]");
    }
}

TEST_CASE("Test Configs fail", "[netprobe][config]")
{
    NetProbeInputStream stream{"net-probe-test"};
    stream.config_set("test_type", "ping");

    CHECK_THROWS_WITH(stream.start(), "no targets specified");
}

TEST_CASE("Netprobe invalid config", "[netprobe][config]")
{
    NetProbeInputStream stream{"net-probe-test"};
    stream.config_set("invalid_config", true);

    CHECK_THROWS_WITH(stream.start(), "invalid_config is an invalid/unsupported config or filter. The valid configs/filters are: test_type, interval_msec, timeout_msec, packets_per_test, packets_interval_msec, packet_payload_size, targets");
}