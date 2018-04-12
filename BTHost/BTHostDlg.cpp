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
#ifdef READ_THREAD
#define WM_USER_ACK             (WM_USER + 1) // posted from BTReadThread
#define WM_USER_RXSINGLEBYTE    (WM_USER + 2)
#define WM_USER_RXDECODEDCMD    (WM_USER + 3)
#endif

#ifdef WSAASYNCSELECT
#define WM_USER_KRT2            (WM_USER + 1)
#endif

#define SEND_TIMEOUT            100
#define STX                     0x02
#define ACK                     0x06
#define NAK                     0x15

#ifdef IOALERTABLE
/*static*/ WSAOVERLAPPED CBTHostDlg::m_RecvOverlappedCompletionRoutine;
/*static*/ char CBTHostDlg::m_buf[0x01];
/*static*/ WSABUF CBTHostDlg::m_readBuffer = { _countof(CBTHostDlg::m_buf), CBTHostDlg::m_buf };
#endif

/*static*/ WSAOVERLAPPED CBTHostDlg::m_SendOverlapped;
/*static*/ char CBTHostDlg::m_sendBuf[0x10000];
/*static*/ WSABUF CBTHostDlg::m_sendBuffer = { _countof(CBTHostDlg::m_sendBuf), CBTHostDlg::m_sendBuf };

/*static*/ SOCKET CBTHostDlg::m_socketLocal = INVALID_SOCKET;

BEGIN_DHTML_EVENT_MAP(CBTHostDlg)
	DHTML_EVENT_ONCLICK(_T("btnSoft1"), OnBtnSoft1)
	DHTML_EVENT_ONCLICK(_T("btnSoft2"), OnBtnSoft2)
END_DHTML_EVENT_MAP()

BEGIN_MESSAGE_MAP(CBTHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
#ifdef READ_THREAD
	ON_MESSAGE(WM_USER_RXSINGLEBYTE, OnRXSingleByte)
#endif
#ifdef WSAASYNCSELECT
	ON_MESSAGE(WM_USER_KRT2, OnAsyncSelectKRT2)
#endif
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
	m_bstrInputBT = KRT2INPUT_BT;
#endif

#ifdef KRT2INPUT_PORT
	m_addrSimulator.sin_family = AF_INET;
	// BTSample\TCPEchoServer, https://msdn.microsoft.com/de-de/library/windows/desktop/ms737593(v=vs.85).aspx
	m_addrSimulator.sin_port = ::htons(KRT2INPUT_PORT);

	ADDRINFOW aiHints; // = { AI_PASSIVE, AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL };
	::ZeroMemory(&aiHints, sizeof(aiHints));
	// aiHints.ai_flags = AI_PASSIVE;
	aiHints.ai_family = AF_INET; // AF_INET, AF_UNSPEC
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;
	ADDRINFOW* pAI = NULL;
	// GetAddrInfoW function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms738519(v=vs.85).aspx
	const int iRetC = ::GetAddrInfo(KRT2INPUT_SERVER, L"27015", &aiHints, &pAI); // "27015", TOSTR(KRT2INPUT_PORT)
	if (0 == iRetC)
	{
		for(ADDRINFOW* pCurrent = pAI; pCurrent; pCurrent = pCurrent->ai_next)
		{
			LPSOCKADDR sockaddr_ip;

			// Hole den Pointer zu der Adresse
			// Verschiedene Felder für IPv4 und IPv6
			if (AF_INET == pCurrent->ai_family)  // IPv4
			{
				sockaddr_ip = (LPSOCKADDR)pCurrent->ai_addr;
			}

			else if (AF_INET6 == pCurrent->ai_family) // IPv6
			{
				sockaddr_ip = (LPSOCKADDR)pCurrent->ai_addr;
			}

			wchar_t ipstringbuffer[46];
			DWORD ipbufferlength = 46;
			if (::WSAAddressToString(sockaddr_ip, (DWORD)pCurrent->ai_addrlen, NULL, ipstringbuffer, &ipbufferlength))
				CBTHostDlg::ShowWSALastError(_T("::WSAAddressToString()"));
			else
				ATLTRACE2(atlTraceGeneral, 0, _T("IP address %ws\n"), ipstringbuffer);

			m_addrSimulator.sin_addr = ((struct sockaddr_in*) pAI->ai_addr)->sin_addr;
			_ASSERT(sizeof(struct sockaddr_in) == pAI->ai_addrlen);
		}
		::FreeAddrInfoW(pAI); // Leere die Linked List
	}
	else
		CBTHostDlg::ShowWSALastError(_T("::getaddrinfo()") KRT2INPUT_SERVER);

	/* m_addrSimulator.sin_addr.s_addr = ::inet_addr("127.0.0.1"); // stdafx.h(18) #define _WINSOCK_DEPRECATED_NO_WARNINGS or
	switch (::InetPton(AF_INET, L"127.0.0.1", &m_addrSimulator.sin_addr)) // wir wollen explicit eine IPv4 addresse
	{
	case 0: // invalid (String encoded) address
		break;
	case 1: // succeeded
		break;
	default:
		CBTHostDlg::ShowWSALastError(_T("::InetPton(\"127.0.0.1\")"));
		break;
	} */
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

	/*
	* die IDR_HTML_BTHOST_DIALOG darf/kann nur verwendet werden wenn GARANTIERT ist das sie keine referenzen enthaelt (BOWER)
	* bzw. alle verwendeten referenzen in kopie zu den resource hinzugefuegt wurden
	*/
	m_nHtmlResID = 0;
	m_strCurrentUrl = _T("http://localhost/krt2mngr/bthost/bthost.htm"); // ohne fehler
	// m_strCurrentUrl = _T("http://ws-psi.estos.de/krt2mngr/sevenseg.html"); // need browser_emulation
	CCommandLineInfo cmdLI;
	theApp.ParseCommandLine(cmdLI);
	if (CCommandLineInfo::FileOpen == cmdLI.m_nShellCommand)
	{
		/*
		* wir steuern/configurieren hier primaer den "ServiceInstanceName"
		* vgl. COMHost da steuern/configurieren wir den "COMPort"
		m_strCurrentUrl = cmdLI.m_strFileName;
		*/

		m_bstrInputBT = cmdLI.m_strFileName;
	}
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

	// Connect();
	return TRUE; // return TRUE  unless you set the focus to a control
}

