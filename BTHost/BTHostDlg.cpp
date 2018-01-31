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
BEGIN_DHTML_EVENT_MAP(CBTHostDlg)
	DHTML_EVENT_ONCLICK(_T("btnSoft1"), OnSendPing) // soft buttons
	DHTML_EVENT_ONCLICK(_T("btnSoft2"), OnConnect)
END_DHTML_EVENT_MAP()

CBTHostDlg::CBTHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_BTHOST_DIALOG, IDR_HTML_BTHOST_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_iRetCWSAStartup = -1;
	m_addrKRT2 = {
		AF_BTH, // USHORT addressFamily;
		BTH_ADDR_NULL, // BTH_ADDR btAddr;
		CLSID_NULL, // GUID serviceClassId;
		0UL // ULONG port;
	};
	/*
	* Setting address family to AF_BTH indicates winsock2 to use Bluetooth sockets
	* Port should be set to 0 if ServiceClassId is specified.
	m_addrKRT2.addressFamily = AF_BTH;
	m_addrKRT2.btAddr = ((ULONGLONG)0x000098d331fd5af2); // KRT21885, Dev B => (98:D3:31:FD:5A:F2)
	m_addrKRT2.serviceClassId = CLSID_NULL; // SerialPortServiceClass_UUID
	m_addrKRT2.port = 1UL;
	*/

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

#ifdef READ_THREAD
	m_ReadThreadArgs.hwndMainDlg = NULL;
	m_ReadThreadArgs.socketLocal = INVALID_SOCKET;
	m_ReadThreadArgs.hEvtTerminate = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hReadThread = INVALID_HANDLE_VALUE;
#endif
	m_socketLocal = INVALID_SOCKET;
}

BEGIN_MESSAGE_MAP(CBTHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

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
	WSADATA data;
	m_iRetCWSAStartup = ::WSAStartup(MAKEWORD(2, 2), &data);
	if (0 != m_iRetCWSAStartup)
		CBTHostDlg::ShowWSALastError(_T("WSAStartup"));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

/*virtual*/ void CBTHostDlg::OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR szUrl)
{
	CDHtmlDialog::OnDocumentComplete(pDisp, szUrl);

	// SetElementProperty(_T("btnSoft1"), DISPID_VALUE, &CComVariant(L"Check hurtz")); // for <input> elements
	SetElementText(_T("btnSoft1"), _T("SendPing")); // _T("Check SPP")
	SetElementText(_T("btnSoft2"), _T("Connect"));
}

/*
* ich haette es lieber als ON_WM_CHAR bearbeitet aber da liegt die CDHtmlDialog klasse dazwischen
* und der handler wird nicht aufgerufen. auch ein zusaetzliches OnGetDlgCode() hilft nix.
*/
STDMETHODIMP CBTHostDlg::TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID)
{
#if (27015 == KRT2INPUT_PORT)
	if (lpMsg->message == WM_CHAR)
	{
		const char ucBuffer[1] = { LOBYTE(LOWORD(lpMsg->wParam)) };
		ATLTRACE2(atlTraceGeneral, 0, _T("SendChar: %hc\n"), *ucBuffer);
		if (SOCKET_ERROR == ::send(m_socketLocal, ucBuffer, 1, 0))
			CBTHostDlg::ShowWSALastError(_T("::send(socketLocal, ...)"));
	}
#endif

	return __super::TranslateAccelerator(lpMsg, pguidCmdGroup, nCmdID);
}

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
#ifdef READ_THREAD
	if (INVALID_HANDLE_VALUE != m_hReadThread)
		::SignalObjectAndWait(m_ReadThreadArgs.hEvtTerminate, m_hReadThread, 5000, FALSE);

	m_hReadThread = INVALID_HANDLE_VALUE;
	::CloseHandle(m_ReadThreadArgs.hEvtTerminate);
	m_ReadThreadArgs.hEvtTerminate = INVALID_HANDLE_VALUE;
	_ASSERT(m_socketLocal == m_ReadThreadArgs.socketLocal);
	m_ReadThreadArgs.socketLocal = INVALID_SOCKET;
	m_ReadThreadArgs.hwndMainDlg = NULL;
#else
#endif

	::closesocket(m_socketLocal);
	m_socketLocal = INVALID_SOCKET;

	if (0 == m_iRetCWSAStartup)
		::WSACleanup();

	CDHtmlDialog::OnClose();
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

