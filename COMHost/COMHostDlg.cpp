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

// CCOMHostDlg dialog
BEGIN_DHTML_EVENT_MAP(CCOMHostDlg)
	DHTML_EVENT_ONCLICK(_T("btnSend"), OnSend)
	DHTML_EVENT_ONCLICK(_T("ButtonCancel"), OnButtonCancel)
END_DHTML_EVENT_MAP()

CCOMHostDlg::CCOMHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_COMHOST_DIALOG, IDR_HTML_COMHOST_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCOMHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CCOMHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
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
	* debuggen der MFC sourcen durch linken der MFC als static lib
	*/
	m_nHtmlResID = 0;
	m_strCurrentUrl = _T("http://localhost/krt2mngr/comhost/comhost.htm"); // ohne fehler
	// m_strCurrentUrl = _T("http://localhost/krt2mngr/sevenseg.html"); // need browser_emulation
	CDHtmlDialog::OnInitDialog();
	// Navigate(_T("http://localhost/krt2mngr/sevenseg.html"));
	SetExternalDispatch(NULL); // this ODER irgend ein DHTML_EVENT_XXX Macro

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

	return TRUE;  // return TRUE  unless you set the focus to a control
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

HRESULT CCOMHostDlg::OnSend(IHTMLElement* /*pElement*/)
{
	// Serial Port Sample, https://code.msdn.microsoft.com/windowsdesktop/Serial-Port-Sample-e8accf30/sourcecode?fileId=67164&pathId=1394200469
	HANDLE hPort1 = CreateFile(TEXT("COM1"), // Name of the port
		GENERIC_READ | GENERIC_WRITE, // Access (read-write) mode
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	_ASSERT(INVALID_HANDLE_VALUE != hPort1);
	return S_OK;
}

HRESULT CCOMHostDlg::OnButtonCancel(IHTMLElement* /*pElement*/)
{
	OnCancel();
	return S_OK;
}