/*virtual*/ void CBTHostDlg::OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR szUrl)
{
	CDHtmlDialog::OnDocumentComplete(pDisp, szUrl);

	m_spHtmlDoc->get_Script(&m_ddScript.p);

	// SetElementProperty(_T("btnSoft1"), DISPID_VALUE, &CComVariant(L"Check hurtz")); // for <input> elements
#ifdef BTNSOFT1_ENUMPROTOCOLS
	SetElementText(_T("btnSoft1"), _T("enumProtocols"));
#endif

#ifdef BTNSOFT1_ENUMRADIO
	SetElementText(_T("btnSoft1"), _T("enumBTRadio"));
#endif

#ifdef BTNSOFT1_DISCOVERDEVICE
	SetElementText(_T("btnSoft1"), _T("enumBTDevice"));
#endif

#ifdef BTNSOFT1_DISCOVERSERVICE
	SetElementText(_T("btnSoft1"), _T("enumBTService"));
#endif

#ifdef BTNSOFT2_CONNECT
	SetElementText(_T("btnSoft2"), _T("connect"));
#endif

#ifdef BTNSOFT2_KRT2PING
	SetElementText(_T("btnSoft2"), _T("SendPing"));
#endif
}

/*virtual*/ BOOL CBTHostDlg::IsExternalDispatchSafe()
{
	return TRUE; // __super::IsExternalDispatchSafe();
}

#ifdef KRT2INPUT_PORT
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

#ifdef READ_THREAD
LRESULT CBTHostDlg::OnRXSingleByte(WPARAM wParam, LPARAM lParam)
{
	_ASSERT(m_dwThreadAffinity == ::GetCurrentThreadId());
	_ASSERT(1 == HIWORD(wParam));
	ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::OnRXSingleByte() Byte: 0x%.2x, length: %hu\n"), LOWORD(wParam), HIWORD(wParam));

	/*
	* siehe auch:
	*   C:\Users\psi\Source\Repos\KRT2Mngr\BTHost\BTHost.htm(78): function OnRXSingleByte(nByte)
	*   C:\Users\psi\Source\Repos\KRT2Mngr\IByteLevel.js(7): function OnRXSingleByte(nByte)
	*/
	m_ddScript.Invoke1(_T("OnRXSingleByte"), &CComVariant(LOWORD(wParam)));

	// read next / continue loop
#ifdef IOALERTABLE
	_ASSERT(((unsigned char)*CBTHostDlg::m_buf) == LOWORD(wParam));
	CBTHostDlg::QueueRead();
#endif

	return 0;
}
#endif

#ifdef WSAASYNCSELECT
// Although WSAAsyncSelect can be called with interest in multiple events, the application window will receive a single message for each network event.
LRESULT CBTHostDlg::OnAsyncSelectKRT2(WPARAM wParam, LPARAM lParam)
{
	ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::OnAsyncSelectKRT2() WSAGETSELECTERROR: 0x%.8x\n"), WSAGETSELECTERROR(lParam));

	switch (WSAGETSELECTEVENT(lParam))
	{
		case FD_READ:
		{
			char buf[0x01];
			WSABUF wsaBuffer = { _countof(buf), buf };
			DWORD dwNumberOfBytesRecvd = 0;
			DWORD dwFlags = 0; // MSG_PARTIAL, MSG_OOB
			const DWORD dwStart = ::GetTickCount();
			const int iRetC = ::WSARecv(CBTHostDlg::m_socketLocal, &wsaBuffer, 1, &dwNumberOfBytesRecvd, &dwFlags, NULL, NULL);
			const DWORD dwTickDiff = ::GetTickCount() - dwStart;
			_ASSERT(1000 > dwTickDiff); // da hat was blockiert???
			if (SOCKET_ERROR != iRetC)
			{
				_ASSERT(_countof(buf) == dwNumberOfBytesRecvd);
				ATLTRACE2(atlTraceGeneral, 0, _T("byte: 0x%.8x received, dwTickDiff: %d\n"), *buf, dwTickDiff);
				m_ddScript.Invoke1(_T("OnRXSingleByte"), &CComVariant(*buf));
			}
			else
				CBTHostDlg::ShowWSALastError(_T("::WSARecv(m_socketLocal, ...)"));
		}
			break;

/*
* The FD_WRITE event is handled slightly differently.
* An FD_WRITE message is posted when a socket is first connected with connect or WSAConnect (after FD_CONNECT, if also registered) or
* accepted with accept or WSAAccept, and then after a send operation fails with WSAEWOULDBLOCK and buffer space becomes available.
* Therefore, an application can assume that sends are possible starting from the first FD_WRITE message and lasting until a send returns WSAEWOULDBLOCK.
* After such a failure the application will be notified that sends are again possible with an FD_WRITE message.
*/
		case FD_WRITE:
			ATLTRACE2(atlTraceGeneral, 0, _T("(R)eady(T)o(S)end\n"));
			break;
	}
	return 0;
}
#endif

