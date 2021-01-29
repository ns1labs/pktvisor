#ifndef PKTVISORD_NETSTREAMHANDLER_H
#define PKTVISORD_NETSTREAMHANDLER_H

#include "AbstractMetricsManager.h"
#include "PcapInputStream.h"
#include "StreamHandler.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wunused-function"
#include <datasketches/cpc/cpc_sketch.hpp>
#include <datasketches/fi/frequent_items_sketch.hpp>
#include <datasketches/kll/kll_sketch.hpp>
#pragma GCC diagnostic pop
#include <Corrade/Utility/Debug.h>
#include <string>

namespace pktvisor::handler {

using namespace pktvisor::input::pcap;

// TODO
struct NetworkRateSketches {
    Rate::QuantileType net_rateIn;
    Rate::QuantileType net_rateOut;
};

class NetworkMetricsBucket final : public pktvisor::AbstractMetricsBucket
{
public:
    const uint8_t START_FI_MAP_SIZE = 7; // 2^7 = 128
    const uint8_t MAX_FI_MAP_SIZE = 13;  // 2^13 = 8192

    datasketches::cpc_sketch _net_srcIPCard;
    datasketches::cpc_sketch _net_dstIPCard;

    datasketches::frequent_items_sketch<std::string> _net_topGeoLoc;
    datasketches::frequent_items_sketch<std::string> _net_topASN;
    datasketches::frequent_items_sketch<uint32_t> _net_topIPv4;
    datasketches::frequent_items_sketch<std::string> _net_topIPv6; // TODO not very efficient, should switch to 16 byte uint

    uint64_t _numPackets = 0;
    uint64_t _numPackets_UDP = 0;
    uint64_t _numPackets_TCP = 0;
    uint64_t _numPackets_OtherL4 = 0;
    uint64_t _numPackets_IPv6 = 0;
    uint64_t _numPackets_in = 0;
    uint64_t _numPackets_out = 0;

    // TODO
    NetworkRateSketches _rateSketches;

public:
    NetworkMetricsBucket()
        : _net_srcIPCard()
        , _net_dstIPCard()
        , _net_topGeoLoc(MAX_FI_MAP_SIZE, START_FI_MAP_SIZE)
        , _net_topASN(MAX_FI_MAP_SIZE, START_FI_MAP_SIZE)
        , _net_topIPv4(MAX_FI_MAP_SIZE, START_FI_MAP_SIZE)
        , _net_topIPv6(MAX_FI_MAP_SIZE, START_FI_MAP_SIZE)
    {
    }

    // pktvisor::AbstractMetricsBucket
    void merge(const AbstractMetricsBucket &other) override;
    void toJSON(json &j) const override;

    void process_packet(bool deep, pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, timespec stamp);
};

class NetworkMetricsManager final : public pktvisor::AbstractMetricsManager<NetworkMetricsBucket>
{
public:
    NetworkMetricsManager(uint periods, int deepSampleRate)
        : pktvisor::AbstractMetricsManager<NetworkMetricsBucket>(periods, deepSampleRate)
    {
    }

#if 0
    void on_period_shift() override
    {
        Corrade::Utility::Debug{} << "period shift";
    }
    void on_period_evict(const NetworkMetricsBucket *bucket) override
    {
        Corrade::Utility::Debug{} << "evict: " << bucket->_numPackets;
    }
#endif

    void process_packet(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, timespec stamp);
};

class NetStreamHandler final : public pktvisor::StreamMetricsHandler<NetworkMetricsManager>
{

    PcapInputStream *_stream;

    sigslot::connection _pkt_connection;
    sigslot::connection _start_tstamp_connection;

    void process_packet(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, timespec stamp);
    void set_initial_tstamp(timespec stamp);

public:
    NetStreamHandler(const std::string &name, PcapInputStream *stream, uint periods, int deepSampleRate);
    ~NetStreamHandler() override;

    // pktvisor::AbstractModule
    void start() override;
    void stop() override;
    json info_json() const override;

    // pktvisor::StreamMetricsHandler
    void toJSON(json &j, uint64_t period, bool merged) override;
};

}

#endif //PKTVISORD_NETSTREAMHANDLER_H
