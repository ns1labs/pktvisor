/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DnstapInputModulePlugin.h"
#include "CoreRegistry.h"
#include "InputStreamManager.h"
#include <Corrade/PluginManager/AbstractManager.h>
#include <Corrade/Utility/FormatStl.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
//Dnstap currently not supported on windows
#include "FakeDnstapInputStream.h"
#else
#include "DnstapInputStream.h"
#endif

CORRADE_PLUGIN_REGISTER(VisorInputDnstap, visor::input::dnstap::DnstapInputModulePlugin,
    "visor.module.input/1.0")

namespace visor::input::dnstap {

void DnstapInputModulePlugin::setup_routes([[maybe_unused]] HttpServer *svr)
{
}

std::unique_ptr<InputStream> DnstapInputModulePlugin::instantiate(const std::string name, const Configurable *config, const Configurable *filter)
{
    auto input_stream = std::make_unique<DnstapInputStream>(name);
    input_stream->config_merge(*config);
    input_stream->config_merge(*filter);
    return input_stream;
}

std::string DnstapInputModulePlugin::generate_input_name(std::string prefix, const Configurable &config, [[maybe_unused]] const Configurable &filter)
{
    return prefix + "-" + config.config_hash();
}
}
