#include "PingProbe.h"

#include "NetProbeException.h"
#include <Packet.h>
#include <TimespecTimeval.h>
#include <uvw/idle.h>

namespace visor::input::netprobe {

sigslot::signal<pcpp::Packet &, timespec> PingReceiver::recv_signal;
thread_local std::atomic<uint32_t> PingProbe::sock_count{0};
thread_local SOCKET PingProbe::_sock{INVALID_SOCKET};

PingReceiver::PingReceiver()
{
    _setup_receiver();
}
PingReceiver::~PingReceiver()
{
    recv_signal.disconnect_all();
    _poll->close();
    if (_async_h && _io_thread) {
        // we have to use AsyncHandle to stop the loop from the same thread the loop is running in
        _async_h->send();
        // waits for _io_loop->run() to return
        if (_io_thread->joinable()) {
            _io_thread->join();
        }
    }
#ifdef _WIN32
    closesocket(_sock);
#else
    close(_sock);
#endif
    _sock = INVALID_SOCKET;
}

void PingReceiver::_setup_receiver()
{
    _io_loop = uvw::Loop::create();
    if (!_io_loop) {
        throw NetProbeException("unable to create io loop");
    }
    // AsyncHandle lets us stop the loop from its own thread
    _async_h = _io_loop->resource<uvw::AsyncHandle>();
    if (!_async_h) {
        throw NetProbeException("unable to initialize AsyncHandle");
    }
    _async_h->once<uvw::AsyncEvent>([this](const auto &, auto &handle) {
        _io_loop->stop();
        _io_loop->close();
        handle.close();
    });

    _sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
#ifdef _WIN32
    if (_sock == INVALID_SOCKET) {
        throw NetProbeException("unable to create receiver socket");
    }
    unsigned long flag = 1;
    if (ioctlsocket(_sock, FIONBIO, &flag) == SOCKET_ERROR) {
        throw NetProbeException("unable to create receiver socket");
    }
#else
    if (_sock == SOCKET_ERROR) {
        _sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    }
    int flag = 1;
    if ((flag = fcntl(_sock, F_GETFL, 0)) == SOCKET_ERROR) {
        throw NetProbeException("unable to create receiver socket");
    }
    if (fcntl(_sock, F_SETFL, flag | O_NONBLOCK) == SOCKET_ERROR) {
        throw NetProbeException("unable to create receiver socket");
    }
#endif

    _poll = _io_loop->resource<uvw::PollHandle>(static_cast<uvw::OSSocketHandle>(_sock));
    if (!_poll) {
        throw NetProbeException("PingProbe - unable to initialize PollHandle");
    }
    _poll->on<uvw::ErrorEvent>([](const auto &, auto &handler) {
        handler.close();
    });

    _poll->on<uvw::PollEvent>([this](const uvw::PollEvent &, uvw::PollHandle &) {
        int rc{0};
        while (rc != SOCKET_ERROR) {
            rc = recv(_sock, _array.data(), _array.size(), 0);
            if (rc != SOCKET_ERROR) {
                timespec stamp;
                std::timespec_get(&stamp, TIME_UTC);
                timeval time;
                TIMESPEC_TO_TIMEVAL(&time, &stamp);
                pcpp::RawPacket raw(reinterpret_cast<uint8_t *>(_array.data()), rc, time, false, pcpp::LINKTYPE_DLT_RAW1);
                pcpp::Packet packet(&raw, pcpp::ICMP);
                recv_signal(packet, stamp);
            }
        }
    });

    _poll->init();
    _poll->start(uvw::PollHandle::Event::READABLE);

    // spawn the loop
    _io_thread = std::make_unique<std::thread>([this] {
        _io_loop->run();
    });
}

bool PingProbe::start(std::shared_ptr<uvw::Loop> io_loop)
{
    if (_init || (!_ip.isValid() && _dns.empty())) {
        return false;
    }

    // TODO support ICMPv6
    if (_ip.isIPv6()) {
        return false;
    }
    // add validator
    _payload_array = validator;
    if (_config.packet_payload_size < validator.size()) {
        _config.packet_payload_size = validator.size();
    }
    _payload_array.resize(_config.packet_payload_size);
    std::fill(_payload_array.begin() + validator.size(), _payload_array.end(), 0);

    _io_loop = io_loop;

    // execute once
    static auto receiver = std::make_unique<PingReceiver>();

    _interval_timer = _io_loop->resource<uvw::TimerHandle>();
    if (!_interval_timer) {
        throw NetProbeException("PingProbe - unable to initialize interval TimerHandle");
    }
    _interval_timer->on<uvw::TimerEvent>([this](const auto &, auto &) {
        _internal_sequence = 0;

        _timeout_timer = _io_loop->resource<uvw::TimerHandle>();
        if (!_timeout_timer) {
            throw NetProbeException("PingProbe - unable to initialize timeout TimerHandle");
        }

        _timeout_timer->on<uvw::TimerEvent>([this](const auto &, auto &) {
            _internal_sequence = _config.packets_per_test;
            _fail(ErrorType::Timeout, TestType::Ping, _name);
            if (_internal_timer) {
                _internal_timer->stop();
            }
            _interval_timer->again();
        });

        if (auto error = _create_socket(); error.has_value()) {
            _fail(error.value(), TestType::Ping, _name);
            return;
        }

        if (auto error = _get_addr(); error.has_value()) {
            _fail(error.value(), TestType::Ping, _name);
            return;
        }

        _internal_timer = _io_loop->resource<uvw::TimerHandle>();
        _internal_timer->on<uvw::TimerEvent>([this](const auto &, auto &) {
            if (_internal_sequence < static_cast<uint8_t>(_config.packets_per_test)) {
                _internal_sequence++;
                _timeout_timer->stop();
                _timeout_timer->start(uvw::TimerHandle::Time{_config.timeout_msec}, uvw::TimerHandle::Time{0});
                _send_icmp_v4(_internal_sequence);
            }
        });

        _recv_connection = PingReceiver::recv_signal.connect([this](pcpp::Packet &packet, timespec stamp) { _recv(packet, TestType::Ping, _name, stamp); });

        (_sequence == UCHAR_MAX) ? _sequence = 0 : _sequence++;
        _send_icmp_v4(_internal_sequence);
        _internal_sequence++;
        _timeout_timer->start(uvw::TimerHandle::Time{_config.timeout_msec}, uvw::TimerHandle::Time{0});
        _internal_timer->start(uvw::TimerHandle::Time{_config.packets_interval_msec}, uvw::TimerHandle::Time{_config.packets_interval_msec});
    });

    ++sock_count;
    _interval_timer->start(uvw::TimerHandle::Time{0}, uvw::TimerHandle::Time{_config.interval_msec});
    _init = true;
    return true;
}

bool PingProbe::stop()
{
    if (_interval_timer) {
        _interval_timer->stop();
    }
    _recv_connection.disconnect();
    _close_socket();
    return true;
}

std::optional<ErrorType> PingProbe::_get_addr()
{
    if (_ip_set) {
        return std::nullopt;
    }

    // don't need dns resolution
    if (_dns.empty()) {
        if (_ip.isIPv4()) {
            uint32_t ip_int(_ip.getIPv4().toInt());
            memcpy(&_sa.sin_addr, &ip_int, sizeof(_sa.sin_addr));
            _sa.sin_family = AF_INET;
            _sin_length = sizeof(_sa);
            _ip_set = true;
            return std::nullopt;
        } else {
            _is_ipv6 = true;
            auto ip_bytes = _ip.getIPv6().toBytes();
            for (int i = 0; i < 16; ++i) {
                _sa6.sin6_addr.s6_addr[i] = ip_bytes[i];
            }
            _sa6.sin6_family = AF_INET6;
            _sin_length = sizeof(_sa6);
            _ip_set = true;
            return std::nullopt;
        }
    }

    // do Dns lookup for interval loop
    auto request = _io_loop->resource<uvw::GetAddrInfoReq>();
    auto response = request->nodeAddrInfoSync(_dns);
    if (!response.first) {
        return ErrorType::DnsLookupFailure;
    }

    auto addr = response.second.get();
    while (addr->ai_next != nullptr) {
        if (addr->ai_family == AF_INET) {
            memcpy(&_sa, reinterpret_cast<sockaddr_in *>(addr->ai_addr), sizeof(struct sockaddr_in));
            _sin_length = sizeof(_sa);
            _sa.sin_family = AF_INET;
            return std::nullopt;
        } else if (addr->ai_family == AF_INET6) {
            memcpy(&_sa6, reinterpret_cast<sockaddr_in6 *>(addr->ai_addr), sizeof(struct sockaddr_in6));
            _sin_length = sizeof(_sa6);
            _sa6.sin6_family = AF_INET6;
        }
        addr = addr->ai_next;
    }
    return ErrorType::InvalidIp;
}

std::optional<ErrorType> PingProbe::_create_socket()
{
    if (_sock != INVALID_SOCKET) {
        return std::nullopt;
    }

    int domain = AF_INET;
    if (_is_ipv6) {
        domain = AF_INET6;
    }

    _sock = socket(domain, SOCK_RAW, IPPROTO_ICMP);
#ifdef _WIN32
    if (_sock == INVALID_SOCKET) {
        return ErrorType::SocketError;
    }
    unsigned long flag = 1;
    if (ioctlsocket(_sock, FIONBIO, &flag) == SOCKET_ERROR) {
        return ErrorType::SocketError;
    }
#else
    if (_sock == INVALID_SOCKET) {
        _sock = socket(domain, SOCK_DGRAM, IPPROTO_ICMP);
    }
    int flag = 1;
    if ((flag = fcntl(_sock, F_GETFL, 0)) == SOCKET_ERROR) {
        return ErrorType::SocketError;
    }
    if (fcntl(_sock, F_SETFL, flag | O_NONBLOCK) == SOCKET_ERROR) {
        return ErrorType::SocketError;
    }
#endif

    return std::nullopt;
}

void PingProbe::_send_icmp_v4(uint8_t sequence)
{
    auto icmp = pcpp::IcmpLayer();
    timespec stamp;
    std::timespec_get(&stamp, TIME_UTC);
    const uint64_t stamp64 = stamp.tv_sec * 1000000000ULL + stamp.tv_nsec;
    icmp.setEchoRequestData(_id, (static_cast<uint16_t>(_sequence) << 8) | sequence, stamp64, _payload_array.data(), _payload_array.size());
    icmp.computeCalculateFields();
    int rc = sendto(_sock, reinterpret_cast<char *>(icmp.getData()), icmp.getDataLen(), 0, reinterpret_cast<sockaddr *>(&_sa), _sin_length);
    if (rc != SOCKET_ERROR) {
        pcpp::Packet packet;
        packet.addLayer(&icmp);
        _send(packet, TestType::Ping, _name, stamp);
    }
}

void PingProbe::_close_socket()
{
    if (--sock_count; sock_count) {
        return;
    }
#ifdef _WIN32
    closesocket(_sock);
#else
    close(_sock);
#endif
    _sock = INVALID_SOCKET;
}
}
