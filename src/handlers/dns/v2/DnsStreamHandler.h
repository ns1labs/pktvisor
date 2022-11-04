/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "AbstractMetricsManager.h"
#include "GeoDB.h"
#include "MockInputStream.h"
#include "PcapInputStream.h"
#include "StreamHandler.h"
#include "TransactionManager.h"
#include "dns.h"
#include "dnstap.pb.h"
#include <Corrade/Utility/Debug.h>
#include <bitset>
#include <limits>
#include <string>

namespace visor::input::dnstap {
class DnstapInputEventProxy;
}

namespace visor::handler::dns::v2 {

using namespace visor::lib::dns;
using namespace visor::lib::transaction;
using namespace visor::input::pcap;
using namespace visor::input::dnstap;
using namespace visor::input::mock;

static constexpr const char *DNS_SCHEMA{"dns"};

// DNS Groups
namespace group {
enum DnsMetrics : visor::MetricGroupIntType {
    Cardinality,
    Counters,
    Quantiles,
    TopEcs,
    TopQtypes,
    TopRcodes,
    TopSize,
    TopQnames,
    TopPorts,
};
}

struct DnsTransaction : public Transaction {
    size_t querySize;
    std::string ecs;
};

enum Protocol : uint64_t {
    DNSTAP_UDP = dnstap::SocketProtocol::UDP,
    DNSTAP_TCP = dnstap::SocketProtocol::TCP,
    DNSTAP_DOT = dnstap::SocketProtocol::DOT,
    DNSTAP_DOH = dnstap::SocketProtocol::DOH,
    PCPP_TCP = pcpp::TCP,
    PCPP_UDP = pcpp::UDP,
    PCPP_UNKOWN = pcpp::UnknownProtocol
};

struct DnsDirection {
    struct Counters {
        Counter xacts;
        Counter UDP;
        Counter TCP;
        Counter DOT;
        Counter DOH;
        Counter IPv4;
        Counter IPv6;
        Counter NX;
        Counter ECS;
        Counter REFUSED;
        Counter SRVFAIL;
        Counter RNOERROR;
        Counter NODATA;
        Counter timeout;
        Counter orphan;
        Counters()
            : xacts(DNS_SCHEMA, {"xacts"}, "Total DNS transactions (query/reply pairs)")
            , UDP(DNS_SCHEMA, {"udp_xacts"}, "Total DNS transactions (query/reply pairs) received over UDP")
            , TCP(DNS_SCHEMA, {"tcp_xacts"}, "Total DNS transactions (query/reply pairs) received over TCP")
            , DOT(DNS_SCHEMA, {"dot_xacts"}, "Total DNS transactions (query/reply pairs) received over DNS over TLS")
            , DOH(DNS_SCHEMA, {"doh_xacts"}, "Total DNS transactions (query/reply pairs) received over DNS over HTTPS")
            , IPv4(DNS_SCHEMA, {"ipv4_xacts"}, "Total DNS transactions (query/reply pairs) received over IPv4")
            , IPv6(DNS_SCHEMA, {"ipv6_xacts"}, "Total DNS transactions (query/reply pairs) received over IPv6")
            , NX(DNS_SCHEMA, {"nxdomain_xacts"}, "Total DNS transactions (query/reply pairs) flagged as reply with return code NXDOMAIN")
            , ECS(DNS_SCHEMA, {"ecs_xacts"}, "Total DNS transactions (query/reply pairs) with the EDNS Client Subnet option set")
            , REFUSED(DNS_SCHEMA, {"refused_xacts"}, "Total DNS transactions (query/reply pairs) flagged as reply with return code REFUSED")
            , SRVFAIL(DNS_SCHEMA, {"srvfail_xacts"}, "Total DNS transactions (query/reply pairs) flagged as reply with return code SRVFAIL")
            , RNOERROR(DNS_SCHEMA, {"noerror_xacts"}, "Total DNS transactions (query/reply pairs) flagged as reply with return code NOERROR")
            , NODATA(DNS_SCHEMA, {"nodata_xacts"}, "Total DNS transactions (query/reply pairs) flagged as reply with return code NOERROR but with an empty answers section")
            , timeout(DNS_SCHEMA, {"timeout_queries"}, "Total number of DNS queries that timed out")
            , orphan(DNS_SCHEMA, {"orphan_responses"}, "Total number of DNS responses that do not have a corresponding query")
        {
        }

