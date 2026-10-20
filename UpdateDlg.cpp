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
#include "Provisioning/UpdateDownload.h"

// Upphone: "Update Available" notice (see UpdateDlg.h)

UpdateDlg* UpdateDlg::s_open = NULL;

// Upphone: result of the download thread, posted to the notice
#define UM_UPDATE_DOWNLOADED (WM_APP + 1)

struct UpdateDownloadJob {
    HWND hwnd;
    CString url;
    CString sha256;
    CString version;
    Provisioning::UpdateDownloadResult result;
};

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
    // Upphone: offered version below the installed one, part by part ("1.4.0" < "1.4.1")
    CString installed = _T(_GLOBAL_VERSION);
    auto next = [](const CString& text, int& pos) { return pos < 0 ? 0 : _ttoi(text.Tokenize(_T("."), pos)); };
    int posOffered = 0, posInstalled = 0;
    for (int i = 0; i < 4; i++) {
        int offered = next(m_version, posOffered);
        int current = next(installed, posInstalled);
        if (offered != current) {
            m_downgrade = offered < current;
            break;
        }
    }
}

// Upphone: required (below the minimum), rollback or plain update
CString UpdateDlg::Title() const
{
    return Translate(m_mandatory ? _T("Update Required") : (m_downgrade ? _T("Version Change") : _T("Update Available")));
}

void UpdateDlg::DoDataExchange(CDataExchange* pDX)
{
    CThemeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDOK, m_ButtonYes);
    DDX_Control(pDX, IDCANCEL, m_ButtonNo);
}

BEGIN_MESSAGE_MAP(UpdateDlg, CThemeDialog)
    ON_WM_CTLCOLOR()
    // Upphone: download, check and installer of an update offered by the Upp platform
    ON_MESSAGE(UM_UPDATE_DOWNLOADED, OnDownloaded)
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL UpdateDlg::OnInitDialog()
{
    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(m_hWnd);

    CString caption;
    // Upphone: the same words as the title (an update below the minimum is required)
    caption.Format(_T("%s - %s"), _T(_GLOBAL_NAME_VISIBLE), Title().GetString());
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

    GetDlgItem(IDC_UPDATE_TITLE)->SetWindowText(Title());
    CString text;
    text.Format(Translate(m_downgrade ? _T("Upp indicated version %s. You are using version %s.")
        : _T("Version %s is available. You are using version %s.")), m_version.GetString(), _T(_GLOBAL_VERSION));
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
    if (m_downloading) {
        return;
    }
    // Upphone: a version offered by the Upp platform comes with the SHA-256 of its installer: the app downloads
    // it, checks it and runs it (specification, RN09). Without it (GitHub release page), the page is opened.
    if (!m_sha256.IsEmpty()) {
        if (mainDlg && mainDlg->messagesDlg && mainDlg->messagesDlg->GetCallsCount(true)) {
            ::MessageBox(m_hWnd, Translate(_T("Finish the call before installing the update.")),
                _T(_GLOBAL_NAME_VISIBLE), MB_ICONINFORMATION);
            return;
        }
        StartDownload();
        return;
    }
    accountSettings.updatesDeclined.Empty();
    accountSettings.GlobalSave();
    MSIP::OpenURL(m_url);
    DestroyWindow();
}

void UpdateDlg::StartDownload()
{
    m_downloading = true;
    m_ButtonYes.EnableWindow(FALSE);
    m_ButtonNo.EnableWindow(FALSE);
    GetDlgItem(IDC_UPDATE_QUESTION)->SetWindowText(Translate(_T("Downloading and checking the update...")));
    UpdateDownloadJob* job = new UpdateDownloadJob{ m_hWnd, m_url, m_sha256, m_version };
    if (!AfxBeginThread(DownloadThread, job)) {
        delete job;
        m_downloading = false;
        m_ButtonYes.EnableWindow(TRUE);
        m_ButtonNo.EnableWindow(TRUE);
    }
}

