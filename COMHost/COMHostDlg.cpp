// COMHostDlg.cpp : implementation file
//

#include "stdafx.h"
#include "COMHost.h"
#include "COMHostDlg.h"
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
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

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

// CCOMHostDlg dialog
#define WM_USER_ACK             (WM_USER + 1) // posted from COMReadThread
#define WM_USER_RXSINGLEBYTE    (WM_USER + 2)
#define WM_USER_RXDECODEDCMD    (WM_USER + 3)

#define SEND_TIMEOUT            100
#define STX                     0x02
#define ACK                     0x06
#define NAK                     0x15

/*
* ueber diese statics wird mit dem GUI(Main)Thread kommuniziert der zugriff ist also zu sichern
*/
enum _KRT2StateMachine
{
	END,
	IDLE,
	WAIT_FOR_CMD,
	WAIT_FOR_MHZ,
	WAIT_FOR_kHZ,
	WAIT_FOR_N0,
	WAIT_FOR_N1,
	WAIT_FOR_N2,
	WAIT_FOR_N3,
	WAIT_FOR_N4,
	WAIT_FOR_N5,
	WAIT_FOR_N6,
	WAIT_FOR_N7,
	WAIT_FOR_MNR,
	WAIT_FOR_CHK
} s_state = IDLE;
HRESULT s_hrSend = NOERROR;

BEGIN_DHTML_EVENT_MAP(CCOMHostDlg)
	// DHTML_EVENT_ONCLICK(_T("btnSoft1"), InitInputOutput) // soft buttons
	DHTML_EVENT_ONCLICK(_T("btnSoft1"), ReceiveAck)
END_DHTML_EVENT_MAP()

BEGIN_MESSAGE_MAP(CCOMHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
	ON_MESSAGE(WM_USER_ACK, OnAck)
	ON_MESSAGE(WM_USER_RXSINGLEBYTE, OnRXSingleByte)
	ON_MESSAGE(WM_USER_RXDECODEDCMD, OnRXDecodedCmd)
	ON_WM_TIMER()
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CCOMHostDlg, CDHtmlDialog)
	DISP_FUNCTION(CCOMHostDlg, "sendCommand", sendCommand, VT_I4, VTS_BSTR VTS_DISPATCH)
	DISP_FUNCTION(CCOMHostDlg, "fireAndForget", fireAndForget, VT_I4, VTS_BSTR)
	DISP_FUNCTION(CCOMHostDlg, "receiveCommand", receiveCommand, VT_EMPTY, VTS_DISPATCH)
END_DISPATCH_MAP()

CCOMHostDlg::CCOMHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_COMHOST_DIALOG, IDR_HTML_COMHOST_DIALOG, pParent)
{
	EnableAutomation();
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
#ifdef KRT2OUTPUT
	m_hFileWrite = INVALID_HANDLE_VALUE;
#endif
	::ZeroMemory(&m_OverlappedWrite, sizeof(OVERLAPPED));
	m_OverlappedWrite.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset is required for overlapped I/O
	m_OverlappedWrite.Internal;
	m_OverlappedWrite.Offset;
	m_OverlappedWrite.Pointer;

	m_ReadThreadArgs.hCOMPort = INVALID_HANDLE_VALUE;
	m_ReadThreadArgs.hEvtCOMPort = ::CreateEvent(NULL, TRUE, FALSE, NULL); // currently never signaled;
	m_ReadThreadArgs.hEvtTerminate = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	m_ReadThreadArgs.hwndMainDlg = NULL;

	m_hReadThread = INVALID_HANDLE_VALUE;
}

void CCOMHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

/*virtual*/ BOOL CCOMHostDlg::OnInitDialog()
{
	/*
	* wir navigieren zu einer externen resource
	* script fehler? welche zone? welche rechte?
	*   siehe auch:
	*     COMHost.cpp(44): CCOMHostApp::InitInstance()
	*     HKEY_CURRENT_USER\SOFTWARE\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION\COMHost.exe (REG_DWORD) 11000
	*/
	m_nHtmlResID = 0;
	// m_strCurrentUrl = _T("http://localhost/krt2mngr/comhost/comhost.htm"); // ohne fehler
	m_strCurrentUrl = _T("http://ws-psi.estos.de/krt2mngr/sevenseg.html"); // need browser_emulation
	CCommandLineInfo cmdLI;
	theApp.ParseCommandLine(cmdLI);
	if (CCommandLineInfo::FileOpen == cmdLI.m_nShellCommand)
		m_strCurrentUrl = cmdLI.m_strFileName;
	CDHtmlDialog::OnInitDialog();
	// Navigate(_T("http://localhost/krt2mngr/sevenseg.html"));
	SetExternalDispatch((LPDISPATCH) GetInterface(&IID_IDispatch));

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
	// when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE); // Set big icon
	SetIcon(m_hIcon, FALSE); // Set small icon

	/*
	* TODO: Add extra initialization here
	* das initialisieren des Input/Output stream gleich hier ist mir etwas zu "hart"
	*/
	InitInputOutput(NULL);

	return TRUE; // return TRUE  unless you set the focus to a control
}

/*virtual*/ BOOL CCOMHostDlg::IsExternalDispatchSafe()
{
	return TRUE; // __super::IsExternalDispatchSafe();
}

/*virtual*/ void CCOMHostDlg::OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR szUrl)
{
	CDHtmlDialog::OnDocumentComplete(pDisp, szUrl);
	// SetElementText(_T("btnSoft1"), _T("InitInputOutput"));
	SetElementText(_T("btnSoft1"), _T("ReceiveAck"));
}

