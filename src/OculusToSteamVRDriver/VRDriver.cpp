#include "VRDriver.hpp"
#include "TrackerDevice.hpp"
#include "ControllerDevice.hpp"
#include "TrackingReferenceDevice.hpp"
#include <Windows.h>
#include <mutex>
#include <thread>
#include <OVR_CAPI.h>

vr::EVRInitError OculusToSteamVR::VRDriver::InitSharedData()
{
    bool didCreateMapping = false;

    //Try to connect to an existing object.
    HANDLE hMapFile = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS, //Read/write access.
        FALSE, //Do not inherit the name.
        L"Local\\ovr_client_shared_data"); //Name of mapping object.
    if (hMapFile == NULL)
    {
        //If it failed/no object was available, try to create a new one.
        hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE, //Use paging file.
            NULL, //Default security.
            PAGE_READWRITE, //Read/write access.
            0, //Maximum object size (high-order DWORD).
            sizeof(SharedData), //Maximum object size (low-order DWORD).
            L"Local\\ovr_client_shared_data" //Name of mapping object.
        );
        if (hMapFile == NULL)
        {
            //If this failed then something went wrong.
            Log("Could not open file mapping object " + GetLastError());
            return vr::VRInitError_IPC_SharedStateInitFailed;
        }
    }
    else didCreateMapping = false;

    if (!didCreateMapping)
    {
        sharedBuffer = (SharedData*)MapViewOfFile(hMapFile, //Handle to map object.
            FILE_MAP_ALL_ACCESS, //Read/write permission.
            0,
            0,
            sizeof(SharedData));
    }
    else
    {
        sharedBuffer = new(MapViewOfFile(
            hMapFile, //Handle to map object.
            FILE_MAP_ALL_ACCESS, //Read/write permission.
            0,
            0,
            sizeof(SharedData)))SharedData();
    }

    if (sharedBuffer == NULL)
    {
        Log("Could not map view of file " + GetLastError());
        CloseHandle(hMapFile);
        return vr::VRInitError_IPC_SharedStateInitFailed;
    }

    //If we did create the mapping then we should be able create and be the initial owner of the mutex, if we didn't create the mapping then we shouldn't be the owner of the mutex.
    sharedMutex = CreateMutexW(0, didCreateMapping, L"Local\\ovr_client_shared_mutex");
    //No need to wait here.
    if (WaitForSingleObject(sharedMutex, 1000) != WAIT_OBJECT_0)
    {
        Log("Could not lock mutex after specified interval " + GetLastError());
        return vr::VRInitError_IPC_MutexInitFailed;
    }
    ReleaseMutex(sharedMutex);

    return vr::VRInitError_None;
}

//Wasn't sure why I couldn't inline this but oh well :).
bool OculusToSteamVR::VRDriver::IsClientAlive(std::chrono::steady_clock::time_point clientTime)
{
    //If the client hasnt updated it's time for more than x seconds then we can assume that it is inactive.
    return clientTime > std::chrono::high_resolution_clock::now() - std::chrono::seconds{ 1 };
}

void OculusToSteamVR::VRDriver::SetupDevices(bool acquireLock)
{
    Log("Setting up devices...");

    if (acquireLock && WaitForSingleObject(sharedMutex, 1000) != WAIT_OBJECT_0)
    {
        Log("Could not lock mutex after specified interval " + GetLastError());
        return;
    }

    haveSetupDevices = true;

    vr::EVRSettingsError settingsError;
    //Add tracking refrences (sensors).
    for (int i = 0; i < sharedBuffer->trackingRefrencesCount; i++) AddDevice(std::make_shared<TrackingReferenceDevice>(i));

    //Add the hmd as a tracked device (if specified in the config).
    if (vr::VRSettings()->GetBool(settings_key_.c_str(), "track_hmd", &settingsError)) AddDevice(std::make_shared<TrackerDevice>(0, OculusDeviceType::HMD));
    if (settingsError == vr::VRSettingsError_UnsetSettingHasNoDefault) vr::VRSettings()->SetBool(settings_key_.c_str(), "track_hmd", false);

    //Add the controllers as trackers (if specified).
    //This isn't a mess :).
    if (vr::VRSettings()->GetBool(settings_key_.c_str(), "controllers_as_trackers", &settingsError))
        for (int i = 0; i < 2; i++) AddDevice(std::make_shared<TrackerDevice>(i, i == 0 ? OculusDeviceType::ControllerLeft : OculusDeviceType::ControllerRight));
    else
        for (int i = 0; i < 2; i++)
            AddDevice(std::make_shared<ControllerDevice>(i + 1, i == 0 ? ControllerDevice::Handedness::LEFT : ControllerDevice::Handedness::RIGHT)); //+1 to reserve index for hmd.
    if (settingsError == vr::VRSettingsError_UnsetSettingHasNoDefault) vr::VRSettings()->SetBool(settings_key_.c_str(), "controllers_as_trackers", true);

    //Add any other tracked objects there may be.
    for (int i = 0; i < sharedBuffer->vrObjectsCount; i++) AddDevice(std::make_shared<TrackerDevice>(i + 2, OculusDeviceType::Object)); //+3 to reserve indexes for hmd and controllers.

    if (acquireLock) ReleaseMutex(sharedMutex);

    Log("Devices Setup.");
}