UINT UpdateDlg::DownloadThread(LPVOID param)
{
    UpdateDownloadJob* job = (UpdateDownloadJob*)param;
    job->result = Provisioning::DownloadUpdate(job->url, job->sha256, job->version);
    // the notice cannot be closed while downloading; if the app was closed, the window no longer exists
    if (!::PostMessage(job->hwnd, UM_UPDATE_DOWNLOADED, 0, (LPARAM)job)) {
        delete job;
    }
    return 0;
}

LRESULT UpdateDlg::OnDownloaded(WPARAM wParam, LPARAM lParam)
{
    UpdateDownloadJob* job = (UpdateDownloadJob*)lParam;
    Provisioning::UpdateDownloadResult result = job->result;
    delete job;
    m_downloading = false;
    CString error = result.error;
    if (result.ok) {
        if (mainDlg && mainDlg->messagesDlg && mainDlg->messagesDlg->GetCallsCount(true)) {
            error = Translate(_T("Finish the call before installing the update."));
        }
        else if (Provisioning::LaunchUpdate(result.path, m_installer, error)) {
            accountSettings.updatesDeclined.Empty();
            accountSettings.GlobalSave();
            // the installer closes the app; until then the notice waits (Windows asks for the permission now)
            m_downloading = true;
            GetDlgItem(IDC_UPDATE_QUESTION)->SetWindowText(Translate(_T("Installing. Confirm the permission of Windows.")));
            if (!m_installer) {
                DestroyWindow();
            }
            else {
                SetTimer(UM_UPDATE_DOWNLOADED, 1000, NULL);
            }
            return 0;
        }
    }
    m_ButtonYes.EnableWindow(TRUE);
    m_ButtonNo.EnableWindow(TRUE);
    GetDlgItem(IDC_UPDATE_QUESTION)->SetWindowText(Translate(_T("Do you want to download it now?")));
    if (!result.ok) {
        // the page in the browser is the way left; the check code stays in the notice
        CString text;
        text.Format(_T("%s\r\n\r\n%s"), error.GetString(), Translate(_T("Open the download page in the browser?")));
        if (::MessageBox(m_hWnd, text, _T(_GLOBAL_NAME_VISIBLE), MB_YESNO | MB_ICONWARNING) == IDYES) {
            MSIP::OpenURL(m_url);
        }
    }
    else {
        ::MessageBox(m_hWnd, error, _T(_GLOBAL_NAME_VISIBLE), MB_ICONWARNING);
    }
    return 0;
}

void UpdateDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent != UM_UPDATE_DOWNLOADED || !m_installer || WaitForSingleObject(m_installer, 0) != WAIT_OBJECT_0) {
        CThemeDialog::OnTimer(nIDEvent);
        return;
    }
    KillTimer(nIDEvent);
    DWORD code = 0;
    GetExitCodeProcess(m_installer, &code);
    CloseHandle(m_installer);
    m_installer = NULL;
    m_downloading = false;
    CString failure = Provisioning::InstallerFailure(code);
    if (failure.IsEmpty()) {
        DestroyWindow();
        return;
    }
    m_ButtonYes.EnableWindow(TRUE);
    m_ButtonNo.EnableWindow(TRUE);
    GetDlgItem(IDC_UPDATE_QUESTION)->SetWindowText(Translate(_T("Do you want to download it now?")));
    ::MessageBox(m_hWnd, failure, _T(_GLOBAL_NAME_VISIBLE), MB_ICONWARNING);
}

void UpdateDlg::OnDestroy()
{
    if (m_installer) {
        KillTimer(UM_UPDATE_DOWNLOADED);
        CloseHandle(m_installer);
        m_installer = NULL;
    }
    CThemeDialog::OnDestroy();
}

void UpdateDlg::OnCancel()
{
    if (m_downloading) {
        return;
    }
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
