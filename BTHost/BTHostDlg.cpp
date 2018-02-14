// BTHostDlg.cpp : implementation file
//

#include "stdafx.h"
#include "BTHost.h"
#include "BTHostDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CAboutDlg dialog used for App About
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CBTHostDlg dialog
#define WM_USER_ACK             (WM_USER + 1) // posted from BTReadThread
#define WM_USER_RXSINGLEBYTE    (WM_USER + 2)
#define WM_USER_RXDECODEDCMD    (WM_USER + 3)

#define SEND_TIMEOUT            100
#define STX                     0x02
#define ACK                     0x06
#define NAK                     0x15

#ifdef IOALERTABLE
/*static*/ WSAOVERLAPPED CBTHostDlg::m_RecvOverlappedCompletionRoutine;
/*static*/ char CBTHostDlg::m_buf[0x01];
/*static*/ WSABUF CBTHostDlg::m_readBuffer = { _countof(CBTHostDlg::m_buf), CBTHostDlg::m_buf };
#endif

/*static*/ SOCKET CBTHostDlg::m_socketLocal = INVALID_SOCKET;

BEGIN_DHTML_EVENT_MAP(CBTHostDlg)
	// DHTML_EVENT_ONCLICK(_T("btnSoft1"), OnSendPing) // soft buttons
	DHTML_EVENT_ONCLICK(_T("btnSoft1"), OnDiscoverService)
	DHTML_EVENT_ONCLICK(_T("btnSoft2"), OnConnect)
END_DHTML_EVENT_MAP()

BEGIN_MESSAGE_MAP(CBTHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
	ON_MESSAGE(WM_USER_RXSINGLEBYTE, OnRXSingleByte)
END_MESSAGE_MAP()

/*
* siehe auch:
*   C:\Users\psi\Source\Repos\KRT2Mngr\BTHost\BTHost.htm(121): window.external.txBytes()
*   C:\Users\psi\Source\Repos\KRT2Mngr\winExtByteLevel.js(57)
*/
BEGIN_DISPATCH_MAP(CBTHostDlg, CDHtmlDialog)
	DISP_FUNCTION(CBTHostDlg, "txBytes", txBytes, VT_EMPTY, VTS_BSTR)
END_DISPATCH_MAP()

CBTHostDlg::CBTHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_BTHOST_DIALOG, IDR_HTML_BTHOST_DIALOG, pParent)
{
#ifdef _DEBUG
	m_dwThreadAffinity = ::GetCurrentThreadId();
#endif

	EnableAutomation();
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_iRetCWSAStartup = -1;
	m_addrKRT2 = {
		AF_BTH, // USHORT addressFamily;
		BTH_ADDR_NULL, // BTH_ADDR btAddr;
		CLSID_NULL, // GUID serviceClassId;
		0UL // ULONG port;
	};

#ifdef KRT2INPUT_BT
	/*
	* Setting address family to AF_BTH indicates winsock2 to use Bluetooth sockets
	* Port should be set to 0 if ServiceClassId is specified.
	*/
	m_addrKRT2.addressFamily = AF_BTH;
	m_addrKRT2.btAddr = ((ULONGLONG)0x000098d331fd5af2); // KRT21885, Dev B => (98:D3:31:FD:5A:F2)
	m_addrKRT2.serviceClassId = CLSID_NULL; // SerialPortServiceClass_UUID
	m_addrKRT2.port = 1UL;
#endif

#ifdef KRT2INPUT_PORT
	m_addrSimulator.sin_family = AF_INET;
	// BTSample\TCPEchoServer, https://msdn.microsoft.com/de-de/library/windows/desktop/ms737593(v=vs.85).aspx
	m_addrSimulator.sin_port = ::htons(KRT2INPUT_PORT);
	// m_addrSimulator.sin_addr.s_addr = ::inet_addr("127.0.0.1"); // stdafx.h(18) #define _WINSOCK_DEPRECATED_NO_WARNINGS or
	switch (::InetPton(AF_INET, L"127.0.0.1", &m_addrSimulator.sin_addr)) // wir wollen explicit eine IPv4 addresse
	{
	case 0: // invalid (String encoded) address
		break;
	case 1: // succeeded
		break;
	default:
		::WSAGetLastError();
		break;
	}
#endif

#ifdef READ_THREAD
	m_ReadThreadArgs.hwndMainDlg = NULL;
	m_ReadThreadArgs.socketLocal = INVALID_SOCKET;
	m_ReadThreadArgs.hEvtTerminate = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hReadThread = INVALID_HANDLE_VALUE;
#endif
}

/*virtual*/void CBTHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

/*virtual*/ BOOL CBTHostDlg::OnInitDialog()
{
	/*
	* wir navigieren zu einer externen resource
	* script fehler? welche zone? welche rechte?
	*   siehe auch:
	*     COMHost.cpp(44): CCOMHostApp::InitInstance()
	*     HKEY_CURRENT_USER\SOFTWARE\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION\COMHost.exe (REG_DWORD) 11000
	*/
	// m_nHtmlResID = 0;
	// m_strCurrentUrl = _T("http://localhost/krt2mngr/comhost/comhost.htm"); // ohne fehler
	m_strCurrentUrl = _T("http://ws-psi.estos.de/krt2mngr/sevenseg.html"); // need browser_emulation
	CDHtmlDialog::OnInitDialog();
	SetExternalDispatch((LPDISPATCH)GetInterface(&IID_IDispatch));

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	WORD wVersionRequested = MAKEWORD(2, 2);
	// WORD wVersionRequested = MAKEWORD(1, 1); // for Overlapped Model
	WSADATA wsaData;
	m_iRetCWSAStartup = ::WSAStartup(wVersionRequested, &wsaData);
	if (0 == m_iRetCWSAStartup)
	{
		_ASSERT(LOBYTE(wsaData.wVersion) == 2 || HIBYTE(wsaData.wVersion) == 2);
	}
	else
		CBTHostDlg::ShowWSALastError(_T("WSAStartup"));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

/*virtual*/ void CBTHostDlg::OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR szUrl)
{
	CDHtmlDialog::OnDocumentComplete(pDisp, szUrl);

	m_spHtmlDoc->get_Script(&m_ddScript.p);

	// SetElementProperty(_T("btnSoft1"), DISPID_VALUE, &CComVariant(L"Check hurtz")); // for <input> elements
	// SetElementText(_T("btnSoft1"), _T("SendPing")); // OnSendPing
	// SetElementText(_T("btnSoft1"), _T("DiscoverDevice")); // OnDiscoverDevice
	SetElementText(_T("btnSoft1"), _T("DiscoverService")); // DiscoverService
	SetElementText(_T("btnSoft2"), _T("Connect")); // OnConnect
}

