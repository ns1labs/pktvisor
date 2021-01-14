#include "PcapInputModulePlugin.h"
#include <Corrade/PluginManager/AbstractManager.h>
#include <Corrade/Utility/FormatStl.h>

CORRADE_PLUGIN_REGISTER(PcapInput, pktvisor::input::PcapInputModulePlugin,
    "com.ns1.module.input/1.0")

namespace pktvisor {
namespace input {

void PcapInputModulePlugin::_setup_routes(HttpServer &svr)
{

    // CREATE
    svr.Post("/api/v1/inputs/pcap", [this](const httplib::Request &req, httplib::Response &res) {
        json result;
        try {
            auto body = json::parse(req.body);
            std::unordered_map<std::string, std::string> schema = {
                {"name", "\\w+"},
                {"iface", "\\w+"}};
            try {
                _check_schema(body, schema);
            } catch (const SchemaException &e) {
                res.status = 400;
                result["error"] = e.what();
                res.set_content(result.dump(), "text/json");
                return;
            }
            if (_input_manager->exists(body["name"])) {
                res.status = 400;
                result["error"] = "input name already exists";
                res.set_content(result.dump(), "text/json");
                return;
            }
            std::string bpf;
            if (body.contains("bpf")) {
                bpf = body["bpf"];
            }
            op_create(body["name"], body["iface"], bpf);
            result["name"] = body["name"];
            result["iface"] = body["iface"];
            res.set_content(result.dump(), "text/json");
        } catch (const std::exception &e) {
            res.status = 500;
            result["error"] = e.what();
            res.set_content(result.dump(), "text/json");
        }
    });

    // DELETE
    svr.Delete("/api/v1/inputs/pcap/(\\w+)", [this](const httplib::Request &req, httplib::Response &res) {
        json result;
        try {
            auto name = req.matches[1];
            if (!_input_manager->exists(name)) {
                res.status = 404;
                result["result"] = "input name does not exist";
                res.set_content(result.dump(), "text/json");
                return;
            }
            auto [input_stream, lock] = _input_manager->get_module(name);
            assert(input_stream);
            auto count = input_stream->consumer_count();
            if (count) {
                res.status = 400;
                result["error"] = Corrade::Utility::formatString("input stream has existing consumers ({}), remove them first", count);
                res.set_content(result.dump(), "text/json");
                return;
            }
            lock.unlock();
            op_delete(name);
            res.set_content(result.dump(), "text/json");
        } catch (const std::exception &e) {
            res.status = 500;
            result["result"] = e.what();
            res.set_content(result.dump(), "text/json");
        }
    });
}
void PcapInputModulePlugin::op_create(const std::string &name, const std::string &iface, const std::string &bpf)
{
    auto input_stream = std::make_unique<PcapInputStream>(name);
    input_stream->set_config("iface", iface);
    input_stream->set_config("bpf", bpf);
    input_stream->start();
    _input_manager->add_module(name, std::move(input_stream));
}

void PcapInputModulePlugin::op_delete(const std::string &name)
{
    auto [input_stream, stream_lock] = _input_manager->get_module(name);
    assert(input_stream);
    if (input_stream->consumer_count()) {
        throw std::runtime_error("unable to remove, stream has consumers");
    }
    input_stream->stop();
    stream_lock.unlock();
    _input_manager->remove_module(name);
}

}
}