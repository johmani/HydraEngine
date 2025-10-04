module;

#define NOMINMAX

#include "HydraEngine/Base.h"
#include <taskflow/taskflow.hpp>

export module HE;

import std;
import nvrhi;

export {

    using std::uint8_t;
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    using std::size_t;
}

export namespace HE {

#define CPP_MODULE
#include "HydraEngine.h"

}