HRESULT CBTHostDlg::OnCheck(IHTMLElement* /*pElement*/)
{
	HRESULT hr = NOERROR;
	HANDLE hRadio = NULL;
	hr = enumBTRadio(hRadio);
	// hr = enumBTDevices(SerialPortServiceClass_UUID);

	return S_OK;
}

HRESULT CBTHostDlg::OnSendPing(IHTMLElement* /*pElement*/)
{
#ifdef KRT2INPUT_PATH
	char* szHeader = "GET " KRT2INPUT_PATH " HTTP/1.1\r\nHost: ws-psi.estos.de\r\n\r\n";
	if (SOCKET_ERROR == ::send(m_socketLocal, szHeader, strlen(szHeader), 0))
		CBTHostDlg::ShowWSALastError(_T("::send(socketLocal, ...)"));
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

#define TESTDATA_LENGTH   0x0a
HRESULT CBTHostDlg::Connect(PSOCKADDR_BTH pRemoteAddr)
{
	char* pszData = (char*) ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, TESTDATA_LENGTH + 1);
	memcpy_s(pszData, 0x0a, "here we go", 0x0a);

	SOCKET LocalSocket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (INVALID_SOCKET != LocalSocket)
	{
		SOCKET socketKRT2 = ::connect(LocalSocket, (struct sockaddr *) pRemoteAddr, sizeof(SOCKADDR_BTH));
		if (INVALID_SOCKET != socketKRT2)
		{
			if (SOCKET_ERROR == ::send(LocalSocket, (char*)pszData, (int)TESTDATA_LENGTH, 0))
				CBTHostDlg::ShowWSALastError(_T("::send(LocalSocket, ...)"));
		}
		else
			CBTHostDlg::ShowWSALastError(_T("::connect(LocalSocket, ...)"));

		if (SOCKET_ERROR == ::closesocket(LocalSocket))
			CBTHostDlg::ShowWSALastError(_T("::closesocket"));

		LocalSocket = INVALID_SOCKET;
	}
	else
		CBTHostDlg::ShowWSALastError(_T("::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM)"));

	if (NULL != pszData) {
		::HeapFree(::GetProcessHeap(), 0, pszData);
		pszData = NULL;
	}
	return E_FAIL;
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
	*/
	m_socketLocal = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET != m_socketLocal)
	{
		if (INVALID_SOCKET != ::connect(m_socketLocal, (SOCKADDR*)addrServer, sizeof(SOCKADDR_IN)))
		{
#ifdef READ_THREAD
			m_ReadThreadArgs.hwndMainDlg = m_hWnd;
			m_ReadThreadArgs.socketLocal = m_socketLocal;
			_ASSERT(INVALID_HANDLE_VALUE != m_ReadThreadArgs.hEvtTerminate);
			m_hReadThread = (HANDLE)_beginthreadex(NULL, 0, CBTHostDlg::COMReadThread, &m_ReadThreadArgs, 0, NULL);

#else
			char buf[0x1000];
			const int iByteCount = ::recv(socketLocal, buf, _countof(buf), 0);
			ATLTRACE2(atlTraceGeneral, 0, _T("  number of bytes received: 0x%.8x\n"), iByteCount);
#endif
		}
		else
			CBTHostDlg::ShowWSALastError(_T("::connect(socketLocal, ...)"));

#ifdef READ_THREAD
#else
		if (SOCKET_ERROR == ::closesocket(socketLocal))
			CBTHostDlg::ShowWSALastError(_T("::closesocket"));
#endif
	}
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
		WSALookupServiceEnd(hLookup);

	return NOERROR;
}

