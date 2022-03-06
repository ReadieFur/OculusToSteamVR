#include "TrackerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackerDevice::TrackerDevice(std::string serial):
    serial_(serial)
{
}

std::string OculusToSteamVR::TrackerDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::TrackerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;

    // Check if this device was asked to be identified
    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events) {
        // Note here, event.trackedDeviceIndex does not necissarily equal this->device_index_, not sure why, but the component handle will match so we can just use that instead
        //if (event.trackedDeviceIndex == this->device_index_) {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration) {
            if (event.data.hapticVibration.componentHandle == this->haptic_component_) {
                this->did_vibrate_ = true;
            }
        }
        //}
    }

    // Check if we need to keep vibrating
    if (this->did_vibrate_) {
        this->vibrate_anim_state_ += (GetDriver()->GetLastFrameTime().count()/1000.f);
        if (this->vibrate_anim_state_ > 1.0f) {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    // Setup pose for this frame
    auto pose = IVRDevice::MakeDefaultPose();

    // Find a HMD
    auto devices = GetDriver()->GetDevices();
    auto hmd = std::find_if(devices.begin(), devices.end(), [](const std::shared_ptr<IVRDevice>& device_ptr) {return device_ptr->GetDeviceType() == DeviceType::HMD; });
    if (hmd != devices.end()) {
        // Found a HMD
        vr::DriverPose_t hmd_pose = (*hmd)->GetPose();

        // Here we setup some transforms so our controllers are offset from the headset by a small amount so we can see them
        linalg::vec<float, 3> hmd_position{ (float)hmd_pose.vecPosition[0], (float)hmd_pose.vecPosition[1], (float)hmd_pose.vecPosition[2] };
        linalg::vec<float, 4> hmd_rotation{ (float)hmd_pose.qRotation.x, (float)hmd_pose.qRotation.y, (float)hmd_pose.qRotation.z, (float)hmd_pose.qRotation.w };

        // Do shaking animation if haptic vibration was requested
        float controller_y = -0.35f + 0.01f * std::sinf(8 * 3.1415f * vibrate_anim_state_);

        linalg::vec<float, 3> hmd_pose_offset = { 0.f, controller_y, -0.5f };

        hmd_pose_offset = linalg::qrot(hmd_rotation, hmd_pose_offset);

        linalg::vec<float, 3> final_pose = hmd_pose_offset + hmd_position;

        pose.vecPosition[0] = final_pose.x;
        pose.vecPosition[1] = final_pose.y;
        pose.vecPosition[2] = final_pose.z;

        pose.qRotation.w = hmd_rotation.w;
        pose.qRotation.x = hmd_rotation.x;
        pose.qRotation.y = hmd_rotation.y;
        pose.qRotation.z = hmd_rotation.z;
    }

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType OculusToSteamVR::TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::TrackerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError OculusToSteamVR::TrackerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracker " + this->serial_);

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Setup inputs and outputs
    GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic", &this->haptic_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click", &this->system_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/touch", &this->system_touch_component_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "example_tracker");

    // Opt out of hand selection
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);

    // Set up a render model path
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "{oculus_to_steamvr}/rendermodels/example_controller");

    // Set controller profile
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{oculus_to_steamvr}/input/example_tracker_bindings.json");

    // Set the icon
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{oculus_to_steamvr}/icons/tracker_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus_to_steamvr}/icons/tracker_not_ready.png");

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::TrackerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void OculusToSteamVR::TrackerDevice::EnterStandby()
{
}

void* OculusToSteamVR::TrackerDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void OculusToSteamVR::TrackerDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t OculusToSteamVR::TrackerDevice::GetPose()
{
    return last_pose_;
}
