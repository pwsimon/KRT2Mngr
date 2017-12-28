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
BEGIN_DHTML_EVENT_MAP(CCOMHostDlg)
	DHTML_EVENT_ONCLICK(_T("btnSend"), OnSend)
	DHTML_EVENT_ONCLICK(_T("btnRead"), OnRead)
END_DHTML_EVENT_MAP()

CCOMHostDlg::CCOMHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_COMHOST_DIALOG, IDR_HTML_COMHOST_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hCOMx = INVALID_HANDLE_VALUE;
}

void CCOMHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CCOMHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

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

void CCOMHostDlg::OnClose()
{
	if (INVALID_HANDLE_VALUE != m_hCOMx)
	{
		::CloseHandle(m_hCOMx);
		m_hCOMx = INVALID_HANDLE_VALUE;
	}

	CDHtmlDialog::OnClose();
}

#define KRT2INPUT _T("krt2input.bin")
// #define KRT2INPUT _T("COM5")
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
		m_hCOMx = ::CreateFile(TEXT("COM5"), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE == m_hCOMx)
		{
			ShowLastError(_T("::CreateFile() Failed"));
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
		ShowLastError(_T("::WriteFile() Failed"));

	DWORD dwErrors = 0;
	COMSTAT COMStat;
	if (!::ClearCommError(m_hCOMx, &dwErrors, &COMStat))
		ShowLastError(_T("::ClearCommError() Failed"));

	return S_OK;
}

HRESULT CCOMHostDlg::ShowLastError(LPCTSTR szCaption)
{
	CString strErrorMsg;
	strErrorMsg.Format(_T("returned: 0x%.8x"), ::GetLastError());
	::MessageBox(NULL, strErrorMsg, szCaption, MB_OK);
	return NOERROR;
}

HRESULT CCOMHostDlg::OnRead(IHTMLElement* /*pElement*/)
{
	HANDLE hPort1 = ::CreateFile(KRT2INPUT, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	_ASSERT(INVALID_HANDLE_VALUE != hPort1);
	BYTE rgCommand[0xff];
	::ZeroMemory(rgCommand, 0xff);
	DWORD dwNumberOfBytesRead = 0; // not used for OVERLAPPED IO
	::ZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));
	m_Overlapped.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL); // non signaled
	m_Overlapped.Internal;
	m_Overlapped.Offset;
	m_Overlapped.Pointer; // = rgCommand;
	if (FALSE == ::ReadFile(hPort1, rgCommand, 13, NULL, &m_Overlapped))
	{
		DWORD dwLastError = ::GetLastError(); // ERROR_IO_PENDING
		// GetOverlappedResult function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx
		BOOL bRetC = ::GetOverlappedResult(hPort1, &m_Overlapped, &dwNumberOfBytesRead, TRUE);
	}
	// WaitCommEvent function, https://msdn.microsoft.com/en-us/library/windows/desktop/ms683209(v=vs.85).aspx

	// m_hThread = (HANDLE)_beginthreadex(NULL, 0, CCOMHostDlg::COMReadThread, this, 0, NULL);
	::CloseHandle(hPort1);
	return S_OK;
}

/*static*/ unsigned int CCOMHostDlg::COMReadThread(void* arguments)
{
	return 0;
}