// CCOMHostDlg message handlers
void CCOMHostDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CCOMHostDlg::OnPaint()
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
HCURSOR CCOMHostDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CCOMHostDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == SEND_TIMEOUT)
	{
		KillTimer(SEND_TIMEOUT);
		s_hrSend = NOERROR;
		m_ddSendCommand.Invoke2((DISPID)0, &CComVariant(_T("timeout")), &CComVariant());
		m_ddSendCommand.Release();
	}

	CDHtmlDialog::OnTimer(nIDEvent);
}

void CCOMHostDlg::OnClose()
{
#ifdef KRT2OUTPUT
	if (INVALID_HANDLE_VALUE != m_hFileWrite)
	{
		::CloseHandle(m_hFileWrite);
		m_hFileWrite = INVALID_HANDLE_VALUE;
	}
#endif

	::CloseHandle(m_OverlappedWrite.hEvent);
	m_OverlappedWrite.hEvent = INVALID_HANDLE_VALUE;

	if (INVALID_HANDLE_VALUE != m_hReadThread)
	{
		// ::SignalObjectAndWait(m_ReadThreadArgs.hEvtTerminate, m_hReadThread, 5000, FALSE);
		CCOMHostDlg::SignalObjectAndWait(m_ReadThreadArgs.hEvtTerminate, m_hReadThread);
		::CloseHandle(m_hReadThread);
	}

	m_hReadThread = INVALID_HANDLE_VALUE;
	::CloseHandle(m_ReadThreadArgs.hEvtTerminate);
	m_ReadThreadArgs.hEvtTerminate = INVALID_HANDLE_VALUE;
	::CloseHandle(m_ReadThreadArgs.hCOMPort);
	m_ReadThreadArgs.hCOMPort = INVALID_HANDLE_VALUE;
	::CloseHandle(m_ReadThreadArgs.hEvtCOMPort);
	m_ReadThreadArgs.hEvtCOMPort = INVALID_HANDLE_VALUE;
	m_ReadThreadArgs.hwndMainDlg = NULL;

	CDHtmlDialog::OnClose();
}

/*
* wir behandeln hier ZWEI unterschiedliche Ack
* - wenn wir ein Command abgeschickt haben verarbeiten wir das Ack indem wir es an unser Script als SUCCEEDED antwort liefern
* - wenn wir im IDLE zustand sind und ein PING ('S') empfangen schicken wir ein Ack zur bestaetigung an das KRT2
*/
LRESULT CCOMHostDlg::OnAck(WPARAM wParam, LPARAM lParam)
{
	if (!m_ddSendCommand)
		sendAck(); // im gegensatz zu sendCommand() verzweigen/stacken wir hier kein "Command"
	else
	{
		KillTimer(SEND_TIMEOUT);
		s_hrSend = NOERROR;
		CComDispatchDriver dd(m_ddSendCommand.Detach()); // wg. synchron
		if (NAK == LOWORD(wParam))
			dd.Invoke2((DISPID)0, &CComVariant(_T("receive FAIL")), &CComVariant());
		else
			dd.Invoke2((DISPID)0, &CComVariant(/* VT_EMPTY */), &CComVariant(_T("receive SUCCEED")));
	}

	return 0;
}

LRESULT CCOMHostDlg::OnRXSingleByte(WPARAM wParam, LPARAM lParam)
{
	ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::OnRXSingleByte(0x%.2x)\n"), LOWORD(wParam));

	/*
	* konsequent muss hier der in JavaScript implementierte parser getrieben werden
	*   z.b. Invoke1(L"DriveStateMachine", CComVariant(LOWORD(wParam)))
	* bzw. einfach ein generisches forward 
	*   z.b. Invoke1(L"OnRXSingleByte", CComVariant(LOWORD(wParam)))
	* oder als JavaScript (callback)
	* window.external.KRT2ReadByte(function(nByte) {
	*   console.log(nByte);
	* })
	*/
	CString strByte;
	strByte.Format(_T("0x%.2x"), LOWORD(wParam));
	CComBSTR bstrCommand = strByte;
	SetElementText(_T("command"), bstrCommand);
	return 0;
}

LRESULT CCOMHostDlg::OnRXDecodedCmd(WPARAM wParam, LPARAM lParam)
{
	if (!m_ddReceiveCommand)
	{
		if(m_bstrReceiveCommand.Length())
			ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::OnRXDecodedCmd(0x%.2x) WARNING: buffer overrun\n"), LOWORD(wParam));

		m_bstrReceiveCommand = (LPCTSTR)lParam;
	}
	else
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::OnRXDecodedCmd(0x%.2x): finish wait for command\n"), LOWORD(wParam));
		ATLTRACE2(atlTraceGeneral, 1, _T("  %s\n"), (LPCTSTR)lParam);
		m_ddReceiveCommand.Invoke1((DISPID)0, &CComVariant((LPCTSTR)lParam));
		m_ddReceiveCommand.Release();
	}

	delete (PTCHAR) lParam;
	return 0;
}