void OculusToSteamVR::VRDriver::LaunchClient()
{
    //https://stackoverflow.com/questions/15435994/how-do-i-open-an-exe-from-another-c-exe
    std::thread([this]()
    {
        //Set the size of the structures.
        ZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);
        ZeroMemory(&processInformation, sizeof(processInformation));

        //This assumes that OculusToSteamVRClient.exe is in ..\.. from this dll which is usually located in bin\win64.
        std::string addonPath = Helpers::GetParentDirectory(Helpers::GetParentDirectory(Helpers::GetParentDirectory(Helpers::GetModulePath())));

        //Start the program up.
        if (!CreateProcessA((addonPath + "\\OculusToSteamVRClient.exe").c_str(), //Name of program to execute.
            NULL, //Command line.
            NULL, //Process handle not inheritable.
            NULL, //Thread handle not inheritable.
            FALSE, //Set handle inheritance to FALSE.
            0, //No creation flags.
            NULL, //Use parent's environment block.
            NULL, //Use parent's starting directory.
            &startupInfo, //Pointer to STARTUPINFO structure.
            &processInformation) //Pointer to PROCESS_INFORMATION structure.
            ) GetDriver()->Log("Couldn't launch client autmoatically.");
        else
        {
            //Wait until child process exits.
            WaitForSingleObject(processInformation.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(processInformation.hProcess, &exitCode);
            GetDriver()->Log((std::string("Client exited prematurely with code: ") + std::to_string(exitCode)).c_str());
            Cleanup();
        }
    }).detach();
}

vr::EVRInitError OculusToSteamVR::VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    // Perform driver context initialisation
    if (vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext); init_error != vr::EVRInitError::VRInitError_None) return init_error;

    Log("Activating OculusToSteamVR...");

    //Setup the shared objects.
    vr::EVRInitError initSharedDataResult = InitSharedData();
    if (initSharedDataResult != vr::VRInitError_None) return initSharedDataResult;

    if (WaitForSingleObject(sharedMutex, 1000) != WAIT_OBJECT_0)
    {
        Log("Could not lock mutex after specified interval " + GetLastError());
        return vr::VRInitError_IPC_ConnectFailedAfterMultipleAttempts;
    }
    if (IsClientAlive(sharedBuffer->clientTime)) SetupDevices(false);
    else LaunchClient();
    ReleaseMutex(sharedMutex);

    Log("OculusToSteamVR Loaded Successfully.");

	return vr::VRInitError_None;
}

void OculusToSteamVR::VRDriver::Cleanup()
{
    if (processInformation.hProcess != NULL)
    {
        CloseHandle(processInformation.hProcess);
        CloseHandle(processInformation.hThread);
    }
}

//The code in here (now moved to the individual device files) isn't all that efficient but I will work on improving that later.
void OculusToSteamVR::VRDriver::RunFrame()
{
    //Collect events.
    vr::VREvent_t event;
    std::vector<vr::VREvent_t> events;
    while (vr::VRServerDriverHost()->PollNextEvent(&event, sizeof(event))) events.push_back(event);
    openvr_events_ = events;

    //Update frame timing.
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    frame_timing_ = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time_);
    last_frame_time_ = now;
    slowLogTime += frame_timing_.count();

    if (slowLogTime >= 5000)
    {
        slowLogTime = 0;
        shouldSlowLog = true;
    }
    else shouldSlowLog = false;

    if (WaitForSingleObject(sharedMutex, 10) != WAIT_OBJECT_0) return; //Mutex lock timeout.

    //Make sure that the client is alive.
    if (IsClientAlive(sharedBuffer->clientTime))
    {
        //Setup devices if they havent been already.
        if (!haveSetupDevices) SetupDevices(false);

        //Update devices.
        for (auto& device : devices_) device->Update(sharedBuffer);
    }
    else
    {
        //Mark the devices as offline.
        for (auto& device : devices_)
        {
            auto pose = device->GetPose();
            pose.deviceIsConnected = false;
            pose.poseIsValid = false;
            pose.result = vr::ETrackingResult::TrackingResult_Uninitialized;
            GetDriverHost()->TrackedDevicePoseUpdated(device->GetDeviceIndex(), pose, sizeof(vr::DriverPose_t));
        }
    }

    ReleaseMutex(sharedMutex);
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
    return devices_;
}

std::vector<vr::VREvent_t> OculusToSteamVR::VRDriver::GetOpenVREvents()
{
    return openvr_events_;
}

std::chrono::milliseconds OculusToSteamVR::VRDriver::GetLastFrameTime()
{
    return frame_timing_;
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
    if(result) devices_.push_back(device);
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

bool OculusToSteamVR::VRDriver::ShouldSlowLog()
{
    return shouldSlowLog;
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
