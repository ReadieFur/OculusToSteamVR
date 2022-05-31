#pragma once

#include <OVR_CAPI.h>

namespace OculusToSteamVR
{
	static class Helpers
	{
	public:
		//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L806
		static ovrQuatf OVRQuatFMul(ovrQuatf q1, ovrQuatf q2)
		{
			ovrQuatf result = { 0 };
			result.x = q1.x * q2.w + q1.y * q2.z - q1.z * q2.y + q1.w * q2.x;
			result.y = -q1.x * q2.z + q1.y * q2.w + q1.z * q2.x + q1.w * q2.y;
			result.z = q1.x * q2.y - q1.y * q2.x + q1.z * q2.w + q1.w * q2.z;
			result.w = -q1.x * q2.x - q1.y * q2.y - q1.z * q2.z + q1.w * q2.w;
			return result;
		}

		//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L372
		static ovrVector3f CrossProduct(const ovrVector3f v, ovrVector3f p)
		{
			return ovrVector3f{ v.y * p.z - v.z * p.y, v.z * p.x - v.x * p.z, v.x * p.y - v.y * p.x };
		}

		//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L832
		static ovrVector3f RotateVector2(ovrVector3f v, ovrQuatf q)
		{
			// nVidia SDK implementation

			ovrVector3f uv, uuv;
			ovrVector3f qvec{ q.x, q.y, q.z };
			uv = CrossProduct(qvec, v);
			uuv = CrossProduct(qvec, uv);
			uv.x *= (2.0f * q.w);
			uv.y *= (2.0f * q.w);
			uv.z *= (2.0f * q.w);
			uuv.x *= 2.0f;
			uuv.y *= 2.0f;
			uuv.z *= 2.0f;

			return ovrVector3f{ v.x + uv.x + uuv.x, v.y + uv.y + uuv.y, v.z + uv.z + uuv.z };
		}
	};
}