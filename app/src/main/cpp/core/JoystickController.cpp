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

#include <windows.h>

#include <assert.h>

#include "JoystickController.h"
#include "DebugTrace.h"
#include "StringUtils.h"

#ifdef WIN32

/****************************************************************************/

#if defined(DEBUG_JOYSTICK) && !defined(NDEBUG)

static const char* DInputErrorStr(HRESULT hResult)
{
	switch (hResult)
	{
		case DIERR_DEVICENOTREG:
			return "DIERR_DEVICENOTREG";

		case DIERR_INVALIDPARAM:
			return "DIERR_INVALIDPARAM";

		case DIERR_NOINTERFACE:
			return "DIERR_NOINTERFACE";

		case DIERR_NOTINITIALIZED:
			return "DIERR_NOTINITIALIZED";

		case DIERR_OUTOFMEMORY:
			return "DIERR_OUTOFMEMORY";

		case DIERR_INPUTLOST:
			return "DIERR_INPUTLOST";

		case DIERR_NOTACQUIRED:
			return "DIERR_NOTACQUIRED";

		case E_PENDING:
			return "E_PENDING";

		case E_ACCESSDENIED:
			return "E_ACCESSDENIED";

		default:
			return "?";
	}
}

#endif

/****************************************************************************/

JoystickDevice::JoystickDevice(JoystickDeviceType Type, const std::string& Name) :
	m_Type(Type),
	m_Name(Name),
	m_Active(false)
{
}

/****************************************************************************/

JoystickDevice::~JoystickDevice()
{
}

/****************************************************************************/

JoystickDeviceType JoystickDevice::GetType() const
{
	return m_Type;
}

/****************************************************************************/

bool JoystickDevice::IsActive() const
{
	return m_Active;
}

/****************************************************************************/

void JoystickDevice::Release()
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("JoystickDevice::Release()\n");
	#endif

	DoRelease();

	m_Active = false;
}

/****************************************************************************/

const std::string& JoystickDevice::GetName() const
{
	return m_Name;
}

/****************************************************************************/

void JoystickDevice::SetActive(bool Active)
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("JoystickDevice::SetActive(%d)\n", (int)Active);
	#endif

	m_Active = Active;
}

/****************************************************************************/

XInputJoystickDevice::XInputJoystickDevice(const std::string& Name, DWORD XInputIndex) :
	JoystickDevice(JoystickDeviceType::XInput, Name),
	m_XInputIndex(XInputIndex),
	m_XInputGetState(nullptr)
{
}

/****************************************************************************/

HRESULT XInputJoystickDevice::Activate(XINPUT_GET_STATE XInputGetState)
{
	m_XInputGetState = XInputGetState;
	SetActive(true);

	return S_OK;
}

/****************************************************************************/

void XInputJoystickDevice::Acquire(bool /* Acquire */)
{
}

/****************************************************************************/

bool XInputJoystickDevice::GetState(JoystickState* pState) const
{
	if (!IsActive())
	{
		return false;
	}

	XINPUT_STATE State;

	DWORD Result = m_XInputGetState(m_XInputIndex, &State);

	if (Result != ERROR_SUCCESS)
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("XInputJoystickDevice::GetState()\n");
		DebugTrace("XInputGetState returned %u\n", Result);
		#endif

		return false;
	}

	pState->LeftX = 32767 - (int)State.Gamepad.sThumbLX;
	pState->LeftY = (int)State.Gamepad.sThumbLY + 32768;

	pState->RightX = 32767 - (int)State.Gamepad.sThumbRX;
	pState->RightY = (int)State.Gamepad.sThumbRY + 32768;

	pState->Buttons = State.Gamepad.wButtons;

	return true;
}

/****************************************************************************/

std::string XInputJoystickDevice::GetID() const
{
	return std::to_string(m_XInputIndex);
}

/****************************************************************************/

void XInputJoystickDevice::DoRelease()
{
}

/****************************************************************************/

DirectInputJoystickDevice::DirectInputJoystickDevice(const std::string& Name, GUID InstanceGuid) :
	JoystickDevice(JoystickDeviceType::DirectInput, Name),
	m_InstanceGuid(InstanceGuid),
	m_pDirectInputDevice(nullptr),
	m_MinX(0),
	m_MaxX(65535),
	m_MinY(0),
	m_MaxY(65535),
	m_MinRX(0),
	m_MaxRX(65535),
	m_MinRY(0),
	m_MaxRY(65535)
{
}

/****************************************************************************/

void DirectInputJoystickDevice::EnumObjects()
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("DirectInputJoystickDevice::EnumObjects\n");
	#endif

	m_pDirectInputDevice->EnumObjects(EnumObjectsCallback, this, DIDFT_AXIS | DIDFT_BUTTON);

	#ifdef DEBUG_JOYSTICK
	DebugTrace("DirectInputJoystickDevice::EnumObjects completed\n");
	#endif
}

