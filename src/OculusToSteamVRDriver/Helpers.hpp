#pragma once

#include <OVR_CAPI.h>
#include <codecvt>

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

		//https://gist.github.com/pwm1234/05280cf2e462853e183d
		static std::string GetModulePath(void* address = Foo)
		{
			char path[FILENAME_MAX];
			HMODULE hm = NULL;

			if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)address,
				&hm))
			{
				throw std::runtime_error(std::string("GetModuleHandle returned " + GetLastError()));
			}
			GetModuleFileNameA(hm, path, sizeof(path));

			std::string p = path;
			return p;
		}

		static std::string GetParentDirectory(std::string path)
		{
			return path.substr(0, path.find_last_of("\\"));
		}

		//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
		static std::string GetLastErrorAsString()
		{
			//Get the error message ID, if any.
			DWORD errorMessageID = ::GetLastError();
			if (errorMessageID == 0) {
				return std::string(); //No error message has been recorded
			}

			LPSTR messageBuffer = nullptr;

			//Ask Win32 to give us the string version of that message ID.
			//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
			size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

			//Copy the error message into a std::string.
			std::string message(messageBuffer, size);

			//Free the Win32's string's buffer.
			LocalFree(messageBuffer);

			return message;
		}

		static std::string GetSerialSuffix(unsigned int index)
		{
			//Max length -> 00000000
			if (index > 99999999) return std::to_string(index - 99999999);
			else if (index > 9999999) return std::to_string(index);
			else if (index > 999999) return std::string("0") + std::to_string(index);
			else if (index > 99999) return std::string("00") + std::to_string(index);
			else if (index > 9999) return std::string("000") + std::to_string(index);
			else if (index > 999) return std::string("0000") + std::to_string(index);
			else if (index > 99) return std::string("00000") + std::to_string(index);
			else if (index > 9) return std::string("000000") + std::to_string(index);
			else if (index > 0) return std::string("0000000") + std::to_string(index);
			else return std::string("00000000");
		}

	private:
		static void Foo(){}
	};
}