HRESULT CBTHostDlg::enumBTServices(
	LPCWSTR szDeviceAddress,
	GUID serviceClass)
{
	//Initialise querying the device services
	WCHAR addressAsString[1000] = { 0 };
	wcscpy_s(addressAsString, szDeviceAddress);

	WSAQUERYSET queryset2;
	memset(&queryset2, 0, sizeof(queryset2));
	queryset2.dwSize = sizeof(queryset2);
	queryset2.dwNameSpace = NS_BTH;
	queryset2.dwNumberOfCsAddrs = 0;
	queryset2.lpszContext = addressAsString;

	queryset2.lpServiceClassId = &serviceClass;

	HANDLE hLookup = NULL;
	//do not set LUP_CONTAINERS, we are querying for services
	//LUP_FLUSHCACHE would be used if we want to do an inquiry
	int result2 = ::WSALookupServiceBegin(&queryset2, 0, &hLookup);
	if (0 != result2)
		CBTHostDlg::ShowWSALastError(_T("WSALookupServiceBegin (service discovery)"));

	while (result2 == NO_ERROR)
	{
		BYTE buffer2[8096];
		memset(buffer2, 0, sizeof(buffer2));
		DWORD bufferLength2 = sizeof(buffer2);
		WSAQUERYSET *pResults2 = (WSAQUERYSET*)&buffer2;
		result2 = ::WSALookupServiceNext(hLookup, LUP_RETURN_ALL, &bufferLength2, pResults2);
		if (result2 == 0)
		{
			CString strServiceName = pResults2->lpszServiceInstanceName;
			// ATLTRACE2(atlTraceGeneral, 0, _T("%ls\n"), pResults2->lpszServiceInstanceName);
			if (pResults2->dwNumberOfCsAddrs && pResults2->lpcsaBuffer)
			{
				PCSADDR_INFO addrService = (PCSADDR_INFO) pResults2->lpcsaBuffer;
				CComBSTR bstrAddress;
				/*
				* nachdem ich DEFINITIV Bluetooth (devices/services) aufzaehle MUSS
				* es sich bei addrService->RemoteAddr.lpSockaddr auch um eine BLUETOOTH_ADDRESS* handeln!
				*/
				// (BLUETOOTH_ADDRESS*)&lpAddress->btAddr
				{
					PSOCKADDR_BTH pSockAddrBT = (PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr;
					_ASSERT(AF_BTH == pSockAddrBT->addressFamily);
					CBTHostDlg::BTAddressToString((BLUETOOTH_ADDRESS*)&pSockAddrBT->btAddr, &bstrAddress);
					ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls, %ls (service)\n"), pResults2->lpszServiceInstanceName, (LPCWSTR)bstrAddress);

					// MUSS die gleiche address/ergebnis haben wie oben nur MIT Port
					CBTHostDlg::BTAddressToString((PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr, &bstrAddress);
					ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls, %ls (service)\n"), pResults2->lpszServiceInstanceName, (LPCWSTR)bstrAddress);
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
					if (WSA_E_NO_MORE == hrResult) { //No more data 
													// 
													// No more devices found.  Exit the lookup. 
													// 
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
/*static*/ unsigned int CBTHostDlg::COMReadThread(void* arguments)
{
	struct _ReadThreadArg* pArgs = (struct _ReadThreadArg*) arguments;
	_ASSERT(NULL != pArgs->hwndMainDlg);

#define IOCOMPLETION
#ifdef IOCOMPLETION
	// Make sure the RecvOverlapped struct is zeroed out
	WSAOVERLAPPED RecvOverlapped;
	SecureZeroMemory((PVOID)& RecvOverlapped, sizeof(WSAOVERLAPPED));
	RecvOverlapped.hEvent = ::WSACreateEvent();

	// WSARecv function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms741688(v=vs.85).aspx
	char buf[0x1];
	WSABUF readBuffer = { _countof(buf), buf };
	DWORD dwNumberOfBytesRecvd = 0;
	DWORD dwFlags = 0;
	WSAEVENT rgWSAEvents[2] = { pArgs->hEvtTerminate, RecvOverlapped.hEvent };
	while (1)
	{
		const int iRetC = ::WSARecv(pArgs->socketLocal, &readBuffer, 1, &dwNumberOfBytesRecvd, &dwFlags, &RecvOverlapped, NULL);
		if (0 == iRetC)
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("number of bytes received(Sync): 0x%.8x, char: %hc\n"), dwNumberOfBytesRecvd, *readBuffer.buf);
			// ::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(rgCommand[0], 0), NULL);
		}

		else
		{
			_ASSERT(SOCKET_ERROR == iRetC);
			const int iLastError = ::WSAGetLastError();
			if (WSA_IO_PENDING == iLastError)
			{
				const DWORD dwEvent = ::WSAWaitForMultipleEvents(_countof(rgWSAEvents), rgWSAEvents, TRUE, 10000, FALSE);
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

					ATLTRACE2(atlTraceGeneral, 0, _T("number of bytes received(Async): 0x%.8x, char: %hc\n"), dwNumberOfBytesRecvd, *readBuffer.buf);
					// ::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(rgCommand[0], 0), NULL);
					::WSAResetEvent(RecvOverlapped.hEvent);

					// If 0 bytes are received, the connection was closed
					if (0 == dwNumberOfBytesRecvd)
						break;
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
#else
#endif

	ATLTRACE2(atlTraceGeneral, 0, _T("Leave COMReadThread()\n"));
	return 0;
}
#endif
