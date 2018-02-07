// BTHost.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "BTHost.h"
#include "BTHostDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CBTHostApp
BEGIN_MESSAGE_MAP(CBTHostApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

// The one and only CBTHostApp object
CBTHostApp theApp;

// CBTHostApp construction
CBTHostApp::CBTHostApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// CBTHostApp initialization
BOOL CBTHostApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	CBTHostDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// Delete the shell manager created above.
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

#ifndef _AFXDLL
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

#ifdef IOALERTABLE
/*virtual*/ BOOL CBTHostApp::PumpMessage()
{
	_AFX_THREAD_STATE* pState = AfxGetThreadState();

	// const DWORD dwWait = WAIT_OBJECT_0; // exact gleiches verhalten wie das orginal PumpMessage()
	const DWORD dwWait = ::MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE); // irgendwas blokiert hier bzw. macht die sache zaeh
	switch (dwWait)
	{
	case WAIT_OBJECT_0:
	{
		if (!::GetMessage(&(pState->m_msgCur), NULL, NULL, NULL))
		{
#ifdef _DEBUG
			TRACE(traceAppMsg, 1, "CWinThread::PumpMessage - Received WM_QUIT.\n");
			pState->m_nDisablePumpCount++; // application must die
#endif
			// Note: prevents calling message loop things in 'ExitInstance'
			// will never be decremented
			return FALSE;
		}

		// process this message
		if (pState->m_msgCur.message != WM_KICKIDLE && !AfxPreTranslateMessage(&(pState->m_msgCur)))
		{
			::TranslateMessage(&(pState->m_msgCur));
			::DispatchMessage(&(pState->m_msgCur));
		}
	}
	break;
	case WAIT_OBJECT_0 + 1:
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("WAIT_OBJECT_0 + 1\n"));
	}
	break;
	case WAIT_IO_COMPLETION:
	{
		ATLTRACE2(atlTraceGeneral, 0, _T("WAIT_IO_COMPLETION\n"));
	}
	break;
	default:
		break;
	}

	return TRUE;
}
#endif