/*virtual*/ BOOL CBTHostDlg::IsExternalDispatchSafe()
{
	return TRUE; // __super::IsExternalDispatchSafe();
}

#if (27015 == KRT2INPUT_PORT)
/*
* das ist nur eine debugging hilfe
*
* ich haette es lieber als ON_WM_CHAR bearbeitet aber da liegt die CDHtmlDialog klasse dazwischen
* und der handler wird nicht aufgerufen. auch ein zusaetzliches OnGetDlgCode() hilft nix.
*/
STDMETHODIMP CBTHostDlg::TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID)
{
	if (lpMsg->message == WM_CHAR)
	{
		char ucBuffer[1];
		if(0x30 <= LOBYTE(LOWORD(lpMsg->wParam)) && 0x39 >= LOBYTE(LOWORD(lpMsg->wParam)))
			ucBuffer[0] = LOBYTE(LOWORD(lpMsg->wParam)) - 0x30;
		else
			ucBuffer[0] = LOBYTE(LOWORD(lpMsg->wParam));

		ATLTRACE2(atlTraceGeneral, 0, _T("SendChar: %hc\n"), *ucBuffer);
		if (SOCKET_ERROR == ::send(CBTHostDlg::m_socketLocal, ucBuffer, 1, 0))
			CBTHostDlg::ShowWSALastError(_T("::send(socketLocal, ...)"));
	}

	return __super::TranslateAccelerator(lpMsg, pguidCmdGroup, nCmdID);
}
#endif

// CBTHostDlg message handlers
void CBTHostDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDHtmlDialog::OnSysCommand(nID, lParam);
	}
}

