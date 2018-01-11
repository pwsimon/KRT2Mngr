// COMHostDlg.h : header file
//

#pragma once

// CCOMHostDlg dialog
class CCOMHostDlg : public CDHtmlDialog
{
// Construction
public:
	CCOMHostDlg(CWnd* pParent = NULL); // standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COMHOST_DIALOG, IDH = IDR_HTML_COMHOST_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	static HRESULT ShowLastError(LPCTSTR szCaption, const DWORD* pdwLastError = NULL);

// Implementation
protected:
	HICON m_hIcon;
#ifdef KRT2OUTPUT
	HANDLE m_hFileWrite;
#endif
	OVERLAPPED m_OverlappedWrite;
	struct _ReadThreadArg {
		HWND hwndMainDlg;
		HANDLE hCOMPort;
		HANDLE hEvtTerminate;
		HANDLE hEvtCOMPort;
	} m_ReadThreadArgs;
	HANDLE m_hReadThread;
	static unsigned int __stdcall COMReadThread(void* arguments);
	static HRESULT DriveStateMachine(HWND hwndMainDlg, BYTE byte, BOOL bAsynchronous);
	static enum _KRT2StateMachine DriveCommand(HWND hwndMainDlg, BYTE byte);
	static DWORD SignalObjectAndWait(HANDLE hEvtTerminate, HANDLE hThread);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	virtual BOOL IsExternalDispatchSafe();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg LRESULT OnRXSingleByte(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRXDecodedCmd(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
	HRESULT InitInputOutput(IHTMLElement*);

	DECLARE_DISPATCH_MAP()
	void sendCommand(BSTR bstrCommand, LPDISPATCH spCallback);
	void receiveCommand(LPDISPATCH pCallback);
	CComBSTR m_bstrReceiveCommand;
	CComDispatchDriver m_ddReceiveCommand;
};