HRESULT CCOMHostDlg::InitInputOutput(IHTMLElement*)
{
	/*
	* Serial Port Sample, https://code.msdn.microsoft.com/windowsdesktop/Serial-Port-Sample-e8accf30/sourcecode?fileId=67164&pathId=1394200469
	* COM1 = \Device\RdpDrPort\;COM1:1\tsclient\COM1
	* auf ASUSPROI5 ist COM5 ein virtueller (ausgehender) Port mit SSP zu KRT21885
	*
	* Communications Resources
	* The CreateFile function can create a handle to a communications resource, such as the serial port COM1.
	* For communications resources, the dwCreationDisposition parameter must be OPEN_EXISTING, the dwShareMode parameter must be zero (exclusive access), and the hTemplateFile parameter must be NULL.
	* Read, write, or read/write access can be specified, and the handle can be opened for overlapped I/O.
	* To specify a COM port number greater than 9, use the following syntax: "\\.\COM10". This syntax works for all port numbers and hardware that allows COM port numbers to be specified.
	* For more information about communications, see:
	*   Communications, https://msdn.microsoft.com/en-us/library/windows/desktop/aa363196(v=vs.85).aspx
	*
	* dummerweise bleibt bei der commandofolge CreateFile/CloseHandle irgendetwas haengen
	* so das ich die commandofolge kein zweites mal ausfuehren kann???
	*/
#ifdef KRT2OUTPUT
	_ASSERT(INVALID_HANDLE_VALUE == m_hFileWrite);
	/*
	* wir koennen File und Device Handle NICHT einfach tauschen. u.A. wegen unterschiedlicher ShareMode.
	* gem. der strategie des kleinsten gemeinsamen nenner waehlen wir EIN HANDLE das mit Read/Write access eroeffnet wird.
	* Creating and Opening Files, https://msdn.microsoft.com/en-us/library/windows/desktop/aa363874(v=vs.85).aspx
	* Serial Communications, https://msdn.microsoft.com/en-us/library/ff802693.aspx
	*   The Platform SDK documentation states that when opening a communications port, the call to CreateFile has the following requirements:
	*   - fdwShareMode must be zero. Communications ports cannot be shared in the same manner that files are shared.
	*     Applications using TAPI can use the TAPI functions to facilitate sharing resources between applications.
	*     For applications not using TAPI, handle inheritance or duplication is necessary to share the communications port.
	*     Handle duplication is beyond the scope of this article; please refer to the Platform SDK documentation for more information.
	*   - fdwCreate must specify the OPEN_EXISTING flag.
	*   - hTemplateFile parameter must be NULL
	*/
	m_hFileWrite = ::CreateFile(KRT2OUTPUT, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	if (INVALID_HANDLE_VALUE == m_hFileWrite)
		CCOMHostDlg::ShowLastError(_T("::CreateFile(") KRT2OUTPUT _T(", GENERIC_WRITE, ...)"));
#endif

#ifdef KRT2INPUT
	_ASSERT(INVALID_HANDLE_VALUE == m_ReadThreadArgs.hCOMPort);
	m_ReadThreadArgs.hCOMPort = ::CreateFile(KRT2INPUT, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (INVALID_HANDLE_VALUE == m_ReadThreadArgs.hCOMPort)
		CCOMHostDlg::ShowLastError(_T("CreateFile(") KRT2INPUT _T(", GENERIC_READ, ...)"));
#endif

#ifdef KRT2COMPORT
	_ASSERT(INVALID_HANDLE_VALUE == m_ReadThreadArgs.hCOMPort);
	m_ReadThreadArgs.hCOMPort = ::CreateFile(KRT2COMPORT, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (INVALID_HANDLE_VALUE == m_ReadThreadArgs.hCOMPort)
		CCOMHostDlg::ShowLastError(_T("::CreateFile(") KRT2COMPORT _T(", GENERIC_READ | GENERIC_WRITE, ...)"));
#endif

	if (INVALID_HANDLE_VALUE != m_ReadThreadArgs.hCOMPort)
	{
		m_ReadThreadArgs.hwndMainDlg = m_hWnd;
		m_hReadThread = (HANDLE)_beginthreadex(NULL, 0, CCOMHostDlg::COMReadThread, &m_ReadThreadArgs, 0, NULL);
	}

	return NOERROR;
}

HRESULT CCOMHostDlg::ReceiveAck(IHTMLElement*)
{
	if (m_ddSendCommand)
	{
		KillTimer(SEND_TIMEOUT);
		/*
		* wir setzen ZUERST das s_hrSend (Flag) zurueck und DANN rufen wir, synchron, in den CallBack
		* so kann der CallBack selbst wieder, synchron, sendCommandaufrufen
		* wir brauchen dieses special fuer den recursiven javascript aufruf mit sendMultipleCommands
		* das gleiche procedere mit dem m_ddSendCommand (nicht so schoen, vieleicht doch ueber ein Post verzoegern???)
		*/
		s_hrSend = NOERROR;
		CComDispatchDriver dd(m_ddSendCommand.Detach());
		m_ddSendCommand.Release();
		dd.Invoke2((DISPID)0, &CComVariant(), &CComVariant(_T("manual SUCCEED")));
	}
	return NOERROR;
}

/*
* das testfile ("krt2input.bin") enthaelt die folgenden UseCases:
*  1.) 'R', 1.2.2 Set frequency & name on passive side, 126.900, PETER
*  2.) 'O', 1.2.6 DUAL-mode on
*  3.) 'o', 1.2.7 DUAL-mode off
*  4.) 'B', low BATT
*  5.) 'D', release low BATT
*  6.) 'J', RX
*  7.) 'V', release RX
*  8.) 'A', 0x0a, 0x03, 0x05, 1.2.3 set volume, squelch, intercom-VOX
*  9.) 'A', command sequence failure by 0x02 (stx)
* 10.) 'O', 1.2.6 DUAL-mode on
*
* sendCommand kennt ab jetzt ZWEI unterschiedliche verhalten
* diese werden ueber den parameter pCallback gesteuert
* 1.) wird auf caller seite ein callback uebergeben so wird
*     eine statemachine getrieben die erst mit einem Ack/Nak/Timeout zurueckgesetzt wird.
* 2.) wird auf caller seite KEIN callback uebergeben so wird
*     KEINE statemachine verwendet.
*/
long CCOMHostDlg::doSendCommand(BSTR bstrCommand)
{
	/*
	* wir muessen hier mindestens ZWEI faelle unterscheiden
	* - unser commandParser ist nicht IDLE. d.H. wir sind noch damit beschaeftigt den input an den commandParser zu dispatchen
	* - wir warten auf die bestaetigung eines bereits gesendeten commands (E_PENDING == m_hrSend)
	*/
	if (NOERROR != s_hrSend)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("  reject send cause: wait for ACK from previous command\n"));
		return E_PENDING;
	}

	if (IDLE != s_state)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("  reject send cause: wait for completion of incomming command\n"));
		return E_PENDING;
	}

	_ASSERT(NULL == m_ddSendCommand);