void CBTHostDlg::OnClose()
{
	ATLTRACE2(atlTraceGeneral, 0, _T("Enter CBTHostDlg::OnClose()\n"));

#ifdef READ_THREAD
	if (INVALID_HANDLE_VALUE != m_hReadThread)
		::SignalObjectAndWait(m_ReadThreadArgs.hEvtTerminate, m_hReadThread, 5000, FALSE);

	m_hReadThread = INVALID_HANDLE_VALUE;
	::CloseHandle(m_ReadThreadArgs.hEvtTerminate);
	m_ReadThreadArgs.hEvtTerminate = INVALID_HANDLE_VALUE;
	_ASSERT(CBTHostDlg::m_socketLocal == m_ReadThreadArgs.socketLocal);
	m_ReadThreadArgs.socketLocal = INVALID_SOCKET;
	m_ReadThreadArgs.hwndMainDlg = NULL;
#else
#endif

	::closesocket(CBTHostDlg::m_socketLocal);
	CBTHostDlg::m_socketLocal = INVALID_SOCKET;

	if (0 == m_iRetCWSAStartup)
		::WSACleanup();

	CDHtmlDialog::OnClose();
	ATLTRACE2(atlTraceGeneral, 0, _T("Leave CBTHostDlg::OnClose()\n"));
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CBTHostDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDHtmlDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CBTHostDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

LRESULT CBTHostDlg::OnRXSingleByte(WPARAM wParam, LPARAM lParam)
{
	_ASSERT(m_dwThreadAffinity == ::GetCurrentThreadId());
	_ASSERT(1 == HIWORD(wParam));
	ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::OnRXSingleByte() msg value: %hc, length: %hu\n"), LOWORD(wParam), HIWORD(wParam));

	/*
	* siehe auch:
	*   C:\Users\psi\Source\Repos\KRT2Mngr\BTHost\BTHost.htm(78): function OnRXSingleByte(nByte)
	*   C:\Users\psi\Source\Repos\KRT2Mngr\IByteLevel.js(7): function OnRXSingleByte(nByte)
	*/
	m_ddScript.Invoke1(_T("OnRXSingleByte"), &CComVariant(LOWORD(wParam)));

	// read next / continue loop
#ifdef IOALERTABLE
	_ASSERT(*CBTHostDlg::m_buf == LOWORD(wParam));
	CBTHostDlg::QueueRead();
#endif

	return 0;
}

HRESULT CBTHostDlg::OnDiscoverDevice(IHTMLElement* /*pElement*/)
{
	HRESULT hr = NOERROR;
	HANDLE hRadio = NULL;
	// hr = enumBTRadio(hRadio);
	hr = enumBTDevices(SerialPortServiceClass_UUID);

	return S_OK;
}

HRESULT CBTHostDlg::OnDiscoverService(IHTMLElement* /*pElement*/)
{
	/*
	* KRT21885       => L"(98:D3:31:FD:5A:F2)"
	* GT-I9300       => L"(0C:14:20:4A:1F:AD)"
	* Blackwire C720 => L"(48:C1:AC:4A:C6:93)"
	*/
	HRESULT hr = enumBTServices(L"(98:D3:31:FD:5A:F2)", SerialPortServiceClass_UUID); // SerialPortServiceClass_UUID
	// hr = enumBTServices(L"(0C:14:20:4A:1F:AD)", L2CAP_PROTOCOL_UUID); // OBEXObjectPushServiceClass_UUID
	return hr;
}

HRESULT CBTHostDlg::OnSendPing(IHTMLElement* /*pElement*/)
{
#ifdef KRT2INPUT_PATH
	char* szHeader = "GET " KRT2INPUT_PATH " HTTP/1.1\r\nHost: ws-psi.estos.de\r\n\r\n";
	if (SOCKET_ERROR == ::send(CBTHostDlg::m_socketLocal, szHeader, strlen(szHeader), 0))
		CBTHostDlg::ShowWSALastError(_T("::send(m_socketLocal, ...)"));
#endif

#ifdef KRT2INPUT_BT
#define O_COMMAND_LEN   0x02
	char rgData[O_COMMAND_LEN] = { STX, 'O' }; // 1.2.6 DUAL-mode on
	if (SOCKET_ERROR == ::send(CBTHostDlg::m_socketLocal, rgData, _countof(rgData), 0))
		CBTHostDlg::ShowWSALastError(_T("::send(m_socketLocal, ...)"));
#endif

	return S_OK;
}

HRESULT CBTHostDlg::OnConnect(IHTMLElement* /*pElement*/)
{
	HRESULT hr = E_NOTIMPL;

#ifdef KRT2INPUT_SERVER
	hr = Connect(&m_addrSimulator); // IPv4 Addr, OHNE DNS, wird im constructor gebaut

	/* hr = CBTHostDlg::NameToTCPAddr(KRT2INPUT_SERVER, m_AddrSimulatorLength, &m_pAddrSimulator);
	if (SUCCEEDED(hr))
	{
		Connect(m_pAddrSimulator, m_AddrSimulatorLength); // IPv6 wird mit CBTHostDlg::NameToTCPAddr() gebaut
	} */
#endif

#ifdef KRT2INPUT_BT
	hr = CBTHostDlg::NameToBthAddr(KRT2INPUT_BT, &m_addrKRT2);
	if (SUCCEEDED(hr))
	{
		m_addrKRT2.port = 1UL;
		Connect(&m_addrKRT2);
	}
#endif

	return S_OK;
}

HRESULT CBTHostDlg::enumBTRadio(HANDLE& hRadio)
{
	HRESULT hr = E_FAIL;
	BLUETOOTH_FIND_RADIO_PARAMS findradioParams;
	memset(&findradioParams, 0x00, sizeof(findradioParams));
	findradioParams.dwSize = sizeof(findradioParams);
	HANDLE _hRadio = NULL;
	HBLUETOOTH_RADIO_FIND hbtrf = ::BluetoothFindFirstRadio(&findradioParams, &_hRadio);
	BOOL bRetC = NULL == hbtrf ? FALSE : TRUE;
	while (bRetC)
	{
		BLUETOOTH_RADIO_INFO RadioInfo;
		::ZeroMemory(&RadioInfo, sizeof(RadioInfo));
		RadioInfo.dwSize = sizeof(RadioInfo);
		DWORD dwRetC = ::BluetoothGetRadioInfo(_hRadio, &RadioInfo);

		// ::MessageBox(NULL, RadioInfo.szName, _T("BluetoothFindXXXRadio"), MB_OK);
		ATLTRACE2(atlTraceGeneral, 0, _T("BluetoothFindXXXRadio: %s\n"), RadioInfo.szName);
		hr = enumBTDevices(_hRadio);
		bRetC = ::BluetoothFindNextRadio(hbtrf, &_hRadio);
		::CloseHandle(_hRadio);
	}

	if (hbtrf)
		::BluetoothFindRadioClose(hbtrf);

	return hr;
}

// like CBTHostDlg::enumBTDevices(GUID serviceClass)
HRESULT CBTHostDlg::enumBTDevices(HANDLE hRadio)
{
	BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams;
	::ZeroMemory(&searchParams, sizeof(searchParams));
	searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
	searchParams.fReturnAuthenticated = 1;
	searchParams.cTimeoutMultiplier = 15;
	searchParams.hRadio = hRadio;

	BLUETOOTH_DEVICE_INFO deviceInfo;
	memset(&deviceInfo, 0x00, sizeof(deviceInfo));
	deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

	HBLUETOOTH_DEVICE_FIND hFindDevice = ::BluetoothFindFirstDevice(&searchParams, &deviceInfo);
	if (hFindDevice)
	{
		do
		{
			// ::MessageBox(NULL, deviceInfo.szName, _T("BluetoothFindXXXDevice"), MB_OK);
			ATLTRACE2(atlTraceGeneral, 0, _T("BluetoothFindXXXDevice: %ls (device)\n"), deviceInfo.szName);
			/*
			* GenericAudioServiceClass_UUID - kein erfolg
			* AudioSinkServiceClass_UUID    - kein erfolg
			* HeadsetServiceClass_UUID      - kein erfolg
			* HandsfreeServiceClass_UUID
			*/
			CComBSTR bstrAddress;
			CBTHostDlg::BTAddressToString(&deviceInfo.Address, &bstrAddress);
			enumBTServices(bstrAddress, SerialPortServiceClass_UUID);
		} while (::BluetoothFindNextDevice(hFindDevice, &deviceInfo));
		::BluetoothFindDeviceClose(hFindDevice);
	}
	return NOERROR;
}

HRESULT CBTHostDlg::Connect(PSOCKADDR_BTH pRemoteAddr)
{
	CBTHostDlg::m_socketLocal = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (INVALID_SOCKET != CBTHostDlg::m_socketLocal)
	{
		if (INVALID_SOCKET != ::connect(CBTHostDlg::m_socketLocal, (struct sockaddr *) pRemoteAddr, sizeof(SOCKADDR_BTH)))
		{
			/*
			* wir lesen IMMER nonblocking.
			* entweder IOALERTABLE (OVERLAPPED_COMPLETION_ROUTINE) ODER
			* READ_THREAD (GetOverlappedResult)
			*/
#ifdef IOALERTABLE
			CBTHostDlg::InitCompletionRoutine();
			CBTHostDlg::QueueRead();
#endif
		}
		else
			CBTHostDlg::ShowWSALastError(_T("::connect(m_socketLocal, ...)"));
	}
	else
		CBTHostDlg::ShowWSALastError(_T("::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM)"));

	return NOERROR;
}

/*
* caller is responsible to free *ppAddrSimulator
*/
/*static*/ HRESULT CBTHostDlg::NameToTCPAddr(
	const LPWSTR pszRemoteName,            // IN
	size_t&           sAddrSimulatorLength, // OUT
	struct sockaddr** ppAddrSimulator)
{
	USES_CONVERSION;

	struct addrinfo hints;
	::ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	struct addrinfo* result = NULL;
	int iResult = ::getaddrinfo(W2CA(pszRemoteName), "80", &hints, &result);
	if (iResult != 0) {
		ATLTRACE2(atlTraceGeneral, 0, _T("::getaddrinfo() failed with error: %d\n"), iResult);
		::WSACleanup();
		return E_FAIL;
	}

	// Attempt to connect to an address until one succeeds
	SOCKET ConnectSocket = INVALID_SOCKET;
	for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		// Create a SOCKET for connecting to server
		ConnectSocket = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			ATLTRACE2(atlTraceGeneral, 0, _T("socket failed with error: %ld\n"), ::WSAGetLastError());
			::WSACleanup();
			return E_FAIL;
		}

		// Connect to server.
		iResult = ::connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			::closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}

		sAddrSimulatorLength = ptr->ai_addrlen;
		*ppAddrSimulator = (struct sockaddr*)new unsigned char[ptr->ai_addrlen];
		memcpy(*ppAddrSimulator, ptr->ai_addr, ptr->ai_addrlen);
		break;
	}
	::freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		ATLTRACE2(atlTraceGeneral, 0, _T("Unable to connect to server!\n"));
		::WSACleanup();
		return E_FAIL;
	}
	::closesocket(ConnectSocket);
	return NOERROR;
}

