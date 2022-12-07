/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DnstapInputStream.h"
#include "DnstapException.h"
#include "ThreadName.h"
#include <filesystem>
#include <uvw/async.h>
#include <uvw/loop.h>
#include <uvw/pipe.h>
#include <uvw/stream.h>
#include <uvw/tcp.h>
#include <uvw/timer.h>

namespace visor::input::dnstap {

DnstapInputStream::DnstapInputStream(const std::string &name)
    : visor::InputStream(name)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    _logger = spdlog::get("visor");
    assert(_logger);
}

void DnstapInputStream::_read_frame_stream_file()
{
    assert(config_exists("dnstap_file"));

#ifndef _WIN32
    // Setup file reader options
    auto fileOptions = fstrm_file_options_init();
    fstrm_file_options_set_file_path(fileOptions, config_get<std::string>("dnstap_file").c_str());

    // Initialize file reader
    auto reader = fstrm_file_reader_init(fileOptions, nullptr);
    if (!reader) {
        throw DnstapException("fstrm_file_reader_init() failed");
    }
    auto result = fstrm_reader_open(reader);
    if (result != fstrm_res_success) {
        throw DnstapException("fstrm_reader_open() failed");
    }

    // Cleanup
    fstrm_file_options_destroy(&fileOptions);

    // Loop over data frames
    for (;;) {
        const uint8_t *data;
        size_t len_data;

        result = fstrm_reader_read(reader, &data, &len_data);
        if (result == fstrm_res_success) {
            // Data frame ready, parse protobuf
            ::dnstap::Dnstap d;
            if (!d.ParseFromArray(data, len_data)) {
                _logger->warn("Dnstap::ParseFromArray fail, skipping frame of size {}", len_data);
                continue;
            }
            if (!d.has_type() || d.type() != ::dnstap::Dnstap_Type_MESSAGE || !d.has_message()) {
                _logger->warn("dnstap data is wrong type or has no message, skipping frame of size {}", len_data);
                continue;
            }

            // Emit signal to handlers
            if (!_filtering(d)) {
                std::shared_lock lock(_input_mutex);
                for (auto &proxy : _event_proxies) {
                    static_cast<DnstapInputEventProxy *>(proxy.get())->dnstap_cb(d, len_data);
                }
            }
        } else if (result == fstrm_res_stop) {
            // Normal end of data stream
            break;
        } else {
            // Abnormal end
            _logger->warn("fstrm_reader_read() data stream ended abnormally: {}", result);
            break;
        }
    }

    fstrm_reader_destroy(&reader);
#endif
}

void DnstapInputStream::start()
{

    if (_running) {
        return;
    }

    validate_configs(_config_defs);

    if (config_exists("dnstap_file")) {
        // read from dnstap file. this is a special case from a command line utility
        _running = true;
        _read_frame_stream_file();
        return;
    } else if (config_exists("socket")) {
        _create_frame_stream_unix_socket();
    } else if (config_exists("tcp")) {
        _create_frame_stream_tcp_socket();
    } else {
        throw DnstapException("config must specify one of: socket, dnstap_file");
    }

    _running = true;
}

