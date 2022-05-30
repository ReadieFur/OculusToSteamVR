#include "VRDriver.hpp"
#include <HMDDevice.hpp>
#include <TrackerDevice.hpp>
#include <ControllerDevice.hpp>
#include <TrackingReferenceDevice.hpp>
#include <Windows.h>
#include <mutex>
#include <thread>
#include <OVR_CAPI.h>

vr::EVRInitError OculusToSteamVR::VRDriver::InitSharedData()
{
    HANDLE hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE, //Use paging file.
        NULL, //Default security.
        PAGE_READWRITE, //Read/write access.
        0, //Maximum object size (high-order DWORD).
        sizeof(SharedData), //Maximum object size (low-order DWORD).
        L"Local\\ovr_client_shared_data" //Name of mapping object.
    );
    if (hMapFile == NULL)
    {
        Log("Could not open file mapping object " + GetLastError());
        return vr::VRInitError_IPC_SharedStateInitFailed;
    }

    sharedBuffer = new(MapViewOfFile(
        hMapFile, //Handle to map object.
        FILE_MAP_ALL_ACCESS,  //Read/write permission.
        0,
        0,
        sizeof(SharedData)))SharedData();
    if (sharedBuffer == NULL)
    {
        Log("Could not map view of file " + GetLastError());
        CloseHandle(hMapFile);
        return vr::VRInitError_IPC_SharedStateInitFailed;
    }

    HANDLE sharedMutex = CreateMutexW(0, true, L"Local\\ovr_client_shared_mutex");
    WaitForSingleObject(
        sharedMutex,    // handle to mutex
        INFINITE);  // no time-out interval
    ReleaseMutex(sharedMutex);

    return vr::VRInitError_None;
}

vr::EVRInitError OculusToSteamVR::VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    // Perform driver context initialisation
    if (vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext); init_error != vr::EVRInitError::VRInitError_None) return init_error;

    Log("Activating OculusToSteamVR...");

    //Setup the shared objects.
    InitSharedData();

    vr::EVRSettingsError settingsError;
    //Add the hmd as a tracked device (if specified in the config).
    if (vr::VRSettings()->GetBool(settings_key_.c_str(), "track_hmd", &settingsError)) this->AddDevice(std::make_shared<TrackerDevice>("oculus_hmd"));
    if (settingsError == vr::VRSettingsError_UnsetSettingHasNoDefault) vr::VRSettings()->SetBool(settings_key_.c_str(), "track_hmd", false);

    //Add the controllers as trackers (if specified).
    if (vr::VRSettings()->GetBool(settings_key_.c_str(), "controllers_as_trackers", &settingsError))
        for (int i = 0; i < 2; i++) this->AddDevice(std::make_shared<TrackerDevice>("oculus_controller" + std::to_string(i)));
    else for (int i = 0; i < 2; i++) this->AddDevice(std::make_shared<ControllerDevice>("oculus_controller" + std::to_string(i)));
    if (settingsError == vr::VRSettingsError_UnsetSettingHasNoDefault) vr::VRSettings()->SetBool(settings_key_.c_str(), "controllers_as_trackers", true);

    Log("OculusToSteamVR Loaded Successfully");

	return vr::VRInitError_None;
}

void OculusToSteamVR::VRDriver::Cleanup()
{
}

void OculusToSteamVR::VRDriver::RunFrame()
{
    //Collect events.
    vr::VREvent_t event;
    std::vector<vr::VREvent_t> events;
    while (vr::VRServerDriverHost()->PollNextEvent(&event, sizeof(event))) events.push_back(event);
    this->openvr_events_ = events;

    //Update frame timing.
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    this->frame_timing_ = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_frame_time_);
    this->last_frame_time_ = now;

    //Update devices.
    for (auto& device : this->devices_)
    {
        std::string deviceSerial = device->GetSerial();
        ovrPosef pose;

        if (deviceSerial == "oculus_hmd")
        {
            //HMD Update.
            pose = sharedBuffer->oTrackingState.HeadPose.ThePose;
        }
        else if (deviceSerial.rfind("oculus_controller", 0) == 0)
        {
            //Controller update.
            //TODO: Modify the interface to store the device type index and inherit similar code to reduce duplication.
            pose = sharedBuffer->oTrackingState.HandPoses[atoi(deviceSerial.substr(17).c_str()) == 0 ? 0 : 1].ThePose;
        }

        device->Update(pose);
        //TODO: Objects.
    }
}

bool OculusToSteamVR::VRDriver::ShouldBlockStandbyMode()
{
    return false;
}

void OculusToSteamVR::VRDriver::EnterStandby()
{
}

void OculusToSteamVR::VRDriver::LeaveStandby()
{
}

std::vector<std::shared_ptr<OculusToSteamVR::IVRDevice>> OculusToSteamVR::VRDriver::GetDevices()
{
    return this->devices_;
}

std::vector<vr::VREvent_t> OculusToSteamVR::VRDriver::GetOpenVREvents()
{
    return this->openvr_events_;
}

std::chrono::milliseconds OculusToSteamVR::VRDriver::GetLastFrameTime()
{
    return this->frame_timing_;
}

bool OculusToSteamVR::VRDriver::AddDevice(std::shared_ptr<IVRDevice> device)
{
    vr::ETrackedDeviceClass openvr_device_class;
    // Remember to update this switch when new device types are added
    switch (device->GetDeviceType())
    {
        case DeviceType::CONTROLLER:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
            break;
        case DeviceType::HMD:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_HMD;
            break;
        case DeviceType::TRACKER:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
            break;
        case DeviceType::TRACKING_REFERENCE:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference;
            break;
        default:
            return false;
    }
    bool result = vr::VRServerDriverHost()->TrackedDeviceAdded(device->GetSerial().c_str(), openvr_device_class, device.get());
    if(result) this->devices_.push_back(device);
    return result;
}

OculusToSteamVR::SettingsValue OculusToSteamVR::VRDriver::GetSettingsValue(std::string key)
{
    vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None;
    int int_value = vr::VRSettings()->GetInt32(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) return int_value;
    err = vr::EVRSettingsError::VRSettingsError_None;
    float float_value = vr::VRSettings()->GetFloat(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) return float_value;
    err = vr::EVRSettingsError::VRSettingsError_None;
    bool bool_value = vr::VRSettings()->GetBool(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) return bool_value;
    std::string str_value;
    str_value.reserve(1024);
    vr::VRSettings()->GetString(settings_key_.c_str(), key.c_str(), str_value.data(), 1024, &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None)  return str_value;
    err = vr::EVRSettingsError::VRSettingsError_None;

    return SettingsValue();
}

void OculusToSteamVR::VRDriver::Log(std::string message)
{
    std::string message_endl = message + "\n";
    vr::VRDriverLog()->Log(message_endl.c_str());
}

vr::IVRDriverInput* OculusToSteamVR::VRDriver::GetInput()
{
    return vr::VRDriverInput();
}

vr::CVRPropertyHelpers* OculusToSteamVR::VRDriver::GetProperties()
{
    return vr::VRProperties();
}

vr::IVRServerDriverHost* OculusToSteamVR::VRDriver::GetDriverHost()
{
    return vr::VRServerDriverHost();
}