        void operator+=(const Counters &other)
        {
            xacts += other.xacts;
            UDP += other.UDP;
            TCP += other.TCP;
            DOH += other.DOH;
            DOT += other.DOT;
            IPv4 += other.IPv4;
            IPv6 += other.IPv6;
            NX += other.NX;
            ECS += other.ECS;
            REFUSED += other.REFUSED;
            SRVFAIL += other.SRVFAIL;
            RNOERROR += other.RNOERROR;
            NODATA += other.NODATA;
            timeout += other.timeout;
            orphan += other.orphan;
        }

        void to_json(json &j) const
        {
            xacts.to_json(j);
            UDP.to_json(j);
            TCP.to_json(j);
            DOH.to_json(j);
            DOT.to_json(j);
            IPv4.to_json(j);
            IPv6.to_json(j);
            NX.to_json(j);
            ECS.to_json(j);
            REFUSED.to_json(j);
            SRVFAIL.to_json(j);
            RNOERROR.to_json(j);
            NODATA.to_json(j);
            timeout.to_json(j);
            orphan.to_json(j);
        }

        void to_prometheus(std::stringstream &out, const Metric::LabelMap &add_labels) const
        {
            xacts.to_prometheus(out, add_labels);
            UDP.to_prometheus(out, add_labels);
            TCP.to_prometheus(out, add_labels);
            DOH.to_prometheus(out, add_labels);
            DOT.to_prometheus(out, add_labels);
            IPv4.to_prometheus(out, add_labels);
            IPv6.to_prometheus(out, add_labels);
            NX.to_prometheus(out, add_labels);
            ECS.to_prometheus(out, add_labels);
            REFUSED.to_prometheus(out, add_labels);
            SRVFAIL.to_prometheus(out, add_labels);
            RNOERROR.to_prometheus(out, add_labels);
            NODATA.to_prometheus(out, add_labels);
            timeout.to_prometheus(out, add_labels);
            orphan.to_prometheus(out, add_labels);
        }
    };
    Counters counters;

    Quantile<uint64_t> dnsTimeUs;
    Quantile<double> dnsRatio;
    Rate dnsRate;

    Cardinality qnameCard;

    TopN<visor::geo::City> topGeoLocECS;
    TopN<std::string> topASNECS;
    TopN<std::string> topQueryECS;
    TopN<std::string> topQname2;
    TopN<std::string> topQname3;
    TopN<std::string> topNX;
    TopN<std::string> topREFUSED;
    TopN<std::string> topSizedQnameResp;
    TopN<std::string> topSRVFAIL;
    TopN<std::string> topNODATA;
    TopN<std::string> topNOERROR;
    TopN<uint16_t> topUDPPort;
    TopN<uint16_t> topQType;
    TopN<uint16_t> topRCode;
    TopN<std::string> topSlow;

