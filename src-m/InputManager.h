#ifndef PKTVISORD_INPUTMANAGER_H
#define PKTVISORD_INPUTMANAGER_H

#include "StreamInput.h"
#include <cpp-httplib/httplib.h>

#include <vector>

namespace pktvisor {

class InputManager
{
    std::vector<StreamInput> _inputs;

public:
    InputManager()
        : _inputs()
    {
    }
};

}

#endif //PKTVISORD_INPUTMANAGER_H
