/*****************************************************************************/
/* LPC.cpp                                Copyright (c) Ladislav Zezula 2005 */
/*---------------------------------------------------------------------------*/
/* Demonstration program of using LPC facility                               */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 22.02.05  1.00  Lad  The first version of LPC.cpp                         */
/*****************************************************************************/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS
#include <tchar.h>
#include <conio.h>
#include <stdio.h>
#include <windows.h>

#include "ntdll.h"

#include <map>
//-----------------------------------------------------------------------------
using namespace std;
//-----------------------------------------------------------------------------
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Local structures

#define LPC_COMMAND_REQUEST_NOREPLY  0x00000000
#define LPC_COMMAND_REQUEST_REPLY    0x00000001
#define LPC_COMMAND_STOP             0x00000002

// This is the data structure transferred through LPC.
// Every structure must begin with PORT_MESSAGE, and must NOT be
// greater that MAX_LPC_DATA

typedef struct _TRANSFERRED_MESSAGE
{
    PORT_MESSAGE Header;

    ULONG   Command;
    WCHAR   MessageText[0x60];

} TRANSFERRED_MESSAGE, *PTRANSFERRED_MESSAGE;

typedef struct _TRANSFERRED_MESSAGE_WOW64
{
	PORT_MESSAGE_WOW64 Header;

	ULONG   Command;
	WCHAR   MessageText[0x60];

} TRANSFERRED_MESSAGE_WOW64, *PTRANSFERRED_MESSAGE_WOW64;

typedef BOOL (WINAPI *ISWOW64PROCESS) (HANDLE, PBOOL);

//-----------------------------------------------------------------------------
// Local variables

LPWSTR LpcPortName = L"\\TestLpcPortName";      // Name of the LPC port
                                                // Must be in the form of "\\name"
HANDLE g_hHeap = NULL;                          // Application heap

//-----------------------------------------------------------------------------
// Server and client thread for testing small LPC messages