HRESULT CBTHostDlg::OnBtnSoft1(IHTMLElement* /*pElement*/)
{
	HRESULT hr = NOERROR;

#ifdef BTNSOFT1_ENUMPROTOCOLS
	hr = enumProtocols();
#endif

#ifdef BTNSOFT1_ENUMRADIO
	HANDLE hRadio = NULL;
	hr = enumBTRadio(hRadio);
#endif

#ifdef BTNSOFT1_DISCOVERDEVICE
	// hr = enumBTDevices(RFCOMM_PROTOCOL_UUID);
	// hr = enumBTDevices(SerialPortServiceClass_UUID);
	hr = enumBTDevices(CLSID_NULL);
#endif

#ifdef BTNSOFT1_DISCOVERSERVICE
	hr = DiscoverService();
#endif

	return S_OK;
}

HRESULT CBTHostDlg::OnBtnSoft2(IHTMLElement* /*pElement*/)
{
	HRESULT hr = NOERROR;

#ifdef BTNSOFT2_CONNECT
	hr = Connect();
#endif

#ifdef BTNSOFT2_KRT2PING
	hr = SendPing();
#endif

	return hr;
}

HRESULT CBTHostDlg::DiscoverService()
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

HRESULT CBTHostDlg::SendPing()
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

HRESULT CBTHostDlg::Connect()
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
	hr = CBTHostDlg::NameToBthAddr(m_bstrInputBT, &m_addrKRT2);
	if (SUCCEEDED(hr))
	{
		m_addrKRT2.port = KRT2INPUT_PORT; // Port/Profile
		Connect(&m_addrKRT2);
	}
#endif

	return S_OK;
}

HRESULT CBTHostDlg::enumProtocols()
{
	DWORD dwBufferLen = 16384;
	LPWSAPROTOCOL_INFO lpProtocolInfo = (LPWSAPROTOCOL_INFO)::HeapAlloc(::GetProcessHeap(), 0, dwBufferLen);
	if (lpProtocolInfo == NULL)
		return E_FAIL;

	INT iNuminfo = ::WSAEnumProtocols(NULL, lpProtocolInfo, &dwBufferLen);
	if (SOCKET_ERROR == iNuminfo)
	{
		const DWORD dwLastError = CBTHostDlg::ShowWSALastError(_T("::WSAEnumProtocols"));
		switch (dwLastError)
		{
			case WSAENOBUFS:
				break;
		}
	}

	for (int i = 0; i < iNuminfo; i++) {
		ATLTRACE2(atlTraceGeneral, 0, L"Winsock Catalog Provider Entry #%d\n", i);
		if (lpProtocolInfo[i].ProtocolChain.ChainLen = 1)
			ATLTRACE2(atlTraceGeneral, 0, L"Entry type: Base Service Provider\n");
		else
			ATLTRACE2(atlTraceGeneral, 0, L"Entry type: Layered Chain Entry\n");

		ATLTRACE2(atlTraceGeneral, 0, L"Protocol: %ws\n", lpProtocolInfo[i].szProtocol);

		WCHAR GuidString[40] = { 0 };
		int iRet = ::StringFromGUID2(lpProtocolInfo[i].ProviderId, (LPOLESTR)& GuidString, 39);
		if (0 == iRet)
			ATLTRACE2(atlTraceGeneral, 0, L"::StringFromGUID2 failed\n");
		else
			ATLTRACE2(atlTraceGeneral, 0, L"Provider ID: %ws\n", GuidString);

		ATLTRACE2(atlTraceGeneral, 0, L"Catalog Entry ID: %u\n", lpProtocolInfo[i].dwCatalogEntryId);

		ATLTRACE2(atlTraceGeneral, 0, L"Version: %d\n", lpProtocolInfo[i].iVersion);

		ATLTRACE2(atlTraceGeneral, 0, L"Address Family: %d\n", lpProtocolInfo[i].iAddressFamily);
		ATLTRACE2(atlTraceGeneral, 0, L"Max Socket Address Length: %d\n", lpProtocolInfo[i].iMaxSockAddr);
		ATLTRACE2(atlTraceGeneral, 0, L"Min Socket Address Length: %d\n", lpProtocolInfo[i].iMinSockAddr);

		ATLTRACE2(atlTraceGeneral, 0, L"Socket Type: %d\n", lpProtocolInfo[i].iSocketType);
		ATLTRACE2(atlTraceGeneral, 0, L"Socket Protocol: %d\n", lpProtocolInfo[i].iProtocol);
		ATLTRACE2(atlTraceGeneral, 0, L"Socket Protocol Max Offset: %d\n", lpProtocolInfo[i].iProtocolMaxOffset);

		ATLTRACE2(atlTraceGeneral, 0, L"Network Byte Order: %d\n", lpProtocolInfo[i].iNetworkByteOrder);
		ATLTRACE2(atlTraceGeneral, 0, L"Security Scheme: %d\n", lpProtocolInfo[i].iSecurityScheme);
		ATLTRACE2(atlTraceGeneral, 0, L"Max Message Size: %u\n", lpProtocolInfo[i].dwMessageSize);

		ATLTRACE2(atlTraceGeneral, 0, L"ServiceFlags1: 0x%x\n", lpProtocolInfo[i].dwServiceFlags1);
		ATLTRACE2(atlTraceGeneral, 0, L"ServiceFlags2: 0x%x\n", lpProtocolInfo[i].dwServiceFlags2);
		ATLTRACE2(atlTraceGeneral, 0, L"ServiceFlags3: 0x%x\n", lpProtocolInfo[i].dwServiceFlags3);
		ATLTRACE2(atlTraceGeneral, 0, L"ServiceFlags4: 0x%x\n", lpProtocolInfo[i].dwServiceFlags4);
		ATLTRACE2(atlTraceGeneral, 0, L"ProviderFlags: 0x%x\n", lpProtocolInfo[i].dwProviderFlags);

		ATLTRACE2(atlTraceGeneral, 0, L"Protocol Chain length: %d\n", lpProtocolInfo[i].ProtocolChain.ChainLen);
	}

	if (lpProtocolInfo)
	{
		::HeapFree(::GetProcessHeap(), 0, lpProtocolInfo);
		lpProtocolInfo = NULL;
	}

	return NOERROR;
}

