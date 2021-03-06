// BTHostDlg.h : header file
//

#pragma once

// CBTHostDlg dialog
class CBTHostDlg : public CDHtmlDialog
{
// Construction
public:
	CBTHostDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_BTHOST_DIALOG, IDH = IDR_HTML_BTHOST_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

// Implementation
protected:
#ifdef _DEBUG
	DWORD m_dwThreadAffinity;
#endif
	HICON m_hIcon;
	int m_iRetCWSAStartup;
	SOCKADDR_BTH m_addrKRT2;
	SOCKADDR_IN m_addrSimulator; // IPv4
	size_t           m_AddrSimulatorLength;
	struct sockaddr* m_pAddrSimulator;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	virtual void OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR szUrl);

	STDMETHOD(TranslateAccelerator)(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID);

// #define READ_THREAD
#ifdef READ_THREAD
	struct _ReadThreadArg {
		HWND hwndMainDlg;
		SOCKET socketLocal;
		HANDLE hEvtTerminate;
	} m_ReadThreadArgs;
	HANDLE m_hReadThread;
	static unsigned int __stdcall COMReadThread(void* arguments);
#endif
#ifdef IOALERTABLE
	WSAOVERLAPPED m_RecvOverlappedCompletionRoutine;
	char m_buf[0x01];
	WSABUF m_readBuffer;
	static void CALLBACK CBTHostDlg::WorkerRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
#endif

	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg LRESULT OnRXSingleByte(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
	HRESULT OnCheck(IHTMLElement *pElement);
	HRESULT OnSendPing(IHTMLElement* /*pElement*/);
	HRESULT OnConnect(IHTMLElement *pElement);

private:
	SOCKET m_socketLocal;
	HRESULT enumBTRadio(HANDLE& hRadio);
	HRESULT enumBTDevices(HANDLE hRadio);
	HRESULT enumBTDevices(GUID serviceClass);
	HRESULT enumBTServices(LPCWSTR szDeviceAddress, GUID serviceClass);
	HRESULT Connect(PSOCKADDR_BTH RemoteAddr);
	HRESULT Connect(PSOCKADDR_IN addrServer);
	HRESULT Connect(struct sockaddr* pAddrSimulator, size_t sAddrSimulatorLength);
	static HRESULT NameToBthAddr(const LPWSTR pszRemoteName, PSOCKADDR_BTH pRemoteBtAddr);
	static HRESULT NameToTCPAddr(const LPWSTR pszRemoteName, size_t& sAddrSimulatorLength, struct sockaddr** ppAddrSimulator);
	static HRESULT BTAddressToString(BLUETOOTH_ADDRESS* pBTAddr, BSTR* pbstrAddress);
	static HRESULT BTAddressToString(PSOCKADDR_BTH lpAddress, BSTR* pbstrAddress);
	static HRESULT ShowWSALastError(LPCTSTR szCaption);
};
