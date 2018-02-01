// BTHost.h : main header file for the PROJECT_NAME application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h" // main symbols

// CBTHostApp:
// See BTHost.cpp for the implementation of this class
//
class CBTHostApp : public CWinApp
{
public:
	CBTHostApp();

// Overrides
public:
#ifdef IOALERTABLE
	virtual BOOL InitInstance();
#endif

// Implementation
	DECLARE_MESSAGE_MAP()
	virtual BOOL PumpMessage();
};

extern CBTHostApp theApp;