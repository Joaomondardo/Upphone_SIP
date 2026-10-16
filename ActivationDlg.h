/*
 * Upphone SIP - provisioning
 * Copyright (C) 2026 Upp Tecnologia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include "resource.h"
#include "ThemeDialog.h"
#include "ButtonEx.h"

// Upphone: activation with the code of the Upp platform: on the first use, from the menu, with a code left by
// the installer and after leaving the account. Modeless (an incoming call can be answered while it is open);
// it deletes itself when closed.
class ActivationDlg : public CThemeDialog
{
public:
	enum { IDD = IDD_ACTIVATION };
	// Opens the window, or brings the open one to the front. message: shown under the code (why it opened);
	// code: filled in; autoStart: activates right away (code of the installer or of an interrupted activation).
	static void Show(CWnd* pParent, const CString& message = _T(""), const CString& code = _T(""), bool autoStart = false);
	static bool IsOpen() { return s_open != NULL; }

protected:
	ActivationDlg(const CString& message, const CString& code, bool autoStart);
	static ActivationDlg* s_open;
	CString m_message;
	CString m_code;
	bool m_autoStart;
	bool m_busy = false;
	bool m_done = false;
	bool m_formatting = false;
	int m_statusLevel = 0; // 0 information, 1 error, 2 success
	CFont m_fontTitle;
	CFont m_fontCode;
	HICON m_hIcon = NULL;
	CButtonEx m_ButtonOk;
	CButtonEx m_ButtonCancel;

	void SetStatus(const CString& text, int level);
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnChangeCode();
	afx_msg void OnClickManual(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnActivationResult(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};
