/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2026  Chris Needham

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

#ifndef JOYSTICK_CONTROLLER_HEADER
#define JOYSTICK_CONTROLLER_HEADER

#ifdef WIN32

#include <xinput.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <string>
#include <vector>

typedef DWORD (WINAPI *XINPUT_GET_STATE)(DWORD, XINPUT_STATE*);

#endif

// Same as XINPUT_STATE values

constexpr int JOYSTICK_BUTTON_UP    = 0x0001;
constexpr int JOYSTICK_BUTTON_DOWN  = 0x0002;
constexpr int JOYSTICK_BUTTON_LEFT  = 0x0004;
constexpr int JOYSTICK_BUTTON_RIGHT = 0x0008;
constexpr int JOYSTICK_BUTTON_DPAD  = 0x000F;

constexpr int JOYSTICK_BUTTON_A       = 0x1000;
constexpr int JOYSTICK_BUTTON_B       = 0x2000;
constexpr int JOYSTICK_BUTTON_X       = 0x4000;
constexpr int JOYSTICK_BUTTON_Y       = 0x8000;
constexpr int JOYSTICK_BUTTON_BUTTONS = 0xF000;

class JoystickState
{
	public:
		int LeftX; // 0 (right) to 65535 (left)
		int LeftY; // 0 (down) to 65535 (up)
		int RightX; // 0 (right) to 65535 (left)
		int RightY; // 0 (down) to 65535 (up)
		int Buttons;
};

enum class JoystickDeviceType
{
    XInput,
    DirectInput
};

#ifdef WIN32

class JoystickDevice
{
	public:
		JoystickDevice(JoystickDeviceType Type, const std::string& Name);
		virtual ~JoystickDevice();

	public:
		JoystickDeviceType GetType() const;
		bool IsActive() const;
		void Release();

		virtual void Acquire(bool Acquire) = 0;
		virtual bool GetState(JoystickState* pState) const = 0;

		const std::string& GetName() const;
		virtual std::string GetID() const = 0;

	protected:
		virtual void DoRelease() = 0;

		void SetActive(bool Active);

	private:
		JoystickDeviceType m_Type;
		bool m_Active;
		std::string m_Name;
};

class XInputJoystickDevice : public JoystickDevice
{
	public:
		XInputJoystickDevice(const std::string& Name,
		                      DWORD XInputIndex);

	public:
		HRESULT Activate(XINPUT_GET_STATE XInputGetState);

		virtual void Acquire(bool Acquire);
		virtual bool GetState(JoystickState* pState) const;

		virtual std::string GetID() const;

	protected:
		virtual void DoRelease();

	private:
		DWORD m_XInputIndex;
		XINPUT_GET_STATE m_XInputGetState;
};

class DirectInputJoystickDevice : public JoystickDevice
{
	public:
		DirectInputJoystickDevice(const std::string& Name,
		                          GUID DirectInputGuid);

	public:
		HRESULT Activate(IDirectInput8* pDirectInput, HWND hWnd);

		virtual void Acquire(bool Acquire);
		virtual bool GetState(JoystickState* pState) const;

		virtual std::string GetID() const;

	protected:
		virtual void DoRelease();

	private:
		void EnumObjects();
		static BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance,
		                                         void* pContext);
		BOOL EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance);

		HRESULT Init(IDirectInput8* pDirectInput, HWND hWnd);

		HRESULT QueryRange(DWORD Object, int* pMin, int* pMax);

	private:
		GUID m_InstanceGuid;
		IDirectInputDevice8* m_pDirectInputDevice;
		int m_MinX;
		int m_MaxX;
		int m_MinY;
		int m_MaxY;
		int m_MinRX;
		int m_MaxRX;
		int m_MinRY;
		int m_MaxRY;
};

class JoystickController
{
	public:
		JoystickController();
		JoystickController(const JoystickController&) = delete;
		JoystickController& operator=(const JoystickController&) = delete;
		~JoystickController();

	public:
		bool Init(HINSTANCE hInstance, HWND hWnd);

		void EnumerateDevices();

		size_t GetDeviceCount() const;
		const JoystickDevice& GetDevice(size_t Index) const;
		int FindDevice(JoystickDeviceType Type,
		               const std::string& ID);

		HRESULT SetDeviceActive(int Index);
		void ReleaseDevice(int Index);
		void ReleaseAllDevices();

		bool GetState(int Index, JoystickState* pState) const;

		void Acquire(bool Acquire);

	private:
		void EnumerateXInputDevices();
		void EnumerateDirectInputDevices();
		static BOOL CALLBACK EnumDInputCallback(const DIDEVICEINSTANCE* pDeviceInstance,
		                                        void* pContext);
		BOOL EnumDInputCallback(const DIDEVICEINSTANCE* pDeviceInstance);

		void ClearDevices();

	private:
		HINSTANCE m_hInstance;
		HWND m_hWnd;
		HMODULE m_hXInputModule;
		XINPUT_GET_STATE m_XInputGetState;
		IDirectInput8* m_pDirectInput;
		std::vector<JoystickDevice*> m_Devices;
};

#endif

// #define DEBUG_JOYSTICK

#endif
