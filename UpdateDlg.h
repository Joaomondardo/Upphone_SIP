/*
 * Copyright (C) 2011-2026 MicroSIP (http://www.microsip.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#include "resource.h"
#include "ThemeDialog.h"
#include "ButtonEx.h"

// Upphone: "Update Available" notice in the app theme, in place of the Windows message box. Modeless, so the
// main window stays usable (an incoming call can be handled while it is open); it deletes itself when closed.
class UpdateDlg : public CThemeDialog
{
public:
	enum { IDD = IDD_UPDATE };
	// opens the notice, or brings the one already open to the front. url: download page offered by the Upp
	// platform (https only; empty = the release page of GitHub); sha256: hash of the installer, shown to check
	// the download; mandatory: the installed version is below the minimum accepted, "No" is not remembered
	static void Show(CWnd* pParent, CString version, CString notes, CString url = _T(""), CString sha256 = _T(""), bool mandatory = false);

protected:
	UpdateDlg(CString version, CString notes, CString url, CString sha256, bool mandatory);
	static UpdateDlg* s_open;
	CString m_version;
	CString m_notes;
	CString m_url;
	CString m_sha256;
	bool m_mandatory = false;
	bool m_downgrade = false; // Upphone: a lower version pinned by the Upp platform (authorized rollback)
	CString Title() const;
	CFont m_fontTitle;
	CFont m_fontQuestion;
	HICON m_hIcon = NULL;
	CButtonEx m_ButtonYes;
	CButtonEx m_ButtonNo;
	// Upphone: installer of the Upp platform being downloaded and checked (Provisioning/UpdateDownload)
	bool m_downloading = false;
	void StartDownload();
	static UINT DownloadThread(LPVOID param);
	afx_msg LRESULT OnDownloaded(WPARAM wParam, LPARAM lParam);
	// the installer started by the app: when it ends with the app still open, the update did not happen
	HANDLE m_installer = NULL;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	DECLARE_MESSAGE_MAP()
};
