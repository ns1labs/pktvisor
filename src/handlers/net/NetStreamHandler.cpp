/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "NetStreamHandler.h"
#include "GeoDB.h"
#include "utils.h"
#include <Corrade/Utility/Debug.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma GCC diagnostic ignored "-Wpedantic"
#include "TcpLayer.h"
#include <IPv4Layer.h>
#include <IPv6Layer.h>
#pragma GCC diagnostic pop
#include <arpa/inet.h>
#include <cpc_union.hpp>
#include <fmt/format.h>

namespace visor::handler::net {

NetStreamHandler::NetStreamHandler(const std::string &name, InputStream *stream, const Configurable *window_config, StreamHandler *handler)
    : visor::StreamMetricsHandler<NetworkMetricsManager>(name, window_config)
{
    // figure out which input stream we have
    if (stream) {
        _pcap_stream = dynamic_cast<PcapInputStream *>(stream);
        _dnstap_stream = dynamic_cast<DnstapInputStream *>(stream);
        _mock_stream = dynamic_cast<MockInputStream *>(stream);
        if (!_pcap_stream && !_mock_stream && !_dnstap_stream) {
            throw StreamHandlerException(fmt::format("NetStreamHandler: unsupported input stream {}", stream->name()));
        }
    }

    if (handler) {
        _dns_handler = dynamic_cast<DnsStreamHandler *>(handler);
        if (!_dns_handler) {
            throw StreamHandlerException(fmt::format("NetStreamHandler: unsupported upstream chained stream handler {}", handler->name()));
        }
    }
}

void NetStreamHandler::start()
{
    if (_running) {
        return;
    }

    // default enabled groups
    _groups.set(group::NetMetrics::Counters);
    _groups.set(group::NetMetrics::Cardinality);
    _groups.set(group::NetMetrics::TopGeo);
    _groups.set(group::NetMetrics::TopIps);

    process_groups(_group_defs);

    if (config_exists("recorded_stream")) {
        _metrics->set_recorded_stream();
    }

    if (_pcap_stream) {
        _pkt_connection = _pcap_stream->packet_signal.connect(&NetStreamHandler::process_packet_cb, this);
        _start_tstamp_connection = _pcap_stream->start_tstamp_signal.connect(&NetStreamHandler::set_start_tstamp, this);
        _end_tstamp_connection = _pcap_stream->end_tstamp_signal.connect(&NetStreamHandler::set_end_tstamp, this);
        _heartbeat_connection = _pcap_stream->heartbeat_signal.connect(&NetStreamHandler::check_period_shift, this);
    } else if (_dnstap_stream) {
        _dnstap_connection = _dnstap_stream->dnstap_signal.connect(&NetStreamHandler::process_dnstap_cb, this);
        _heartbeat_connection = _dnstap_stream->heartbeat_signal.connect(&NetStreamHandler::check_period_shift, this);
    } else if (_dns_handler) {
        _pkt_udp_connection = _dns_handler->udp_signal.connect(&NetStreamHandler::process_udp_packet_cb, this);
    }

    _running = true;
}

void NetStreamHandler::stop()
{
    if (!_running) {
        return;
    }

    if (_pcap_stream) {
        _pkt_connection.disconnect();
        _start_tstamp_connection.disconnect();
        _end_tstamp_connection.disconnect();
    } else if (_dnstap_stream) {
        _dnstap_connection.disconnect();
    } else if (_dns_handler) {
        _pkt_udp_connection.disconnect();
    }
    _heartbeat_connection.disconnect();

    _running = false;
}

NetStreamHandler::~NetStreamHandler()
{
}

// callback from input module
void NetStreamHandler::process_packet_cb(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, timespec stamp)
{
    _metrics->process_packet(payload, dir, l3, l4, stamp);
}

void NetStreamHandler::set_start_tstamp(timespec stamp)
{
    _metrics->set_start_tstamp(stamp);
}

void NetStreamHandler::set_end_tstamp(timespec stamp)
{
    _metrics->set_end_tstamp(stamp);
}

void NetStreamHandler::process_dnstap_cb(const dnstap::Dnstap &payload, size_t size)
{
    _metrics->process_dnstap(payload, size);
}

void NetStreamHandler::process_udp_packet_cb(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, [[maybe_unused]] uint32_t flowkey, timespec stamp)
{
    _metrics->process_packet(payload, dir, l3, pcpp::UDP, stamp);
}

void NetworkMetricsBucket::specialized_merge(const AbstractMetricsBucket &o)
{
    // static because caller guarantees only our own bucket type
    const auto &other = static_cast<const NetworkMetricsBucket &>(o);

    // rates maintain their own thread safety
    _rate_in.merge(other._rate_in);
    _rate_out.merge(other._rate_out);
    _throughput_in.merge(other._throughput_in);
    _throughput_out.merge(other._throughput_out);

    std::shared_lock r_lock(other._mutex);
    std::unique_lock w_lock(_mutex);

    if (group_enabled(group::NetMetrics::Counters)) {
        _counters.UDP += other._counters.UDP;
        _counters.TCP += other._counters.TCP;
        _counters.TCP_SYN += other._counters.TCP_SYN;
        _counters.OtherL4 += other._counters.OtherL4;
        _counters.IPv4 += other._counters.IPv4;
        _counters.IPv6 += other._counters.IPv6;
        _counters.total_in += other._counters.total_in;
        _counters.total_out += other._counters.total_out;
    }

    if (group_enabled(group::NetMetrics::Cardinality)) {
        _srcIPCard.merge(other._srcIPCard);
        _dstIPCard.merge(other._dstIPCard);
    }

    if (group_enabled(group::NetMetrics::TopIps)) {
        _topIPv4.merge(other._topIPv4);
        _topIPv6.merge(other._topIPv6);
    }
    if (group_enabled(group::NetMetrics::TopGeo)) {
        _topGeoLoc.merge(other._topGeoLoc);
        _topASN.merge(other._topASN);
    }

    _payload_size.merge(other._payload_size);
}

void NetworkMetricsBucket::to_prometheus(std::stringstream &out, Metric::LabelMap add_labels) const
{

    _rate_in.to_prometheus(out, add_labels);
    _rate_out.to_prometheus(out, add_labels);
    _throughput_in.to_prometheus(out, add_labels);
    _throughput_out.to_prometheus(out, add_labels);

    {
        auto [num_events, num_samples, event_rate, event_lock] = event_data_locked(); // thread safe

        event_rate->to_prometheus(out, add_labels);
        num_events->to_prometheus(out, add_labels);
        num_samples->to_prometheus(out, add_labels);
    }

    std::shared_lock r_lock(_mutex);

    if (group_enabled(group::NetMetrics::Counters)) {
        _counters.UDP.to_prometheus(out, add_labels);
        _counters.TCP.to_prometheus(out, add_labels);
        _counters.TCP_SYN.to_prometheus(out, add_labels);
        _counters.OtherL4.to_prometheus(out, add_labels);
        _counters.IPv4.to_prometheus(out, add_labels);
        _counters.IPv6.to_prometheus(out, add_labels);
        _counters.total_in.to_prometheus(out, add_labels);
        _counters.total_out.to_prometheus(out, add_labels);
    }

    if (group_enabled(group::NetMetrics::Cardinality)) {
        _srcIPCard.to_prometheus(out, add_labels);
        _dstIPCard.to_prometheus(out, add_labels);
    }

    if (group_enabled(group::NetMetrics::TopIps)) {
        _topIPv4.to_prometheus(out, add_labels, [](const uint32_t &val) { return pcpp::IPv4Address(val).toString(); });
        _topIPv6.to_prometheus(out, add_labels);
    }

    if (group_enabled(group::NetMetrics::TopGeo)) {
        _topGeoLoc.to_prometheus(out, add_labels);
        _topASN.to_prometheus(out, add_labels);
    }

    _payload_size.to_prometheus(out, add_labels);
}

void NetworkMetricsBucket::to_json(json &j) const
{

    // do rates first, which handle their own locking
    bool live_rates = !read_only() && !recorded_stream();
    _rate_in.to_json(j, live_rates);
    _rate_out.to_json(j, live_rates);
    _throughput_in.to_json(j, live_rates);
    _throughput_out.to_json(j, live_rates);

    {
        auto [num_events, num_samples, event_rate, event_lock] = event_data_locked(); // thread safe

        event_rate->to_json(j, live_rates);
        num_events->to_json(j);
        num_samples->to_json(j);
    }

    std::shared_lock r_lock(_mutex);

    if (group_enabled(group::NetMetrics::Counters)) {
        _counters.UDP.to_json(j);
        _counters.TCP.to_json(j);
        _counters.TCP_SYN.to_json(j);
        _counters.OtherL4.to_json(j);
        _counters.IPv4.to_json(j);
        _counters.IPv6.to_json(j);
        _counters.total_in.to_json(j);
        _counters.total_out.to_json(j);
    }

    if (group_enabled(group::NetMetrics::Cardinality)) {
        _srcIPCard.to_json(j);
        _dstIPCard.to_json(j);
    }

    if (group_enabled(group::NetMetrics::TopIps)) {
        _topIPv4.to_json(j, [](const uint32_t &val) { return pcpp::IPv4Address(val).toString(); });
        _topIPv6.to_json(j);
    }

    if (group_enabled(group::NetMetrics::TopGeo)) {
        _topGeoLoc.to_json(j);
        _topASN.to_json(j);
    }

    _payload_size.to_json(j);
}

// the main bucket analysis
void NetworkMetricsBucket::process_packet(bool deep, pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4)
{
    if (!deep) {
        process_net_layer(dir, l3, l4, payload.getRawPacket()->getRawDataLen());
        return;
    }

    bool syn_flag = false;
    if (l4 == pcpp::TCP) {
        pcpp::TcpLayer *tcpLayer = payload.getLayerOfType<pcpp::TcpLayer>();
        if (tcpLayer) {
            syn_flag = tcpLayer->getTcpHeader()->synFlag;
        }
    }

    NetworkPacket packet(dir, l3, l4, payload.getRawPacket()->getRawDataLen(), syn_flag, false);

    auto IP4layer = payload.getLayerOfType<pcpp::IPv4Layer>();
    auto IP6layer = payload.getLayerOfType<pcpp::IPv6Layer>();

    if (IP4layer) {
        packet.is_ipv6 = false;
        if (dir == PacketDirection::toHost) {
            packet.ipv4_in = IP4layer->getSrcIPv4Address();
        } else if (dir == PacketDirection::fromHost) {
            packet.ipv4_out = IP4layer->getDstIPv4Address();
        }
    } else if (IP6layer) {
        packet.is_ipv6 = true;
        if (dir == PacketDirection::toHost) {
            packet.ipv6_in = IP6layer->getSrcIPv6Address();
        } else if (dir == PacketDirection::fromHost) {
            packet.ipv6_out = IP6layer->getDstIPv6Address();
        }
    }

    process_net_layer(packet);
}
void NetworkMetricsBucket::process_dnstap(bool deep, const dnstap::Dnstap &payload, size_t size)
{
    pcpp::ProtocolType l3;
    bool is_ipv6{false};
    if (payload.message().has_socket_family()) {
        if (payload.message().socket_family() == dnstap::INET6) {
            l3 = pcpp::IPv6;
            is_ipv6 = true;
        } else if (payload.message().socket_family() == dnstap::INET) {
            l3 = pcpp::IPv4;
        }
    }

    pcpp::ProtocolType l4;
    if (payload.message().has_socket_protocol()) {
        switch (payload.message().socket_protocol()) {
        case dnstap::UDP:
            l4 = pcpp::UDP;
            break;
        case dnstap::TCP:
            l4 = pcpp::TCP;
            break;
        }
    }

    PacketDirection dir;
    switch (payload.message().type()) {
    case dnstap::Message_Type_FORWARDER_RESPONSE:
    case dnstap::Message_Type_STUB_RESPONSE:
    case dnstap::Message_Type_TOOL_RESPONSE:
    case dnstap::Message_Type_UPDATE_RESPONSE:
    case dnstap::Message_Type_CLIENT_RESPONSE:
    case dnstap::Message_Type_AUTH_RESPONSE:
    case dnstap::Message_Type_RESOLVER_RESPONSE:
        dir = PacketDirection::toHost;
        break;
    case dnstap::Message_Type_FORWARDER_QUERY:
    case dnstap::Message_Type_STUB_QUERY:
    case dnstap::Message_Type_TOOL_QUERY:
    case dnstap::Message_Type_UPDATE_QUERY:
    case dnstap::Message_Type_CLIENT_QUERY:
    case dnstap::Message_Type_AUTH_QUERY:
    case dnstap::Message_Type_RESOLVER_QUERY:
        dir = PacketDirection::fromHost;
        break;
    }

    if (!deep) {
        process_net_layer(dir, l3, l4, size);
        return;
    }
    NetworkPacket packet(dir, l3, l4, size, false, is_ipv6);

    if (!is_ipv6 && payload.message().has_query_address() && payload.message().query_address().size() == 4) {
        packet.ipv4_in = pcpp::IPv4Address(reinterpret_cast<const uint8_t *>(payload.message().query_address().data()));
    } else if (is_ipv6 && payload.message().has_query_address() && payload.message().query_address().size() == 16) {
        packet.ipv6_in = pcpp::IPv6Address(reinterpret_cast<const uint8_t *>(payload.message().query_address().data()));
    }

    if (!is_ipv6 && payload.message().has_response_address() && payload.message().response_address().size() == 4) {
        packet.ipv4_out = pcpp::IPv4Address(reinterpret_cast<const uint8_t *>(payload.message().response_address().data()));
    } else if (is_ipv6 && payload.message().has_response_address() && payload.message().response_address().size() == 16) {
        packet.ipv6_out = pcpp::IPv6Address(reinterpret_cast<const uint8_t *>(payload.message().response_address().data()));
    }

    process_net_layer(packet);
}

void NetworkMetricsBucket::process_net_layer(PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, size_t payload_size)
{
    std::unique_lock lock(_mutex);

    switch (dir) {
    case PacketDirection::fromHost:
        ++_rate_out;
        _throughput_out += payload_size;
        break;
    case PacketDirection::toHost:
        ++_rate_in;
        _throughput_in += payload_size;
        break;
    case PacketDirection::unknown:
        break;
    }

    if (group_enabled(group::NetMetrics::Counters)) {
        switch (dir) {
        case PacketDirection::fromHost:
            ++_counters.total_out;
            break;
        case PacketDirection::toHost:
            ++_counters.total_in;
            break;
        case PacketDirection::unknown:
            break;
        }

        switch (l3) {
        case pcpp::IPv6:
            ++_counters.IPv6;
            break;
        case pcpp::IPv4:
            ++_counters.IPv4;
            break;
        default:
            break;
        }

        switch (l4) {
        case pcpp::UDP:
            ++_counters.UDP;
            break;
        case pcpp::TCP:
            ++_counters.TCP;
            break;
        default:
            ++_counters.OtherL4;
            break;
        }
    }

    _payload_size.update(payload_size);
}

void NetworkMetricsBucket::process_net_layer(NetworkPacket &packet)
{
    std::unique_lock lock(_mutex);

    switch (packet.dir) {
    case PacketDirection::fromHost:
        ++_rate_out;
        _throughput_out += packet.payload_size;
        break;
    case PacketDirection::toHost:
        ++_rate_in;
        _throughput_in += packet.payload_size;
        break;
    case PacketDirection::unknown:
        break;
    }

    if (group_enabled(group::NetMetrics::Counters)) {
        switch (packet.dir) {
        case PacketDirection::fromHost:
            ++_counters.total_out;
            break;
        case PacketDirection::toHost:
            ++_counters.total_in;
            break;
        case PacketDirection::unknown:
            break;
        }

        switch (packet.l3) {
        case pcpp::IPv6:
            ++_counters.IPv6;
            break;
        case pcpp::IPv4:
            ++_counters.IPv4;
            break;
        default:
            break;
        }

        switch (packet.l4) {
        case pcpp::UDP:
            ++_counters.UDP;
            break;
        case pcpp::TCP:
            ++_counters.TCP;
            if (packet.syn_flag) {
                ++_counters.TCP_SYN;
            }
            break;
        default:
            ++_counters.OtherL4;
            break;
        }
    }

    _payload_size.update(packet.payload_size);

    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;

    if (!packet.is_ipv6 && packet.ipv4_in.isValid()) {
        group_enabled(group::NetMetrics::Cardinality) ? _srcIPCard.update(packet.ipv4_in.toInt()) : void();
        group_enabled(group::NetMetrics::TopIps) ? _topIPv4.update(packet.ipv4_in.toInt()) : void();
        if (geo::enabled() && group_enabled(group::NetMetrics::TopGeo)) {
            if (IPv4tosockaddr(packet.ipv4_in, &sa4)) {
                if (geo::GeoIP().enabled()) {
                    _topGeoLoc.update(geo::GeoIP().getGeoLocString(reinterpret_cast<struct sockaddr *>(&sa4)));
                }
                if (geo::GeoASN().enabled()) {
                    _topASN.update(geo::GeoASN().getASNString(reinterpret_cast<struct sockaddr *>(&sa4)));
                }
            }
        }
    } else if (packet.is_ipv6 && packet.ipv6_in.isValid()) {
        group_enabled(group::NetMetrics::Cardinality) ? _srcIPCard.update(reinterpret_cast<const void *>(packet.ipv6_in.toBytes()), 16) : void();
        group_enabled(group::NetMetrics::TopIps) ? _topIPv6.update(packet.ipv6_in.toString()) : void();
        if (geo::enabled() && group_enabled(group::NetMetrics::TopGeo)) {
            if (IPv6tosockaddr(packet.ipv6_in, &sa6)) {
                if (geo::GeoIP().enabled()) {
                    _topGeoLoc.update(geo::GeoIP().getGeoLocString(reinterpret_cast<struct sockaddr *>(&sa6)));
                }
                if (geo::GeoASN().enabled()) {
                    _topASN.update(geo::GeoASN().getASNString(reinterpret_cast<struct sockaddr *>(&sa6)));
                }
            }
        }
    }

    if (!packet.is_ipv6 && packet.ipv4_out.isValid()) {
        group_enabled(group::NetMetrics::Cardinality) ? _dstIPCard.update(packet.ipv4_out.toInt()) : void();
        group_enabled(group::NetMetrics::TopIps) ? _topIPv4.update(packet.ipv4_out.toInt()) : void();
        if (geo::enabled() && group_enabled(group::NetMetrics::TopGeo)) {
            if (IPv4tosockaddr(packet.ipv4_out, &sa4)) {
                if (geo::GeoIP().enabled()) {
                    _topGeoLoc.update(geo::GeoIP().getGeoLocString(reinterpret_cast<struct sockaddr *>(&sa4)));
                }
                if (geo::GeoASN().enabled()) {
                    _topASN.update(geo::GeoASN().getASNString(reinterpret_cast<struct sockaddr *>(&sa4)));
                }
            }
        }
    } else if (packet.is_ipv6 && packet.ipv6_out.isValid()) {
        group_enabled(group::NetMetrics::Cardinality) ? _dstIPCard.update(reinterpret_cast<const void *>(packet.ipv6_out.toBytes()), 16) : void();
        group_enabled(group::NetMetrics::TopIps) ? _topIPv6.update(packet.ipv6_out.toString()) : void();
        if (geo::enabled() && group_enabled(group::NetMetrics::TopGeo)) {
            if (IPv6tosockaddr(packet.ipv6_out, &sa6)) {
                if (geo::GeoIP().enabled()) {
                    _topGeoLoc.update(geo::GeoIP().getGeoLocString(reinterpret_cast<struct sockaddr *>(&sa6)));
                }
                if (geo::GeoASN().enabled()) {
                    _topASN.update(geo::GeoASN().getASNString(reinterpret_cast<struct sockaddr *>(&sa6)));
                }
            }
        }
    }
}

// the general metrics manager entry point
void NetworkMetricsManager::process_packet(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, timespec stamp)
{
    // base event
    new_event(stamp);
    // process in the "live" bucket
    live_bucket()->process_packet(_deep_sampling_now, payload, dir, l3, l4);
}

void NetworkMetricsManager::process_dnstap(const dnstap::Dnstap &payload, size_t size)
{
    // dnstap message type
    auto mtype = payload.message().type();
    // set proper timestamp. use dnstap version if available, otherwise "now"
    timespec stamp;
    switch (mtype) {
    case dnstap::Message_Type_CLIENT_RESPONSE:
    case dnstap::Message_Type_AUTH_RESPONSE:
    case dnstap::Message_Type_RESOLVER_RESPONSE:
        if (payload.message().has_response_time_sec()) {
            stamp.tv_sec = payload.message().response_time_sec();
            stamp.tv_nsec = payload.message().response_time_nsec();
        }
        break;
    case dnstap::Message_Type_CLIENT_QUERY:
    case dnstap::Message_Type_AUTH_QUERY:
    case dnstap::Message_Type_RESOLVER_QUERY:
        if (payload.message().has_query_time_sec()) {
            stamp.tv_sec = payload.message().query_time_sec();
            stamp.tv_nsec = payload.message().query_time_nsec();
        }
        break;
    default:
        // use now()
        std::timespec_get(&stamp, TIME_UTC);
    }
    // base event
    new_event(stamp);
    // process in the "live" bucket. this will parse the resources if we are deep sampling
    live_bucket()->process_dnstap(_deep_sampling_now, payload, size);
}

}
