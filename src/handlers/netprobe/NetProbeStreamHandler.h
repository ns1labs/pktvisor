/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "AbstractMetricsManager.h"
#include "NetProbeInputStream.h"
#include "StreamHandler.h"
#include <Corrade/Utility/Debug.h>
#include <IcmpLayer.h>
#include <limits>
#include <string>

namespace visor::handler::netprobe {

using namespace visor::input::netprobe;

static constexpr const char *NET_PROBE_SCHEMA{"netprobe"};

struct Target {
    Quantile<uint64_t> time_ms;
    Counter fail;

    Target()
        : time_ms(NET_PROBE_SCHEMA, {"time_ms"}, "Net Probe quantile")
        , fail(NET_PROBE_SCHEMA, {"fail"}, "Total Net Probe failt")
    {
    }
};

class NetProbeMetricsBucket final : public visor::AbstractMetricsBucket
{

protected:
    mutable std::shared_mutex _mutex;

    // total numPackets is tracked in base class num_events
    struct counters {
        Counter total;
        Counter filtered;
        counters()
            : total(NET_PROBE_SCHEMA, {"wire_packets", "total"}, "Total Net Probe wire packets matching the configured filter(s)")
            , filtered(NET_PROBE_SCHEMA, {"wire_packets", "filtered"}, "Total Net Probe wire packets seen that did not match the configured filter(s) (if any)")
        {
        }
    };
    counters _counters;
    Rate _rate_total;
    std::map<std::string, std::unique_ptr<Target>> _targets_metrics;

public:
    NetProbeMetricsBucket()
        : _rate_total(NET_PROBE_SCHEMA, {"rates", "total"}, "Rate of all Net Probe wire packets (combined ingress and egress) in packets per second")
    {
        set_event_rate_info(NET_PROBE_SCHEMA, {"rates", "events"}, "Rate of all Net Probe wire packets before filtering per second");
        set_num_events_info(NET_PROBE_SCHEMA, {"wire_packets", "events"}, "Total Net Probe wire packets events");
        set_num_sample_info(NET_PROBE_SCHEMA, {"wire_packets", "deep_samples"}, "Total Net Probe wire packets that were sampled for deep inspection");
    }

    // get a copy of the counters
    counters counters() const
    {
        std::shared_lock lock(_mutex);
        return _counters;
    }

    // visor::AbstractMetricsBucket
    void specialized_merge(const AbstractMetricsBucket &other, Metric::Aggregate agg_operator) override;
    void to_json(json &j) const override;
    void to_prometheus(std::stringstream &out, Metric::LabelMap add_labels = {}) const override;
    void update_topn_metrics(size_t, uint64_t) override
    {
    }

    void on_set_read_only() override
    {
        // stop rate collection
        _rate_total.cancel();
    }

    void process_filtered();
    void process_netprobe_icmp(bool deep, pcpp::IcmpLayer *layer, std::string target, timespec stamp);
    void process_fail_event(std::string target);
};

class NetProbeMetricsManager final : public visor::AbstractMetricsManager<NetProbeMetricsBucket>
{
public:
    NetProbeMetricsManager(const Configurable *window_config)
        : visor::AbstractMetricsManager<NetProbeMetricsBucket>(window_config)
    {
    }

    void process_filtered(timespec stamp);
    void process_netprobe_icmp(pcpp::IcmpLayer *layer, std::string target, timespec stamp);
    void process_fail_event(std::string target);
};

class NetProbeStreamHandler final : public visor::StreamMetricsHandler<NetProbeMetricsManager>
{

    NetProbeInputEventProxy *_netprobe_proxy;

    sigslot::connection _probe_recv_connection;
    sigslot::connection _probe_fail_connection;
    sigslot::connection _heartbeat_connection;

    void probe_signal_recv(pcpp::Packet &, TestType, const std::string &);
    void probe_signal_fail(ErrorType, TestType, const std::string &);

    bool _filtering(pcpp::Packet *payload);

public:
    NetProbeStreamHandler(const std::string &name, InputEventProxy *proxy, const Configurable *window_config);
    ~NetProbeStreamHandler() = default;

    // visor::AbstractModule
    std::string schema_key() const override
    {
        return NET_PROBE_SCHEMA;
    }

    void start() override;
    void stop() override;
};

}
