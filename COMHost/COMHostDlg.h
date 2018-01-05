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
	static HRESULT ShowLastError(LPCTSTR szCaption);

	HRESULT OnSend(IHTMLElement* pElement);
	HRESULT OnRead(IHTMLElement* pElement);

// Implementation
protected:
	HICON m_hIcon;
	HANDLE m_hCOMx;
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
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg LRESULT OnRXSingleByte(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRXDecodedCmd(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
};
