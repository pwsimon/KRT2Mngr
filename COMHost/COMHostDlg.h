// COMHostDlg.h : header file
//

#pragma once

// CCOMHostDlg dialog
class CCOMHostDlg : public CDHtmlDialog
{
// Construction
public:
	CCOMHostDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COMHOST_DIALOG, IDH = IDR_HTML_COMHOST_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	HRESULT OnSend(IHTMLElement* pElement);
	HRESULT OnRead(IHTMLElement* pElement);

// Implementation
protected:
	HICON m_hIcon;
	OVERLAPPED m_Overlapped;
	HANDLE m_hThread;
	static unsigned int __stdcall COMReadThread(void* arguments);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();

	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
};
