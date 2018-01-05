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
#define WM_USER_RXSINGLEBYTE    (WM_USER + 1) // posted from COMReadThread
#define WM_USER_RXDECODEDCMD    (WM_USER + 2)

OVERLAPPED s_Overlapped;

BEGIN_DHTML_EVENT_MAP(CCOMHostDlg)
	DHTML_EVENT_ONCLICK(_T("btnSend"), OnSend)
	DHTML_EVENT_ONCLICK(_T("btnRead"), OnRead)
END_DHTML_EVENT_MAP()

BEGIN_MESSAGE_MAP(CCOMHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
	ON_MESSAGE(WM_USER_RXSINGLEBYTE, OnRXSingleByte)
	ON_MESSAGE(WM_USER_RXDECODEDCMD, OnRXDecodedCmd)
END_MESSAGE_MAP()

CCOMHostDlg::CCOMHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_COMHOST_DIALOG, IDR_HTML_COMHOST_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hCOMx = INVALID_HANDLE_VALUE;
	m_hReadThread = INVALID_HANDLE_VALUE;
}

void CCOMHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

// CCOMHostDlg message handlers
BOOL CCOMHostDlg::OnInitDialog()
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

	// TODO: Add extra initialization here

	return TRUE; // return TRUE  unless you set the focus to a control
}

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
	ATLTRACE2(atlTraceGeneral, 0, _T("CCOMHostDlg::OnRXDecodedCmd(0x%.2x): %s\n"), LOWORD(wParam), (LPCTSTR)lParam);
	CComBSTR bstrCommand = (LPCTSTR) lParam;
	delete[] (PTCHAR) lParam;
	SetElementText(_T("command"), bstrCommand);
	return 0;
}

void CCOMHostDlg::OnClose()
{
	if (INVALID_HANDLE_VALUE != m_hCOMx)
	{
		::CloseHandle(m_hCOMx);
		m_hCOMx = INVALID_HANDLE_VALUE;
	}

	// ::SignalObjectAndWait(m_ReadThreadArgs.hEvtTerminate, m_hReadThread, 5000, FALSE);
	CCOMHostDlg::SignalObjectAndWait(m_ReadThreadArgs.hEvtTerminate, m_hReadThread);
	::CloseHandle(m_hReadThread);
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

/*
* dieses testfile enthaelt die folgenden UseCases:
* 1.) 'R', 1.2.2 Set frequency & name on passive side, 126.900, PETER
* 2.) 'O', 1.2.6 DUAL-mode on
* 3.) 'o', 1.2.7 DUAL-mode off
* 4.) 'B', low BATT
* 5.) 'D', release low BATT
* 6.) 'J', RX
* 7.) 'V', release RX
*/
// #define KRT2INPUT  _T("krt2input.bin")
#define KRT2INPUT  _T("COM5")
#define KRT2OUTPUT _T("COM5")
HRESULT CCOMHostDlg::OnSend(IHTMLElement* /*pElement*/)
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
	if (INVALID_HANDLE_VALUE == m_hCOMx)
	{
		m_hCOMx = ::CreateFile(KRT2OUTPUT, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE == m_hCOMx)
		{
			CCOMHostDlg::ShowLastError(_T("::CreateFile() Failed"));
			return E_FAIL;
		}
	}

	DCB dcb;
	::ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	BOOL bRetC = ::GetCommState(m_hCOMx, &dcb);

	/* COMMPROP CommProp;
	bRetC = ::GetCommProperties(m_hCOMx, &CommProp);

	COMMTIMEOUTS CommTimeouts;
	bRetC = ::GetCommTimeouts(m_hCOMx, &CommTimeouts); */

	/*
	* bstrCommand enthaelt die hexcodierten bytes space delemitted
	* die codierung wird aktuell mit einem PreFix z.B. '0x' uebertragen
	*
	* Hinweis(e):
	* - als generischen ansatz koennte man hier ein ByteArray uebergeben
	*   da Arrays im JavaScript Objecte sind koennen wir NICHT EINAFCH via C/C++ darauf zugreifen
	*/
	CComBSTR bstrCommand = GetElementText(_T("command"));
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

	DWORD dwNumBytesWritten;
	if (::WriteFile(m_hCOMx, rgCommand, uiIndex, &dwNumBytesWritten, NULL))
		_ASSERT(uiIndex == dwNumBytesWritten);
	else
		CCOMHostDlg::ShowLastError(_T("::WriteFile() Failed"));

	DWORD dwErrors = 0;
	COMSTAT COMStat;
	if (!::ClearCommError(m_hCOMx, &dwErrors, &COMStat))
		CCOMHostDlg::ShowLastError(_T("::ClearCommError() Failed"));

	return S_OK;
}

