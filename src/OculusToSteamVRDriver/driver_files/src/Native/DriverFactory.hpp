#pragma once

#include <cstdlib>
#include <memory>

#include <openvr_driver.h>

#include <Driver/IVRDriver.hpp>

extern "C" __declspec(dllexport) void* HmdDriverFactory(const char* interface_name, int* return_code);

namespace OculusToSteamVRDriver {
    std::shared_ptr<OculusToSteamVRDriver::IVRDriver> GetDriver();
}