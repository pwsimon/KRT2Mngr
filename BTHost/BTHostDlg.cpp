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
	DHTML_EVENT_ONCLICK(_T("btnCheck"), OnCheck)
	DHTML_EVENT_ONCLICK(_T("ButtonCancel"), OnButtonCancel)
END_DHTML_EVENT_MAP()

CBTHostDlg::CBTHostDlg(CWnd* pParent /*=NULL*/)
	: CDHtmlDialog(IDD_BTHOST_DIALOG, IDR_HTML_BTHOST_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CBTHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBTHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// CBTHostDlg message handlers
BOOL CBTHostDlg::OnInitDialog()
{
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

	return TRUE;  // return TRUE  unless you set the focus to a control
}

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
	HANDLE hRadio = NULL;
	HRESULT hr = enumBTRadio(hRadio);
	return S_OK;
}

HRESULT CBTHostDlg::OnButtonCancel(IHTMLElement* /*pElement*/)
{
	OnCancel();
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

		::MessageBox(NULL, RadioInfo.szName, _T("BluetoothFindXXXRadio"), MB_OK);
		hr = enumBTDevices(_hRadio);
		bRetC = ::BluetoothFindNextRadio(hbtrf, &_hRadio);
		::CloseHandle(_hRadio);
	}

	if (hbtrf)
		::BluetoothFindRadioClose(hbtrf);

	return hr;
}

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
			CComBSTR bstrAddress;
			CBTHostDlg::BTAddressToString(&deviceInfo.Address, &bstrAddress);
			::MessageBox(NULL, deviceInfo.szName, _T("BluetoothFindXXXDevice"), MB_OK);
			CBTHostDlg::enumBTServices(bstrAddress, SerialPortServiceClass_UUID);
		} while (::BluetoothFindNextDevice(hFindDevice, &deviceInfo));
		::BluetoothFindDeviceClose(hFindDevice);
	}
	return NOERROR;
}

HRESULT CBTHostDlg::enumBTServices(
	LPCWSTR szDeviceAddress,
	GUID serviceClass)
{
	//Initialise quering the device services
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
	//do not set LUP_CONTAINERS, we are queriing for services
	//LUP_FLUSHCACHE would be used if we want to do an inquiry
	int result2 = ::WSALookupServiceBegin(&queryset2, 0, &hLookup);

	//Start querying for device services
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
			ATLTRACE2(atlTraceGeneral, 0, _T("%ls"), pResults2->lpszServiceInstanceName);
			if (pResults2->dwNumberOfCsAddrs && pResults2->lpcsaBuffer)
			{
				CSADDR_INFO * addrService = (CSADDR_INFO *)pResults2->lpcsaBuffer;
				CComBSTR bstrAddress;
				CBTHostDlg::BTAddressToString((PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr, &bstrAddress);
				ATLTRACE2(atlTraceGeneral, 0, _T("%ls"), (LPCWSTR) bstrAddress);
			}
		}
	}

	if (hLookup)
		::WSALookupServiceEnd(hLookup);
	hLookup = NULL;
	return 0;
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

	*pbstrAddress = ::SysAllocString(lpszAddressString);
	return NOERROR;
}