/*
* AdpterListe:
* - Broadcom, Broadcom BCM20702 Bluetooth 4.0 USB Device          => aktuell der beste
* - LogiLink, Bluetooth 2.0 USB Device                            => OK
* - SiteCom, CSR Harmony Wireless Software Stack Version 2.1.63.0 => 90% ausfall
*/
HRESULT CBTHostDlg::enumBTRadio(HANDLE& hRadio)
{
	HRESULT hr = E_FAIL;
	BLUETOOTH_FIND_RADIO_PARAMS findradioParams;
	memset(&findradioParams, 0x00, sizeof(findradioParams));
	findradioParams.dwSize = sizeof(findradioParams);
	HANDLE _hRadio = NULL;
	HBLUETOOTH_RADIO_FIND hbtrf = ::BluetoothFindFirstRadio(&findradioParams, &_hRadio);
	if (NULL != hbtrf)
	{
		BOOL bRetC = TRUE;
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
	}
	else
	{
		// ::MessageBox(NULL, _T("No Radio found"), _T("::BluetoothFindFirstRadio"), MB_OK);
		ATLTRACE2(atlTraceGeneral, 0, _T("::BluetoothFindFirstRadio() returned: No Radio found\n"));
	}

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
	// A socket created by the socket function will have the overlapped attribute (WSA_FLAG_OVERLAPPED) as the default.
	SOCKET socketLocal = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (INVALID_SOCKET != socketLocal)
	{
		if (INVALID_SOCKET != ::connect(socketLocal, (struct sockaddr *) pRemoteAddr, sizeof(SOCKADDR_BTH)))
		{
			/*
			* wir lesen IMMER nonblocking.
			* entweder IOALERTABLE (OVERLAPPED_COMPLETION_ROUTINE) ODER
			* READ_THREAD (GetOverlappedResult)
			*/
			CBTHostDlg::m_socketLocal = socketLocal;

#ifdef IOALERTABLE
			CBTHostDlg::InitCompletionRoutine();
			CBTHostDlg::QueueRead();
#endif

#ifdef READ_THREAD
			m_ReadThreadArgs.hwndMainDlg = m_hWnd;
			m_ReadThreadArgs.socketLocal = socketLocal;
			_ASSERT(INVALID_HANDLE_VALUE != m_ReadThreadArgs.hEvtTerminate);
			m_hReadThread = (HANDLE)_beginthreadex(NULL, 0, CBTHostDlg::BTReadThread, &m_ReadThreadArgs, 0, NULL);
#endif
		}
		else
		{
			CBTHostDlg::ShowWSALastError(_T("::connect(socketLocal, ...)"));

			::closesocket(socketLocal);
			socketLocal = INVALID_SOCKET;
		}
	}
	else
		CBTHostDlg::ShowWSALastError(_T("::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM)"));

	return NOERROR;
}

