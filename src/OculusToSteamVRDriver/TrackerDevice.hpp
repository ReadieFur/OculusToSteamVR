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
            TrackerDevice(std::string serial, OculusDeviceType oculusDeviceType);
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
        vr::TrackedDeviceIndex_t device_index_ = vr::k_unTrackedDeviceIndexInvalid;
        std::string serial_;
        OculusDeviceType oculus_device_type_;

        vr::DriverPose_t last_pose_ = IVRDevice::MakeDefaultPose();
    };
};