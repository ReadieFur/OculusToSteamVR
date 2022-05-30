#pragma once

#include <chrono>
#include <cmath>

#include <linalg.h>

#include <IVRDevice.hpp>
#include <DriverFactory.hpp>

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

            ControllerDevice(std::string serial, Handedness handedness);
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
        vr::TrackedDeviceIndex_t device_index_ = vr::k_unTrackedDeviceIndexInvalid;
        std::string serial_;
        Handedness handedness_;
        ovrHandType oHandType_;

        vr::DriverPose_t last_pose_;

        bool did_vibrate_ = false;
        float vibrate_anim_state_ = 0.f;

        vr::VRInputComponentHandle_t haptic_component_ = 0;

        vr::VRInputComponentHandle_t a_button_click_component_ = 0;
        vr::VRInputComponentHandle_t a_button_touch_component_ = 0;

        vr::VRInputComponentHandle_t b_button_click_component_ = 0;
        vr::VRInputComponentHandle_t b_button_touch_component_ = 0;

        vr::VRInputComponentHandle_t x_button_click_component_ = 0;
        vr::VRInputComponentHandle_t x_button_touch_component_ = 0;

        vr::VRInputComponentHandle_t y_button_click_component_ = 0;
        vr::VRInputComponentHandle_t y_button_touch_component_ = 0;

        vr::VRInputComponentHandle_t trigger_value_component_ = 0;
        vr::VRInputComponentHandle_t trigger_click_component_ = 0;
        vr::VRInputComponentHandle_t trigger_touch_component_ = 0;

        vr::VRInputComponentHandle_t grip_value_component_ = 0;

        vr::VRInputComponentHandle_t system_click_component_ = 0;

        vr::VRInputComponentHandle_t joystick_click_component_ = 0;
        vr::VRInputComponentHandle_t joystick_touch_component_ = 0;
        vr::VRInputComponentHandle_t joystick_x_component_ = 0;
        vr::VRInputComponentHandle_t joystick_y_component_ = 0;

        //vr::VRInputComponentHandle_t skeleton_left_component_ = 0;
        //vr::VRInputComponentHandle_t skeleton_right_component_ = 0;
    };
};