/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "InputStream.h"
#include "NetProbe.h"
#include <Packet.h>
#include <spdlog/spdlog.h>
#include <uvw.hpp>

namespace visor::input::netprobe {

class NetProbeInputStream : public visor::InputStream
{
    static uint16_t _id;
    TestType _type{TestType::Ping};
    uint64_t _interval_msec{5000};
    uint64_t _timeout_msec{2000};
    uint64_t _packets_per_test{1};
    uint64_t _packets_interval_msec{25};
    uint64_t _packet_payload_size{48};
    std::map<std::string, pcpp::IPAddress> _ip_list;
    std::map<std::string, std::string> _dns_list;
    std::shared_ptr<spdlog::logger> _logger;

    std::unique_ptr<std::thread> _io_thread;
    std::shared_ptr<uvw::Loop> _io_loop;
    std::shared_ptr<uvw::AsyncHandle> _async_h;
    std::shared_ptr<uvw::TimerHandle> _timer;

    std::vector<std::unique_ptr<NetProbe>> _probes;

    static const inline std::map<std::string, TestType> _test_defs = {
        {"ping", TestType::Ping},
        {"http", TestType::HTTP},
        {"udp", TestType::UDP},
        {"tcp", TestType::TCP}};

    void _create_netprobe_loop();
    void _send_cb(pcpp::Packet &, TestType, const std::string &, timespec);
    void _recv_cb(pcpp::Packet &, TestType, const std::string &, timespec);
    void _fail_cb(ErrorType, TestType, const std::string &);

public:
    NetProbeInputStream(const std::string &name);
    ~NetProbeInputStream() = default;

    // visor::AbstractModule
    std::string schema_key() const override
    {
        return "netprobe";
    }
    void start() override;
    void stop() override;
    void info_json(json &j) const override;
    std::unique_ptr<InputEventProxy> create_event_proxy(const Configurable &filter) override;
};

class NetProbeInputEventProxy : public visor::InputEventProxy
{
public:
    NetProbeInputEventProxy(const std::string &name, const Configurable &filter)
        : InputEventProxy(name, filter)
    {
    }

    ~NetProbeInputEventProxy() = default;

    size_t consumer_count() const override
    {
        return policy_signal.slot_count() + heartbeat_signal.slot_count() + probe_recv_signal.slot_count() + probe_send_signal.slot_count() + probe_fail_signal.slot_count();
    }

    void probe_send_cb(pcpp::Packet &p, TestType t, const std::string &n, timespec s)
    {
        probe_send_signal(p, t, n, s);
    }

    void probe_recv_cb(pcpp::Packet &p, TestType t, const std::string &n, timespec s)
    {
        probe_recv_signal(p, t, n, s);
    }

    void probe_fail_cb(ErrorType e, TestType t, const std::string &n)
    {
        probe_fail_signal(e, t, n);
    }

    // handler functionality
    // IF THIS changes, see consumer_count()
    // note: these are mutable because consumer_count() calls slot_count() which is not const (unclear if it could/should be)
    mutable sigslot::signal<pcpp::Packet &, TestType, const std::string &, timespec> probe_send_signal;
    mutable sigslot::signal<pcpp::Packet &, TestType, const std::string &, timespec> probe_recv_signal;
    mutable sigslot::signal<ErrorType, TestType, const std::string &> probe_fail_signal;
};

}
