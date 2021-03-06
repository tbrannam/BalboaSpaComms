// BalBoaSpa.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#pragma comment(lib, "Ws2_32.lib")

#include <conio.h>
#include <crtdbg.h>


void DumpHexData(
	const BYTE *pData,
	size_t iBytes)
{
	for (int i = 0; i < iBytes; i++)
	{
		BYTE c = pData[i];
		wprintf(L"%02x ", c);
	}
	wprintf(L"\n");
}

void DumpHexData(
	const CByteArray &array)
{
	DumpHexData(&array[0], array.size());
}


void TestBroadcast()
{
	SpaAddressVector Spas;

	wprintf(L" Hit key to exit.\r");
	UINT uiFound = 0, uiBroadcast = 0;
	do
	{
		if (!DiscoverSpas(Spas))
		{
			wprintf(L"Error discovering spas\n");
		}

		uiBroadcast++;
		if (Spas.size() > 0)
		{
			uiFound++;
		}

		wprintf(L"Discovered %zd spas.\n", Spas.size());
		for (auto i = Spas.begin(); i < Spas.end(); i++)
		{
			const CSpaAddress &Spa = *i;
			TCHAR szIpAddr[64];

			InetNtop(Spa.m_SpaAddress.sin_family, &Spa.m_SpaAddress.sin_addr, szIpAddr, sizeof(szIpAddr) / sizeof(szIpAddr[0]));

			wprintf(L"Spa MAC Address: %S\nSpa IP Address %s\n Hit a key to exit.\r", Spa.m_strMACAddress.c_str(), szIpAddr);

		}
		Sleep(1000);
	} while (_kbhit() == 0);
	(void)_getch();

	wprintf(L"\nBroadcast: %d\nFound: %d\n\nHit a key to exit.\n", uiBroadcast, uiFound);

	(void)_getch();

	return;
}


class TestCallback:
	public IMonitorCallback
{
public:
	TestCallback();
	~TestCallback();

	BOOL AwaitMessage(DWORD dwTimeout);


private:
	void ProcessStatusMessage(const StatusMessage &);
	void ProcessConfigResponse(const ConfigResponseMessage &);
	void ProcessFilterConfigResponse(const FilterConfigResponseMessage &);
	void ProcessVersionInfoResponse(const VersionInfoResponseMessage &);
	void ProcessControlConfig2Response(const ControlConfig2ResponseMessage &);
	void ProcessUnknownMessageRaw(const CByteArray &);

	void Dispose();
	void OnFatalError();

	HANDLE m_hEvent;


public:
	// Public members of a global object?  How Sleazy!
	StatusMessage m_StatusMessage;
	ConfigResponseMessage m_ConfigResponseMessage;
	FilterConfigResponseMessage m_FilterConfigResponse;
	VersionInfoResponseMessage m_VersionInfoResponse;
	ControlConfig2ResponseMessage m_ControlConfig2Response;
	CByteArray m_RawUnknownMessage;

	enum HandledMessages
	{
		hmStatus,
		hmConfigResponse,
		hmFilterConfigResponse,
		hmVersionInfoResponse,
		hmControlConfig2Response,
		hmUnknownResponse,
		hmNetworkError,

		HM_COUNT
	};

	BOOL m_fChangedMessages[HM_COUNT];

};