void DnstapInputStream::_create_frame_stream_tcp_socket()
{
    assert(config_exists("tcp"));

    // split address and port
    auto tcp_config = config_get<std::string>("tcp");
    if (tcp_config.find(':') == std::string::npos) {
        throw DnstapException("invalid tcp address specification, use HOST:PORT");
    }

    std::string host;
    unsigned int port;
    try {
        host = tcp_config.substr(0, tcp_config.find(':'));
        port = std::stoul(tcp_config.substr(tcp_config.find(':') + 1, std::string::npos));
    } catch (std::exception &err) {
        throw DnstapException("unable to parse tcp address specification, use HOST:PORT");
    }

    // main io loop, run in its own thread
    _io_loop = uvw::Loop::create();
    if (!_io_loop) {
        throw DnstapException("unable to create io loop");
    }
    // AsyncHandle lets us stop the loop from its own thread
    _async_h = _io_loop->resource<uvw::AsyncHandle>();
    if (!_async_h) {
        throw DnstapException("unable to initialize AsyncHandle");
    }
    _async_h->once<uvw::AsyncEvent>([this](const auto &, auto &handle) {
        _timer->stop();
        _timer->close();
        _tcp_server_h->stop();
        _tcp_server_h->close();
        _io_loop->stop();
        _io_loop->close();
        handle.close();
    });
    _async_h->on<uvw::ErrorEvent>([this](const auto &err, auto &handle) {
        _logger->error("[{}] AsyncEvent error: {}", _name, err.what());
        handle.close();
    });

    _timer = _io_loop->resource<uvw::TimerHandle>();
    if (!_timer) {
        throw DnstapException("unable to initialize TimerHandle");
    }
    _timer->on<uvw::TimerEvent>([this](const auto &, auto &) {
        timespec stamp;
        // use now()
        std::timespec_get(&stamp, TIME_UTC);
        std::shared_lock lock(_input_mutex);
        for (auto &proxy : _event_proxies) {
            static_cast<DnstapInputEventProxy *>(proxy.get())->heartbeat_cb(stamp);
        }
    });
    _timer->on<uvw::ErrorEvent>([this](const auto &err, auto &handle) {
        _logger->error("[{}] TimerEvent error: {}", _name, err.what());
        handle.close();
    });

    // setup server socket
    _tcp_server_h = _io_loop->resource<uvw::TCPHandle>();
    if (!_tcp_server_h) {
        throw DnstapException("unable to initialize server PipeHandle");
    }

    _tcp_server_h->on<uvw::ErrorEvent>([this](const auto &err, auto &) {
        _logger->error("[{}] socket error: {}", _name, err.what());
        throw DnstapException(err.what());
    });

    // ListenEvent happens on client connection
    _tcp_server_h->on<uvw::ListenEvent>([this](const uvw::ListenEvent &, uvw::TCPHandle &) {
        auto client = _io_loop->resource<uvw::TCPHandle>();
        if (!client) {
            throw DnstapException("unable to initialize connected client TCPHandle");
        }

        auto on_data_frame = [this](const void *data, std::size_t len_data) {
            // Data frame ready, parse protobuf
            ::dnstap::Dnstap d;
            if (!d.ParseFromArray(data, len_data)) {
                _logger->warn("Dnstap::ParseFromArray fail, skipping frame of size {}", len_data);
                return;
            }
            if (!d.has_type() || d.type() != ::dnstap::Dnstap_Type_MESSAGE || !d.has_message()) {
                _logger->warn("dnstap data is wrong type or has no message, skipping frame of size {}", len_data);
                return;
            }

            // Emit signal to handlers
            if (!_filtering(d)) {
                std::shared_lock lock(_input_mutex);
                for (auto &proxy : _event_proxies) {
                    static_cast<DnstapInputEventProxy *>(proxy.get())->dnstap_cb(d, len_data);
                }
            }
        };

        client->on<uvw::ErrorEvent>([this](const uvw::ErrorEvent &err, uvw::TCPHandle &c_sock) {
            _logger->error("[{}]: dnstap client socket error: {}", _name, err.what());
            c_sock.stop();
            c_sock.close();
        });

        // client sent data
        client->on<uvw::DataEvent>([this](const uvw::DataEvent &data, uvw::TCPHandle &c_sock) {
            assert(_tcp_sessions[c_sock.fd()]);
            try {
                _tcp_sessions[c_sock.fd()]->receive_socket_data(reinterpret_cast<uint8_t *>(data.data.get()), data.length);
            } catch (DnstapException &err) {
                _logger->error("[{}] dnstap client read error: {}", _name, err.what());
                c_sock.stop();
                c_sock.close();
            }
        });
        // client was closed
        client->on<uvw::CloseEvent>([this](const uvw::CloseEvent &, uvw::TCPHandle &c_sock) {
            _logger->info("[{}]: dnstap client disconnected", _name);
            _tcp_sessions.erase(c_sock.fd());
        });
        // client read EOF
        client->on<uvw::EndEvent>([this](const uvw::EndEvent &, uvw::TCPHandle &c_sock) {
            _logger->info("[{}]: dnstap client EOF {}", _name, c_sock.peer().ip);
            c_sock.stop();
            c_sock.close();
        });

        _tcp_server_h->accept(*client);
        _logger->info("[{}]: dnstap client connected {}", _name, client->peer().ip);
        _tcp_sessions[client->fd()] = std::make_unique<FrameSessionData<uvw::TCPHandle>>(client, CONTENT_TYPE, on_data_frame);
        client->read();
    });

    _logger->info("[{}]: opening dnstap server on {}", _name, config_get<std::string>("tcp"));
    _tcp_server_h->bind(host, port);
    _tcp_server_h->listen();

    // spawn the loop
    _io_thread = std::make_unique<std::thread>([this] {
        _timer->start(uvw::TimerHandle::Time{1000}, uvw::TimerHandle::Time{HEARTBEAT_INTERVAL * 1000});
        thread::change_self_name(schema_key(), name());
        _io_loop->run();
    });
}
void DnstapInputStream::_create_frame_stream_unix_socket()
{
    assert(config_exists("socket"));

    // main io loop, run in its own thread
    _io_loop = uvw::Loop::create();
    if (!_io_loop) {
        throw DnstapException("unable to create io loop");
    }
    // AsyncHandle lets us stop the loop from its own thread
    _async_h = _io_loop->resource<uvw::AsyncHandle>();
    if (!_async_h) {
        throw DnstapException("unable to initialize AsyncHandle");
    }
    _async_h->once<uvw::AsyncEvent>([this](const auto &, auto &handle) {
        _timer->stop();
        _timer->close();
        _unix_server_h->stop();
        _unix_server_h->close();
        _io_loop->stop();
        _io_loop->close();
        handle.close();
    });
    _async_h->on<uvw::ErrorEvent>([this](const auto &err, auto &handle) {
        _logger->error("[{}] AsyncEvent error: {}", _name, err.what());
        handle.close();
    });

    _timer = _io_loop->resource<uvw::TimerHandle>();
    if (!_timer) {
        throw DnstapException("unable to initialize TimerHandle");
    }
    _timer->on<uvw::TimerEvent>([this](const auto &, auto &) {
        timespec stamp;
        // use now()
        std::timespec_get(&stamp, TIME_UTC);
        std::shared_lock lock(_input_mutex);
        for (auto &proxy : _event_proxies) {
            static_cast<DnstapInputEventProxy *>(proxy.get())->heartbeat_cb(stamp);
        }
    });
    _timer->on<uvw::ErrorEvent>([this](const auto &err, auto &handle) {
        _logger->error("[{}] TimerEvent error: {}", _name, err.what());
        handle.close();
    });

    // setup server socket
    _unix_server_h = _io_loop->resource<uvw::PipeHandle>();
    if (!_unix_server_h) {
        throw DnstapException("unable to initialize server PipeHandle");
    }

    _unix_server_h->on<uvw::ErrorEvent>([this](const auto &err, auto &) {
        _logger->error("[{}] socket error: {}", _name, err.what());
        throw DnstapException(err.what());
    });

    // ListenEvent happens on client connection
    _unix_server_h->on<uvw::ListenEvent>([this](const uvw::ListenEvent &, uvw::PipeHandle &) {
        auto client = _io_loop->resource<uvw::PipeHandle>();
        if (!client) {
            throw DnstapException("unable to initialize connected client PipeHandle");
        }

        auto on_data_frame = [this](const void *data, std::size_t len_data) {
            // Data frame ready, parse protobuf
            ::dnstap::Dnstap d;
            if (!d.ParseFromArray(data, len_data)) {
                _logger->warn("Dnstap::ParseFromArray fail, skipping frame of size {}", len_data);
                return;
            }
            if (!d.has_type() || d.type() != ::dnstap::Dnstap_Type_MESSAGE || !d.has_message()) {
                _logger->warn("dnstap data is wrong type or has no message, skipping frame of size {}", len_data);
                return;
            }
            // Emit signal to handlers
            if (!_filtering(d)) {
                std::shared_lock lock(_input_mutex);
                for (auto &proxy : _event_proxies) {
                    static_cast<DnstapInputEventProxy *>(proxy.get())->dnstap_cb(d, len_data);
                }
            }
        };

        client->on<uvw::ErrorEvent>([this](const uvw::ErrorEvent &err, uvw::PipeHandle &c_sock) {
            _logger->error("[{}]: dnstap client socket error: {}", _name, err.what());
            c_sock.stop();
            c_sock.close();
        });

        // client sent data
        client->on<uvw::DataEvent>([this](const uvw::DataEvent &data, uvw::PipeHandle &c_sock) {
            assert(_unix_sessions[c_sock.fd()]);
            try {
                _unix_sessions[c_sock.fd()]->receive_socket_data(reinterpret_cast<uint8_t *>(data.data.get()), data.length);
            } catch (DnstapException &err) {
                _logger->error("[{}] dnstap client read error: {}", _name, err.what());
                c_sock.stop();
                c_sock.close();
            }
        });
        // client was closed
        client->on<uvw::CloseEvent>([this](const uvw::CloseEvent &, uvw::PipeHandle &c_sock) {
            _logger->info("[{}]: dnstap client disconnected", _name);
            _unix_sessions.erase(c_sock.fd());
        });
        // client read EOF
        client->on<uvw::EndEvent>([this](const uvw::EndEvent &, uvw::PipeHandle &c_sock) {
            _logger->info("[{}]: dnstap client EOF {}", _name, c_sock.fd());
            c_sock.stop();
            c_sock.close();
        });

        _unix_server_h->accept(*client);
        _logger->info("[{}]: dnstap client connected {}", _name, client->fd());
        _unix_sessions[client->fd()] = std::make_unique<FrameSessionData<uvw::PipeHandle>>(client, CONTENT_TYPE, on_data_frame);
        client->read();
    });

    // attempt to remove socket if it exists, ignore errors
    std::filesystem::remove(config_get<std::string>("socket"));

    _logger->info("[{}]: opening dnstap server on {}", _name, config_get<std::string>("socket"));
    _unix_server_h->bind(config_get<std::string>("socket"));
    _unix_server_h->listen();

    // spawn the loop
    _io_thread = std::make_unique<std::thread>([this] {
        _timer->start(uvw::TimerHandle::Time{1000}, uvw::TimerHandle::Time{HEARTBEAT_INTERVAL * 1000});
        thread::change_self_name(schema_key(), name());
        _io_loop->run();
    });
}

void DnstapInputStream::stop()
{
    if (!_running) {
        return;
    }

    if (_async_h && _io_thread) {
        // we have to use AsyncHandle to stop the loop from the same thread the loop is running in
        _async_h->send();
        // waits for _io_loop->run() to return
        if (_io_thread->joinable()) {
            _io_thread->join();
        }
    }

    _running = false;
}

void DnstapInputStream::info_json(json &j) const
{
    common_info_json(j);
}

std::unique_ptr<InputEventProxy> DnstapInputStream::create_event_proxy(const Configurable &filter)
{
    return std::make_unique<DnstapInputEventProxy>(_name, filter);
}
}
