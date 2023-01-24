#include "VRTracker.h"

#include "Entrypoint.h"

OculusToSteamVR_Driver::VRTracker::VRTracker(unsigned int objectIndex)
{
	objectID = objectIndex;
	serialNumber = "ODT-0000000" + std::to_string(objectIndex + 5);
	modelNumber = "Oculus Rift CV1 Tracker " + std::to_string(objectIndex);
}

vr::EVRInitError OculusToSteamVR_Driver::VRTracker::Activate(uint32_t objectId)
{
    vr::PropertyContainerHandle_t properties = vr::VRProperties()->TrackedDeviceToPropertyContainer(objectId);

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_TrackingSystemName_String, "lighthouse");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_RenderModelName_String, "{htc}vr_tracker_vive_1_0");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ModelNumber_String, "Vive Tracker Pro MV");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ManufacturerName_String, "HTC");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_HardwareRevision_String, "14");
    vr::VRProperties()->SetUint64Property(properties, vr::Prop_HardwareRevision_Uint64, 14U);
    vr::VRProperties()->SetInt32Property(properties, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_GenericTracker);
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ResourceRoot_String, "htc");

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_InputProfilePath_String, "{htc}/input/vive_tracker_profile.json");
    vr::VRProperties()->SetInt32Property(properties, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_Invalid);
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ControllerType_String, "vive_tracker");

    //vr::VRProperties()->SetStringProperty(properties, vr::Prop_ModelNumber_String, m_sModelNumber.c_str()); //Not sure if this is needed.
    //vr::VRProperties()->SetStringProperty(properties, vr::Prop_SerialNumber_String, m_sSerialNumber.c_str());

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceOff_String, "{htc}/icons/tracker_status_off.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceSearching_String, "{htc}/icons/tracker_status_searching.gif");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{htc}/icons/tracker_status_searching_alert.gif");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceReady_String, "{htc}/icons/tracker_status_ready.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{htc}/icons/tracker_status_ready_alert.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceNotReady_String, "{htc}/icons/tracker_status_error.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceStandby_String, "{htc}/icons/tracker_status_standby.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceAlertLow_String, "{htc}/icons/tracker_status_ready_low.png");

    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasDisplayComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasCameraComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasDriverDirectModeComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasVirtualDisplayComponent_Bool, false);

    vr::VRProperties()->SetUint64Property(properties, vr::Prop_CurrentUniverseId_Uint64, 31);

    return vr::VRInitError_None;
}