TestCallback::TestCallback()
{
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

TestCallback::~TestCallback()
{
	CloseHandle(m_hEvent);
}


BOOL
TestCallback::AwaitMessage(
	DWORD dwTimeout)
{
	return (WaitForSingleObject(m_hEvent, dwTimeout) == WAIT_OBJECT_0);
}

void
TestCallback::ProcessStatusMessage(
	const StatusMessage &Message)
{
	m_StatusMessage = Message;

	m_fChangedMessages[hmStatus] = TRUE;
	SetEvent(m_hEvent);
}

void
TestCallback::ProcessConfigResponse(
	const ConfigResponseMessage &Message)
{
	m_ConfigResponseMessage = Message;
	m_fChangedMessages[hmConfigResponse] = TRUE;
	SetEvent(m_hEvent);

}


void
TestCallback::ProcessFilterConfigResponse(
	const FilterConfigResponseMessage &Message)
{
	m_FilterConfigResponse = Message;
	m_fChangedMessages[hmFilterConfigResponse] = TRUE;
	SetEvent(m_hEvent);
}


void
TestCallback::ProcessVersionInfoResponse(
	const VersionInfoResponseMessage &Message)
{
	m_VersionInfoResponse = Message;
	m_fChangedMessages[hmVersionInfoResponse] = TRUE;
	SetEvent(m_hEvent);
}

void TestCallback::ProcessControlConfig2Response(
	const ControlConfig2ResponseMessage &Message)
{
	m_ControlConfig2Response = Message;
	m_fChangedMessages[hmControlConfig2Response] = TRUE;
	SetEvent(m_hEvent);

}

void TestCallback::ProcessUnknownMessageRaw(const CByteArray &Message)
{
	m_RawUnknownMessage = Message;
	m_fChangedMessages[hmUnknownResponse] = TRUE;
	SetEvent(m_hEvent);
}



void
TestCallback::Dispose()
{}

void
TestCallback::OnFatalError()
{
	m_fChangedMessages[hmNetworkError] = TRUE;
	SetEvent(m_hEvent);
}


TestCallback GlobalCallbackClass;

const WCHAR * szPumpSpeeds[] = {L" Off", L" Low", L"High"};

int main()
{
	WSADATA wsaData;

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	//TestBroadcast();

	SpaAddressVector Spas;

	while (Spas.size() == 0)
	{
		if (!DiscoverSpas(Spas))
		{
			wprintf(L"Error discovering spas\n");
		}

		wprintf(L"Discovered %zd spas.\n", Spas.size());
		for (auto i = Spas.begin(); i < Spas.end(); i++)
		{
			const CSpaAddress &Spa = *i;
			TCHAR szIpAddr[64];

			InetNtop(Spa.m_SpaAddress.sin_family, &Spa.m_SpaAddress.sin_addr, szIpAddr, sizeof(szIpAddr) / sizeof(szIpAddr[0]));

			wprintf(L"Spa MAC Address: %S\nSpa IP Address %s\n Hit a key to exit.\r", Spa.m_strMACAddress.c_str(), szIpAddr);

		}
	}

	if (Spas.size() > 0)
	{

		for (auto i = Spas.cbegin(); i < Spas.cend(); i++)
		{
			TCHAR szIpAddr[64];

			InetNtop(i->m_SpaAddress.sin_family, &i->m_SpaAddress.sin_addr, szIpAddr, sizeof(szIpAddr) / sizeof(szIpAddr[0]));

			wprintf_s(L"Spa MAC Address: %S %s\n", (*i).m_strMACAddress.c_str(), szIpAddr);

			CSpaComms SpaComms(*i, &GlobalCallbackClass);

			if (!SpaComms.StartMonitor())
			{
				wprintf_s(L"StartMonitor() for %s failed.\n",
						  szIpAddr);
				continue;
			}

			
			if (!SpaComms.SendConfigRequest())
			{
				wprintf_s(L"SendConfigRequest() failed.\n");
			}

			if (!SpaComms.SendFilterConfigRequest())
			{
				wprintf_s(L"SendFilterConfigRequest() failed.\n");
			}

			if (!SpaComms.SendVerInfoRequest())
			{
				wprintf_s(L"SendVerInfoRequest() failed.\n");
			}

			if (!SpaComms.SendControlConfig2Request())
			{
				wprintf_s(L"SendControlConfig2Request() failed.\n");
			}

			do
			{

				if (GlobalCallbackClass.AwaitMessage(1000))
				{
					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmStatus])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmStatus] = FALSE;
						wprintf_s(L"Spa Status: Time %02d:%02d, Temp %d/%d %s, Range: %s, Heat: %s, Lights: %s, Pump 1:%s, Pump 2:%s\n",
								  GlobalCallbackClass.m_StatusMessage.m_Time.m_Hour,
								  GlobalCallbackClass.m_StatusMessage.m_Time.m_Minute,
								  GlobalCallbackClass.m_StatusMessage.m_CurrentTemp / ((GlobalCallbackClass.m_StatusMessage.m_TempScale == tsFahrenheight) ? 1 : 2),
								  GlobalCallbackClass.m_StatusMessage.m_SetPointTemp / ((GlobalCallbackClass.m_StatusMessage.m_TempScale == tsFahrenheight) ? 1 : 2),
								  GlobalCallbackClass.m_StatusMessage.m_TempScale == tsFahrenheight ? L"F" : L"C",
								  GlobalCallbackClass.m_StatusMessage.m_HeatRange ? L"High" : L" Low",
								  GlobalCallbackClass.m_StatusMessage.m_fHeating ? L" On" : L"Off",
								  GlobalCallbackClass.m_StatusMessage.m_fLights ? L" On" : L"Off",
								  szPumpSpeeds[GlobalCallbackClass.m_StatusMessage.m_Pump1Status],
								  szPumpSpeeds[GlobalCallbackClass.m_StatusMessage.m_Pump2Status]);

						wprintf_s(L"\t");
						DumpHexData(GlobalCallbackClass.m_StatusMessage.m_RawMessage);
					}

					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmConfigResponse])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmConfigResponse] = FALSE;
						wprintf_s(L"Configuration Response: MAC Address %S\n\t",
								  GlobalCallbackClass.m_ConfigResponseMessage.m_strMACAddress.c_str());
						DumpHexData(GlobalCallbackClass.m_ConfigResponseMessage.m_RawMessage);
					}

					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmFilterConfigResponse])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmFilterConfigResponse] = FALSE;
						wprintf_s(L"Filter 1: Start %02d:%02d, %d minutes.\n",
								  GlobalCallbackClass.m_FilterConfigResponse.m_Filter1StartTime.m_Hour,
								  GlobalCallbackClass.m_FilterConfigResponse.m_Filter1StartTime.m_Minute,
								  GlobalCallbackClass.m_FilterConfigResponse.m_uiFilter1Duration);
						if (GlobalCallbackClass.m_FilterConfigResponse.m_fFilter2Enabled)
						{
							wprintf_s(L"Filter 2: Start %02d:%02d, %d minutes.\n",
									  GlobalCallbackClass.m_FilterConfigResponse.m_Filter2StartTime.m_Hour,
									  GlobalCallbackClass.m_FilterConfigResponse.m_Filter2StartTime.m_Minute,
									  GlobalCallbackClass.m_FilterConfigResponse.m_uiFilter2Duration);
						}
						else
						{
							wprintf_s(L"Filter 2 disabled.\n");
						}
					}

					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmVersionInfoResponse])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmVersionInfoResponse] = FALSE;
						wprintf_s(L"Model name: %S, Software ID M%d_%d V%d, Current Setup: %d, Configuration Signature: %08X\n",
								  GlobalCallbackClass.m_VersionInfoResponse.m_strModelName.c_str(),
								  GlobalCallbackClass.m_VersionInfoResponse.SoftwareID[0],
								  GlobalCallbackClass.m_VersionInfoResponse.SoftwareID[1],
								  GlobalCallbackClass.m_VersionInfoResponse.SoftwareID[2],
								  GlobalCallbackClass.m_VersionInfoResponse.CurrentSetup,
								  GlobalCallbackClass.m_VersionInfoResponse.ConfigurationSignature);
					}

					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmVersionInfoResponse])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmVersionInfoResponse] = FALSE;
						wprintf_s(L"ControlConfig2 Response: ");
						DumpHexData(GlobalCallbackClass.m_ControlConfig2Response.m_RawMessage);
					}

					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmUnknownResponse])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmUnknownResponse] = FALSE;

						wprintf_s(L"Unknown Message: ");
						DumpHexData(GlobalCallbackClass.m_RawUnknownMessage);
					}

					if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmNetworkError])
					{
						GlobalCallbackClass.m_fChangedMessages[TestCallback::hmNetworkError] = FALSE;
						wprintf_s(L"Fatal Network Error, restarting Monitor.\n");
						SpaComms.EndMonitor();
						if (!SpaComms.StartMonitor())
						{
							wprintf_s(L"Unable to restart Monitor.\n");
						}
					}
				}
				else
				{
					// Timed out, do nothing.
				}
			} while (!_kbhit());
			_getch();

			//SpaComms.SendToggleRequest(CSpaComms::tsiLights);
		}
	}
	
	wprintf_s(L"Hit a key to exit.\n");
	_getch();

	WSACleanup();
	return 0;
}