/****************************************************************************/

BOOL CALLBACK DirectInputJoystickDevice::EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance,
                                                             void* pContext)
{
	DirectInputJoystickDevice* pJoystickDevice = reinterpret_cast<DirectInputJoystickDevice*>(pContext);

	return pJoystickDevice->EnumObjectsCallback(pDeviceObjectInstance);
}

/****************************************************************************/

BOOL DirectInputJoystickDevice::EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance)
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("Type: %08X, Collection %d: %s\n", pDeviceObjectInstance->dwType,
	                                              pDeviceObjectInstance->wCollectionNumber,
	                                              pDeviceObjectInstance->tszName);
	#else
	UNREFERENCED_PARAMETER(pDeviceObjectInstance);
	#endif

	return TRUE;
}

/****************************************************************************/

HRESULT DirectInputJoystickDevice::Activate(IDirectInput8* pDirectInput, HWND hWnd)
{
	if (m_pDirectInputDevice != nullptr)
	{
		return S_OK;
	}

	HRESULT hResult = Init(pDirectInput, hWnd);

	if (SUCCEEDED(hResult))
	{
		SetActive(true);
	}

	return hResult;
}

/****************************************************************************/

HRESULT DirectInputJoystickDevice::Init(IDirectInput8* pDirectInput, HWND hWnd)
{
	HRESULT hResult = pDirectInput->CreateDevice(m_InstanceGuid,
	                                             &m_pDirectInputDevice,
	                                             nullptr);

	if (FAILED(hResult))
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("m_pDirectInput->CreateDevice() returned %s\n", DInputErrorStr(hResult));
		#endif

		return hResult;
	}

	// EnumObjects();

	hResult = m_pDirectInputDevice->SetDataFormat(&c_dfDIJoystick);

	if (FAILED(hResult))
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("m_pDirectInputDevice->SetDataFormat() returned %s\n", DInputErrorStr(hResult));
		#endif

		Release();
		return hResult;
	}

	hResult = m_pDirectInputDevice->SetCooperativeLevel(hWnd,
	                                                    DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	if (FAILED(hResult))
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("m_pDirectInputDevice->SetCooperativeLevel() returned %s\n", DInputErrorStr(hResult));
		#endif

		Release();
		return hResult;
	}

	QueryRange(DIJOFS_X, &m_MinX, &m_MaxX);
	QueryRange(DIJOFS_Y, &m_MinY, &m_MaxY);
	QueryRange(DIJOFS_RX, &m_MinRX, &m_MaxRX);
	QueryRange(DIJOFS_RY, &m_MinRY, &m_MaxRY);

	hResult = m_pDirectInputDevice->Acquire();

	if (FAILED(hResult))
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("m_pDirectInputDevice->Acquire() returned %s\n", DInputErrorStr(hResult));
		#endif
	}

	return hResult;
}

/****************************************************************************/

HRESULT DirectInputJoystickDevice::QueryRange(DWORD Object, int* pMin, int* pMax)
{
	DIPROPRANGE Range;
	ZeroMemory(&Range, sizeof(Range));
	Range.diph.dwSize = sizeof(DIPROPRANGE);
	Range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	Range.diph.dwHow = DIPH_BYOFFSET;
	Range.diph.dwObj = Object;

	HRESULT hResult = m_pDirectInputDevice->GetProperty(DIPROP_RANGE, &Range.diph);

	if (SUCCEEDED(hResult))
	{
		*pMin = Range.lMin;
		*pMax = Range.lMax;
	}

	return hResult;
}

/****************************************************************************/

void DirectInputJoystickDevice::Acquire(bool Acquire)
{
	assert(m_pDirectInputDevice != nullptr);

	if (Acquire)
	{
		HRESULT hResult = m_pDirectInputDevice->Acquire();

		if (FAILED(hResult))
		{
			#ifdef DEBUG_JOYSTICK
			DebugTrace("m_pDirectInputDevice->Acquire() returned %s\n", DInputErrorStr(hResult));
			#endif
		}
	}
	else
	{
		HRESULT hResult = m_pDirectInputDevice->Unacquire();

		if (FAILED(hResult))
		{
			#ifdef DEBUG_JOYSTICK
			DebugTrace("m_pDirectInputDevice->Unacquire() returned %s\n", DInputErrorStr(hResult));
			#endif
		}
	}
}

/****************************************************************************/

static int NormalizeRange(int Value, int Min, int Max)
{
    return ((Value - Min) * 65535LL) / (Max - Min);
}

/****************************************************************************/

