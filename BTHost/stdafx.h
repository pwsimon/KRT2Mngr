// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

// #define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // MFC support for ribbons and control bars
#include <afxdhtml.h>        // HTML Dialogs

#include <bluetoothapis.h>
#include <ws2bth.h>
#include <ws2tcpip.h>
#define KRT2INPUT_SERVER   _T("ws-psi.estos.de")
#define KRT2INPUT_PORT     27015 // TCPEchoServer 27015, 1 Bluetooth SerialPort, 80 any HTTP server
#define TOSTR(s) #s
// #define KRT2INPUT_PATH     "/krt2mngr/comhost/krt2input.bin"
// #define KRT2INPUT_BT       _T("GT-I9300") // 0C:14:20:4A:1F:AD
// #define KRT2INPUT_BT       _T("KRT21885") // 98:D3:31:FD:5A:F2

// function behind btnSoft1
// #define BTNSOFT1_ENUMPROTOCOLS
// #define BTNSOFT1_ENUMRADIO
// #define BTNSOFT1_DISCOVERDEVICE
#define BTNSOFT1_DISCOVERSERVICE

// function behind btnSoft2
#define BTNSOFT2_CONNECT
// #define BTNSOFT2_KRT2PING

// either READ_THREAD, IOALERTABLE or WSAASYNCSELECT
// #define READ_THREAD
// #define IOALERTABLE
/*
* https://support.microsoft.com/en-us/help/181611/socket-overlapped-i-o-versus-blocking-nonblocking-mode
*   WSAAsyncSelect maps socket notifications to Windows messages and is the best model for a single threaded GUI application.
*/
#define TESTCASE_NONBLOCKING
// #define WSAASYNCSELECT

// either SEND_ASYNC or not
// #define SEND_ASYNC

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
