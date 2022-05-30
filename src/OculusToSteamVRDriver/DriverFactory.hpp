#pragma once

#include <cstdlib>
#include <memory>

#include <openvr_driver.h>

#include <IVRDriver.hpp>

extern "C" __declspec(dllexport) void* HmdDriverFactory(const char* interface_name, int* return_code);

namespace OculusToSteamVR {
    std::shared_ptr<OculusToSteamVR::IVRDriver> GetDriver();
}