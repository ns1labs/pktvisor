/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <IpAddress.h>
#include <Packet.h>
#include <uvw/loop.h>

namespace visor::input::netprobe {

enum class ErrorType {
    Timeout,
    SocketError,
    DnsNotFound,
    InvalidIp
};

enum class TestType {
    Ping,
    HTTP,
    UDP,
    TCP
};

typedef std::function<void(pcpp::Packet &, TestType, const std::string &)> RecvCallback;
typedef std::function<void(ErrorType, TestType, const std::string &)> FailCallback;

class NetProbe
{
protected:
    uint64_t _interval_msec{0};
    uint64_t _timeout_msec{0};
    uint64_t _packets_per_test{0};
    uint64_t _packets_interval_msec{0};
    uint64_t _packet_payload_size{0};
    std::string _name;
    std::string _dns;
    pcpp::IPAddress _ip;
    std::shared_ptr<uvw::Loop> _io_loop;
    RecvCallback _success;
    FailCallback _fail;

public:
    NetProbe()
    {
    }

    virtual ~NetProbe()
    {
    }

    void set_configs(uint64_t interval_msec, uint64_t timeout_msec, uint64_t packets_per_test, uint64_t packets_interval_msec, uint64_t packet_payload_size)
    {
        _interval_msec = interval_msec;
        _timeout_msec = timeout_msec;
        _packets_per_test = packets_per_test;
        _packets_interval_msec = packets_interval_msec;
        _packet_payload_size = packet_payload_size;
    }

    void set_target(const pcpp::IPAddress &ip, const std::string &dns)
    {
        if (dns.empty()) {
            _ip = ip;
            if (_ip.isIPv4()) {
                _name = _ip.getIPv4().toString();
            } else {
                _name = _ip.getIPv6().toString();
            }
        } else {
            _dns = dns;
            _name = dns;
        }
    }

    void set_callbacks(RecvCallback success, FailCallback fail)
    {
        _success = success;
        _fail = fail;
    }

    virtual bool start(std::shared_ptr<uvw::Loop> io_loop) = 0;
    virtual bool stop() = 0;
};
}