bool DirectInputJoystickDevice::GetState(JoystickState* pState) const
{
	if (m_pDirectInputDevice == nullptr)
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("DirectInputJoystickDevice::GetState()\n");
		DebugTrace("m_pDirectInputDevice is NULL\n");
		#endif

		return false;
	}

	HRESULT hResult = m_pDirectInputDevice->Poll();

	if (FAILED(hResult))
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("DirectInputJoystickDevice::GetState()\n");
		DebugTrace("m_pDirectInputDevice->Poll() returned %s\n", DInputErrorStr(hResult));
		#endif

		return hResult == DIERR_NOTACQUIRED;
	}

	DIJOYSTATE State;

	hResult = m_pDirectInputDevice->GetDeviceState(sizeof(State), &State);

	if (FAILED(hResult))
	{
		#ifdef DEBUG_JOYSTICK
		DebugTrace("DirectInputJoystickDevice::GetState()\n");
		DebugTrace("m_pDirectInputDevice->GetDeviceState() returned %s\n", DInputErrorStr(hResult));
		#endif

		return hResult == DIERR_NOTACQUIRED;
	}

	pState->LeftX = 65535 - NormalizeRange(State.lX, m_MinX, m_MaxX);
	pState->LeftY = 65535 - NormalizeRange(State.lY, m_MinY, m_MaxY);

	pState->RightX = 65535 - NormalizeRange(State.lRx, m_MinRX, m_MaxRX);
	pState->RightY = 65535 - NormalizeRange(State.lRy, m_MinRX, m_MaxRX);

	pState->Buttons = 0;

	for (int i = 0; i < 4; i++)
	{
		if (State.rgbButtons[i] & 0x80)
		{
			pState->Buttons |= JOYSTICK_BUTTON_A << i;
		}
	}

	switch (State.rgdwPOV[0])
	{
		case 0:
			pState->Buttons |= JOYSTICK_BUTTON_UP;
			break;

		case 4500:
			pState->Buttons |= JOYSTICK_BUTTON_RIGHT | JOYSTICK_BUTTON_UP;
			break;

		case 9000:
			pState->Buttons |= JOYSTICK_BUTTON_RIGHT;
			break;

		case 13500:
			pState->Buttons |= JOYSTICK_BUTTON_RIGHT | JOYSTICK_BUTTON_DOWN;
			break;

		case 18000:
			pState->Buttons |= JOYSTICK_BUTTON_DOWN;
			break;

		case 22500:
			pState->Buttons |= JOYSTICK_BUTTON_LEFT | JOYSTICK_BUTTON_DOWN;
			break;

		case 27000:
			pState->Buttons |= JOYSTICK_BUTTON_LEFT;
			break;

		case 31500:
			pState->Buttons |= JOYSTICK_BUTTON_LEFT | JOYSTICK_BUTTON_UP;
			break;
	}

	return true;
}

/****************************************************************************/

std::string DirectInputJoystickDevice::GetID() const
{
	return GuidToString(m_InstanceGuid);
}

/****************************************************************************/

void DirectInputJoystickDevice::DoRelease()
{
	if (m_pDirectInputDevice != nullptr)
	{
		m_pDirectInputDevice->Release();
		m_pDirectInputDevice = nullptr;
	}
}

/****************************************************************************/

JoystickController::JoystickController() :
	m_hInstance(nullptr),
	m_hWnd(nullptr),
	m_hXInputModule(nullptr),
	m_XInputGetState(nullptr),
	m_pDirectInput(nullptr)
{
}

/****************************************************************************/

JoystickController::~JoystickController()
{
	if (m_hXInputModule != nullptr)
	{
		FreeLibrary(m_hXInputModule);
		m_hXInputModule = nullptr;
	}

	ClearDevices();

	if (m_pDirectInput != nullptr)
	{
		m_pDirectInput->Release();
		m_pDirectInput = nullptr;
	}
}

/****************************************************************************/

bool JoystickController::Init(HINSTANCE hInstance, HWND hWnd)
{
	assert(m_hInstance == nullptr);
	assert(m_hWnd == nullptr);

	m_hInstance = hInstance;
	m_hWnd = hWnd;

	static const char* const XInputLibraries[] =
	{
		"xinput1_4.dll",
		"xinput1_3.dll",
		"xinput9_1_0.dll"
	};

	for (size_t i = 0; i < _countof(XInputLibraries); i++)
	{
		m_hXInputModule = LoadLibrary(XInputLibraries[i]);

		if (m_hXInputModule != nullptr)
		{
			m_XInputGetState = reinterpret_cast<XINPUT_GET_STATE>(
				GetProcAddress(m_hXInputModule, "XInputGetState")
			);
			break;
		}
	}

	DirectInput8Create(m_hInstance,
	                   DIRECTINPUT_VERSION,
	                   IID_IDirectInput8,
	                   reinterpret_cast<void**>(&m_pDirectInput),
	                   nullptr);

	return m_hXInputModule != nullptr || m_pDirectInput != nullptr;
}