#ifdef KRT2COMPORT
	DCB dcb;
	::ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	BOOL bRetC = ::GetCommState(m_ReadThreadArgs.hCOMPort, &dcb);

	/* COMMPROP CommProp;
	bRetC = ::GetCommProperties(m_ReadThreadArgs.hCOMPort, &CommProp);

	COMMTIMEOUTS CommTimeouts;
	bRetC = ::GetCommTimeouts(m_ReadThreadArgs.hCOMPort, &CommTimeouts); */
#endif

	/*
	* bstrCommand enthaelt die hexcodierten bytes space delemitted
	* die codierung wird aktuell mit einem PreFix z.B. '0x' uebertragen
	*
	* Hinweis(e):
	* - als generischen ansatz koennte man hier ein ByteArray uebergeben
	*   da Arrays im JavaScript Objecte sind koennen wir NICHT EINAFCH via C/C++ darauf zugreifen
	*/
	CString strCommand(bstrCommand); // die variable koennen wir uns durch _tcstok_s() sparen
	BYTE rgCommand[0x100];
	int iStart = 0;
	unsigned int uiIndex = 0;
	CString strToken = strCommand.Tokenize(_T(" "), iStart);
	for (uiIndex = 0; strToken.GetLength(); uiIndex += 1)
	{
		rgCommand[uiIndex] = _tcstol(strToken, NULL, 16);
		strToken = strCommand.Tokenize(_T(" "), iStart);
	}

	/*
	* die 5 ist natuerlich NICHT allgemein gueltig
	* nur fuer hex-codiert mit PreFix 0xFF UND finalem space
	_ASSERT(uiIndex == strCommand.GetLength() / 5);
	*/
#ifdef KRT2OUTPUT
	if (FALSE == ::WriteFile(m_hFileWrite, rgCommand, uiIndex, NULL, &m_OverlappedWrite))
	{
		const DWORD dwLastError = ::GetLastError();
		if (dwLastError != ERROR_IO_PENDING)
			CCOMHostDlg::ShowLastError(_T("::WriteFile()"), &dwLastError);

		// GetOverlappedResult function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
		DWORD dwNumBytesWritten = -1;
		// WaitCommEvent function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
		if (FALSE == ::GetOverlappedResult(m_hFileWrite, &m_OverlappedWrite, &dwNumBytesWritten, TRUE))
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("  Port closed/EOF\n"));
			DWORD dwError = ::GetLastError();
			_ASSERT(ERROR_HANDLE_EOF == dwError);
		}

		_ASSERT(uiIndex == dwNumBytesWritten);
		ATLTRACE2(atlTraceGeneral, 1, _T("  0x%.8x bytes send\n"), dwNumBytesWritten);
		m_OverlappedWrite.Offset += dwNumBytesWritten;
	}
	else
	{
		/*
		* If an operation is completed immediately, an application needs to be ready to continue processing normally.
		*   Serial Communications, https://msdn.microsoft.com/en-us/library/ff802693.aspx
		*/
	}
#endif

#ifdef KRT2COMPORT
	if (FALSE == ::WriteFile(m_ReadThreadArgs.hCOMPort, rgCommand, uiIndex, NULL, &m_OverlappedWrite))
	{
		const DWORD dwLastError = ::GetLastError();
		if (dwLastError != ERROR_IO_PENDING)
		{
			CCOMHostDlg::ShowLastError(_T("::WriteFile()"), &dwLastError);
			s_hrSend = NOERROR;
			return dwLastError;
		}

		// GetOverlappedResult function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
		DWORD dwNumBytesWritten = -1;
		// WaitCommEvent function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
		if (FALSE == ::GetOverlappedResult(m_ReadThreadArgs.hCOMPort, &m_OverlappedWrite, &dwNumBytesWritten, TRUE))
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("  Port closed/EOF\n"));
			DWORD dwError = ::GetLastError();
			_ASSERT(ERROR_HANDLE_EOF == dwError);
		}

		_ASSERT(uiIndex == dwNumBytesWritten);
		ATLTRACE2(atlTraceGeneral, 1, _T("  0x%.8x bytes send\n"), dwNumBytesWritten);
		m_OverlappedWrite.Offset += dwNumBytesWritten;
	}
	else
	{
		/*
		* If an operation is completed immediately, an application needs to be ready to continue processing normally.
		*   Serial Communications, https://msdn.microsoft.com/en-us/library/ff802693.aspx
		*/
	}

	DWORD dwErrors = 0;
	COMSTAT COMStat;
	if (!::ClearCommError(m_ReadThreadArgs.hCOMPort, &dwErrors, &COMStat))
		CCOMHostDlg::ShowLastError(_T("::ClearCommError()"));
#endif

	return NOERROR;
}

/*
* diese wrapper existieren nur weil ich aus dem JavaScript keine window.external methode mit null aufrufen
* kann die als VTS_DISPATCH declariert ist.
*/
long CCOMHostDlg::fireAndForget(BSTR bstrCommand)
{
	ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::IDispatch::fireAndForget(%ls)\n"), bstrCommand);
	return doSendCommand(bstrCommand);
}

long CCOMHostDlg::sendCommand(BSTR bstrCommand, LPDISPATCH pCallback)
{
	ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::IDispatch::sendCommand(%ls)\n"), bstrCommand);
	HRESULT hr = doSendCommand(bstrCommand);
	if(SUCCEEDED(hr) && pCallback)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("  requires Ack/Nak, support callback/timeout\n"));

		m_ddSendCommand = pCallback;
		const UINT_PTR tSendTimeout = SetTimer(SEND_TIMEOUT, 5000, NULL);
		_ASSERT(SEND_TIMEOUT == tSendTimeout); // in userem fall gibt es NUR EINE instance
		s_hrSend = E_PENDING;
	}
	return hr;
}

