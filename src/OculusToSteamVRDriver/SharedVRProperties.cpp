#include "SharedVRProperties.h"

void OculusToSteamVR_Driver::SharedVRProperties::SetCommonProperties(vr::PropertyContainerHandle_t properties, std::string modelNumber, std::string serialNumber)
{
	vr::VRProperties()->SetUint64Property(properties, vr::Prop_CurrentUniverseId_Uint64, 31);
	vr::VRProperties()->SetStringProperty(properties, vr::Prop_TrackingSystemName_String, "lighthouse");
	vr::VRProperties()->SetStringProperty(properties, vr::Prop_ModelNumber_String, modelNumber.c_str());
	vr::VRProperties()->SetStringProperty(properties, vr::Prop_SerialNumber_String, serialNumber.c_str());
}