/*static*/ HRESULT CCOMHostDlg::ShowLastError(LPCTSTR szCaption)
{
	const DWORD dwMessageId(::GetLastError());
	CString strDesc;
	DWORD dwRetC = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwMessageId,
		LANG_SYSTEM_DEFAULT, // MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
		strDesc.GetBufferSetLength(0xff), 0xff,
		NULL); // _In_opt_ va_list *Arguments
	strDesc.ReleaseBuffer();
	CString strMsg;
	strMsg.Format(_T("%s, returned: 0x%.8x"), dwMessageId);
	::MessageBox(NULL, strMsg, szCaption, MB_OK);
	return NOERROR;
}

HRESULT CCOMHostDlg::OnRead(IHTMLElement* /*pElement*/)
{
	const DWORD dwFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED; // fuer "COM<Port>" ist das FILE_ATTRIBUTE_NORMAL evtl. ueberfluessig
	m_ReadThreadArgs.hCOMPort = ::CreateFile(KRT2INPUT, GENERIC_READ, 0, NULL, OPEN_EXISTING, dwFlags, NULL);
	_ASSERT(INVALID_HANDLE_VALUE != m_ReadThreadArgs.hCOMPort);

	/*
	*/
	m_ReadThreadArgs.hEvtCOMPort = ::CreateEvent(NULL, TRUE, FALSE, NULL); // currently never signaled;
	m_ReadThreadArgs.hEvtTerminate = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	m_ReadThreadArgs.hwndMainDlg = m_hWnd;
	m_hReadThread = (HANDLE)_beginthreadex(NULL, 0, CCOMHostDlg::COMReadThread, &m_ReadThreadArgs, 0, NULL);

	return S_OK;
}

