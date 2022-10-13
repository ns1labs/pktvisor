/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "NetProbeStreamHandler.h"

namespace visor::handler::netprobe {

NetProbeStreamHandler::NetProbeStreamHandler(const std::string &name, InputEventProxy *proxy, const Configurable *window_config)
    : visor::StreamMetricsHandler<NetProbeMetricsManager>(name, window_config)
{
    assert(proxy);
    // figure out which input event proxy we have
    _netprobe_proxy = dynamic_cast<NetProbeInputEventProxy *>(proxy);
    if (!_netprobe_proxy) {
        throw StreamHandlerException(fmt::format("NetProbeStreamHandler: unsupported input event proxy {}", proxy->name()));
    }
}

void NetProbeStreamHandler::start()
{
    if (_running) {
        return;
    }

    if (config_exists("recorded_stream")) {
        _metrics->set_recorded_stream();
    }

    if (_netprobe_proxy) {
        _probe_send_connection = _netprobe_proxy->probe_send_signal.connect(&NetProbeStreamHandler::probe_signal_send, this);
        _probe_recv_connection = _netprobe_proxy->probe_recv_signal.connect(&NetProbeStreamHandler::probe_signal_recv, this);
        _probe_fail_connection = _netprobe_proxy->probe_fail_signal.connect(&NetProbeStreamHandler::probe_signal_fail, this);
        _heartbeat_connection = _netprobe_proxy->heartbeat_signal.connect(&NetProbeStreamHandler::check_period_shift, this);
    }

    _running = true;
}

void NetProbeStreamHandler::stop()
{
    if (!_running) {
        return;
    }

    if (_netprobe_proxy) {
        _probe_send_connection.disconnect();
        _probe_recv_connection.disconnect();
        _probe_fail_connection.disconnect();
        _heartbeat_connection.disconnect();
    }

    _running = false;
}

void NetProbeStreamHandler::probe_signal_send(pcpp::Packet &payload, TestType type, const std::string &name, timespec stamp)
{
    if (type == TestType::Ping) {
        if (auto icmp = payload.getLayerOfType<pcpp::IcmpLayer>(); icmp != nullptr) {
            _metrics->process_netprobe_icmp(icmp, name, stamp);
        }
    }
}

void NetProbeStreamHandler::probe_signal_recv(pcpp::Packet &payload, TestType type, const std::string &name, timespec stamp)
{
    if (type == TestType::Ping) {
        if (auto icmp = payload.getLayerOfType<pcpp::IcmpLayer>(); icmp != nullptr) {
            _metrics->process_netprobe_icmp(icmp, name, stamp);
        }
    }
}

void NetProbeStreamHandler::probe_signal_fail([[maybe_unused]] ErrorType error, [[maybe_unused]] TestType type, [[maybe_unused]] const std::string &name)
{
    std::cerr << "fail\n";
}

void NetProbeMetricsBucket::specialized_merge(const AbstractMetricsBucket &o, Metric::Aggregate agg_operator)
{
    // static because caller guarantees only our own bucket type
    const auto &other = static_cast<const NetProbeMetricsBucket &>(o);

    std::shared_lock r_lock(other._mutex);
    std::unique_lock w_lock(_mutex);

    for (const auto &target : other._targets_metrics) {
        const auto &targetId = target.first;
        if (!_targets_metrics.count(targetId)) {
            _targets_metrics[targetId] = std::make_unique<Target>();
        }

        _targets_metrics[targetId]->attempts += target.second->attempts;
        _targets_metrics[targetId]->successes += target.second->successes;
        _targets_metrics[targetId]->time_us.merge(target.second->time_us, agg_operator);
    }
}

void NetProbeMetricsBucket::to_prometheus(std::stringstream &out, Metric::LabelMap add_labels) const
{
    std::shared_lock r_lock(_mutex);

    for (const auto &target : _targets_metrics) {
        auto target_labels = add_labels;
        auto targetId = target.first;
        target_labels["target"] = targetId;

        try {
            target.second->minimum.clear();
            target.second->minimum += target.second->time_us.get_min();
            target.second->minimum.to_prometheus(out, target_labels);

            target.second->maximum.clear();
            target.second->maximum += target.second->time_us.get_max();
            target.second->maximum.to_prometheus(out, target_labels);

            target.second->time_us.to_prometheus(out, target_labels);

            target.second->attempts.to_prometheus(out, target_labels);
            target.second->successes.to_prometheus(out, target_labels);
        } catch (const std::exception &) {
        }
    }
}

void NetProbeMetricsBucket::to_json(json &j) const
{

    std::shared_lock r_lock(_mutex);

    for (const auto &target : _targets_metrics) {
        auto targetId = target.first;

        try {
            target.second->minimum.clear();
            target.second->minimum += target.second->time_us.get_min();
            target.second->minimum.to_json(j["targets"][targetId]);

            target.second->maximum.clear();
            target.second->maximum += target.second->time_us.get_max();
            target.second->maximum.to_json(j["targets"][targetId]);

            target.second->time_us.to_json(j["targets"][targetId]);

            target.second->attempts.to_json(j["targets"][targetId]);
            target.second->successes.to_json(j["targets"][targetId]);
        } catch (const std::exception &) {
        }
    }
}

void NetProbeMetricsBucket::process_filtered()
{
}

bool NetProbeStreamHandler::_filtering([[maybe_unused]] pcpp::Packet *payload)
{
    // no filters yet
    return false;
}

void NetProbeMetricsBucket::process_netprobe_icmp([[maybe_unused]] bool deep, [[maybe_unused]] pcpp::IcmpLayer *layer, const std::string &target)
{
    if (!_targets_metrics.count(target)) {
        _targets_metrics[target] = std::make_unique<Target>();
    }
    ++_targets_metrics[target]->attempts;
}

void NetProbeMetricsBucket::new_icmp_transaction([[maybe_unused]] bool deep, NetProbeTransaction xact)
{
    std::unique_lock lock(_mutex);

    if (!_targets_metrics.count(xact.target)) {
        _targets_metrics[xact.target] = std::make_unique<Target>();
    }
    ++_targets_metrics[xact.target]->successes;
    const uint64_t time_nsec = xact.totalTS.tv_sec * 1000000000ULL + xact.totalTS.tv_nsec;
    _targets_metrics[xact.target]->time_us.update(time_nsec / 1000);
}

void NetProbeMetricsManager::process_netprobe_icmp(pcpp::IcmpLayer *layer, const std::string &target, timespec stamp)
{
    // base event
    new_event(stamp);

    if (layer->getMessageType() == pcpp::ICMP_ECHO_REQUEST) {
        if (auto request = layer->getEchoRequestData(); request != nullptr) {
            _request_reply_manager_list[target].start_transaction(request->header->id, request->header->sequence, stamp, target);
        }
        live_bucket()->process_netprobe_icmp(_deep_sampling_now, layer, target);
    } else if (layer->getMessageType() == pcpp::ICMP_ECHO_REPLY) {
        if (auto reply = layer->getEchoReplyData(); reply != nullptr) {
            auto xact = _request_reply_manager_list[target].maybe_end_transaction(reply->header->id, reply->header->sequence, stamp);
            if (xact.first) {
                live_bucket()->new_icmp_transaction(_deep_sampling_now, xact.second);
            }
        }
    }
}

void NetProbeMetricsManager::process_filtered(timespec stamp)
{
    // base event, no sample
    new_event(stamp, false);
    live_bucket()->process_filtered();
}

}