/*
* caller is responsible to free *ppAddrSimulator
*/
/*static*/ HRESULT CBTHostDlg::NameToTCPAddr(
	const LPWSTR      pszRemoteName,        // IN
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
	if (0 != iResult)
	{
		// CBTHostDlg::ShowWSALastError(_T("::getaddrinfo()"));
		ATLTRACE2(atlTraceGeneral, 0, _T("::getaddrinfo() failed with error: %d\n"), iResult);
		return E_FAIL;
	}

	// Attempt to connect to an address until one succeeds
	SOCKET ConnectSocket = INVALID_SOCKET;
	for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		// Create a SOCKET for connecting to server
		ConnectSocket = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET)
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("socket failed with error: %ld\n"), ::WSAGetLastError());
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

	if (INVALID_SOCKET == ConnectSocket)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("Unable to connect to server!\n"));
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
	* Hinweis:
	*   A socket created by the socket function will have the overlapped attribute (WSA_FLAG_OVERLAPPED) as the default.
	*/
	_ASSERT(INVALID_SOCKET == CBTHostDlg::m_socketLocal);
#ifdef USE_WSA
	SOCKET socketLocal = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0 /* WSA_FLAG_OVERLAPPED */);
#else
	SOCKET socketLocal = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

#ifdef TESTCASE_NONBLOCKING_SEND
#ifdef USE_WSA
#else
	ATLTRACE2(atlTraceGeneral, 0, _T("TESTCASE_NONBLOCKING: reduce buffersizes (SO_SNDBUF/SO_RCVBUF) to ZERO!\n"));
	DWORD dwBufSize = 0;
	if (SOCKET_ERROR == ::setsockopt(socketLocal, SOL_SOCKET, SO_SNDBUF, (const char*)&dwBufSize, sizeof(dwBufSize)))
		CBTHostDlg::ShowWSALastError(_T("::setsockopt(..., SO_SNDBUF, 0, ...)"));
	if (SOCKET_ERROR == ::setsockopt(socketLocal, SOL_SOCKET, SO_RCVBUF, (const char*)&dwBufSize, sizeof(dwBufSize)))
		CBTHostDlg::ShowWSALastError(_T("::setsockopt(..., SO_RCVBUF, 0, ...)"));

	int iOptLen = sizeof(dwBufSize);
	if (SOCKET_ERROR == ::getsockopt(socketLocal, SOL_SOCKET, SO_SNDBUF, (char*)&dwBufSize, &iOptLen))
		CBTHostDlg::ShowWSALastError(_T("::getsockopt(..., SO_SNDBUF, ...)"));
	ATLTRACE2(atlTraceGeneral, 0, _T("SO_SNDBUF: %d\n"), dwBufSize);
