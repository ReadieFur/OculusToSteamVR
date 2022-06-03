#pragma once

#include <chrono>
#include <cmath>

#include <linalg.h>

#include "IVRDevice.hpp"
#include "DriverFactory.hpp"
#include "OculusDeviceType.hpp"

namespace OculusToSteamVR
{
    class TrackerDevice : public IVRDevice
    {
        public:
            TrackerDevice(unsigned int index, OculusDeviceType oculusDeviceType);
            ~TrackerDevice() = default;

            //Inherited via IVRDevice.
            virtual std::string GetSerial() override;
            virtual void Update(SharedData* sharedBuffer) override;
            virtual vr::TrackedDeviceIndex_t GetDeviceIndex() override;
            virtual DeviceType GetDeviceType() override;

            virtual vr::EVRInitError Activate(uint32_t unObjectId) override;
            virtual void Deactivate() override;
            virtual void EnterStandby() override;
            virtual void* GetComponent(const char* pchComponentNameAndVersion) override;
            virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override;
            virtual vr::DriverPose_t GetPose() override;

    private:
        vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
        unsigned int index;
        std::string serial;
        OculusDeviceType oculusDeviceType;

        vr::DriverPose_t lastPose = IVRDevice::MakeDefaultPose();
    };
};