HRESULT CBTHostDlg::Connect(
	PSOCKADDR_IN addrServer)
{
	/*
	* socket Function, https://msdn.microsoft.com/de-de/library/windows/desktop/ms740506(v=vs.85).aspx
	* der call MUSS sicherstellen das die "addrServer" auch wirklich eine IPv4 (AF_INET) enthaelt
	* SOCK_STREAM, A socket type that provides sequenced, reliable, two-way, connection-based byte streams with an OOB data transmission mechanism.
	*/
	CBTHostDlg::m_socketLocal = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET != CBTHostDlg::m_socketLocal)
	{
		if (INVALID_SOCKET != ::connect(CBTHostDlg::m_socketLocal, (SOCKADDR*)addrServer, sizeof(SOCKADDR_IN)))
		{
			/*
			* wir lesen IMMER nonblocking.
			* entweder IOALERTABLE (OVERLAPPED_COMPLETION_ROUTINE) ODER
			* READ_THREAD (GetOverlappedResult)
			*/
#ifdef IOALERTABLE
			CBTHostDlg::InitCompletionRoutine();
			CBTHostDlg::QueueRead();
#endif

#ifdef READ_THREAD
			m_ReadThreadArgs.hwndMainDlg = m_hWnd;
			m_ReadThreadArgs.socketLocal = CBTHostDlg::m_socketLocal;
			_ASSERT(INVALID_HANDLE_VALUE != m_ReadThreadArgs.hEvtTerminate);
			m_hReadThread = (HANDLE)_beginthreadex(NULL, 0, CBTHostDlg::BTReadThread, &m_ReadThreadArgs, 0, NULL);
#endif
		}
		else
			CBTHostDlg::ShowWSALastError(_T("::connect(m_socketLocal, ...)"));
	}
	else
		CBTHostDlg::ShowWSALastError(_T("::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)"));

	return NOERROR;
}

/*
* HTTP Request using Sockets in C, https://stackoverflow.com/questions/30470505/http-request-using-sockets-in-c?rq=1
* most simple
*/
HRESULT CBTHostDlg::Connect(
	struct sockaddr* pAddrSimulator,
	size_t sAddrSimulatorLength)
{
	/*
	* socket Function, https://msdn.microsoft.com/de-de/library/windows/desktop/ms740506(v=vs.85).aspx
	* SOCKET LocalSocket = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	*/
	SOCKET LocalSocket = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP); // AF_INET
	if (INVALID_SOCKET != LocalSocket)
	{
		if (INVALID_SOCKET != ::connect(LocalSocket, pAddrSimulator, sAddrSimulatorLength))
		{
			char* header = "GET /index.html HTTP/1.1\nHost: www.example.com\n";
			if (SOCKET_ERROR == ::send(LocalSocket, header, sizeof header, 0))
				CBTHostDlg::ShowWSALastError(_T("::send(LocalSocket, ...)"));

			// byte_count = ::recv(LocalSocket, buf, sizeof buf, 0);
		}
		else
			CBTHostDlg::ShowWSALastError(_T("::connect(LocalSocket, ...)"));

		if (SOCKET_ERROR == ::closesocket(LocalSocket))
			CBTHostDlg::ShowWSALastError(_T("::closesocket"));

		LocalSocket = INVALID_SOCKET;
	}
	return NOERROR;
}

