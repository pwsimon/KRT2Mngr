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
	HICON m_hIcon;
	int m_iRetCWSAStartup;
	SOCKADDR_BTH m_addrKRT2;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	virtual void OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR szUrl);

	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();

	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
	HRESULT OnCheck(IHTMLElement *pElement);
	HRESULT OnConnect(IHTMLElement *pElement);

private:
	HRESULT enumBTRadio(HANDLE& hRadio);
	HRESULT enumBTDevices(HANDLE hRadio);
	HRESULT enumBTDevices(GUID serviceClass);
	HRESULT enumBTServices(LPCWSTR szDeviceAddress, GUID serviceClass);
	HRESULT Connect(PSOCKADDR_BTH RemoteAddr);
	static HRESULT NameToBthAddr(const LPWSTR pszRemoteName, PSOCKADDR_BTH pRemoteBtAddr);
	static HRESULT BTAddressToString(BLUETOOTH_ADDRESS* pBTAddr, BSTR* pbstrAddress);
	static HRESULT BTAddressToString(PSOCKADDR_BTH lpAddress, BSTR* pbstrAddress);
	static HRESULT ShowWSALastError(LPCTSTR szCaption);
};