#endif
#endif

	if (INVALID_SOCKET != socketLocal)
	{
#ifdef USE_WSA
		if(SOCKET_ERROR != ::WSAConnect(socketLocal, (SOCKADDR*)addrServer, sizeof(SOCKADDR_IN), NULL, NULL, NULL, NULL))
#else
		if (INVALID_SOCKET != ::connect(socketLocal, (SOCKADDR*)addrServer, sizeof(SOCKADDR_IN)))
#endif
		{
			/*
			* wir lesen IMMER nonblocking.
			* entweder IOALERTABLE (OVERLAPPED_COMPLETION_ROUTINE), READ_THREAD (GetOverlappedResult) ODER
			* WSAASYNCSELECT
			*/
			CBTHostDlg::m_socketLocal = socketLocal;

#ifdef IOALERTABLE
			CBTHostDlg::InitCompletionRoutine();
			CBTHostDlg::QueueRead();
#endif

#ifdef READ_THREAD
			m_ReadThreadArgs.hwndMainDlg = m_hWnd;
			m_ReadThreadArgs.socketLocal = socketLocal;
			_ASSERT(INVALID_HANDLE_VALUE != m_ReadThreadArgs.hEvtTerminate);
			m_hReadThread = (HANDLE)_beginthreadex(NULL, 0, CBTHostDlg::BTReadThread, &m_ReadThreadArgs, 0, NULL);
#endif

#ifdef WSAASYNCSELECT
			/*
			* WSAAsyncSelect Function, https://msdn.microsoft.com/de-de/library/windows/desktop/ms741540(v=vs.85).aspx
			* The WSAAsyncSelect() Model Program Example, http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancediomethod5b.html
			*/
			if (SOCKET_ERROR == ::WSAAsyncSelect(CBTHostDlg::m_socketLocal, m_hWnd, WM_USER_KRT2, FD_READ | FD_WRITE))
				CBTHostDlg::ShowWSALastError(_T("::WSAAsyncSelect(..., FD_READ | FD_WRITE)"));

			/*
			* sobald daten verfuegbar sind werden wir via. WM_USER_KRT2 benachrichtigt und
			* koennen GLUECKLICHERWEISE GARANTIERT ein byte lesen
			*/
#endif

#ifdef NONBLOCKING
	#ifdef USE_WSA
			unsigned long ulMode = 1;
			DWORD dwBytesReturned = 0;
			if(::WSAIoctl(socketLocal, FIONBIO, &ulMode, sizeof(ulMode), NULL, 0, &dwBytesReturned, NULL, NULL))
				CBTHostDlg::ShowWSALastError(_T("::WSAIoctl(..., FIONBIO, 1)"));
	#else

			u_long argp = 1;
			if (SOCKET_ERROR == ::ioctlsocket(socketLocal, FIONBIO, &argp))
				CBTHostDlg::ShowWSALastError(_T("::ioctlsocket(..., FIONBIO, 1)"));
	#endif
#endif

		}
		else
		{
			CBTHostDlg::ShowWSALastError(_T("::connect(socketLocal, ...)"));

			::closesocket(socketLocal);
			socketLocal = INVALID_SOCKET;
		}
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
	{
		const DWORD dwLastError = CBTHostDlg::ShowWSALastError(_T("WSALookupServiceBegin (device discovery)"));
		switch (dwLastError)
		{
		case WSASERVICE_NOT_FOUND:
			// propably no BluetoothRadio installed/plugedin
			break;
		}
	}

	/*
	* Initialisation succeded, start looking for devices
	* do not use fixed sized (hard-coded) buffers!
	* see NameToBthAddr() for correct allocation
	*/
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

				if (CLSID_NULL != serviceClass)
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
	WCHAR szGUID[0x0100];
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
					WCHAR szAddress[0x0100];
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
#define CXN_MAX_INQUIRY_RETRY             3
#define CXN_DELAY_NEXT_INQUIRY            15
/*static*/ HRESULT CBTHostDlg::NameToBthAddr(
	const LPWSTR pszRemoteName,
	PSOCKADDR_BTH pRemoteBtAddr)
{
	HRESULT hrResult = NOERROR;
	BOOL    bContinueLookup = FALSE;
	BOOL    bRemoteDeviceFound = FALSE;
	ULONG   ulFlags = 0;
	ULONG   ulPQSSize = sizeof(WSAQUERYSET);
	HANDLE  hLookup = NULL;

	::ZeroMemory(pRemoteBtAddr, sizeof(*pRemoteBtAddr));

	/*
	* der hier allozierte speicher ist fuer ::WSALookupServiceNext() i.d.R. zu klein. (kann man sich gleich sparen)
	* fuer das ::WSALookupServiceBegin() ist der speicher ausreichend (es wird ja kein device zurueckgeliefert)
	* wir fangen den fehler ab (WSAEFAULT == ::WSALookupServiceNext) und allozieren die gewuenschte groesse
	*
	* HeapAlloc function, https://msdn.microsoft.com/de-de/library/windows/desktop/aa366597(v=vs.85).aspx
	* If the function fails, it does not call SetLastError. An application cannot call GetLastError for extended error information.
	*/
	PWSAQUERYSET pWSAQuerySet = (PWSAQUERYSET)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, ulPQSSize);
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
				ATLTRACE2(atlTraceGeneral, 0, _T("*INFO* | Inquiring device from cache...\n"));

			else
			{
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
				::Sleep(CXN_DELAY_NEXT_INQUIRY * 1000);

				ATLTRACE2(atlTraceGeneral, 0, _T("*INFO* | Inquiring device ...\n"));
			}

			// Start the lookup service
			hLookup = 0;
			bContinueLookup = FALSE;
			::ZeroMemory(pWSAQuerySet, ulPQSSize);
			pWSAQuerySet->dwNameSpace = NS_BTH;
			pWSAQuerySet->dwSize = sizeof(WSAQUERYSET);
			hrResult = ::WSALookupServiceBegin(pWSAQuerySet, ulFlags, &hLookup);
			if (0 != hrResult)
			{
				const DWORD dwLastError = CBTHostDlg::ShowWSALastError(_T("WSALookupServiceBegin (device discovery)"));
				switch (dwLastError)
				{
					case WSASERVICE_NOT_FOUND:
						// propably no BluetoothRadio installed/plugedin
						break;
				}
			}

			// Even if we have an error, we want to continue until we
			// reach the CXN_MAX_INQUIRY_RETRY
			if ((NOERROR == hrResult) && (NULL != hLookup))
				bContinueLookup = TRUE;

			else if (0 < iRetryCount)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("=CRITICAL= | WSALookupServiceBegin() failed with error code %d, WSAGetLastError = %d\n"), hrResult, ::WSAGetLastError());
				break;
			}

			while (bContinueLookup)
			{
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
				if (NO_ERROR == ::WSALookupServiceNext(hLookup, ulFlags, &ulPQSSize, pWSAQuerySet))
				{
					//
					// Compare the name to see if this is the device we are looking for.
					//
					if ((pWSAQuerySet->lpszServiceInstanceName != NULL) && (0 == _wcsicmp(pWSAQuerySet->lpszServiceInstanceName, pszRemoteName)))
					{
						//
						// Found a remote bluetooth device with matching name.
						// Get the address of the device and exit the lookup.
						//
						CComBSTR bstrAddress;
						CBTHostDlg::BTAddressToString((PSOCKADDR_BTH)pWSAQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr, &bstrAddress);
						ATLTRACE2(atlTraceGeneral, 0, _T("Address for: %ls is: %ls\n"), pszRemoteName, (LPCWSTR)bstrAddress);

						::CopyMemory(pRemoteBtAddr, (PSOCKADDR_BTH)pWSAQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr, sizeof(*pRemoteBtAddr));
						bRemoteDeviceFound = TRUE;
						bContinueLookup = FALSE;
					}
				}
				else
				{
					hrResult = ::WSAGetLastError();
					if (WSA_E_NO_MORE == hrResult)
					{
						// No more data
						// No more devices found.  Exit the lookup.
						bContinueLookup = FALSE;
					}

					else if (WSAEFAULT == hrResult)
					{
						//
						// The buffer for QUERYSET was insufficient.
						// In such case 3rd parameter "ulPQSSize" of function "WSALookupServiceNext()" receives
						// the required size.  So we can use this parameter to reallocate memory for QUERYSET.
						//
						::HeapFree(::GetProcessHeap(), 0, pWSAQuerySet);
						pWSAQuerySet = (PWSAQUERYSET)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, ulPQSSize);
						if (NULL == pWSAQuerySet)
						{
							ATLTRACE2(atlTraceGeneral, 0, _T("!ERROR! | Unable to allocate memory for WSAQERYSET\n"));
							hrResult = STATUS_NO_MEMORY;
							bContinueLookup = FALSE;
						}
					}
					else
					{
						ATLTRACE2(atlTraceGeneral, 0, _T("=CRITICAL= | WSALookupServiceNext() failed with error code %d\n"), hrResult);
						bContinueLookup = FALSE;
					}
				}
			}

			//
			// End the lookup service
			//
			::WSALookupServiceEnd(hLookup);

			if (STATUS_NO_MEMORY == hrResult)
				break;
		}
	}

	if (NULL != pWSAQuerySet)
	{
		::HeapFree(::GetProcessHeap(), 0, pWSAQuerySet);
		pWSAQuerySet = NULL;
	}

	if (bRemoteDeviceFound)
		hrResult = NOERROR;
	else
		hrResult = E_INVALIDARG;

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