    DnsDirection()
        : counters()
        , dnsTimeUs(DNS_SCHEMA, {"xact_time_us"}, "Quantiles of transaction timing (query/reply pairs) in microseconds")
        , dnsRatio(DNS_SCHEMA, {"response_query_size_ratio"}, "Quantiles of ratio of packet sizes in a DNS transaction (reply/query)")
        , dnsRate(DNS_SCHEMA, {"dns_xact_rates"}, "Rate of all DNS transaction (reply/query) per second")
        , qnameCard(DNS_SCHEMA, {"cardinality", "qname"}, "Cardinality of unique QNAMES, both ingress and egress")
        , topGeoLocECS(DNS_SCHEMA, "geo_loc", {"top_geo_loc_ecs_xacts"}, "Top GeoIP ECS locations")
        , topASNECS(DNS_SCHEMA, "asn", {"top_asn_ecs_xacts"}, "Top ASNs by ECS")
        , topQueryECS(DNS_SCHEMA, "ecs", {"top_query_ecs_xacts"}, "Top EDNS Client Subnet (ECS) observed in DNS queries")
        , topQname2(DNS_SCHEMA, "qname", {"top_qname2_xacts"}, "Top QNAMES, aggregated at a depth of two labels")
        , topQname3(DNS_SCHEMA, "qname", {"top_qname3_xacts"}, "Top QNAMES, aggregated at a depth of three labels")
        , topNX(DNS_SCHEMA, "qname", {"top_nxdomain_xacts"}, "Top QNAMES with result code NXDOMAIN")
        , topREFUSED(DNS_SCHEMA, "qname", {"top_refused_xacts"}, "Top QNAMES with result code REFUSED")
        , topSizedQnameResp(DNS_SCHEMA, "qname", {"top_response_bytes"}, "Top QNAMES by response volume in bytes")
        , topSRVFAIL(DNS_SCHEMA, "qname", {"top_srvfail_xacts"}, "Top QNAMES with result code SRVFAIL")
        , topNODATA(DNS_SCHEMA, "qname", {"top_nodata_xacts"}, "Top QNAMES with result code NOERROR and empty answer section")
        , topNOERROR(DNS_SCHEMA, "qname", {"top_noerror_xacts"}, "Top QNAMES with result code NOERROR")
        , topUDPPort(DNS_SCHEMA, "port", {"top_udp_ports_xacts"}, "Top UDP source port on the query side of a transaction")
        , topQType(DNS_SCHEMA, "qtype", {"top_qtype_xacts"}, "Top query types")
        , topRCode(DNS_SCHEMA, "rcode", {"top_rcode_xacts"}, "Top result codes")
        , topSlow(DNS_SCHEMA, "qname", {"top_slow_xacts"}, "Top QNAMES in transactions where host is the server and transaction speed is slower than p90")
    {
    }

    void update_topn_metrics(size_t topn_count, uint64_t percentile_threshold)
    {
        topGeoLocECS.set_settings(topn_count, percentile_threshold);
        topASNECS.set_settings(topn_count, percentile_threshold);
        topQueryECS.set_settings(topn_count, percentile_threshold);
        topQname2.set_settings(topn_count, percentile_threshold);
        topQname3.set_settings(topn_count, percentile_threshold);
        topNX.set_settings(topn_count, percentile_threshold);
        topREFUSED.set_settings(topn_count, percentile_threshold);
        topSizedQnameResp.set_settings(topn_count, percentile_threshold);
        topSRVFAIL.set_settings(topn_count, percentile_threshold);
        topNODATA.set_settings(topn_count, percentile_threshold);
        topNOERROR.set_settings(topn_count, percentile_threshold);
        topUDPPort.set_settings(topn_count, percentile_threshold);
        topQType.set_settings(topn_count, percentile_threshold);
        topRCode.set_settings(topn_count, percentile_threshold);
        topSlow.set_settings(topn_count, percentile_threshold);
    }
};

class DnsMetricsBucket final : public visor::AbstractMetricsBucket
{
protected:
    mutable std::shared_mutex _mutex;
    size_t _topn_count{10};
    uint64_t _topn_percentile_threshold{0};
    inline static const std::unordered_map<PacketDirection, std::string> _dir_str = {
        {PacketDirection::toHost, "in"},
        {PacketDirection::fromHost, "out"},
        {PacketDirection::unknown, "unknown"}};
    std::map<PacketDirection, DnsDirection> _dns;
    Counter _filtered;

public:
    DnsMetricsBucket()
        : _filtered(DNS_SCHEMA, {"filtered_packets"}, "Total DNS wire packets seen that did not match the configured filter(s) (if any)")
    {
        set_num_events_info(DNS_SCHEMA, {"observed_packets"}, "Total DNS wire packets events");
        set_num_sample_info(DNS_SCHEMA, {"deep_sampled_packets"}, "Total DNS wire packets that were sampled for deep inspection");
    }

    auto get_xact_data_locked(PacketDirection dir) const
    {
        std::shared_lock lock(_mutex);
        struct retVals {
            const Quantile<uint64_t> &xact;
            std::shared_lock<std::shared_mutex> lock;
        };
        return retVals{_dns.at(dir).dnsTimeUs, std::move(lock)};
    }

    void dir_setup(PacketDirection dir)
    {
        std::unique_lock lock(_mutex);
        if (!_dns.count(dir)) {
            _dns[dir].update_topn_metrics(_topn_count, _topn_percentile_threshold);
        }
    }

