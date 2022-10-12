/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <chrono>
#include <memory>
#include <robin_hood.h>

namespace visor::handler::netprobe {

struct NetProbeTransaction {
    std::string target;
    timespec requestTS;
    timespec totalTS;
};

class RequestReplyManager
{
    typedef robin_hood::unordered_map<std::string, NetProbeTransaction> NetProbeXactMap;

    unsigned int _ttl_secs;
    NetProbeXactMap _netprobe_transactions;

public:
    RequestReplyManager(unsigned int ttl_secs = 3)
        : _ttl_secs(ttl_secs)
    {
    }

    void start_transaction(uint16_t id, uint16_t sequence, timespec stamp, std::string target);

    std::pair<bool, NetProbeTransaction> maybe_end_transaction(std::string target, uint16_t id, uint16_t sequence, timespec stamp);

    size_t purge_old_transactions(timespec now);

    [[nodiscard]] NetProbeXactMap::size_type open_transaction_count() const
    {
        return _netprobe_transactions.size();
    }
};

}