/*
* im gegensatz zu einem NORMALEN command ist das VIEL einfacher
* es wird einfach ein BYTE gesendet und es gibt KEINE bestaetigung.
*/
HRESULT CCOMHostDlg::sendAck()
{
	BYTE rgCommand[0x01] = { 0x06 };
	DWORD dwNumBytesWritten = -1;
	::WriteFile(m_ReadThreadArgs.hCOMPort, rgCommand, 1, &dwNumBytesWritten, NULL);
	return NOERROR;
}

/*
* die GUI MUSS hier folgende strategie fahren:
* mit dem Init muss die UI ein receiveCommand() absetzen. mit dem callback muss das receiveCommand() erneuert werden.
* das hat den unschoenen nachteil das mit dem Close/Destroy immer der m_ddReceiveCommand geprueft und freigegeben werden muss.
*/
void CCOMHostDlg::receiveCommand(
	LPDISPATCH pCallback)
{
	if (m_bstrReceiveCommand.Length())
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::IDispatch::receiveCommand() return buffered command (immediately)\n"));

		CComDispatchDriver ddCallback(pCallback);
		ddCallback.Invoke1((DISPID)0, &CComVariant(m_bstrReceiveCommand));
		m_bstrReceiveCommand.Empty();
	}
	else
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::IDispatch::receiveCommand() return command if available\n"));

		m_ddReceiveCommand = pCallback;
	}
}

/*static*/ DWORD CCOMHostDlg::SignalObjectAndWait(
	HANDLE hEvtTerminate,
	HANDLE hThread)
{
	if (INVALID_HANDLE_VALUE == hEvtTerminate)
		return E_INVALIDARG;
	if (INVALID_HANDLE_VALUE == hThread)
		return E_INVALIDARG;

	::SetEvent(hEvtTerminate);
	const DWORD dwThreadTerminated = ::WaitForSingleObject(hThread, 5000);
	if (WAIT_OBJECT_0 == dwThreadTerminated)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("m_hReadThread signaled, Thread terminated\n"));
	}

	else if (WAIT_TIMEOUT == dwThreadTerminated)
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("WARNING: m_hReadThread NOT terminated in time\n"));
	}

	else if (WAIT_FAILED == dwThreadTerminated)
		CCOMHostDlg::ShowLastError(_T("SignalObjectAndWait"));

	return dwThreadTerminated;
}

/*static*/ HRESULT CCOMHostDlg::ShowLastError(
	LPCTSTR szCaption,
	const DWORD* pdwLastError /* = NULL */)
{
	const DWORD dwMessageId = pdwLastError ? *pdwLastError : ::GetLastError();

	CString strCaption;
	strCaption.Format(_T("%s returned: 0x%.8x"), szCaption, dwMessageId);

	CString strDesc;
	DWORD dwRetC = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwMessageId,
		LANG_SYSTEM_DEFAULT, // MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
		strDesc.GetBufferSetLength(0xff), 0xff,
		NULL); // _In_opt_ va_list *Arguments
	strDesc.ReleaseBuffer();

	::MessageBox(NULL, (LPCTSTR)strDesc, (LPCTSTR)strCaption, MB_OK);
	return HRESULT_FROM_WIN32(dwMessageId);
}