    void inc_xact_timed_out(uint64_t c, PacketDirection dir)
    {
        std::unique_lock lock(_mutex);
        _dns[dir].counters.timeout += c;
    }

    void inc_xact_orphan(uint64_t c, PacketDirection dir)
    {
        std::unique_lock lock(_mutex);
        _dns[dir].counters.orphan += c;
    }

    // get a copy of the counters
    DnsDirection::Counters counters(PacketDirection dir) const
    {
        std::shared_lock lock(_mutex);
        return _dns.at(dir).counters;
    }

    // visor::AbstractMetricsBucket
    void specialized_merge(const AbstractMetricsBucket &other, Metric::Aggregate agg_operator) override;
    void to_json(json &j) const override;
    void to_prometheus(std::stringstream &out, Metric::LabelMap add_labels = {}) const override;
    void update_topn_metrics(size_t topn_count, uint64_t percentile_threshold) override
    {
        _topn_count = topn_count;
        _topn_percentile_threshold = percentile_threshold;
    }

    void on_set_read_only() override
    {
        // stop rate collection
        for (auto &dns : _dns) {
            dns.second.dnsRate.cancel();
        }
    }

    void process_filtered();
    void new_dns_transaction(bool deep, float per90th, DnsLayer &dns, PacketDirection dir, DnsTransaction xact, pcpp::ProtocolType l3, Protocol l4, uint16_t port, size_t suffix_size = 0);
};

class DnsMetricsManager final : public visor::AbstractMetricsManager<DnsMetricsBucket>
{
    using DnsXactID = std::pair<uint32_t, uint16_t>;
    struct DirTransaction {
        TransactionManager<DnsXactID, DnsTransaction> xact_map;
        float per_90th{0.0};
    };
    std::map<PacketDirection, DirTransaction> _pair_manager = {{PacketDirection::toHost, DirTransaction()},
        {PacketDirection::fromHost, DirTransaction()},
        {PacketDirection::unknown, DirTransaction()}};

public:
    DnsMetricsManager(const Configurable *window_config)
        : visor::AbstractMetricsManager<DnsMetricsBucket>(window_config)
    {
    }

    void on_period_shift(timespec stamp, [[maybe_unused]] const DnsMetricsBucket *maybe_expiring_bucket) override
    {
        // DNS transaction support
        for (auto &manager : _pair_manager) {
            if (auto timed_out = manager.second.xact_map.purge_old_transactions(stamp); timed_out) {
                live_bucket()->inc_xact_timed_out(timed_out, manager.first);
            }
            if (auto [xact, lock] = bucket(1)->get_xact_data_locked(manager.first); xact.get_n()) {
                manager.second.per_90th = xact.get_quantile(0.90);
            }
        }
    }

    size_t num_open_transactions() const
    {
        size_t count{0};
        for (const auto &manager : _pair_manager) {
            count += manager.second.xact_map.open_transaction_count();
        }
        return count;
    }

    void process_filtered(timespec stamp);
    void process_dns_layer(DnsLayer &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, uint32_t flowkey, uint16_t port, size_t suffix_size, timespec stamp);
    void process_dnstap(const dnstap::Dnstap &payload, PacketDirection dir, bool filtered);
};

class DnsTcpSessionData final : public TcpSessionData
{
public:
    static constexpr size_t MIN_DNS_QUERY_SIZE = 17;

    DnsTcpSessionData(got_msg_cb got_data_handler)
        : TcpSessionData(got_data_handler)
    {
    }

    ~DnsTcpSessionData() = default;

    void receive_tcp_data(const uint8_t *data, size_t len) override;
};

class DnsStreamHandler final : public visor::StreamMetricsHandler<DnsMetricsManager>
{
    static constexpr size_t DNSTAP_TYPE_SIZE = 15;

    struct DnsCacheData {
        uint32_t flowKey = 0;
        timespec timestamp = timespec();
        std::unique_ptr<DnsLayer> dnsLayer;
    };
    static thread_local DnsCacheData _cached_dns_layer;

    // the input event proxy we support (only one will be in use at a time)
    PcapInputEventProxy *_pcap_proxy{nullptr};
    MockInputEventProxy *_mock_proxy{nullptr};
    DnstapInputEventProxy *_dnstap_proxy{nullptr};

