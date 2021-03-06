// BalBoaSpa.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#pragma comment(lib, "Ws2_32.lib")

#include <conio.h>
#include <crtdbg.h>

using std::mutex;
using std::lock_guard;


void Log(const WCHAR *pFormat, ...)
{
	//  Whoops, we leak this.  Oh well.
	static FILE *fhLogFile = NULL;
	const size_t BufferSize = 1024;

	if (fhLogFile == NULL)
	{
		_wfopen_s(&fhLogFile, L"SpaLogFile.txt", L"wtS,ccs=UNICODE");
	}

	WCHAR szBuffer[BufferSize];
	va_list var_args;

	va_start(var_args, pFormat);

	_vsnwprintf_s(szBuffer, BufferSize, _TRUNCATE, pFormat, var_args);

	wprintf(szBuffer);
	if (fhLogFile != NULL)
	{
		fputws(szBuffer, fhLogFile);
	}
}


void DumpHexData(
	const CByteArray &array)
{
	for (auto pByte = array.cbegin(); pByte < array.cend(); pByte++)
	{
		BYTE c = *pByte;
		Log(L"%02x ", c);
	}
	Log(L"\n");
}


class TestCallback:
	public IMonitorCallback
{
public:
	TestCallback();
	~TestCallback();

	BOOL AwaitMessage(DWORD dwTimeout);

private:
	//  All called by the monitoring class through the base interface.
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

		HM_COUNT
	};

	BOOL m_fChangedMessages[HM_COUNT];
	mutex m_mutex;
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
	lock_guard<mutex> lg(m_mutex);

	_ASSERT(!m_fChangedMessages[hmStatus]);

	m_StatusMessage = Message;

	m_fChangedMessages[hmStatus] = TRUE;
	SetEvent(m_hEvent);
}

void
TestCallback::ProcessConfigResponse(
	const ConfigResponseMessage &Message)
{
	lock_guard<mutex> lg(m_mutex);

	_ASSERT(!m_fChangedMessages[hmConfigResponse]);

	//
	// Filter out sequential duplicates
	if (m_ConfigResponseMessage.m_RawMessage != Message.m_RawMessage)
	{
		m_ConfigResponseMessage = Message;
		m_fChangedMessages[hmConfigResponse] = TRUE;
		SetEvent(m_hEvent);
	}
}


void
TestCallback::ProcessFilterConfigResponse(
	const FilterConfigResponseMessage &Message)
{
	lock_guard<mutex> lg(m_mutex);

	_ASSERT(!m_fChangedMessages[hmFilterConfigResponse]);

	if (m_FilterConfigResponse.m_RawMessage != Message.m_RawMessage)
	{
		m_FilterConfigResponse = Message;
		m_fChangedMessages[hmFilterConfigResponse] = TRUE;
		SetEvent(m_hEvent);
	}
}


void
TestCallback::ProcessVersionInfoResponse(
	const VersionInfoResponseMessage &Message)
{
	lock_guard<mutex> lg(m_mutex);

	_ASSERT(!m_fChangedMessages[hmVersionInfoResponse]);

	if (m_VersionInfoResponse.m_RawMessage != Message.m_RawMessage)
	{
		m_VersionInfoResponse = Message;
		m_fChangedMessages[hmVersionInfoResponse] = TRUE;
		SetEvent(m_hEvent);
	}
}

void TestCallback::ProcessControlConfig2Response(
	const ControlConfig2ResponseMessage &Message)
{
	lock_guard<mutex> lg(m_mutex);

	_ASSERT(!m_fChangedMessages[hmControlConfig2Response]);

	if (m_ControlConfig2Response.m_RawMessage != Message.m_RawMessage)
	{
		m_ControlConfig2Response = Message;
		m_fChangedMessages[hmControlConfig2Response] = TRUE;
		SetEvent(m_hEvent);
	}
}

void TestCallback::ProcessUnknownMessageRaw(const CByteArray &Message)
{
	lock_guard<mutex> lg(m_mutex);

	m_RawUnknownMessage = Message;
	m_fChangedMessages[hmUnknownResponse] = TRUE;
	SetEvent(m_hEvent);
}



void
TestCallback::Dispose()
{
	//Global object, no cleanup needed.
}

void
TestCallback::OnFatalError()
{}


TestCallback GlobalCallbackClass;

volatile BOOL fStartExercizeThread = FALSE;

void __cdecl
ExercizeThread(void *pArg)
{
	CSpaComms *pSpa = (CSpaComms *)pArg;

	while (!fStartExercizeThread)
	{ 
		Sleep(1000);
	}

	//pSpa->SendToggleRequest(CSpaComms::tsiPump1);
	Sleep(500);

	pSpa->SendToggleRequest(CSpaComms::tsiPump2);
	Sleep(500);

	//pSpa->SendToggleRequest(CSpaComms::tsiLights);
	Sleep(500);

	return ;
}

const WCHAR * szPumpSpeeds[] = {L" Off", L" Low", L"High"};

