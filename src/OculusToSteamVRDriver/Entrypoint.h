#pragma once

#include <memory>
#include "Driver.h"

extern "C" __declspec(dllexport) void* HmdDriverFactory(const char* interface_name, int* return_code);

namespace OculusToSteamVR_Driver
{
    std::shared_ptr<OculusToSteamVR_Driver::Driver> GetDriver();
}
