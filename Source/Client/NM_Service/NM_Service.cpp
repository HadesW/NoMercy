// https://www.codeproject.com/Articles/499465/Simple-Windows-Service-in-Cplusplus

#include "stdafx.h"
#include "Utils.h"

#include <lazy_importer.hpp>
#include <xorstr.hpp>

#define SERVICE_NAME (LPWSTR)L"\\??\\NoMercySvc"

typedef bool(__cdecl* TInitializeServiceFunction)(bool bProtected);

static std::string		s_szModuleName	= xorstr("NoMercy.dll").crypt_get();
static std::string		s_szLogFile		= xorstr("NoMercyService.log").crypt_get();

SERVICE_STATUS			g_ServiceStatus		= { 0 };
SERVICE_STATUS_HANDLE	g_StatusHandle		= NULL;
HANDLE					g_ServiceStopEvent	= INVALID_HANDLE_VALUE;
HANDLE					g_hServiceThread	= INVALID_HANDLE_VALUE;

// -------------------

bool InitNoMercy(bool bProtected)
{
	NNoMercyUtils::FileLog(s_szLogFile, xorstr("Initilization started!").crypt_get());

	LI_FIND(CreateDirectoryA)(xorstr("NoMercy").crypt_get(), NULL);

	auto szModuleNameWithPath = NNoMercyUtils::ExePath() + xorstr("/").crypt_get() + s_szModuleName;
	if (NNoMercyUtils::IsFileExist(szModuleNameWithPath.c_str()) == false)
	{
		NNoMercyUtils::FileLogf(s_szLogFile, xorstr("Error! DLL file(%s) not found!").crypt_get(), szModuleNameWithPath.c_str());
		return false;
	}
	NNoMercyUtils::FileLog(s_szLogFile, xorstr("DLL module succesfully found!").crypt_get());

	auto hModule = LI_FIND(LoadLibraryA)(s_szModuleName.c_str());
	if (!hModule)
	{
		NNoMercyUtils::FileLogf(s_szLogFile, xorstr("Error! DLL file can not load! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLogf(s_szLogFile, xorstr("DLL file succesfully loaded!").crypt_get());

	auto InitializeFunction = reinterpret_cast<TInitializeServiceFunction>(LI_FIND(GetProcAddress)(hModule, xorstr("InitializeService").crypt_get()));
	if (!InitializeFunction)
	{
		NNoMercyUtils::FileLogf(s_szLogFile, xorstr("Error! Initialize function not found! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLog(s_szLogFile, xorstr("Initialize function found!").crypt_get());

	if (!InitializeFunction(bProtected))
	{
		NNoMercyUtils::FileLogf(s_szLogFile, xorstr("Error! Initilization call fail! Error code: %u").crypt_get(), LI_FIND(GetLastError)());
		return false;
	}
	NNoMercyUtils::FileLog(s_szLogFile, xorstr("Initializion completed!").crypt_get());
	return true;
}

VOID WINAPI ServiceHandler(DWORD CtrlCode)
{
	NNoMercyUtils::DebugLogf("Service ctrl code: %u handled!", CtrlCode);

	switch (CtrlCode)
	{
		// TODO: implement other handlers
		/*
		case SERVICE_CONTROL_PAUSE:
		case SERVICE_CONTROL_CONTINUE:
		case SERVICE_CONTROL_SHUTDOWN:
		*/

		case SERVICE_CONTROL_STOP:
		{
			if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
				break;

			g_ServiceStatus.dwControlsAccepted	= 0;
			g_ServiceStatus.dwCurrentState		= SERVICE_STOP_PENDING;
			g_ServiceStatus.dwWin32ExitCode		= 0;
			g_ServiceStatus.dwCheckPoint		= 4;

			if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
			{
				NNoMercyUtils::DebugLogf("SetServiceStatus(4) fail! Error: %u\n", GetLastError());
			}
			SetEvent(g_ServiceStopEvent);
			CloseHandle(g_ServiceStopEvent);

			if (g_hServiceThread)
			{
				TerminateThread(g_hServiceThread, CtrlCode);
				CloseHandle(g_hServiceThread);
			}

		} break;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}
}

DWORD WINAPI ServiceThread(LPVOID lpParam)
{
	if (InitNoMercy(true) == false)
	{
		NNoMercyUtils::DebugLog("NoMercy core can not loaded!\n");
		return ERROR_INVALID_FUNCTION;
	}
	
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
//		NNoMercyUtils::DebugLog("NoMercy Service still works!\n");
		Sleep(100);
	}

	NNoMercyUtils::DebugLog("NoMercy Service will stop!\n");
	return ERROR_SUCCESS;
}


VOID WINAPI ServiceMain(DWORD wNumServicesArgs, LPWSTR * lpServiceArgVectors)
{
	NNoMercyUtils::DebugLogf("NoMercy service main routine has been started!\n");

	g_StatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceHandler);
	if (g_StatusHandle == NULL)
	{
		auto dwErrorCode = GetLastError();

		NNoMercyUtils::DebugLogf("RegisterServiceCtrlHandlerA fail! Error: %u\n", dwErrorCode);
		NNoMercyUtils::WriteErrorLogEntry("RegisterServiceCtrlHandlerA", dwErrorCode);
		return;
	}

	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType				= SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted			= 0;
	g_ServiceStatus.dwCurrentState				= SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode				= NO_ERROR;
	g_ServiceStatus.dwServiceSpecificExitCode	= 0;
	g_ServiceStatus.dwCheckPoint				= 0;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		NNoMercyUtils::DebugLogf("SetServiceStatus(0) fail! Error: %u\n", GetLastError());
		return;
	}

	g_ServiceStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			NNoMercyUtils::DebugLogf("SetServiceStatus(1) fail! Error: %u\n", GetLastError());
		}
		return;
	}

	g_ServiceStatus.dwControlsAccepted	= SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState		= SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode		= 0;
	g_ServiceStatus.dwCheckPoint		= 2; // 0

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		NNoMercyUtils::DebugLogf("SetServiceStatus(2) fail! Error: %u\n", GetLastError());
	}

	g_hServiceThread = CreateThread(NULL, 0, ServiceThread, lpServiceArgVectors[wNumServicesArgs - 1], 0, NULL);
	WaitForSingleObject(g_hServiceThread, INFINITE);
	CloseHandle(g_ServiceStopEvent);

	g_ServiceStatus.dwControlsAccepted	= 0;
	g_ServiceStatus.dwCurrentState		= SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode		= 0;
	g_ServiceStatus.dwCheckPoint		= 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		NNoMercyUtils::DebugLogf("SetServiceStatus(3) fail! Error: %u\n", GetLastError());
	}

	return;
}

void OnTerminate()
{
	// TODO: OnTerminate routine
}

int main(int argc, char* argv[])
{
	std::set_terminate(&OnTerminate);

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, ServiceMain },
		{ NULL, NULL }
	};
	if (StartServiceCtrlDispatcherW(ServiceTable) == FALSE)
	{
		auto dwErrorCode = GetLastError();

		NNoMercyUtils::DebugLogf("NoMercy can not loaded! Error: %u\n", dwErrorCode);
		NNoMercyUtils::WriteErrorLogEntry("StartServiceCtrlDispatcher", dwErrorCode);

#ifdef _DEBUG
		ServiceThread(nullptr); // Allow quick start for debug build
#endif

		return dwErrorCode;
	}

	NNoMercyUtils::DebugLogf("NoMercy service will close!\n");
	return ERROR_SUCCESS;
}