HRESULT CBTHostDlg::enumBTDevices(GUID serviceClass)
{
	WSAQUERYSET queryset;
	memset(&queryset, 0, sizeof(WSAQUERYSET));
	queryset.dwSize = sizeof(WSAQUERYSET);
	queryset.dwNameSpace = NS_BTH;

	HANDLE hLookup = NULL;
	/*
	* set LUP_FLUSHCACHE to start inquiry
	* set LUP_CONTAINERS to get devices (not services)
	* Bluetooth and WSALookupServiceBegin for Device Inquiry, https://msdn.microsoft.com/de-de/library/windows/desktop/aa362913(v=vs.85).aspx
	*/
	int result = ::WSALookupServiceBegin(&queryset, LUP_CONTAINERS, &hLookup);
	if (0 != result)
		CBTHostDlg::ShowWSALastError(_T("WSALookupServiceBegin (device discovery)"));

	//Initialisation succeded, start looking for devices
	BYTE buffer[4096];
	memset(buffer, 0, sizeof(buffer));
	DWORD bufferLength = sizeof(buffer);

	WSAQUERYSET *pResults = (WSAQUERYSET*)&buffer;
	while (result == 0)
	{
		result = ::WSALookupServiceNext(hLookup, LUP_RETURN_NAME | LUP_RETURN_ADDR | LUP_RETURN_TYPE | LUP_RETURN_BLOB | LUP_RES_SERVICE, &bufferLength, pResults);
		if (result == 0)
		{
			// A device found, print the name of the device
			CString strDisplayName = pResults->lpszServiceInstanceName;
			ATLTRACE2(atlTraceGeneral, 0, _T("%ls\n"), pResults->lpszServiceInstanceName);
			if (pResults->dwNumberOfCsAddrs && pResults->lpcsaBuffer)
			{
				CSADDR_INFO* addr = (CSADDR_INFO*) pResults->lpcsaBuffer;
				CComBSTR bstrAddress;
				CBTHostDlg::BTAddressToString((PSOCKADDR_BTH)addr->RemoteAddr.lpSockaddr, &bstrAddress);
				ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls (device)\n"), (LPCWSTR)bstrAddress);

				enumBTServices(bstrAddress, serviceClass);
			}
		}
	}

	if (hLookup)
		::WSALookupServiceEnd(hLookup);
	hLookup = NULL;
	return NOERROR;
}

HRESULT CBTHostDlg::enumBTServices(
	LPCWSTR szDeviceAddress,
	GUID serviceClass)
{
	WCHAR szGUID[0x100];
	::StringFromGUID2(serviceClass, szGUID, _countof(szGUID));
	ATLTRACE2(atlTraceGeneral, 0, _T("enumBTServices(%s, %s)\n"), szDeviceAddress, szGUID);

	/*
	* Initialise querying the device services
	*/
	WCHAR addressAsString[1000] = { 0 };
	wcscpy_s(addressAsString, szDeviceAddress);

	WSAQUERYSET queryset;
	memset(&queryset, 0, sizeof(WSAQUERYSET));
	queryset.dwSize = sizeof(WSAQUERYSET);
	queryset.dwNameSpace = NS_BTH;
	queryset.dwNumberOfCsAddrs = 0;
	queryset.lpszContext = addressAsString;

	queryset.lpServiceClassId = &serviceClass;

	HANDLE hLookup = NULL;
	/*
	* do not set LUP_CONTAINERS, we are querying for services
	* LUP_FLUSHCACHE would be used if we want to do an inquiry
	* Bluetooth and WSALookupServiceBegin for Service Discovery, https://msdn.microsoft.com/en-us/library/windows/desktop/aa362914(v=vs.85).aspx
	*/
	int result = ::WSALookupServiceBegin(&queryset, 0, &hLookup);
	if (0 != result)
		CBTHostDlg::ShowWSALastError(_T("WSALookupServiceBegin (service discovery)"));

	while (result == NO_ERROR)
	{
		BYTE buffer[0x2000];
		memset(buffer, 0, sizeof(buffer));
		DWORD bufferLength = sizeof(buffer);
		WSAQUERYSET* pResults = (WSAQUERYSET*)&buffer;
		result = ::WSALookupServiceNext(hLookup, LUP_RETURN_ALL, &bufferLength, pResults);
		if (result == 0)
		{
			CString strServiceName = pResults->lpszServiceInstanceName;
			// ATLTRACE2(atlTraceGeneral, 0, _T("%ls\n"), pResults->lpszServiceInstanceName);
			if (pResults->dwNumberOfCsAddrs && pResults->lpcsaBuffer)
			{
				PCSADDR_INFO addrService = (PCSADDR_INFO) pResults->lpcsaBuffer;
				CComBSTR bstrAddress;
				/*
				* nachdem ich DEFINITIV Bluetooth (devices/services) aufzaehle MUSS
				* es sich bei addrService->RemoteAddr.lpSockaddr auch um eine BLUETOOTH_ADDRESS* handeln!
				*/
				// (BLUETOOTH_ADDRESS*)&lpAddress->btAddr
				{
					/* PSOCKADDR_BTH pSockAddrBT = (PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr;
					_ASSERT(AF_BTH == pSockAddrBT->addressFamily);
					CBTHostDlg::BTAddressToString((BLUETOOTH_ADDRESS*)&pSockAddrBT->btAddr, &bstrAddress);
					ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls, %ls (service)\n"), pResults->lpszServiceInstanceName, (LPCWSTR)bstrAddress); */

					/* MUSS die gleiche address/ergebnis haben wie oben nur MIT Port
					CBTHostDlg::BTAddressToString((PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr, &bstrAddress);
					ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls, %ls (service)\n"), pResults->lpszServiceInstanceName, (LPCWSTR)bstrAddress); */

					// MUSS die gleiche address/ergebnis haben wie oben nur API
					WSAPROTOCOL_INFO ProtocolInfo;
					WCHAR szAddress[0x100];
					DWORD dwAddressLen = _countof(szAddress);
					if(0 != ::WSAAddressToString(addrService->RemoteAddr.lpSockaddr, sizeof(SOCKADDR_BTH), NULL /* &ProtocolInfo */, szAddress, &dwAddressLen))
						CBTHostDlg::ShowWSALastError(_T("WSAAddressToString (service discovery)"));
					else
						ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls, %ls (service)\n"), pResults->lpszServiceInstanceName, szAddress);
				}

				/*
				* eigentlich wuerde ich am liebsten AUSSCHLIESSLICH mit einer BLUETOOTH_ADDRESS dealen.
				* fuer das Connect() brauch ich aber unbedingt eine PSOCKADDR_BTH pRemoteAddr.
				* wie erweitert/baut man eine CSADDR_INFO/SOCKET_ADDRESS bzw. SOCKADDR_BTH wenn man NUR eine BLUETOOTH_ADDRESS hat?
				* Hinweis(e):
				* 1.) eine SOCKET_ADDRESS ist eine SOCKADDR_BTH
				*/
				m_addrKRT2 = *((PSOCKADDR_BTH) addrService->RemoteAddr.lpSockaddr);
			}
		}
	}

	if (hLookup)
		::WSALookupServiceEnd(hLookup);
	hLookup = NULL;
	return 0;
}