DWORD WINAPI ServerThread1(LPVOID parameter)
{
    SECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa;              // Object attributes for the name
    UNICODE_STRING portname;
    NTSTATUS status;
    HANDLE hport = NULL;
	HANDLE hserver;
	// 数据不需要太大
    BYTE buffer[0x100];
	//map<unsigned long long, void *> *servers;
	//map<unsigned long long, void *>::iterator it;
	unsigned long long threadid;
    BOOL WeHaveToStop = FALSE;
    int nError;
	int wow64 = (int)parameter;

	//servers = new map<unsigned long long, void *>;

	//
		// Initialize security descriptor that will be set to
		// "Everyone has the full access"
		//

	if (InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
	{
		//
		// Set the empty DACL to the security descriptor
		//

		if (SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
		{
			//
			// Initialize attributes for the port and create it
			//

			RtlInitUnicodeString(&portname, LpcPortName);
			InitializeObjectAttributes(&oa, &portname, 0, NULL, &sd);

			_tprintf(_T("Server: Creating LPC port \"%s\" (NtCreatePort) ...\n"), LpcPortName);
			status = NtCreatePort(&hport,
				&oa,
				NULL,
				sizeof(PORT_MESSAGE) + MAX_LPC_DATA,
				0);
			_tprintf(_T("Server: NtCreatePort result 0x%08lX\n"), status);

			if (NT_SUCCESS(status))
			{
				//
				// Process all incoming LPC messages
				//

				PTRANSFERRED_MESSAGE pmessage;
				PTRANSFERRED_MESSAGE_WOW64 pmessage_wow64;

				//
				// Create the data buffer for the request
				//

				pmessage = (PTRANSFERRED_MESSAGE)buffer;
				pmessage_wow64 = (PTRANSFERRED_MESSAGE_WOW64)buffer;

				while (WeHaveToStop == FALSE)
				{
					// 没错, listen 是多余的
					status = NtReplyWaitReceivePort(hport, NULL, NULL, (PPORT_MESSAGE)pmessage);
					// STATUS_INVALID_HANDLE: win2k without admin rights will perform an endless loop here
					// STATUS_NOT_IMPLEMENTED
					if (status == 0xC0000002L || status == STATUS_INVALID_HANDLE)
					{
						break;
					}
					if (NT_SUCCESS(status))
					{
						switch (wow64 ? pmessage_wow64->Header.u2.s2.Type : pmessage->Header.u2.s2.Type)
						{
						case LPC_CONNECTION_REQUEST:
							status = NtAcceptConnectPort(&hserver, NULL, (PPORT_MESSAGE)pmessage, TRUE, NULL, NULL);
							if (NT_SUCCESS(status))
							{
								//
								// Complete the connection
								//
								status = NtCompleteConnectPort(hserver);
								if (NT_SUCCESS(status))
								{
									// 这里获取的hserver，之后不需要进行操作, 只需要保存

									threadid = wow64 ? pmessage_wow64->Header.ClientId.UniqueThread : (unsigned long long)pmessage->Header.ClientId.UniqueThread;

									//servers->insert(pair<unsigned long long, void *>(threadid, (void *)hserver));
								}
								else
								{
									NtClose(hserver);
								}
							}
							break;
						case LPC_REQUEST:
							// If a request has been received, answer to it.
							switch (wow64 ? pmessage_wow64->Command : pmessage->Command)
							{
							case LPC_COMMAND_REQUEST_NOREPLY:
								_tprintf(_T("Server: Received request \"%s\"\n"), wow64 ? pmessage_wow64->MessageText : pmessage->MessageText);
								break;      // Nothing more to do

							case LPC_COMMAND_REQUEST_REPLY:
								_tprintf(_T("Server: Received request \"%s\"\n"), wow64 ? pmessage_wow64->MessageText : pmessage->MessageText);
								_tprintf(_T("Server: Sending reply (NtReplyPort) ...\n"), LpcPortName);
								status = NtReplyPort(hport, (PPORT_MESSAGE)pmessage);
								_tprintf(_T("Server: NtReplyPort result 0x%08lX\n"), status);
								break;

							case LPC_COMMAND_STOP:      // Stop the work
								_tprintf(_T("Server: Stopping ...\n"));
								WeHaveToStop = TRUE;
								break;
							}
							break;
						case LPC_PORT_CLOSED:
						case LPC_CLIENT_DIED:
							threadid = wow64 ? pmessage_wow64->Header.ClientId.UniqueThread : (unsigned long long)pmessage->Header.ClientId.UniqueThread;
							//it = servers->find(threadid);
							//if (it != servers->end())
							{
								//NtClose((HANDLE)it->second);
							}
							break;
						default:
							break;
						}
					}

				}

				//it = servers->begin();
				//while (it != servers->end())
				{
					//NtClose((HANDLE)it->second);

					//it = servers->erase(it);
				}

				NtClose(hport);
			}
		}
	}

	//delete servers;
    
    return 0;
}


DWORD WINAPI ClientThread1(LPVOID parameter)
{
    SECURITY_QUALITY_OF_SERVICE sqos;
	unsigned char request_buffer[0x100];
	unsigned char reply_buffer[0x100];
    PTRANSFERRED_MESSAGE prequest_message = (PTRANSFERRED_MESSAGE)request_buffer;
    PTRANSFERRED_MESSAGE preply_message = (PTRANSFERRED_MESSAGE)reply_buffer;
	PTRANSFERRED_MESSAGE_WOW64 prequest_message_wow64 = (PTRANSFERRED_MESSAGE_WOW64)request_buffer;
	PTRANSFERRED_MESSAGE_WOW64 preply_message_wow64 = (PTRANSFERRED_MESSAGE_WOW64)reply_buffer;
    UNICODE_STRING portname;
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE hport;
    ULONG MaxMessageLength = 0;
	int wow64 = (int)parameter;

	//
	// Allocate space for message to be transferred through LPC
	//

	RtlInitUnicodeString(&portname, LpcPortName);
	sqos.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
	sqos.ImpersonationLevel = SecurityImpersonation;
	sqos.EffectiveOnly = FALSE;
	sqos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;

	_tprintf(_T("Client: Test sending LPC data of size less than 0x%lX bytes ...\n"), MAX_LPC_DATA);
	_tprintf(_T("Client: Connecting to port \"%s\" (NtConnectPort) ...\n"), LpcPortName);
	status = NtConnectPort(&hport,
		&portname,
		&sqos,
		NULL,
		NULL,
		&MaxMessageLength,
		NULL,
		NULL);
	_tprintf(_T("Client: NtConnectPort result 0x%08lX\n"), status);
	if (NT_SUCCESS(status))
	{
		//
		// Initialize the request header, reply header and fill request text
		//

		int ss = 0;

		// 连接上之后先发送第一个数据
		// 先随便一个长度
		if (wow64)
		{
			// 
			InitializeMessageHeaderWOW64(&prequest_message_wow64->Header, 0x60, 0);
			wsprintf(prequest_message_wow64->MessageText, L"Message text through LPC %d", ss);
			prequest_message_wow64->Command = LPC_COMMAND_REQUEST_REPLY;
		}
		else
		{
			InitializeMessageHeader(&prequest_message->Header, 0x60, 0);
			wsprintf(prequest_message->MessageText, L"Message text through LPC %d", ss);
			prequest_message->Command = LPC_COMMAND_REQUEST_REPLY;
		}
		ss++;
		// preply_message 不需要初始化
		while (NT_SUCCESS(status = NtRequestWaitReplyPort(hport, (PPORT_MESSAGE)prequest_message, (PPORT_MESSAGE)preply_message)))
		{
			switch (wow64 ? preply_message_wow64->Header.u2.s2.Type : preply_message->Header.u2.s2.Type)
			{
			case LPC_CONNECTION_REQUEST:
				break;
			case LPC_REQUEST:
				// If a request has been received, answer to it.
				switch (wow64 ? preply_message_wow64->Command : preply_message->Command)
				{
				case LPC_COMMAND_REQUEST_NOREPLY:
					break;      // Nothing more to do

				case LPC_COMMAND_REQUEST_REPLY:
					break;

				case LPC_COMMAND_STOP:      // Stop the work
					break;
				}
				break;
			case LPC_PORT_CLOSED:
			case LPC_CLIENT_DIED:
				break;
			default:
				break;
			}

			if (wow64)
			{
				// 
				InitializeMessageHeaderWOW64(&prequest_message_wow64->Header, 0x60, 0);
				wsprintf(prequest_message_wow64->MessageText, L"Message text through LPC %d", ss);
				prequest_message_wow64->Command = LPC_COMMAND_REQUEST_REPLY;
			}
			else
			{
				InitializeMessageHeader(&prequest_message->Header, 0x60, 0);
				wsprintf(prequest_message->MessageText, L"Message text through LPC %d", ss);
				prequest_message->Command = LPC_COMMAND_REQUEST_REPLY;
			}
			ss++;
		}

		NtClose(hport);
	}

    return 0;
}
//-----------------------------------------------------------------------------
// Server and client thread for testing large LPC messages

#define LARGE_MESSAGE_SIZE 0x9000

DWORD WINAPI ServerThread2(LPVOID)
{
    OBJECT_ATTRIBUTES ObjAttr;
    REMOTE_PORT_VIEW ClientView;
    UNICODE_STRING PortName;
    LARGE_INTEGER SectionSize = {LARGE_MESSAGE_SIZE};
    PORT_MESSAGE MessageHeader;
    PORT_VIEW ServerView;
    NTSTATUS Status;
    HANDLE LpcPortHandle = NULL;
    HANDLE SectionHandle = NULL;
    HANDLE ServerHandle = NULL;

    __try   // try-finally
    {
        //
        // Create a memory section in the pagefile used for transfer the data
        // to the client
        //

        _tprintf(_T("Server: Creating section for transferring data (NtCreateSection) ...\n"));
        Status = NtCreateSection(&SectionHandle,
                                  SECTION_MAP_READ | SECTION_MAP_WRITE,
                                  NULL,     // Backed by the pagefile
                                 &SectionSize,
                                  PAGE_READWRITE,
                                  SEC_COMMIT,
                                  NULL);
        _tprintf(_T("Server: NtCreateSection result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Create the named port
        //

        RtlInitUnicodeString(&PortName, LpcPortName);
        InitializeObjectAttributes(&ObjAttr, &PortName, 0, NULL, NULL);
        _tprintf(_T("Server: Creating port \"%s\" (NtCreatePort) ...\n"), LpcPortName);
        Status = NtCreatePort(&LpcPortHandle,
                              &ObjAttr,
                               NULL,
                               sizeof(PORT_MESSAGE),
                               0);
        _tprintf(_T("Server: NtCreatePort result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Listen the port and receive request
        //

        _tprintf(_T("Server: Listening to port (NtListenPort) ...\n"), LpcPortName);
        Status = NtListenPort(LpcPortHandle, &MessageHeader);
        _tprintf(_T("Server: NtCreatePort result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;


        //
        // Fill local and remote memory views. When the LPC
        // message comes to the client, the section will be remapped
        // to be accessible to the listener, even if the client is in another
        // process or different processor mode (UserMode/KernelMode)
        //

        ServerView.Length        = sizeof(PORT_VIEW);
        ServerView.SectionHandle = SectionHandle;
        ServerView.SectionOffset = 0;
        ServerView.ViewSize      = LARGE_MESSAGE_SIZE;
        ClientView.Length        = sizeof(REMOTE_PORT_VIEW);

        //
        // Accept the port connection and receive the client's view
        //

        _tprintf(_T("Server: Accepting connection (NtAcceptConnectPort) ...\n"));
        Status = NtAcceptConnectPort(&ServerHandle,
                                      NULL,
                                     &MessageHeader,
                                      TRUE,
                                     &ServerView,
                                     &ClientView);
        _tprintf(_T("Server: NtAcceptConnectPort result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Complete the connection
        //

        _tprintf(_T("Server: Completing connection (NtCompleteConnectPort) ...\n"));
        Status = NtCompleteConnectPort(ServerHandle);
        _tprintf(_T("Server: NtCompleteConnectPort result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Now (wait for and) accept the data request coming from the port.
        //

        _tprintf(_T("Server: Replying, waiting for receive (NtReplyWaitReceivePort) ...\n"));
        Status = NtReplyWaitReceivePort(ServerHandle,
                                        NULL,
                                        NULL,
                                       &MessageHeader);
        _tprintf(_T("Server: NtReplyWaitReceivePort result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Now, when the connection is done, is time for giving data to the client.
        // Just fill the entire client's data with 'S'
        //

        wcscpy((PWSTR)ClientView.ViewBase, L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n"
                                           L"This is a long reply from the server\n");

        //
        // Reply to the message
        //

        _tprintf(_T("Server: Replying (NtReplyPort) ...\n"));
        Status = NtReplyPort(LpcPortHandle, &MessageHeader);
        _tprintf(_T("Server: NtReplyPort result 0x%08lX\n"), Status);
    }
    __finally
    {
        if(ServerHandle != NULL)
            NtClose(ServerHandle);
        if(LpcPortHandle != NULL)
            NtClose(LpcPortHandle);
        if(SectionHandle != NULL)
            NtClose(SectionHandle);
    }

    return 0;
}


DWORD WINAPI ClientThread2(LPVOID)
{
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    REMOTE_PORT_VIEW ServerView;
    UNICODE_STRING PortName;
    LARGE_INTEGER SectionSize = {LARGE_MESSAGE_SIZE};
    PORT_MESSAGE MessageHeader;
    PORT_VIEW ClientView;
    NTSTATUS Status = STATUS_SUCCESS;
    HANDLE SectionHandle = NULL;
    HANDLE PortHandle = NULL;

    __try
    {
        //
        // Create a memory section in the pagefile used for transfer the data
        // to the client
        //

        _tprintf(_T("Client: Test sending LPC data with size of %lX ...\n"), LARGE_MESSAGE_SIZE);
        _tprintf(_T("Client: Creating section for transferring data (NtCreateSection) ...\n"));
        Status = NtCreateSection(&SectionHandle,
                                  SECTION_MAP_READ | SECTION_MAP_WRITE,
                                  NULL,         // Backed by the pagefile
                                 &SectionSize,
                                  PAGE_READWRITE,
                                  SEC_COMMIT,
                                  NULL);
        _tprintf(_T("Client: NtCreateSection result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Initialize the parameters of LPC port
        //

        RtlInitUnicodeString(&PortName, LpcPortName);
        SecurityQos.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
        SecurityQos.ImpersonationLevel = SecurityImpersonation;
        SecurityQos.EffectiveOnly = FALSE;
        SecurityQos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;

        //
        // Fill local and remote memory view. When the LPC
        // message comes to the listener, the section will be remapped
        // to be accessible to the listener, even if the listener is in another
        // process or different processor mode (UserMode/KernelMode)
        //

        ClientView.Length        = sizeof(PORT_VIEW);
        ClientView.SectionHandle = SectionHandle;
        ClientView.SectionOffset = 0;
        ClientView.ViewSize      = LARGE_MESSAGE_SIZE;
        ServerView.Length        = sizeof(REMOTE_PORT_VIEW);

        //
        // Connect to the port
        //

        _tprintf(_T("Client: Connecting to port \"%s\" (NtConnectPort) ...\n"), LpcPortName);
        Status = NtConnectPort(&PortHandle,
                               &PortName,
                               &SecurityQos,
                               &ClientView,
                               &ServerView,
                                0,
                                NULL,
                                NULL);
        _tprintf(_T("Client: NtConnectPort result 0x%08lX\n"), Status);
        if(!NT_SUCCESS(Status))
            __leave;

        //
        // Initialize the request header. Give data to the server
        //

        InitializeMessageHeader(&MessageHeader, sizeof(PORT_MESSAGE), 0);
        wcscpy((PWSTR)ServerView.ViewBase, L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n"
                                           L"This is a long message data from the client\n");

        //
        // Send the data request, and wait for reply
        //

        _tprintf(_T("Client: Sending request, waiting for reply (NtRequestWaitReplyPort)\n"));
        Status = NtRequestWaitReplyPort(PortHandle, &MessageHeader, &MessageHeader);
        _tprintf(_T("Client: NtRequestWaitReplyPort result 0x%08lX\n"), Status);
    }

    __finally
    {
        if(PortHandle != NULL)
            NtClose(PortHandle);

        if(SectionHandle != NULL)
            NtClose(SectionHandle);
    }

    return 0;
}

static BOOL Is32BitProcessUnderWOW64()
{
    ISWOW64PROCESS pfnIsWoW64Process = NULL;
    HMODULE hKernel32 = GetModuleHandle(_T("Kernel32.dll"));
    BOOL bIsWow64Process = FALSE;

    if(hKernel32 != NULL)
    {
        pfnIsWoW64Process = (ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");

        if(pfnIsWoW64Process != NULL)
            pfnIsWoW64Process(GetCurrentProcess(), &bIsWow64Process);
    }

    return bIsWow64Process;
}

//-----------------------------------------------------------------------------
// main

int _tmain(void)
{
    HANDLE hThread[2];
    DWORD dwThreadId;

    //
    // Save the handle of the main heap
    //
    
    g_hHeap = GetProcessHeap();

    //
    // Note for 64-bit Windows: Functions that use PORT_MESSAGE structure
    // as one of the parameters DO NOT WORK under WoW64. The reason is that
    // the layer between 32-bit NTDLL and 64-bit NTDLL does not translate
    // the structure to its 64-bit layout and most of the functions
    // fail with STATUS_INVALID_PARAMETER (0xC000000D)
    //

	int wow64 = Is32BitProcessUnderWOW64();

    //
    // Run both threads testing small LPC messages
    //
  
    hThread[0] = CreateThread(NULL, 0, ServerThread1, (LPVOID)wow64, 0, &dwThreadId);
	Sleep(1000);
    hThread[1] = CreateThread(NULL, 0, ClientThread1, (LPVOID)wow64, 0, &dwThreadId);
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);
  
    //
    // Run both threads testing large LPC messages
    //
/*
    hThread[0] = CreateThread(NULL, 0, ServerThread2, NULL, 0, &dwThreadId);
    hThread[1] = CreateThread(NULL, 0, ClientThread2, NULL, 0, &dwThreadId);
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);
*/
    _getch();
    return 0;
}