/*static*/ DWORD CBTHostDlg::ShowWSALastError(LPCTSTR szCaption)
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
	return dwMessageId;
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
			ATLTRACE2(atlTraceGeneral, 1, _T("number of bytes received(Sync): 0x%.8x, char: %hc\n"), dwNumberOfBytesRecvd, *readBuffer.buf);
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

					ATLTRACE2(atlTraceGeneral, 1, _T("number of bytes received(Async): 0x%.8x, char: %hc\n"), dwNumberOfBytesRecvd, *readBuffer.buf);
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
	const int iRetC = ::WSARecv(CBTHostDlg::m_socketLocal, &CBTHostDlg::m_readBuffer, 1, &dwNumberOfBytesRecvd, &dwFlags, &CBTHostDlg::m_RecvOverlappedCompletionRoutine, CBTHostDlg::RecvCompletionRoutine);
	if (SOCKET_ERROR == iRetC)
	{
		const int iLastError = ::WSAGetLastError();
		// const int iLastError = CBTHostDlg::ShowWSALastError(_T("::WSARecv(m_socketLocal, ...)"));
		if (WSA_IO_PENDING != iLastError)
			return E_FAIL;
	}
	return NOERROR;
}

/*static*/ void CALLBACK CBTHostDlg::RecvCompletionRoutine(DWORD dwError, DWORD dwBytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	/*
	* die CompletionRoutine wird offensichtlich durch den MainThread ausgefuehrt
	* wenn wir die static WorkerRoutine() durch eine MemberFunction ersetzen haben wir gewonnen!
	_ASSERT(m_dwThreadAffinity == ::GetCurrentThreadId());
	*/

	// ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::RecvCompletionRoutine() dwBytesTransferred: 0x%.8x, ::GetCurrentThreadId(): 0x%.8x\n"), dwBytesTransferred, ::GetCurrentThreadId());
	if (0 < dwBytesTransferred)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::RecvCompletionRoutine() dwBytesTransferred: 0x%.8x, Byte: 0x%.2x\n"), dwBytesTransferred, (unsigned char) *CBTHostDlg::m_buf);
		/*
		* wir muessen aus diesem "static" context in den instance context von CBTHostDlg wechseln damit wir zugriff auf m_ddScript haben.
		* z.B. SendMessage()
		*/
		AfxGetMainWnd()->PostMessage(WM_USER_RXSINGLEBYTE, MAKEWPARAM((unsigned char) *CBTHostDlg::m_buf, dwBytesTransferred), NULL);
	}
}
#endif

