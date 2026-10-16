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

// Upphone: "Minha conta" of a provisioned account: the extension, this computer and, separately, the
// activation, the connection with the Upp platform, the registration in the PBX, the configuration and the
// audio. The user can test the audio, send a diagnosis, reload the configuration or leave the account; the
// technical settings are not shown. Modeless; it deletes itself when closed.
class MyAccountDlg : public CThemeDialog
{
public:
	enum { IDD = IDD_MY_ACCOUNT };
	static void Show(CWnd* pParent);
	static void CloseIfOpen();

protected:
	MyAccountDlg();
	static MyAccountDlg* s_open;
	CFont m_fontTitle;
	CFont m_fontLabel;
	HICON m_hIcon = NULL;
	int m_levels[6] = {};
	CButtonEx m_ButtonAudio;
	CButtonEx m_ButtonEcho;
	CButtonEx m_ButtonDiag;
	CButtonEx m_ButtonReload;
	CButtonEx m_ButtonLogout;
	CButtonEx m_ButtonClose;

	bool Update(); // false when the window closed itself (the computer left the account)
	void SetValue(int row, const CString& label, const CString& value, int level);
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnAudio();
	afx_msg void OnEcho();
	afx_msg void OnDiagnostics();
	afx_msg void OnReload();
	afx_msg void OnLogout();
	DECLARE_MESSAGE_MAP()
};