/*
* hier liegen die funktionen, variable die AUSSCHLIESSLICH vom COMReadThread genutzt werden
* daher ist hier KEINE synchronisation noetig
*/
OVERLAPPED s_OverlappedRead;
enum _KRT2StateMachine const* s_pCurrentCmd = NULL;
unsigned int s_iIndexCmd = 1;
/*static*/ unsigned int CCOMHostDlg::COMReadThread(void* arguments)
{
	struct _ReadThreadArg* pArgs = (struct _ReadThreadArg*) arguments;
	_ASSERT(NULL != pArgs->hwndMainDlg);

	BYTE rgCommand[0x01]; // wir muessen BYTE weise lesen denn es gibt auch KRT2 commands die nur EIN BYTE lang sind z.B. 'S'
	::ZeroMemory(rgCommand, _countof(rgCommand));
	::ZeroMemory(&s_OverlappedRead, sizeof(OVERLAPPED));
	s_OverlappedRead.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset is required for overlapped I/O
	s_OverlappedRead.Internal;
	s_OverlappedRead.Offset;
	s_OverlappedRead.Pointer;

	HANDLE rgHandles[3] = { pArgs->hEvtTerminate, pArgs->hEvtCOMPort, s_OverlappedRead.hEvent };
	while(TRUE)
	{
		if (FALSE == ::ReadFile(pArgs->hCOMPort, rgCommand, 1, NULL, &s_OverlappedRead))
		{
			const DWORD dwLastError = ::GetLastError();
			if(dwLastError != ERROR_IO_PENDING)
				CCOMHostDlg::ShowLastError(_T("::ReadFile()"), &dwLastError);

			const DWORD dwEvent = ::WaitForMultipleObjects(_countof(rgHandles), rgHandles, FALSE, 10000);
			if (WAIT_TIMEOUT == dwEvent)
			{
				/*
				* wenn sich nichts auf der leitung tut fuehren wir einen RESET auf unseren CommandParser aus
				* wenn z.B. irgendwas auf der leitung verloren geht wuerde der parser UNENDLICH auf das naechste byte warten.
				* in der Praxis ist es aber so dass, das KRT2 ein commando "zusammenhaengend" abschickt
				* da die BaudRate fixed 9600bps ist und auch die maximale Command lenght bekannt ist
				* kann ich hier den parser problemlos zuruecksetzen.
				*/
				ATLTRACE2(atlTraceGeneral, 0, _T("run idle and continue loop. Parser.State: 0x%.2x\n"), s_state);
				s_pCurrentCmd = NULL; // wait for / reset to - IDLE
				s_iIndexCmd = 1;
				s_state = IDLE;
				continue;
			}

			else if (WAIT_FAILED == dwEvent)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("invalid arg, exit loop\n"));
				break;
			}

			else if (WAIT_OBJECT_0 == dwEvent)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("hEvtTerminate signaled, exit loop\n"));
				break;
			}

			else if (WAIT_OBJECT_0 + 1 == dwEvent)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("hEvtCOMPort signaled, get status and continue\n"));
				continue;
			}

			else if (WAIT_OBJECT_0 + 2 == dwEvent)
			{
				ATLTRACE2(atlTraceGeneral, 1, _T("s_OverlappedRead.hEvent signaled\n"));
				::ResetEvent(s_OverlappedRead.hEvent);

				// GetOverlappedResult function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
				DWORD dwNumberOfBytesRead = -1;
				// WaitCommEvent function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
				if (FALSE == ::GetOverlappedResult(pArgs->hCOMPort, &s_OverlappedRead, &dwNumberOfBytesRead, FALSE))
				{
					ATLTRACE2(atlTraceGeneral, 0, _T("  Port closed/EOF\n"));
					DWORD dwError = ::GetLastError();
					_ASSERT(ERROR_HANDLE_EOF == dwError);
					break;
				}

#ifdef KRT2INPUT
				/*
				* A common mistake in overlapped I/O is to reuse an OVERLAPPED structure before the previous overlapped operation is completed.
				* If a new overlapped operation is issued before a previous operation is completed, a new OVERLAPPED structure must be allocated for it.
				* A new manual-reset event for the hEvent member of the OVERLAPPED structure must also be created.
				* Once an overlapped operation is complete, the OVERLAPPED structure and its event are free for reuse.
				*   Serial Communications, https://msdn.microsoft.com/en-us/library/ff802693.aspx
				*
				* OVERLAPPED structure, https://msdn.microsoft.com/de-de/library/windows/desktop/ms684342(v=vs.85).aspx
				* This member is nonzero only when performing I/O requests on a seeking device that supports the concept of an offset (also referred to as a file pointer mechanism), such as a file. Otherwise, this member must be zero.
				*/
				s_OverlappedRead.Offset += dwNumberOfBytesRead;
#endif

#ifdef DISPATCH_LOWLEVEL_BYTERECEIVE
				::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(rgCommand[0], 0), NULL);
#endif
#ifdef DRIVE_COMMANDPARSER
				CCOMHostDlg::DriveStateMachine(pArgs->hwndMainDlg, rgCommand[0], TRUE);
#else
				/*
				* JEDES, einzelne, BYTE MUSS hier vorbei
				* dennoch sehe ich beim pArgs->hCOMPort == COM (Type) offensichtlich nur einzelne BYTES???
				*/
				ATLTRACE2(atlTraceGeneral, 0, _T("dwNumberOfBytesRead: 0x%.8x, rgCommand[0]: %hc (Async)\n"), dwNumberOfBytesRead, rgCommand[0]);
#endif

				ATLTRACE2(atlTraceGeneral, 1, _T("  continue\n"));
				continue;
			}

			else
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("WAIT_ABANDONED_0, ...\n"));
				break;
			}

		}
		else
		{
			/*
			* If an operation is completed immediately, an application needs to be ready to continue processing normally.
			*   Serial Communications, https://msdn.microsoft.com/en-us/library/ff802693.aspx
			*/
#ifdef DISPATCH_LOWLEVEL_BYTERECEIVE
			::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(rgCommand[0], 0), NULL);
#endif
#ifdef DRIVE_COMMANDPARSER
			CCOMHostDlg::DriveStateMachine(pArgs->hwndMainDlg, rgCommand[0], FALSE);
#else
			ATLTRACE2(atlTraceGeneral, 0, _T("dwNumberOfBytesRead: 0x00000001, rgCommand[0]: %hc (Sync)\n"), rgCommand[0]);
#endif
		}
	}

	::CloseHandle(s_OverlappedRead.hEvent);
	s_OverlappedRead.hEvent = INVALID_HANDLE_VALUE;
	ATLTRACE2(atlTraceGeneral, 0, _T("Leave COMReadThread()\n"));
	return 0;
}

#define B_CMD_FORMAT_JSON _T("{ \"nCommand\": \"B\" }")
#define C_CMD_FORMAT_JSON _T("{ \"nCommand\": \"C\" }")
#define D_CMD_FORMAT_JSON _T("{ \"nCommand\": \"D\" }")
#define J_CMD_FORMAT_JSON _T("{ \"nCommand\": \"J\" }")
#define V_CMD_FORMAT_JSON _T("{ \"nCommand\": \"V\" }")
#define O_CMD_FORMAT_JSON _T("{ \"nCommand\": \"O\" }")
#define o_CMD_FORMAT_JSON _T("{ \"nCommand\": \"o\" }")
#define U_CMD_FORMAT_JSON _T("{ \"nCommand\": \"U\", \"nFrequence\": %d, \"sStation\": \"%hs\" }")
#define R_CMD_WITHOUT_MEM
#ifdef R_CMD_WITHOUT_MEM
	#define R_CMD_FORMAT_JSON _T("{ \"nCommand\": \"R\", \"nFrequence\": %d, \"sStation\": \"%hs\" }")
#else
	#define R_CMD_FORMAT_JSON _T("{ \"nCommand\": \"R\", \"nFrequence\": %d, \"sStation\": \"%hs\", \"nIndex\", %02x }")