/*static*/ unsigned int CCOMHostDlg::COMReadThread(void* arguments)
{
	struct _ReadThreadArg* pArgs = (struct _ReadThreadArg*) arguments;

	BYTE rgCommand[0x01]; // wir muessen BYTE weise lesen denn es gibt auch KRT2 commands die nur EIN BYTE lang sind z.B. 'S'
	::ZeroMemory(rgCommand, _countof(rgCommand));
	::ZeroMemory(&s_Overlapped, sizeof(OVERLAPPED));
	s_Overlapped.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset is required for overlapped I/O
	s_Overlapped.Internal;
	s_Overlapped.Offset;
	s_Overlapped.Pointer;

	HANDLE rgHandles[3] = { pArgs->hEvtTerminate, pArgs->hEvtCOMPort, s_Overlapped.hEvent };
	while(TRUE)
	{
		if (FALSE == ::ReadFile(pArgs->hCOMPort, rgCommand, 1, NULL, &s_Overlapped))
		{
			_ASSERT(ERROR_IO_PENDING == ::GetLastError());
			const DWORD dwEvent = ::WaitForMultipleObjects(_countof(rgHandles), rgHandles, FALSE, 10000);
			if (WAIT_TIMEOUT == dwEvent)
			{
				ATLTRACE2(atlTraceGeneral, 0, _T("run idle and continue loop\n"));
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
				ATLTRACE2(atlTraceGeneral, 1, _T("s_Overlapped.hEvent signaled\n"));

				// GetOverlappedResult function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
				DWORD dwNumberOfBytesRead = -1;
				// WaitCommEvent function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
				if (FALSE == ::GetOverlappedResult(pArgs->hCOMPort, &s_Overlapped, &dwNumberOfBytesRead, FALSE))
				{
					ATLTRACE2(atlTraceGeneral, 0, _T("  Port closed/EOF\n"));
					DWORD dwError = ::GetLastError();
					_ASSERT(ERROR_HANDLE_EOF == dwError);
					break;
				}

				ATLTRACE2(atlTraceGeneral, 1, _T("  0x%.8x bytes received\n"), dwNumberOfBytesRead);
				/*
				* A common mistake in overlapped I/O is to reuse an OVERLAPPED structure before the previous overlapped operation is completed.
				* If a new overlapped operation is issued before a previous operation is completed, a new OVERLAPPED structure must be allocated for it.
				* A new manual-reset event for the hEvent member of the OVERLAPPED structure must also be created.
				* Once an overlapped operation is complete, the OVERLAPPED structure and its event are free for reuse.
				*   Serial Communications, https://msdn.microsoft.com/en-us/library/ff802693.aspx
				*/
				s_Overlapped.Offset += dwNumberOfBytesRead;

				ATLTRACE2(atlTraceGeneral, 1, _T("  process 0x%.8x\n"), rgCommand[0]);
#ifdef DISPATCH_LOWLEVEL_BYTERECEIVE
				::PostMessage(pArgs->hwndMainDlg, WM_USER_RXSINGLEBYTE, MAKEWPARAM(rgCommand[0], 0), NULL);
#endif
#ifdef DRIVE_COMMANDPARSER
				CCOMHostDlg::DriveStateMachine(pArgs->hwndMainDlg, rgCommand[0], TRUE);
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
#endif
		}
	}

	ATLTRACE2(atlTraceGeneral, 0, _T("Leave COMReadThread()\n"));
	return 0;
}

#define B_CMD_FORMAT_JSON _T("{ \"nCommand\": \"B\" }")
#define D_CMD_FORMAT_JSON _T("{ \"nCommand\": \"D\" }")
#define J_CMD_FORMAT_JSON _T("{ \"nCommand\": \"J\" }")
#define V_CMD_FORMAT_JSON _T("{ \"nCommand\": \"V\" }")
#define O_CMD_FORMAT_JSON _T("{ \"nCommand\": \"O\" }")
#define o_CMD_FORMAT_JSON _T("{ \"nCommand\": \"o\" }")
#define U_CMD_FORMAT_JSON _T("{ \"nCommand\": \"U\", \"nFrequence\": %d, \"sStation\": \"%hs\" }")
#define R_CMD_FORMAT_JSON _T("{ \"nCommand\": \"R\", \"nFrequence\": %d, \"sStation\": \"%hs\", \"nIndex\", %d }")
#define A_CMD_FORMAT_JSON _T("{ \"nCommand\": \"A\", \"nVolume\": %d, \"nSquelch\": %d, \"nVOX\": %d }")

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
enum _KRT2StateMachine const* s_pCurrentCmd = NULL;
unsigned int s_iIndexCmd = 1;
BYTE s_rgValuesCmd[0x10]; // hier stehen die werte der stellungsparameter fuer das s_pCurrentCmd
/*
* 1.2 Out-going strings (from Radio after changing on panel):
* MultiByte Commands
* Hinweis:
*   nicht verwechseln mit den dazugehoehrigen "Incomming" messages.
*   z.B. die 1.3.5 Set a single record & name to database with address hat exact das dasselbe format wie die s_R122 message
*/
const enum _KRT2StateMachine s_U121[13] = { (enum _KRT2StateMachine)'U', WAIT_FOR_MHZ, WAIT_FOR_kHZ, WAIT_FOR_N0, WAIT_FOR_N1, WAIT_FOR_N2, WAIT_FOR_N3, WAIT_FOR_N4, WAIT_FOR_N5, WAIT_FOR_N6, WAIT_FOR_N7, WAIT_FOR_CHK, END };
const enum _KRT2StateMachine s_R122[14] = { (enum _KRT2StateMachine)'R', WAIT_FOR_MHZ, WAIT_FOR_kHZ, WAIT_FOR_N0, WAIT_FOR_N1, WAIT_FOR_N2, WAIT_FOR_N3, WAIT_FOR_N4, WAIT_FOR_N5, WAIT_FOR_N6, WAIT_FOR_N7, WAIT_FOR_MNR, WAIT_FOR_CHK, END };
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

	switch (s_state)
	{
	case IDLE:
		if ('S' == byte)
		{
			ATLTRACE2(atlTraceGeneral, 0, _T("keep alive / ping - received\n"));
			break;
		}

		else if (0x02 == byte)
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
		case 'R':
		{
			const int iCheckSum = s_rgValuesCmd[1 /* WAIT_FOR_MHZ */] ^ s_rgValuesCmd[2 /* WAIT_FOR_kHZ */]; // these indexes are command specific
			_ASSERT(iCheckSum == s_rgValuesCmd[12 /* WAIT_FOR_CHK */]);
			const int iFrequence = s_rgValuesCmd[1 /* WAIT_FOR_MHZ */] * 1000 + s_rgValuesCmd[2 /* WAIT_FOR_kHZ */] * 5;
			const char szStation[9] = { s_rgValuesCmd[3], s_rgValuesCmd[4], s_rgValuesCmd[5], s_rgValuesCmd[6], s_rgValuesCmd[7], s_rgValuesCmd[8], s_rgValuesCmd[9], '\0' }; // WAIT_FOR_N0 - WAIT_FOR_N7
			// ATLTRACE2(atlTraceGeneral, 0, _T("command R, %hs complete\n"), szStation);
			CString strCommand;
			strCommand.Format(R_CMD_FORMAT_JSON, iFrequence, szStation, s_rgValuesCmd[11 /* WAIT_FOR_MNR */]);
			const size_t _size = strCommand.GetLength() + 1;
			PTCHAR lParam = new TCHAR[_size];
			_tcscpy_s(lParam, _size, strCommand.GetBuffer());
			::PostMessage(hwndMainDlg, WM_USER_RXDECODEDCMD, MAKEWPARAM('R', 0), (LPARAM) lParam);
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
