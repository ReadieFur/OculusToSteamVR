#pragma once

#include <vector>
#include <memory>

#include <openvr_driver.h>

#include <Driver/IVRDriver.hpp>
#include <Driver/IVRDevice.hpp>

namespace OculusToSteamVR {
    class VRDriver : public IVRDriver {
    public:
        // Inherited via IVRDriver
        virtual std::vector<std::shared_ptr<IVRDevice>> GetDevices() override;
        virtual std::vector<vr::VREvent_t> GetOpenVREvents() override;
        virtual std::chrono::milliseconds GetLastFrameTime() override;
        virtual bool AddDevice(std::shared_ptr<IVRDevice> device) override;
        virtual SettingsValue GetSettingsValue(std::string key) override;
        virtual void Log(std::string message) override;

        virtual vr::IVRDriverInput* GetInput() override;
        virtual vr::CVRPropertyHelpers* GetProperties() override;
        virtual vr::IVRServerDriverHost* GetDriverHost() override;

        // Inherited via IServerTrackedDeviceProvider
        virtual vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
        virtual void Cleanup() override;
        virtual void RunFrame() override;
        virtual bool ShouldBlockStandbyMode() override;
        virtual void EnterStandby() override;
        virtual void LeaveStandby() override;
        virtual ~VRDriver() = default;

    private:
        std::vector<std::shared_ptr<IVRDevice>> devices_;
        std::vector<vr::VREvent_t> openvr_events_;
        std::chrono::milliseconds frame_timing_ = std::chrono::milliseconds(16);
        std::chrono::system_clock::time_point last_frame_time_ = std::chrono::system_clock::now();
        std::string settings_key_ = "driver_oculus_to_steamvr";

        bool oculusVRInitialized = false;
        bool inControllerMode = true;
        bool isManuallyCalibrating = false;
        bool isCalibratingPosition = true;
        float calibrationButtonTime = 1.0f;
        /*I'm storing the rotation offset in euler angles becuase when I was trying to load the values back in as quaternions
        I couldn't flip the rotation correctly for the left hand, very likley due to my limited knowledge with quaternions.*/
        OVR::Vector3<float> eulerAnglesOffset;
        float modeToggleButtonTime = 1.0f;

        float GetSettingsFloat(std::string key, float defaultValue);

        static bool OculusRenderLoop(
            bool retryCreate,
            std::shared_ptr<OculusToSteamVR::IVRDriver> driver,
            ovrSession session,
            ovrGraphicsLuid luid
        );
    };
};