#endif
#define A_CMD_FORMAT_JSON _T("{ \"nCommand\": \"A\", \"nVolume\": %d, \"nSquelch\": %d, \"nVOX\": %d }")
/*
* hier stehen die werte der stellungsparameter fuer das s_pCurrentCmd
*
* ich verwende hier explizit KEINE BYTES, ich brauch zwar den vierfachen platz ABER:
* dafuer ist der zugriff schneller (alignment) UND fuer printf (%x) einfacher
*/
unsigned int s_rgValuesCmd[16];
/*
* 1.2 Out-going strings (from Radio after changing on panel):
* MultiByte Commands
* Hinweis:
*   nicht verwechseln mit den dazugehoehrigen "Incomming" messages.
*   z.B. die 1.3.5 Set a single record & name to database with address hat exact das dasselbe format wie die s_R122 message
*/
const enum _KRT2StateMachine s_U121[13] = { (enum _KRT2StateMachine)'U', WAIT_FOR_MHZ, WAIT_FOR_kHZ, WAIT_FOR_N0, WAIT_FOR_N1, WAIT_FOR_N2, WAIT_FOR_N3, WAIT_FOR_N4, WAIT_FOR_N5, WAIT_FOR_N6, WAIT_FOR_N7, WAIT_FOR_CHK, END };
#ifdef R_CMD_WITHOUT_MEM
	const enum _KRT2StateMachine s_R122[13] = { (enum _KRT2StateMachine)'R', WAIT_FOR_MHZ, WAIT_FOR_kHZ, WAIT_FOR_N0, WAIT_FOR_N1, WAIT_FOR_N2, WAIT_FOR_N3, WAIT_FOR_N4, WAIT_FOR_N5, WAIT_FOR_N6, WAIT_FOR_N7, WAIT_FOR_CHK, END };
#else
	const enum _KRT2StateMachine s_R122[14] = { (enum _KRT2StateMachine)'R', WAIT_FOR_MHZ, WAIT_FOR_kHZ, WAIT_FOR_N0, WAIT_FOR_N1, WAIT_FOR_N2, WAIT_FOR_N3, WAIT_FOR_N4, WAIT_FOR_N5, WAIT_FOR_N6, WAIT_FOR_N7, WAIT_FOR_MNR, WAIT_FOR_CHK, END };
#endif
const enum _KRT2StateMachine s_A123[6] =  { (enum _KRT2StateMachine)'A', WAIT_FOR_N0, WAIT_FOR_N1, WAIT_FOR_N2, WAIT_FOR_CHK, END };
/*
* return value:
*   NOERROR:       reset to IDLE. command parsing finished.
*   S_FALSE:       continue parsing command (consume bytes)
*/
/*static*/ HRESULT CCOMHostDlg::DriveStateMachine(
	HWND hwndMainDlg,
	BYTE byte,
	BOOL bAsynchronous)
{
	HRESULT hr = S_FALSE;
	if (bAsynchronous)
		ATLTRACE2(atlTraceGeneral, 1, _T("  process result from asynchronous read 0x%.8x\n"), byte);
	else
		ATLTRACE2(atlTraceGeneral, 1, _T("  process result from synchronous read 0x%.8x\n"), byte);

	if (E_PENDING == s_hrSend)
	{
		// DriveSend
		s_hrSend = NOERROR; // send complete, return to receive mode
		::PostMessage(hwndMainDlg, WM_USER_ACK, MAKEWPARAM(byte, 0), (LPARAM)NULL);
	}
	else
		hr = CCOMHostDlg::DriveRead(hwndMainDlg, byte);

	return hr;
}