/*
* NameToBthAddr converts a bluetooth device name to a bluetooth address,
* if required by performing inquiry with remote name requests.
* This function demonstrates device inquiry, with optional LUP flags.
*/
#define CXN_TEST_DATA_STRING              (L"~!@#$%^&*()-_=+?<>1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") 
#define CXN_TRANSFER_DATA_LENGTH          (sizeof(CXN_TEST_DATA_STRING)) 

#define CXN_BDADDR_STR_LEN                17   // 6 two-digit hex values plus 5 colons 
#define CXN_MAX_INQUIRY_RETRY             3
#define CXN_DELAY_NEXT_INQUIRY            15
#define CXN_ERROR                         1
#define CXN_DEFAULT_LISTEN_BACKLOG        4
/*static*/ HRESULT CBTHostDlg::NameToBthAddr(
	_In_ const LPWSTR pszRemoteName,
	_Out_ PSOCKADDR_BTH pRemoteBtAddr)
{
	HRESULT hrResult = NOERROR;
	BOOL    bContinueLookup = FALSE, bRemoteDeviceFound = FALSE;
	ULONG   ulFlags = 0, ulPQSSize = sizeof(WSAQUERYSET);
	HANDLE  hLookup = NULL;

	::ZeroMemory(pRemoteBtAddr, sizeof(*pRemoteBtAddr));

	// HeapAlloc function, https://msdn.microsoft.com/de-de/library/windows/desktop/aa366597(v=vs.85).aspx
	// If the function fails, it does not call SetLastError. An application cannot call GetLastError for extended error information.
	PWSAQUERYSET pWSAQuerySet = (PWSAQUERYSET)::HeapAlloc(GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		ulPQSSize);
	if (NULL == pWSAQuerySet)
		return E_OUTOFMEMORY;

	//
	// Search for the device with the correct name
	//
	if (NOERROR == hrResult)
	{
		for (INT iRetryCount = 0;
			!bRemoteDeviceFound && (iRetryCount < CXN_MAX_INQUIRY_RETRY);
			iRetryCount++) {
			//
			// WSALookupService is used for both service search and device inquiry
			// LUP_CONTAINERS is the flag which signals that we're doing a device inquiry.
			//
			ulFlags = LUP_CONTAINERS;

			//
			// Friendly device name (if available) will be returned in lpszServiceInstanceName
			//
			ulFlags |= LUP_RETURN_NAME;

			//
			// BTH_ADDR will be returned in lpcsaBuffer member of WSAQUERYSET
			//
			ulFlags |= LUP_RETURN_ADDR;

			if (0 == iRetryCount)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("*INFO* | Inquiring device from cache...\n"));
			}
			else {
				//
				// Flush the device cache for all inquiries, except for the first inquiry
				//
				// By setting LUP_FLUSHCACHE flag, we're asking the lookup service to do
				// a fresh lookup instead of pulling the information from device cache.
				//
				ulFlags |= LUP_FLUSHCACHE;

				//
				// Pause for some time before all the inquiries after the first inquiry
				//
				// Remote Name requests will arrive after device inquiry has
				// completed.  Without a window to receive IN_RANGE notifications,
				// we don't have a direct mechanism to determine when remote
				// name requests have completed.
				//
				ATLTRACE2(atlTraceGeneral, 0, _T("*INFO* | Unable to find device.  Waiting for %d seconds before re-inquiry...\n"), CXN_DELAY_NEXT_INQUIRY);
				Sleep(CXN_DELAY_NEXT_INQUIRY * 1000);

				ATLTRACE2(atlTraceGeneral, 0, _T("*INFO* | Inquiring device ...\n"));
			}

			//
			// Start the lookup service
			//
			hrResult = NOERROR;
			hLookup = 0;
			bContinueLookup = FALSE;
			::ZeroMemory(pWSAQuerySet, ulPQSSize);
			pWSAQuerySet->dwNameSpace = NS_BTH;
			pWSAQuerySet->dwSize = sizeof(WSAQUERYSET);
			hrResult = ::WSALookupServiceBegin(pWSAQuerySet, ulFlags, &hLookup);

			//
			// Even if we have an error, we want to continue until we
			// reach the CXN_MAX_INQUIRY_RETRY
			//
			if ((NOERROR == hrResult) && (NULL != hLookup)) {
				bContinueLookup = TRUE;
			}
			else if (0 < iRetryCount) {
				ATLTRACE2(atlTraceGeneral, 0, _T("=CRITICAL= | WSALookupServiceBegin() failed with error code %d, WSAGetLastError = %d\n"), hrResult, ::WSAGetLastError());
				break;
			}

			while (bContinueLookup) {
				//
				// Get information about next bluetooth device
				//
				// Note you may pass the same WSAQUERYSET from LookupBegin
				// as long as you don't need to modify any of the pointer
				// members of the structure, etc.
				//
				// ZeroMemory(pWSAQuerySet, ulPQSSize);
				// pWSAQuerySet->dwNameSpace = NS_BTH;
				// pWSAQuerySet->dwSize = sizeof(WSAQUERYSET);
				if (NO_ERROR == ::WSALookupServiceNext(hLookup,
					ulFlags,
					&ulPQSSize,
					pWSAQuerySet)) {

					// 
					// Compare the name to see if this is the device we are looking for. 
					// 
					if ((pWSAQuerySet->lpszServiceInstanceName != NULL) &&
						(0 == _wcsicmp(pWSAQuerySet->lpszServiceInstanceName, pszRemoteName))) {
						//
						// Found a remote bluetooth device with matching name. 
						// Get the address of the device and exit the lookup. 
						//
						::CopyMemory(pRemoteBtAddr,
							(PSOCKADDR_BTH)pWSAQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr,
							sizeof(*pRemoteBtAddr));
						bRemoteDeviceFound = TRUE;
						bContinueLookup = FALSE;
					}
				}
				else {
					hrResult = ::WSAGetLastError();
					if (WSA_E_NO_MORE == hrResult)
					{
						// No more data 
						// No more devices found.  Exit the lookup. 
						bContinueLookup = FALSE;
					}
					else if (WSAEFAULT == hrResult) {
						//
						// The buffer for QUERYSET was insufficient.
						// In such case 3rd parameter "ulPQSSize" of function "WSALookupServiceNext()" receives
						// the required size.  So we can use this parameter to reallocate memory for QUERYSET.
						//
						::HeapFree(::GetProcessHeap(), 0, pWSAQuerySet);
						pWSAQuerySet = (PWSAQUERYSET)::HeapAlloc(::GetProcessHeap(),
							HEAP_ZERO_MEMORY,
							ulPQSSize);
						if (NULL == pWSAQuerySet) {
							wprintf(L"!ERROR! | Unable to allocate memory for WSAQERYSET\n");
							hrResult = STATUS_NO_MEMORY;
							bContinueLookup = FALSE;
						}
					}
					else {
						wprintf(L"=CRITICAL= | WSALookupServiceNext() failed with error code %d\n", hrResult);
						bContinueLookup = FALSE;
					}
				}
			}

			//
			// End the lookup service
			//
			::WSALookupServiceEnd(hLookup);

			if (STATUS_NO_MEMORY == hrResult) {
				break;
			}
		}
	}

	if (NULL != pWSAQuerySet) {
		::HeapFree(::GetProcessHeap(), 0, pWSAQuerySet);
		pWSAQuerySet = NULL;
	}

	if (bRemoteDeviceFound) {
		hrResult = NOERROR;
	}
	else {
		hrResult = E_INVALIDARG;
	}

	return hrResult;
}

