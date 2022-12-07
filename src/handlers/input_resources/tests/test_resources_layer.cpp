#include <catch2/catch.hpp>

#include "DnstapInputStream.h"
#include "FlowInputStream.h"
#include "InputResourcesStreamHandler.h"
#include "PcapInputStream.h"
#include "Policies.h"

using namespace visor::handler::resources;

TEST_CASE("Check resources for pcap input", "[pcap][resources]")
{
    PcapInputStream stream{"pcap-test"};
    stream.config_set("pcap_file", "tests/fixtures/dns_ipv4_udp.pcap");
    stream.config_set("bpf", std::string());

    visor::Config c;
    auto stream_proxy = stream.add_event_proxy(c);
    c.config_set<uint64_t>("num_periods", 1);
    InputResourcesStreamHandler resources_handler{"resource-test", stream_proxy, &c};

    resources_handler.start();
    stream.start();
    // add and remove policy
    auto policy = std::make_unique<visor::Policy>("policy-test");
    stream.add_policy(policy.get());
    stream.remove_policy(policy.get());
    resources_handler.stop();
    stream.stop();

    auto event_data = resources_handler.metrics()->bucket(0)->event_data_locked();

    CHECK(resources_handler.metrics()->current_periods() == 1);
    CHECK(event_data.num_events->value() >= 1);

    nlohmann::json j;
    resources_handler.metrics()->bucket(0)->to_json(j);

    CHECK(j["cpu_usage"]["p50"] != nullptr);
    CHECK(j["memory_bytes"]["p50"] != nullptr);
    CHECK(j["policy_count"] == 0);
    CHECK(j["handler_count"] == 0);

    std::stringstream output;
    std::string line;
    resources_handler.metrics()->bucket(0)->to_prometheus(output, {{"policy", "default"}});
    std::getline(output, line);
    CHECK(line == "# HELP base_total Total number of events");
    std::getline(output, line);
    CHECK(line == "# TYPE base_total gauge");
}

TEST_CASE("Check resources for dnstap input", "[dnstap][resources][!mayfail]")
{
    DnstapInputStream stream{"dnstap-test"};
    stream.config_set("dnstap_file", "inputs/dnstap/tests/fixtures/fixture.dnstap");
    stream.config_set<visor::Configurable::StringList>("only_hosts", {"192.168.0.0/24", "2001:db8::/48"});
    visor::Config c;
    auto stream_proxy = stream.add_event_proxy(c);
    c.config_set<uint64_t>("num_periods", 1);
    InputResourcesStreamHandler resources_handler{"resource-test", stream_proxy, &c};

    resources_handler.start();
    stream.start();
    stream.stop();
    resources_handler.stop();

    auto event_data = resources_handler.metrics()->bucket(0)->event_data_locked();

    CHECK(resources_handler.metrics()->current_periods() == 1);
    CHECK(event_data.num_events->value() == 1);

    nlohmann::json j;
    resources_handler.metrics()->bucket(0)->to_json(j);

    CHECK(j["cpu_usage"]["p50"] != nullptr);
    CHECK(j["memory_bytes"]["p50"] != nullptr);
    CHECK(j["policy_count"] == 0);
    CHECK(j["handler_count"] == 0);
}

TEST_CASE("Check resources for sflow input", "[sflow][resources]")
{
    FlowInputStream stream{"sflow-test"};
    stream.config_set("flow_type", "sflow");
    stream.config_set("pcap_file", "tests/fixtures/ecmp.pcap");

    visor::Config c;
    auto stream_proxy = stream.add_event_proxy(c);
    c.config_set<uint64_t>("num_periods", 1);
    InputResourcesStreamHandler resources_handler{"resource-test", stream_proxy, &c};

    resources_handler.start();
    stream.start();
    stream.stop();
    resources_handler.stop();

    auto event_data = resources_handler.metrics()->bucket(0)->event_data_locked();

    CHECK(resources_handler.metrics()->current_periods() == 1);
    CHECK(event_data.num_events->value() == 1);

    nlohmann::json j;
    resources_handler.metrics()->bucket(0)->to_json(j);

    CHECK(j["cpu_usage"]["p50"] != nullptr);
    CHECK(j["memory_bytes"]["p50"] != nullptr);
    CHECK(j["policy_count"] == 0);
    CHECK(j["handler_count"] == 0);
}