/*
* wir treiben hier einen IRRSINNS aufwand ein nonblocking send zu implementieren.
* das hat wirklich nur akademischen nutzen denn:
*   Windows stellt hier typischerweise buffer in der groessenordnung von 512kB bereit
*   so das es unter beruecksichtigung des KRT2 protokolles und den dort definierten groessen NIEMALS zu einer blockade kommt.
*/
void CBTHostDlg::txBytes(
	BSTR bstrBytes)
{
	// convert intel Hex format (bstrBytes) into byte buffer (rgData)
	char* pWrite = CBTHostDlg::m_sendBuf;
	{ // local variable scope
		WCHAR* pcNext = NULL;
		LPWSTR szToken = _tcstok_s(bstrBytes, L" ", &pcNext); // bstrBytes will be modified!
		while (NULL != szToken)
		{
			*pWrite++ = _tcstol(szToken, NULL, 16);
			szToken = _tcstok_s(NULL, L" ", &pcNext);
		}

#ifdef TESTCASE_NONBLOCKING_SEND
		while (pWrite < CBTHostDlg::m_sendBuf + _countof(CBTHostDlg::m_sendBuf)) // fill the buffer
			*pWrite++ = ' ';
		_ASSERT(_countof(CBTHostDlg::m_sendBuf) == CBTHostDlg::m_sendBuffer.len);
#else

		CBTHostDlg::m_sendBuffer.len = pWrite - CBTHostDlg::m_sendBuf;
#endif
	}

#ifdef USE_WSA
	/*
	* WSASend Function, https://msdn.microsoft.com/de-de/library/windows/desktop/ms742203(v=vs.85).aspx
	*/
	DWORD dwBytesSent = 0;
	DWORD dwFlags = 0;
	const int iRetC = ::WSASend(CBTHostDlg::m_socketLocal, &CBTHostDlg::m_sendBuffer, 1, &dwBytesSent, dwFlags, NULL, NULL);
	if (0 == iRetC)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::txBytes() number of bytes send(Sync): 0x%.8x\n"), dwBytesSent);
	}

	else
	{
		_ASSERT(SOCKET_ERROR == iRetC);
		const int iLastError = ::WSAGetLastError();
		if (WSAEWOULDBLOCK == iLastError) // A non-blocking socket operation could not be completed immediately. (10035L)
		{
#ifdef WSAASYNCSELECT
			ATLTRACE2(atlTraceGeneral, 0, _T("wait for FD_WRITE\n"));
#else
			ATLTRACE2(atlTraceGeneral, 0, _T("send incomplete, try again\n"));
#endif
		}
	}
#else

	/*
	* Alertable I/O, https://msdn.microsoft.com/en-us/library/windows/desktop/aa363772(v=vs.85).aspx
	*
	* send function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms740149(v=vs.85).aspx
	* error: deprecated function
	*/
	const DWORD dwStart = ::GetTickCount();
	const int iBytesSend = ::send(CBTHostDlg::m_socketLocal, CBTHostDlg::m_sendBuffer.buf, CBTHostDlg::m_sendBuffer.len, 0);
	const DWORD dwTickDiff = ::GetTickCount() - dwStart;
	_ASSERT(1000 > dwTickDiff);
	if (SOCKET_ERROR == iBytesSend)
	{
	}

	else if (CBTHostDlg::m_sendBuffer.len < iBytesSend)
	{
		const int iLastError = ::WSAGetLastError();
		if (WSAEWOULDBLOCK == iLastError)
			ATLTRACE2(atlTraceGeneral, 0, _T("wait for FD_WRITE to continue\n"));

		// um das zu forcieren braucht es: TESTCASE_NONBLOCKING
		CBTHostDlg::ShowWSALastError(_T("::send(m_socketLocal, ...)"));
		ATLTRACE2(atlTraceGeneral, 0, _T("block splitted/segmented! sizeofFirstBlock: 0x%.8x\n"), iBytesSend);
}
	else
	{
		_ASSERT(CBTHostDlg::m_sendBuffer.len == iBytesSend);
		ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::txBytes() number of bytes send(Sync): 0x%.8x, dwTickDiff: %d\n"), iBytesSend, dwTickDiff);
	}
#endif

#ifdef IOALERTABLE
	{ // send data
		// Make sure the SendOverlapped struct is zeroed out
		SecureZeroMemory((PVOID)& CBTHostDlg::m_SendOverlapped, sizeof(WSAOVERLAPPED));

		/*
		* WSASend Function, https://msdn.microsoft.com/de-de/library/windows/desktop/ms742203(v=vs.85).aspx
		*/
		DWORD dwFlags = 0;
		const int iRetC = ::WSASend(CBTHostDlg::m_socketLocal, &CBTHostDlg::m_sendBuffer, 1, NULL, dwFlags, &CBTHostDlg::m_SendOverlapped, CBTHostDlg::SendCompletionRoutine);
		if (0 == iRetC)
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::txBytes() number of bytes send(Sync): 0x%.8x\n"), pWrite - CBTHostDlg::m_sendBuf);
		}

		else
		{
			_ASSERT(SOCKET_ERROR == iRetC);
			const int iLastError = ::WSAGetLastError();
			if (WSA_IO_PENDING == iLastError)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("wait for SendCompletionROUTINE\n"));
			}
		}
	}
#endif
}

#ifdef IOALERTABLE
/*static*/ void CBTHostDlg::SendCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	ATLTRACE2(atlTraceGeneral, 0, _T("CBTHostDlg::SendCompletionRoutine() cbTransferred: 0x%.8x\n"), cbTransferred);
}
#endif
