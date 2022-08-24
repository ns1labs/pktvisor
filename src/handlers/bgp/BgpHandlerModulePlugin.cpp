/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BgpHandlerModulePlugin.h"
#include "CoreRegistry.h"
#include "BgpStreamHandler.h"
#include "HandlerManager.h"
#include "InputStreamManager.h"
#include <Corrade/PluginManager/AbstractManager.h>
#include <nlohmann/json.hpp>

CORRADE_PLUGIN_REGISTER(VisorHandlerBgp, visor::handler::bgp::BgpHandlerModulePlugin,
   "visor.module.handler/1.0")

namespace visor::handler::bgp {

using namespace visor::input::pcap;
using json = nlohmann::json;

void BgpHandlerModulePlugin::setup_routes(HttpServer *svr)
{
}
std::unique_ptr<StreamHandler> BgpHandlerModulePlugin::instantiate(const std::string &name, InputEventProxy *proxy, const Configurable *config, const Configurable *filter, StreamHandler *stream_handler)
{
   // TODO using config as both window config and module config
   auto handler_module = std::make_unique<BgpStreamHandler>(name, proxy, config, stream_handler);
   handler_module->config_merge(*config);
   handler_module->config_merge(*filter);
   return handler_module;
}

}