/****************************************************************************/

void JoystickController::EnumerateDevices()
{
	ClearDevices();

	EnumerateXInputDevices();
	EnumerateDirectInputDevices();
}

/****************************************************************************/

void JoystickController::EnumerateXInputDevices()
{
	if (m_hXInputModule == nullptr)
	{
		return;
	}

	for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		XINPUT_STATE State;

		if (m_XInputGetState(i, &State) == ERROR_SUCCESS)
		{
			XInputJoystickDevice* pDevice = new XInputJoystickDevice(
				"XBox Controller - Slot " + std::to_string(i) + " [XInput]",
				i
			);

			m_Devices.emplace_back(pDevice);
		}
	}
}

/****************************************************************************/

void JoystickController::EnumerateDirectInputDevices()
{
	if (m_pDirectInput == nullptr)
	{
		return;
	}

	m_pDirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL,
	                            EnumDInputCallback,
	                            this,
	                            DIEDFL_ATTACHEDONLY);
}

/****************************************************************************/

BOOL CALLBACK JoystickController::EnumDInputCallback(const DIDEVICEINSTANCE* pDeviceInstance,
                                                     void* pContext)
{
	JoystickController* pController = reinterpret_cast<JoystickController*>(pContext);

	return pController->EnumDInputCallback(pDeviceInstance);
}

/****************************************************************************/

BOOL JoystickController::EnumDInputCallback(const DIDEVICEINSTANCE* pDeviceInstance)
{
	DirectInputJoystickDevice* pDevice = new DirectInputJoystickDevice(
		std::string(pDeviceInstance->tszProductName) + " [DirectInput]",
		pDeviceInstance->guidInstance
	);

	m_Devices.emplace_back(pDevice);

	return DIENUM_CONTINUE;
}

/****************************************************************************/

void JoystickController::ClearDevices()
{
	for (size_t i = 0; i < m_Devices.size(); i++)
	{
		m_Devices[i]->Release();

		delete m_Devices[i];
	}

	m_Devices.clear();
}

/****************************************************************************/

size_t JoystickController::GetDeviceCount() const
{
	return m_Devices.size();
}

/****************************************************************************/

const JoystickDevice& JoystickController::GetDevice(size_t Index) const
{
	return *m_Devices[Index];
}

/****************************************************************************/

int JoystickController::FindDevice(JoystickDeviceType Type,
                                   const std::string& ID)
{
	for (int i = 0; i < (int)m_Devices.size(); i++)
	{
		if (m_Devices[i]->GetType() == Type &&
		    m_Devices[i]->GetID() == ID)
		{
			return i;
		}
	}

	return -1;
}

/****************************************************************************/

HRESULT JoystickController::SetDeviceActive(int Index)
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("JoystickController::SetDeviceActive(%u)\n", Index);
	#endif

	HRESULT hResult = E_FAIL;

	if (Index >= 0 && Index < (int)m_Devices.size())
	{
		JoystickDevice* pDevice = m_Devices[Index];

		switch (pDevice->GetType())
		{
			case JoystickDeviceType::XInput:
				hResult = static_cast<XInputJoystickDevice*>(pDevice)->Activate(m_XInputGetState);
				break;

			case JoystickDeviceType::DirectInput:
				hResult = static_cast<DirectInputJoystickDevice*>(pDevice)->Activate(m_pDirectInput, m_hWnd);
				break;

			default:
				assert(false);
				break;
		}
	}

	return hResult;
}

/****************************************************************************/

void JoystickController::ReleaseDevice(int Index)
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("JoystickController::ReleaseDevice(%u)\n", Index);
	#endif

	if (Index >= 0 && Index < (int)m_Devices.size())
	{
		m_Devices[Index]->Release();
	}
}

/****************************************************************************/

void JoystickController::ReleaseAllDevices()
{
	for (size_t i = 0; i < m_Devices.size(); i++)
	{
		m_Devices[i]->Release();
	}
}

/****************************************************************************/

bool JoystickController::GetState(int DeviceIndex, JoystickState* pState) const
{
	return m_Devices[DeviceIndex]->GetState(pState);
}

/****************************************************************************/

// Called when the BeebEm window is activated and deactivated. This is used
// to acquire or release access to DirectInput devices.

void JoystickController::Acquire(bool Acquire)
{
	#ifdef DEBUG_JOYSTICK
	DebugTrace("JoystickController::Acquire(%d)\n", (int)Acquire);
	#endif

	for (size_t i = 0; i < m_Devices.size(); i++)
	{
		JoystickDevice* pDevice = m_Devices[i];

		if (pDevice->IsActive())
		{
			pDevice->Acquire(Acquire);
		}
	}
}

#endif // WIN32

/****************************************************************************/
