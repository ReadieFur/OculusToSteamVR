#pragma once

#include <chrono>
#include <cmath>

#include <linalg.h>

#include <Driver/IVRDevice.hpp>
#include <Native/DriverFactory.hpp>

namespace OculusToSteamVR {
    class ControllerDevice : public IVRDevice {
        public:

            ControllerDevice(std::string serial, Handedness handedness = Handedness::ANY);
            ~ControllerDevice() = default;

            // Inherited via IVRDevice
            virtual void Enable() override;
            virtual void Disable() override;

            virtual std::string GetSerial() override;
            virtual void Update() override;
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

        vr::DriverPose_t last_pose_;

        bool did_identify_ = false;
        float identify_anim_state_ = 0.0f;
        int calibrationSamples = 0;
        ovrPosef calibrationSampleData;

        //Extern issue or something, wasn't bothered to fix so I placed the variables in the ControllerDevice.cpp file.
        //static bool isManuallyCalibrating;
        //static float calibrationButtonTime;

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