/*static*/ HRESULT CBTHostDlg::BTAddressToString(
	BLUETOOTH_ADDRESS* pBTAddr,
	BSTR* pbstrAddress)
{
	wchar_t lpszAddressString[100] = { 0 };
	DWORD dwAddressStringLength = 99;

	swprintf_s(lpszAddressString, dwAddressStringLength, L"(%02X:%02X:%02X:%02X:%02X:%02X)",
		pBTAddr->rgBytes[5], pBTAddr->rgBytes[4],
		pBTAddr->rgBytes[3], pBTAddr->rgBytes[2],
		pBTAddr->rgBytes[1], pBTAddr->rgBytes[0]);

	if(NULL != *pbstrAddress)
		::SysReAllocString(pbstrAddress, lpszAddressString);
	else
		*pbstrAddress = ::SysAllocString(lpszAddressString);
	return NO_ERROR;
}

/*static*/ HRESULT CBTHostDlg::BTAddressToString(
	PSOCKADDR_BTH lpAddress,
	BSTR* pbstrAddress)
{
	wchar_t lpszAddressString[100] = { 0 };
	DWORD dwAddressStringLength = 99;

	BLUETOOTH_ADDRESS* pBTAddr = (BLUETOOTH_ADDRESS*)&lpAddress->btAddr;

	if (lpAddress->port == 0)
		swprintf_s(lpszAddressString, dwAddressStringLength, L"(%02X:%02X:%02X:%02X:%02X:%02X)",
			pBTAddr->rgBytes[5], pBTAddr->rgBytes[4],
			pBTAddr->rgBytes[3], pBTAddr->rgBytes[2],
			pBTAddr->rgBytes[1], pBTAddr->rgBytes[0]);
	else
		swprintf_s(lpszAddressString, dwAddressStringLength, L"(%02X:%02X:%02X:%02X:%02X:%02X):%d",
			pBTAddr->rgBytes[5], pBTAddr->rgBytes[4],
			pBTAddr->rgBytes[3], pBTAddr->rgBytes[2],
			pBTAddr->rgBytes[1], pBTAddr->rgBytes[0],
			lpAddress->port);

	if (NULL != *pbstrAddress)
		::SysReAllocString(pbstrAddress, lpszAddressString);
	else
		*pbstrAddress = ::SysAllocString(lpszAddressString);
	return NOERROR;
}

/*static*/ HRESULT CBTHostDlg::ShowWSALastError(LPCTSTR szCaption)
{
	const DWORD dwMessageId(::WSAGetLastError());
	CString strDesc;
	DWORD dwRetC = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwMessageId,
		LANG_SYSTEM_DEFAULT, // MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
		strDesc.GetBufferSetLength(0xff), 0xff,
		NULL); // _In_opt_ va_list *Arguments
	strDesc.ReleaseBuffer();
	CString strMsg;
	strMsg.Format(_T("%s, returned: 0x%.8x"), (LPCTSTR) strDesc, dwMessageId);
	::MessageBox(NULL, strMsg, szCaption, MB_OK);
	return NOERROR;
}