/*static*/ HRESULT CCOMHostDlg::DriveRead(
	HWND hwndMainDlg,
	BYTE byte)
{
	HRESULT hr = S_FALSE;
	switch (s_state)
	{
	case IDLE:
		if ('S' == byte)
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("keep alive / ping - received\n"));
			::PostMessage(hwndMainDlg, WM_USER_ACK, MAKEWPARAM(byte, 0), (LPARAM)NULL);
			break;
		}

		else if (STX == byte)
		{
			s_state = WAIT_FOR_CMD;
			break;
		}
		// keep this state until next 'stx'
		break;
	case WAIT_FOR_CMD:
		/*
		* SingleByte commands (1.2.8 Status reports)
		*/
		if ('B' == byte) // low BATT
		{
			// ATLTRACE2(atlTraceGeneral, 0, _T("single byte command: low BATT\n"));
			CString strCommand;
			strCommand.Format(B_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		/*
		* Exchange of Frequencies (active against passive)
		* but NOT explicit documented as OUTGOING command
		*/
		else if ('C' == byte)
		{
			CString strCommand;
			strCommand.Format(C_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		else if ('D' == byte) // release low BATT
		{
			CString strCommand;
			strCommand.Format(D_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		else if ('J' == byte) // RX
		{
			CString strCommand;
			strCommand.Format(J_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		else if ('V' == byte) // release RX
		{
			CString strCommand;
			strCommand.Format(V_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		else if ('O' == byte) // 1.2.6 DUAL-mode on
		{
			// ATLTRACE2(atlTraceGeneral, 0, _T("single byte command: DUAL-mode on\n"));
			CString strCommand;
			strCommand.Format(O_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		else if ('o' == byte) // 1.2.7 DUAL-mode off
		{
			// ATLTRACE2(atlTraceGeneral, 0, _T("single byte command: DUAL-mode off\n"));
			CString strCommand;
			strCommand.Format(o_CMD_FORMAT_JSON);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM(byte, 0), (LPARAM)lParam);
			hr = NOERROR;
			_ASSERT(s_state != IDLE); // ensure transition
			s_state = IDLE;
		}

		// begin parsing of MultiByte commands
		else if ('A' == byte)
		{
			s_pCurrentCmd = s_A123;
			s_iIndexCmd = 1; // skip command id
			s_state = s_pCurrentCmd[s_iIndexCmd];
		}

		else if ('U' == byte)
		{
			s_pCurrentCmd = s_U121;
			s_iIndexCmd = 1; // skip command id
			s_state = s_pCurrentCmd[s_iIndexCmd];
		}

		else if ('R' == byte)
		{
			s_pCurrentCmd = s_R122;
			s_iIndexCmd = 1; // skip command id
			s_state = s_pCurrentCmd[s_iIndexCmd];
		}

		else
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("unknown command, reset to IDLE / abort\n"));
			hr = NOERROR;
			s_state = IDLE; // unknown command, reset to IDLE / abort
		}
		break;
	case WAIT_FOR_MHZ:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_kHZ:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N0:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N1:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N2:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N3:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N4:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N5:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N6:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_N7:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_MNR:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	case WAIT_FOR_CHK:
		s_state = CCOMHostDlg::DriveCommand(hwndMainDlg, byte);
		break;
	}
	return hr;
}

/*static*/ enum _KRT2StateMachine CCOMHostDlg::DriveCommand(
	HWND hwndMainDlg,
	BYTE byte)
{
	s_rgValuesCmd[s_iIndexCmd] = byte;
	s_iIndexCmd += 1; // read next
	if (END == s_pCurrentCmd[s_iIndexCmd])
	{
		switch (s_pCurrentCmd[0])
		{
			case 'A':
			{
				/*
				* die iCheckSum wird aus der SUMME (+) von squelch und VOX gebildet
				* Hinweis:
				*   ich hatte die doku so interpretiert das hier squelch UND VOX xor verknuepft werden
				*/
				const int iCheckSum = s_rgValuesCmd[2 /* squelch */] + s_rgValuesCmd[3 /* VOX */]; // these indexes are command specific
				_ASSERT(iCheckSum == s_rgValuesCmd[4 /* WAIT_FOR_CHK */]);
				CString strCommand;
				strCommand.Format(A_CMD_FORMAT_JSON, s_rgValuesCmd[1 /* Volume */], s_rgValuesCmd[2 /* squelch */], s_rgValuesCmd[3 /* VOX */]);
				const size_t _size = strCommand.GetLength() + 1;
				PTCHAR lParam = new TCHAR[_size];
				_tcscpy_s(lParam, _size, strCommand.GetBuffer());
				::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM('A', 0), (LPARAM)lParam);
			}
			break;
			case 'R':
			{
#ifdef R_CMD_WITHOUT_MEM
				const int iCheckSum = s_rgValuesCmd[1 /* WAIT_FOR_MHZ */] ^ s_rgValuesCmd[2 /* WAIT_FOR_kHZ */]; // these indexes are command specific
				_ASSERT(iCheckSum == s_rgValuesCmd[11 /* WAIT_FOR_CHK */]);
				const int iFrequence = s_rgValuesCmd[1 /* WAIT_FOR_MHZ */] * 1000 + s_rgValuesCmd[2 /* WAIT_FOR_kHZ */] * 5;
				const char szStation[9] = { s_rgValuesCmd[3], s_rgValuesCmd[4], s_rgValuesCmd[5], s_rgValuesCmd[6], s_rgValuesCmd[7], s_rgValuesCmd[8], s_rgValuesCmd[9], s_rgValuesCmd[10], '\0' }; // WAIT_FOR_N0 - WAIT_FOR_N7
				// ATLTRACE2(atlTraceGeneral, 0, _T("command R, %hs complete\n"), szStation);
				CString strCommand;
				strCommand.Format(R_CMD_FORMAT_JSON, iFrequence, szStation);
				const size_t _size = strCommand.GetLength() + 1;
				PTCHAR lParam = new TCHAR[_size];
				_tcscpy_s(lParam, _size, strCommand.GetBuffer());
				::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM('R', 0), (LPARAM)lParam);

#else
				const int iCheckSum = s_rgValuesCmd[1 /* WAIT_FOR_MHZ */] ^ s_rgValuesCmd[2 /* WAIT_FOR_kHZ */]; // these indexes are command specific
				_ASSERT(iCheckSum == s_rgValuesCmd[12 /* WAIT_FOR_CHK */]);
				const int iFrequence = s_rgValuesCmd[1 /* WAIT_FOR_MHZ */] * 1000 + s_rgValuesCmd[2 /* WAIT_FOR_kHZ */] * 5;
				const char szStation[9] = { s_rgValuesCmd[3], s_rgValuesCmd[4], s_rgValuesCmd[5], s_rgValuesCmd[6], s_rgValuesCmd[7], s_rgValuesCmd[8], s_rgValuesCmd[9], s_rgValuesCmd[10], '\0' }; // WAIT_FOR_N0 - WAIT_FOR_N7
				// ATLTRACE2(atlTraceGeneral, 0, _T("command R, %hs complete\n"), szStation);
				CString strCommand;
				strCommand.Format(R_CMD_FORMAT_JSON, iFrequence, szStation, s_rgValuesCmd[11 /* WAIT_FOR_MNR */]);
				const size_t _size = strCommand.GetLength() + 1;
				PTCHAR lParam = new TCHAR[_size];
				_tcscpy_s(lParam, _size, strCommand.GetBuffer());
				::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM('R', 0), (LPARAM) lParam);
#endif
			}
			break;
			default:
				ATLTRACE2(atlTraceGeneral, 0, _T("  command %c and arguments complete\n"), s_pCurrentCmd[0]);
				break;
		}
		s_pCurrentCmd = NULL; // wait for / reset to - next command
		s_iIndexCmd = 1;
		return IDLE;
	}
	else
		return s_pCurrentCmd[s_iIndexCmd];
}