int main()
{
	WSADATA wsaData;

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0)
	{
		Log(L"WSAStartup failed: %d\n", iResult);
		return 1;
	}

	SpaAddressVector Spas;

	if (!DiscoverSpas(Spas))
	{
		Log(L"Error discovering spas\n");
	}

	Log(L"Discovered %lld spas.\n", Spas.size());

	for (auto pSpa = Spas.cbegin(); pSpa < Spas.cend(); pSpa++)
	{
		TCHAR szIpAddr[64];

		InetNtop(pSpa->m_SpaAddress.sin_family, &(pSpa->m_SpaAddress.sin_addr), szIpAddr, sizeof(szIpAddr) / sizeof(szIpAddr[0]));

		Log(L"\t MAC: %S, IP:%s\n", pSpa->m_strMACAddress.c_str(), szIpAddr);
	}

	if (Spas.size() > 0)
	{
		auto FirstSpaAddress = *(Spas.begin());

		Log(L"Connecting to spa with MAC Address: %S\n", FirstSpaAddress.m_strMACAddress.c_str());

		CSpaComms SpaComms(FirstSpaAddress, &GlobalCallbackClass);

		if (!SpaComms.StartMonitor())
		{
			Log(L"StartMonitor() failed.\n");
		}

		SpaComms.SendConfigRequest();
		SpaComms.SendFilterConfigRequest();
		SpaComms.SendVerInfoRequest();
		SpaComms.SendControlConfig2Request();
		
		_beginthread(ExercizeThread, 0, &SpaComms);
		//  Allow previous messages to arrive before processing
		Sleep(250);
		
		do
		{
			if (GlobalCallbackClass.AwaitMessage(100))
			{
				lock_guard<mutex> lg(GlobalCallbackClass.m_mutex);

				if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmConfigResponse])
				{
					GlobalCallbackClass.m_fChangedMessages[TestCallback::hmConfigResponse] = FALSE;
					Log(L"Configuration Response: MAC Address %S\n\t",
							  GlobalCallbackClass.m_ConfigResponseMessage.m_strMACAddress.c_str());
					DumpHexData(GlobalCallbackClass.m_ConfigResponseMessage.m_RawMessage);
				}

				if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmFilterConfigResponse])
				{
					GlobalCallbackClass.m_fChangedMessages[TestCallback::hmFilterConfigResponse] = FALSE;
					Log(L"Filter 1: Start %02d:%02d, %d min.  ",
							  GlobalCallbackClass.m_FilterConfigResponse.m_Filter1StartTime.m_Hour,
							  GlobalCallbackClass.m_FilterConfigResponse.m_Filter1StartTime.m_Minute,
							  GlobalCallbackClass.m_FilterConfigResponse.m_uiFilter1Duration);
					if (GlobalCallbackClass.m_FilterConfigResponse.m_fFilter2Enabled)
					{
						Log(L"Filter 2: Start %02d:%02d, %d min.\n",
								  GlobalCallbackClass.m_FilterConfigResponse.m_Filter2StartTime.m_Hour,
								  GlobalCallbackClass.m_FilterConfigResponse.m_Filter2StartTime.m_Minute,
								  GlobalCallbackClass.m_FilterConfigResponse.m_uiFilter2Duration);
					}
					else
					{
						Log(L"Filter 2 disabled.\n");
					}
					Log(L"\t");
					DumpHexData(GlobalCallbackClass.m_FilterConfigResponse.m_RawMessage);
				}

				if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmVersionInfoResponse])
				{
					GlobalCallbackClass.m_fChangedMessages[TestCallback::hmVersionInfoResponse] = FALSE;
					Log(L"Model name: %S, Software ID M%d_%d V%d, Current Setup: %d, Configuration Signature: %08X\n",
							  GlobalCallbackClass.m_VersionInfoResponse.m_strModelName.c_str(),
							  GlobalCallbackClass.m_VersionInfoResponse.SoftwareID[0],
							  GlobalCallbackClass.m_VersionInfoResponse.SoftwareID[1],
							  GlobalCallbackClass.m_VersionInfoResponse.SoftwareID[2],
							  GlobalCallbackClass.m_VersionInfoResponse.CurrentSetup,
							  GlobalCallbackClass.m_VersionInfoResponse.ConfigurationSignature);
					Log(L"\t");
					DumpHexData(GlobalCallbackClass.m_VersionInfoResponse.m_RawMessage);
				}

				if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmControlConfig2Response])
				{
					GlobalCallbackClass.m_fChangedMessages[TestCallback::hmControlConfig2Response] = FALSE;
					Log(L"ControlConfig2 Response:\n\t");
					DumpHexData(GlobalCallbackClass.m_ControlConfig2Response.m_RawMessage);
				}

				if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmUnknownResponse])
				{
					GlobalCallbackClass.m_fChangedMessages[TestCallback::hmUnknownResponse] = FALSE;

					Log(L"Unknown Message:\n\t");
					DumpHexData(GlobalCallbackClass.m_RawUnknownMessage);
				}

				if (GlobalCallbackClass.m_fChangedMessages[TestCallback::hmStatus])
				{
					GlobalCallbackClass.m_fChangedMessages[TestCallback::hmStatus] = FALSE;
					Log(L"Spa Status: Time %02d:%02d, Temp %d/%d %s, Range: %s, Heat: %s, Lights: %s, Pump 1:%s, Pump 2:%s\n",
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

					Log(L"\t");
					DumpHexData(GlobalCallbackClass.m_StatusMessage.m_RawMessage);

					fStartExercizeThread = TRUE;

					//  Something changed, see if anything else did
					SpaComms.SendConfigRequest();
					SpaComms.SendFilterConfigRequest();
					SpaComms.SendVerInfoRequest();
					SpaComms.SendControlConfig2Request();
				}
			}
			else
			{
				// Timed out, do nothing.
			}
		} while (!_kbhit());
		_getch();
	}

	wprintf_s(L"Hit a key to exit.\n");
	_getch();

	WSACleanup();
	return 0;
}

