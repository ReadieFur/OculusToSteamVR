#pragma once

#include <chrono>
#include <cmath>

#include <linalg.h>

#include "IVRDevice.hpp"
#include "DriverFactory.hpp"

#include "Helpers.hpp"

namespace OculusToSteamVR
{
    class ControllerDevice : public IVRDevice
    {
        public:
            enum class Handedness
            {
                LEFT,
                RIGHT
            };

            ControllerDevice(unsigned int index, Handedness handedness);
            ~ControllerDevice() = default;

            //Inherited via IVRDevice.
            virtual std::string GetSerial() override;
            virtual void Update(SharedData* sharedBuffer) override;
            virtual vr::TrackedDeviceIndex_t GetDeviceIndex() override;
            virtual DeviceType GetDeviceType() override;
            virtual Handedness GetHandedness();

            virtual vr::EVRInitError Activate(uint32_t unObjectId) override;
            virtual void Deactivate() override;
            virtual void EnterStandby() override;
            virtual void* GetComponent(const char* pchComponentNameAndVersion) override;
            virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override;
            virtual vr::DriverPose_t GetPose() override;

    private:
        //https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L872
        const ovrQuatf quatOffsets = { 0.3420201, 0, 0, 0.9396926 };
        const ovrVector3f rightVectorOffsets = { 0.00571, 0.04078, -0.03531 };
        const ovrVector3f vectorOffsets2 = { -0.000999998, -0.1, 0.0019 };

        vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
        std::string serial;
        Handedness handedness;
        ovrHandType oHandType;

        std::chrono::steady_clock::time_point lastVibrationTime;
        vr::VREvent_HapticVibration_t currentVibration;

        vr::DriverPose_t lastPose;

        vr::VRInputComponentHandle_t hapticComponent = 0;

        vr::VRInputComponentHandle_t aButtonClickComponent = 0;
        vr::VRInputComponentHandle_t aButtonTouchComponent = 0;

        vr::VRInputComponentHandle_t bButtonClickComponent = 0;
        vr::VRInputComponentHandle_t bButtonTouchComponent = 0;

        vr::VRInputComponentHandle_t xButtonClickComponent = 0;
        vr::VRInputComponentHandle_t xButtonTouchComponent = 0;

        vr::VRInputComponentHandle_t yButtonClickComponent = 0;
        vr::VRInputComponentHandle_t yButtonTouchComponent = 0;

        vr::VRInputComponentHandle_t triggerValueComponent = 0;
        vr::VRInputComponentHandle_t triggerClickComponent = 0;
        vr::VRInputComponentHandle_t triggerTouchComponent = 0;

        vr::VRInputComponentHandle_t gripValueComponent = 0;

        vr::VRInputComponentHandle_t systemClickComponent = 0;

        vr::VRInputComponentHandle_t joystickClickComponent = 0;
        vr::VRInputComponentHandle_t joystickTouchComponent = 0;
        vr::VRInputComponentHandle_t joystickXComponent = 0;
        vr::VRInputComponentHandle_t joystickYComponent = 0;
    };
};