    typedef uint32_t flowKey;
    std::unordered_map<flowKey, TcpFlowData> _tcp_connections;

    sigslot::connection _dnstap_connection;

    sigslot::connection _pkt_udp_connection;
    sigslot::connection _start_tstamp_connection;
    sigslot::connection _end_tstamp_connection;

    sigslot::connection _tcp_start_connection;
    sigslot::connection _tcp_end_connection;
    sigslot::connection _tcp_message_connection;

    sigslot::connection _heartbeat_connection;

    void process_udp_packet_cb(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, uint32_t flowkey, timespec stamp);
    void process_dnstap_cb(const dnstap::Dnstap &, size_t);
    void tcp_message_ready_cb(int8_t side, const pcpp::TcpStreamData &tcpData);
    void tcp_connection_start_cb(const pcpp::ConnectionData &connectionData);
    void tcp_connection_end_cb(const pcpp::ConnectionData &connectionData, pcpp::TcpReassembly::ConnectionEndReason reason);
    void set_start_tstamp(timespec stamp);
    void set_end_tstamp(timespec stamp);

    static const inline std::map<std::string, std::pair<int, int>> _dnstap_map_types = {
        {"auth", {dnstap::Message_Type_AUTH_QUERY, dnstap::Message_Type_AUTH_RESPONSE}},
        {"resolver", {dnstap::Message_Type_RESOLVER_QUERY, dnstap::Message_Type_RESOLVER_RESPONSE}},
        {"client", {dnstap::Message_Type_CLIENT_QUERY, dnstap::Message_Type_CLIENT_RESPONSE}},
        {"forwarder", {dnstap::Message_Type_FORWARDER_QUERY, dnstap::Message_Type_FORWARDER_RESPONSE}},
        {"stub", {dnstap::Message_Type_STUB_QUERY, dnstap::Message_Type_STUB_RESPONSE}},
        {"tool", {dnstap::Message_Type_TOOL_QUERY, dnstap::Message_Type_TOOL_RESPONSE}},
        {"update", {dnstap::Message_Type_UPDATE_QUERY, dnstap::Message_Type_UPDATE_RESPONSE}}};

    // DNS Filters
    enum Filters {
        ExcludingRCode,
        OnlyRCode,
        OnlyQtype,
        AnswerCount,
        OnlyQNameSuffix,
        OnlyDNSSECResponse,
        DnstapMsgType,
        GeoLocNotFound,
        AsnNotFound,
        DisableIn,
        DisableOut,
        DisableUndefDir,
        FiltersMAX
    };
    std::bitset<Filters::FiltersMAX> _f_enabled;
    enum Configs {
        PublicSuffixList,
        ConfigsMAX
    };
    std::bitset<Configs::ConfigsMAX> _c_enabled;
    uint16_t _f_rcode{0};
    uint64_t _f_answer_count{0};
    std::vector<std::string> _f_qnames;
    std::vector<uint16_t> _f_qtypes;
    size_t _static_suffix_size{0};
    std::bitset<DNSTAP_TYPE_SIZE> _f_dnstap_types;

    static const inline StreamMetricsHandler::GroupDefType _group_defs = {
        {"cardinality", group::DnsMetrics::Cardinality},
        {"counters", group::DnsMetrics::Counters},
        {"quantiles", group::DnsMetrics::Quantiles},
        {"top_ecs", group::DnsMetrics::TopEcs},
        {"top_qtypes", group::DnsMetrics::TopQtypes},
        {"top_rcodes", group::DnsMetrics::TopRcodes},
        {"top_size", group::DnsMetrics::TopSize},
        {"top_qnames", group::DnsMetrics::TopQnames},
        {"top_ports", group::DnsMetrics::TopPorts}};

    bool _filtering(DnsLayer &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, uint16_t port, timespec stamp);
    bool _configs(DnsLayer &payload);

public:
    DnsStreamHandler(const std::string &name, InputEventProxy *proxy, const Configurable *window_config);
    ~DnsStreamHandler() = default;

    // visor::AbstractModule
    std::string schema_key() const override
    {
        return DNS_SCHEMA;
    }

    void start() override;
    void stop() override;
    void info_json(json &j) const override;
};
}
