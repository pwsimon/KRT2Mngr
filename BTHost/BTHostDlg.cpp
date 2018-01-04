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
	m_iRetCWSAStartup = -1;
}

void CBTHostDlg::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBTHostDlg, CDHtmlDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
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
	WSADATA data;
	m_iRetCWSAStartup = ::WSAStartup(MAKEWORD(2, 2), &data);
	if (0 != m_iRetCWSAStartup)
		CBTHostDlg::ShowWSALastError(_T("WSAStartup"));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CBTHostDlg::OnClose()
{
	if (0 == m_iRetCWSAStartup)
		::WSACleanup();

	CDHtmlDialog::OnClose();
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
	HRESULT hr = NOERROR;
	HANDLE hRadio = NULL;
	hr = enumBTRadio(hRadio);
	// hr = enumBTDevices(SerialPortServiceClass_UUID);

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
	SOCKET LocalSocket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (INVALID_SOCKET != LocalSocket)
	{
		if (SOCKET_ERROR == ::connect(LocalSocket,
			(struct sockaddr *) pRemoteAddr,
			sizeof(SOCKADDR_BTH))) {
			CBTHostDlg::ShowWSALastError(_T("connect failed"));
		}

		if (SOCKET_ERROR == ::closesocket(LocalSocket))
		{
			CBTHostDlg::ShowWSALastError(_T("closesocket failed"));
		}

		LocalSocket = INVALID_SOCKET;
	}

	return E_FAIL;
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
				CSADDR_INFO * addrService = (CSADDR_INFO *)pResults2->lpcsaBuffer;
				CComBSTR bstrAddress;
				CBTHostDlg::BTAddressToString((PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr, &bstrAddress);
				ATLTRACE2(atlTraceGeneral, 0, _T("found: %ls, %ls (service)\n"), pResults2->lpszServiceInstanceName, (LPCWSTR) bstrAddress);
				// Connect((PSOCKADDR_BTH)addrService->RemoteAddr.lpSockaddr);
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
#define CXN_SUCCESS                       0
#define CXN_ERROR                         1
#define CXN_DEFAULT_LISTEN_BACKLOG        4
/*static*/ ULONG CBTHostDlg::NameToBthAddr(
	_In_ const LPWSTR pszRemoteName,
	_Out_ PSOCKADDR_BTH pRemoteBtAddr)
{
	INT             iResult = CXN_SUCCESS;
	BOOL            bContinueLookup = FALSE, bRemoteDeviceFound = FALSE;
	ULONG           ulFlags = 0, ulPQSSize = sizeof(WSAQUERYSET);
	HANDLE          hLookup = NULL;
	PWSAQUERYSET    pWSAQuerySet = NULL;

	ZeroMemory(pRemoteBtAddr, sizeof(*pRemoteBtAddr));

	pWSAQuerySet = (PWSAQUERYSET)HeapAlloc(GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		ulPQSSize);
	if (NULL == pWSAQuerySet) {
		iResult = STATUS_NO_MEMORY;
		wprintf(L"!ERROR! | Unable to allocate memory for WSAQUERYSET\n");
	}

	// 
	// Search for the device with the correct name 
	// 
	if (CXN_SUCCESS == iResult) {

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

			if (0 == iRetryCount) {
				wprintf(L"*INFO* | Inquiring device from cache...\n");
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
				wprintf(L"*INFO* | Unable to find device.  Waiting for %d seconds before re-inquiry...\n", CXN_DELAY_NEXT_INQUIRY);
				Sleep(CXN_DELAY_NEXT_INQUIRY * 1000);

				wprintf(L"*INFO* | Inquiring device ...\n");
			}

			// 
			// Start the lookup service 
			// 
			iResult = CXN_SUCCESS;
			hLookup = 0;
			bContinueLookup = FALSE;
			ZeroMemory(pWSAQuerySet, ulPQSSize);
			pWSAQuerySet->dwNameSpace = NS_BTH;
			pWSAQuerySet->dwSize = sizeof(WSAQUERYSET);
			iResult = WSALookupServiceBegin(pWSAQuerySet, ulFlags, &hLookup);

			// 
			// Even if we have an error, we want to continue until we 
			// reach the CXN_MAX_INQUIRY_RETRY 
			// 
			if ((NO_ERROR == iResult) && (NULL != hLookup)) {
				bContinueLookup = TRUE;
			}
			else if (0 < iRetryCount) {
				wprintf(L"=CRITICAL= | WSALookupServiceBegin() failed with error code %d, WSAGetLastError = %d\n", iResult, WSAGetLastError());
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
				if (NO_ERROR == WSALookupServiceNext(hLookup,
					ulFlags,
					&ulPQSSize,
					pWSAQuerySet)) {

					// 
					// Compare the name to see if this is the device we are looking for. 
					// 
					if ((pWSAQuerySet->lpszServiceInstanceName != NULL) &&
						(CXN_SUCCESS == _wcsicmp(pWSAQuerySet->lpszServiceInstanceName, pszRemoteName))) {
						// 
						// Found a remote bluetooth device with matching name. 
						// Get the address of the device and exit the lookup. 
						// 
						CopyMemory(pRemoteBtAddr,
							(PSOCKADDR_BTH)pWSAQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr,
							sizeof(*pRemoteBtAddr));
						bRemoteDeviceFound = TRUE;
						bContinueLookup = FALSE;
					}
				}
				else {
					iResult = WSAGetLastError();
					if (WSA_E_NO_MORE == iResult) { //No more data 
													// 
													// No more devices found.  Exit the lookup. 
													// 
						bContinueLookup = FALSE;
					}
					else if (WSAEFAULT == iResult) {
						// 
						// The buffer for QUERYSET was insufficient. 
						// In such case 3rd parameter "ulPQSSize" of function "WSALookupServiceNext()" receives 
						// the required size.  So we can use this parameter to reallocate memory for QUERYSET. 
						// 
						HeapFree(GetProcessHeap(), 0, pWSAQuerySet);
						pWSAQuerySet = (PWSAQUERYSET)HeapAlloc(GetProcessHeap(),
							HEAP_ZERO_MEMORY,
							ulPQSSize);
						if (NULL == pWSAQuerySet) {
							wprintf(L"!ERROR! | Unable to allocate memory for WSAQERYSET\n");
							iResult = STATUS_NO_MEMORY;
							bContinueLookup = FALSE;
						}
					}
					else {
						wprintf(L"=CRITICAL= | WSALookupServiceNext() failed with error code %d\n", iResult);
						bContinueLookup = FALSE;
					}
				}
			}

			// 
			// End the lookup service 
			// 
			WSALookupServiceEnd(hLookup);

			if (STATUS_NO_MEMORY == iResult) {
				break;
			}
		}
	}

	if (NULL != pWSAQuerySet) {
		HeapFree(GetProcessHeap(), 0, pWSAQuerySet);
		pWSAQuerySet = NULL;
	}

	if (bRemoteDeviceFound) {
		iResult = CXN_SUCCESS;
	}
	else {
		iResult = CXN_ERROR;
	}

	return iResult;
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