#ifdef READ_THREAD
/*static*/ unsigned int CBTHostDlg::BTReadThread(void* arguments)
{
	struct _ReadThreadArg* pArgs = (struct _ReadThreadArg*) arguments;
	_ASSERT(NULL != pArgs->hwndMainDlg);

	// Make sure the RecvOverlapped struct is zeroed out
	WSAOVERLAPPED RecvOverlapped;
	SecureZeroMemory((PVOID)& RecvOverlapped, sizeof(WSAOVERLAPPED));
	RecvOverlapped.hEvent = ::WSACreateEvent();

	/*
	* wir MUESSEN Byte weise lesen (denn mit jedem byte kann der CommandParser entscheiden dass, das aktuelle command zuende ist)
	* ergo: nehmen wir hier einen lokalen Buffer
	* 1.) wir muessen um dieses Byte an das WebBrowserCtrl zu dispatchen UNBEDINGT den Thread wechseln
	* 2.) sparen wir uns die Synchronisation (Mutex) fuer den zugriff auf einen static Buffer
	*
	* WSARecv function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms741688(v=vs.85).aspx
	*/
	char buf[0x01];
	WSABUF readBuffer = { _countof(buf), buf };
	DWORD dwNumberOfBytesRecvd = 0;
	DWORD dwFlags = 0; // MSG_PARTIAL, MSG_OOB
	WSAEVENT rgWSAEvents[2] = { pArgs->hEvtTerminate, RecvOverlapped.hEvent };
	while (1)
	{
		const int iRetC = ::WSARecv(pArgs->socketLocal, &readBuffer, 1, &dwNumberOfBytesRecvd, &dwFlags, &RecvOverlapped, NULL);
		if (0 == iRetC)
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("number of bytes received(Sync): 0x%.8x, char: %hc\n"), dwNumberOfBytesRecvd, *readBuffer.buf);
			::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(buf[0], dwNumberOfBytesRecvd), NULL);
		}

		else
		{
			_ASSERT(SOCKET_ERROR == iRetC);
			const int iLastError = ::WSAGetLastError();
			if (WSA_IO_PENDING == iLastError)
			{
				const DWORD dwEvent = ::WSAWaitForMultipleEvents(_countof(rgWSAEvents), rgWSAEvents, FALSE, 10000, FALSE); // WSA_INFINITE
				if (WSA_WAIT_FAILED == dwEvent)
				{
					ATLTRACE2(atlTraceGeneral, 0, _T("::WSAWaitForMultipleEvents() failed with error: 0x%.8x\n"), ::WSAGetLastError());
					break;
				}

				else if (WSA_WAIT_TIMEOUT == dwEvent)
				{
					ATLTRACE2(atlTraceGeneral, 0, _T("run idle and continue loop\n"));
				}

				else if (WSA_WAIT_EVENT_0 == dwEvent)
				{
					ATLTRACE2(atlTraceGeneral, 0, _T("hEvtTerminate signaled, exit loop\n"));
					break;
				}

				else if (WSA_WAIT_EVENT_0 + 1 == dwEvent)
				{
					if (FALSE == ::WSAGetOverlappedResult(pArgs->socketLocal, &RecvOverlapped, &dwNumberOfBytesRecvd, FALSE, &dwFlags))
					{
						ATLTRACE2(atlTraceGeneral, 0, _T("::WSAGetOverlappedResult() failed with error: 0x%.8x\n"), ::WSAGetLastError());
						break;
					}

					// If 0 bytes are received, the connection was closed
					if (0 == dwNumberOfBytesRecvd)
						break;

					ATLTRACE2(atlTraceGeneral, 0, _T("number of bytes received(Async): 0x%.8x, char: %hc\n"), dwNumberOfBytesRecvd, *readBuffer.buf);
					::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(buf[0], dwNumberOfBytesRecvd), NULL);
					::WSAResetEvent(RecvOverlapped.hEvent);
				}

				else
				{
					ATLTRACE2(atlTraceGeneral, 0, _T("ERROR: unknown result, break\n"));
					break;
				}
			}
			else
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("::WSARecv() failed with error: 0x%.8x\n"), iLastError);
				break;
			}
		}
	}
	::WSACloseEvent(RecvOverlapped.hEvent);

	ATLTRACE2(atlTraceGeneral, 0, _T("Leave BTReadThread()\n"));
	return 0;
}
#endif

#ifdef IOALERTABLE
/*static*/HRESULT CBTHostDlg::InitCompletionRoutine()
{
	// Make sure the RecvOverlapped struct is zeroed out
	SecureZeroMemory((PVOID)& CBTHostDlg::m_RecvOverlappedCompletionRoutine, sizeof(WSAOVERLAPPED));
	return NOERROR;
}

/*static*/ HRESULT CBTHostDlg::QueueRead()
{
	// WSARecv function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms741688(v=vs.85).aspx
	CBTHostDlg::m_readBuffer = { _countof(CBTHostDlg::m_buf), CBTHostDlg::m_buf };
	DWORD dwNumberOfBytesRecvd = 0;
	DWORD dwFlags = 0; // MSG_PARTIAL, MSG_OOB
	const int iRetC = ::WSARecv(CBTHostDlg::m_socketLocal, &CBTHostDlg::m_readBuffer, 1, &dwNumberOfBytesRecvd, &dwFlags, &CBTHostDlg::m_RecvOverlappedCompletionRoutine, CBTHostDlg::WorkerRoutine);
	if (SOCKET_ERROR == iRetC)
	{
		_ASSERT(WSA_IO_PENDING == ::WSAGetLastError());
	}
	return NOERROR;
}

/*static*/ void CALLBACK CBTHostDlg::WorkerRoutine(DWORD dwError, DWORD dwBytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	/*
	* die CompletionRoutine wird offensichtlich durch den MainThread ausgefuehrt
	* wenn wir die static WorkerRoutine() durch eine MemberFunction ersetzen haben wir gewonnen!
	_ASSERT(m_dwThreadAffinity == ::GetCurrentThreadId());
	*/

	ATLTRACE2(atlTraceGeneral, 0, _T("CompletionRoutine() dwBytesTransferred: 0x%.8x, ::GetCurrentThreadId(): 0x%.8x\n"), dwBytesTransferred, ::GetCurrentThreadId());
	if (0 < dwBytesTransferred)
	{
		AfxGetMainWnd()->PostMessage(WM_USER_RXSINGLEBYTE, MAKEWPARAM(*CBTHostDlg::m_buf, dwBytesTransferred), NULL);
	}
}
#endif

void CBTHostDlg::txBytes(
	BSTR bstrBytes)
{
	char rgData[0x100];
	char* pWrite = rgData;
	WCHAR* pcNext = NULL;
	LPWSTR szToken = _tcstok_s(bstrBytes, L" ", &pcNext); // bstrBytes will be modified!
	while (NULL != szToken)
	{
		*pWrite++ = _tcstol(szToken, NULL, 16);
		szToken = _tcstok_s(NULL, L" ", &pcNext);
	}

	if (SOCKET_ERROR == ::send(CBTHostDlg::m_socketLocal, rgData, pWrite - rgData, 0))
		CBTHostDlg::ShowWSALastError(_T("::send(m_socketLocal, ...)"));

	ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::txBytes() NoOfBytes: 0x%.8x\n"), pWrite - rgData);
}
