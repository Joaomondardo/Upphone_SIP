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

#include "stdafx.h"
#include "define.h"
#include "UpdateDlg.h"
#include "mainDlg.h"
#include "langpack.h"

// Upphone: "Update Available" notice (see UpdateDlg.h)

UpdateDlg* UpdateDlg::s_open = NULL;

void UpdateDlg::Show(CWnd* pParent, CString version, CString notes, CString url, CString sha256, bool mandatory)
{
    if (s_open) {
        s_open->ShowWindow(SW_SHOWNORMAL);
        s_open->SetForegroundWindow();
        return;
    }
    UpdateDlg* dlg = new UpdateDlg(version, notes, url, sha256, mandatory);
    if (!dlg->Create(IDD, pParent)) {
        delete dlg;
        return;
    }
    s_open = dlg;
    // centered on the screen, the main window may be in the tray; topmost, so it is not hidden behind other windows
    dlg->CenterWindow(CWnd::GetDesktopWindow());
    dlg->SetWindowPos(&CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    dlg->ShowWindow(SW_SHOWNORMAL);
    dlg->SetForegroundWindow();
}

UpdateDlg::UpdateDlg(CString version, CString notes, CString url, CString sha256, bool mandatory)
    : CThemeDialog(UpdateDlg::IDD, NULL), m_version(version), m_notes(notes), m_url(url), m_sha256(sha256), m_mandatory(mandatory)
{
    // Upphone: only an https page offered by the Upp platform, otherwise the fixed release page
    if (m_url.Left(8).CompareNoCase(_T("https://")) != 0) {
        m_url = _T(_GLOBAL_UPDATES_PAGE);
    }
}

void UpdateDlg::DoDataExchange(CDataExchange* pDX)
{
    CThemeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDOK, m_ButtonYes);
    DDX_Control(pDX, IDCANCEL, m_ButtonNo);
}

BEGIN_MESSAGE_MAP(UpdateDlg, CThemeDialog)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL UpdateDlg::OnInitDialog()
{
    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(m_hWnd);

    CString caption;
    caption.Format(_T("%s - %s"), _T(_GLOBAL_NAME_VISIBLE), Translate(_T("Update Available")));
    SetWindowText(caption);

    LOGFONT lf;
    GetFont()->GetLogFont(&lf);
    lf.lfHeight = -MulDiv(15, dpiY, 96);
    lf.lfWeight = FW_BOLD;
    m_fontTitle.CreateFontIndirect(&lf);
    GetDlgItem(IDC_UPDATE_TITLE)->SetFont(&m_fontTitle);
    lf.lfHeight = -MulDiv(12, dpiY, 96);
    lf.lfWeight = FW_SEMIBOLD;
    m_fontQuestion.CreateFontIndirect(&lf);
    GetDlgItem(IDC_UPDATE_QUESTION)->SetFont(&m_fontQuestion);

    int iconSize = MulDiv(32, dpiY, 96);
    m_hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, iconSize, iconSize, LR_SHARED);
    CStatic* icon = (CStatic*)GetDlgItem(IDC_UPDATE_ICON);
    icon->SetWindowPos(NULL, 0, 0, iconSize, iconSize, SWP_NOMOVE | SWP_NOZORDER);
    icon->SetIcon(m_hIcon);

    GetDlgItem(IDC_UPDATE_TITLE)->SetWindowText(Translate(m_mandatory ? _T("Update Required") : _T("Update Available")));
    CString text;
    text.Format(Translate(_T("Version %s is available. You are using version %s.")), m_version.GetString(), _T(_GLOBAL_VERSION));
    GetDlgItem(IDC_UPDATE_TEXT)->SetWindowText(text);
    if (!m_notes.IsEmpty() || !m_sha256.IsEmpty()) {
        CString notes;
        if (!m_notes.IsEmpty()) {
            notes.Format(_T("%s\r\n%s"), Translate(_T("What's new:")), m_notes.Left(1500).GetString());
        }
        if (!m_sha256.IsEmpty()) {
            // Upphone: to check the downloaded installer (Get-FileHash in PowerShell)
            notes.AppendFormat(_T("%s%s\r\n%s"), notes.IsEmpty() ? _T("") : _T("\r\n\r\n"),
                Translate(_T("Installer check code (SHA-256):")), m_sha256.GetString());
        }
        CEdit* edit = (CEdit*)GetDlgItem(IDC_UPDATE_NOTES);
        edit->SetWindowText(notes);
        // some room between the box border and the text
        CRect rect;
        edit->GetRect(&rect);
        rect.DeflateRect(MulDiv(6, dpiY, 96), MulDiv(4, dpiY, 96));
        edit->SetRectNP(&rect);
    }
    else {
        GetDlgItem(IDC_UPDATE_NOTES)->ShowWindow(SW_HIDE);
    }
    GetDlgItem(IDC_UPDATE_QUESTION)->SetWindowText(Translate(_T("Do you want to download it now?")));

    // "Yes" in the accent color, like Call and Answer
    m_ButtonYes.m_FaceColor = _GLOBAL_BRAND_ACCENT;
    m_ButtonYes.m_TextColor = RGB(255, 255, 255);
    m_ButtonYes.EnableWindow(TRUE);
    m_ButtonNo.m_FaceColor = _GLOBAL_THEME_SURFACE;
    m_ButtonNo.m_TextColor = _GLOBAL_THEME_TEXT;
    m_ButtonNo.EnableWindow(TRUE);
    GotoDlgCtrl(&m_ButtonYes);
    return FALSE;
}

HBRUSH UpdateDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH br = CThemeDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if (m_bAllowDark && pWnd) {
        switch (pWnd->GetDlgCtrlID()) {
        case IDC_UPDATE_TEXT:
            pDC->SetTextColor(_GLOBAL_THEME_TEXT2);
            break;
        case IDC_UPDATE_NOTES:
            // the notes in a box, on the surface color
            pDC->SetTextColor(_GLOBAL_THEME_TEXT);
            pDC->SetBkColor(_GLOBAL_THEME_SURFACE);
            pDC->SetBkMode(OPAQUE);
            return ThemeBrush(THEME_SURFACE);
        }
    }
    return br;
}

void UpdateDlg::OnOK()
{
    accountSettings.updatesDeclined.Empty();
    accountSettings.GlobalSave();
    MSIP::OpenURL(m_url);
    DestroyWindow();
}

void UpdateDlg::OnCancel()
{
    // "No" (or closing the notice): this version is offered again only by "Check for Updates"; a version below
    // the minimum is always offered again
    if (!m_mandatory) {
        accountSettings.updatesDeclined = m_version;
        accountSettings.GlobalSave();
    }
    DestroyWindow();
}

void UpdateDlg::PostNcDestroy()
{
    CThemeDialog::PostNcDestroy();
    if (s_open == this) {
        s_open = NULL;
    }
    delete this;
}
