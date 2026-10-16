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

#include "stdafx.h"
#include "define.h"
#include "ActivationDlg.h"
#include "mainDlg.h"
#include "langpack.h"
#include "Provisioning/Provisioning.h"

// Upphone: activation window (see ActivationDlg.h)

ActivationDlg* ActivationDlg::s_open = NULL;

void ActivationDlg::Show(CWnd* pParent, const CString& message, const CString& code, bool autoStart)
{
    if (s_open) {
        s_open->ShowWindow(SW_SHOWNORMAL);
        s_open->SetForegroundWindow();
        return;
    }
    ActivationDlg* dlg = new ActivationDlg(message, code, autoStart);
    if (!dlg->Create(IDD, pParent)) {
        delete dlg;
        return;
    }
    s_open = dlg;
    dlg->CenterWindow(CWnd::GetDesktopWindow());
    dlg->ShowWindow(SW_SHOWNORMAL);
    dlg->SetForegroundWindow();
}

ActivationDlg::ActivationDlg(const CString& message, const CString& code, bool autoStart)
    : CThemeDialog(ActivationDlg::IDD, NULL), m_message(message), m_code(code), m_autoStart(autoStart)
{
}

void ActivationDlg::DoDataExchange(CDataExchange* pDX)
{
    CThemeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDOK, m_ButtonOk);
    DDX_Control(pDX, IDCANCEL, m_ButtonCancel);
}

BEGIN_MESSAGE_MAP(ActivationDlg, CThemeDialog)
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(IDC_ACTIVATION_CODE, &ActivationDlg::OnChangeCode)
    ON_NOTIFY(NM_CLICK, IDC_ACTIVATION_MANUAL, &ActivationDlg::OnClickManual)
    ON_NOTIFY(NM_RETURN, IDC_ACTIVATION_MANUAL, &ActivationDlg::OnClickManual)
    ON_MESSAGE(UM_PROVISIONING_ACTIVATION, &ActivationDlg::OnActivationResult)
END_MESSAGE_MAP()

BOOL ActivationDlg::OnInitDialog()
{
    CString link;
    link.Format(_T("<a>%s</a>"), Translate(_T("Manual setup (technicians)")));
    GetDlgItem(IDC_ACTIVATION_MANUAL)->SetWindowText(link);

    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(m_hWnd);

    CString caption;
    caption.Format(_T("%s - %s"), _T(_GLOBAL_NAME_VISIBLE), Translate(_T("Activation")));
    SetWindowText(caption);

    LOGFONT lf;
    GetFont()->GetLogFont(&lf);
    lf.lfHeight = -MulDiv(15, dpiY, 96);
    lf.lfWeight = FW_BOLD;
    m_fontTitle.CreateFontIndirect(&lf);
    GetDlgItem(IDC_ACTIVATION_TITLE)->SetFont(&m_fontTitle);
    // the code in a monospaced font: 0/O and 1/I are easy to tell apart
    lf.lfHeight = -MulDiv(17, dpiY, 96);
    lf.lfWeight = FW_SEMIBOLD;
    lstrcpyn(lf.lfFaceName, _T("Consolas"), LF_FACESIZE);
    m_fontCode.CreateFontIndirect(&lf);
    CEdit* edit = (CEdit*)GetDlgItem(IDC_ACTIVATION_CODE);
    edit->SetFont(&m_fontCode);
    edit->SetLimitText(24);

    int iconSize = MulDiv(32, dpiY, 96);
    m_hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, iconSize, iconSize, LR_SHARED);
    CStatic* icon = (CStatic*)GetDlgItem(IDC_ACTIVATION_ICON);
    icon->SetWindowPos(NULL, 0, 0, iconSize, iconSize, SWP_NOMOVE | SWP_NOZORDER);
    icon->SetIcon(m_hIcon);

    GetDlgItem(IDC_ACTIVATION_TITLE)->SetWindowText(Translate(_T("Activate Upphone SIP")));
    GetDlgItem(IDC_ACTIVATION_TEXT)->SetWindowText(Translate(_T("Type the activation code sent by Upp. It is valid for a short time and can be used only once.")));

    m_ButtonOk.m_FaceColor = _GLOBAL_BRAND_ACCENT;
    m_ButtonOk.m_TextColor = RGB(255, 255, 255);
    m_ButtonOk.EnableWindow(TRUE);
    m_ButtonCancel.m_FaceColor = _GLOBAL_THEME_SURFACE;
    m_ButtonCancel.m_TextColor = _GLOBAL_THEME_TEXT;
    m_ButtonCancel.EnableWindow(TRUE);

    if (!m_code.IsEmpty()) {
        edit->SetWindowText(m_code);
    }
    if (!m_message.IsEmpty()) {
        SetStatus(m_message, 0);
    }
    GotoDlgCtrl(edit);
    if (m_autoStart && !m_code.IsEmpty()) {
        PostMessage(WM_COMMAND, IDOK);
    }
    return FALSE;
}

HBRUSH ActivationDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH br = CThemeDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if (m_bAllowDark && pWnd) {
        switch (pWnd->GetDlgCtrlID()) {
        case IDC_ACTIVATION_TEXT:
        case IDC_ACTIVATION_CODE_LABEL:
            pDC->SetTextColor(_GLOBAL_THEME_TEXT2);
            break;
        case IDC_ACTIVATION_STATUS:
            pDC->SetTextColor(m_statusLevel == 1 ? _GLOBAL_THEME_DANGER
                : (m_statusLevel == 2 ? _GLOBAL_THEME_SUCCESS : _GLOBAL_THEME_TEXT));
            break;
        }
    }
    return br;
}

void ActivationDlg::SetStatus(const CString& text, int level)
{
    m_statusLevel = level;
    CWnd* status = GetDlgItem(IDC_ACTIVATION_STATUS);
    status->SetWindowText(text);
    status->Invalidate();
}

void ActivationDlg::OnChangeCode()
{
    if (m_formatting) {
        return;
    }
    // as typed or pasted: upper case, only letters and digits, a dash every 4 characters
    CEdit* edit = (CEdit*)GetDlgItem(IDC_ACTIVATION_CODE);
    CString text;
    edit->GetWindowText(text);
    CString chars;
    for (int i = 0; i < text.GetLength(); i++) {
        TCHAR c = (TCHAR)_totupper(text[i]);
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) {
            chars += c;
        }
    }
    chars = chars.Left(16);
    CString formatted;
    for (int i = 0; i < chars.GetLength(); i++) {
        if (i && i % 4 == 0) {
            formatted += _T('-');
        }
        formatted += chars[i];
    }
    if (formatted != text) {
        m_formatting = true;
        edit->SetWindowText(formatted);
        edit->SetSel(formatted.GetLength(), formatted.GetLength());
        m_formatting = false;
    }
    if (m_statusLevel == 1) {
        SetStatus(_T(""), 0);
    }
}

void ActivationDlg::OnOK()
{
    if (m_done) {
        DestroyWindow();
        return;
    }
    if (m_busy) {
        return;
    }
    CString code;
    GetDlgItem(IDC_ACTIVATION_CODE)->GetWindowText(code);
    CString error;
    if (!provisioning.StartActivation(code, m_hWnd, error)) {
        SetStatus(error, 1);
        GotoDlgCtrl(GetDlgItem(IDC_ACTIVATION_CODE));
        return;
    }
    m_busy = true;
    GetDlgItem(IDC_ACTIVATION_CODE)->EnableWindow(FALSE);
    m_ButtonOk.EnableWindow(FALSE);
    SetStatus(Translate(_T("Activating... this takes a few seconds.")), 0);
}

LRESULT ActivationDlg::OnActivationResult(WPARAM wParam, LPARAM lParam)
{
    CString* message = (CString*)lParam;
    m_busy = false;
    if (wParam) {
        m_done = true;
        SetStatus(*message, 2);
        m_ButtonOk.SetWindowText(Translate(_T("Done")));
        m_ButtonOk.EnableWindow(TRUE);
        m_ButtonCancel.ShowWindow(SW_HIDE);
        GetDlgItem(IDC_ACTIVATION_MANUAL)->ShowWindow(SW_HIDE);
        GotoDlgCtrl(&m_ButtonOk);
    }
    else {
        SetStatus(*message, 1);
        // the computer may have been activated with a configuration this app refuses: nothing to retry here
        bool activated = provisioning.IsProvisioned();
        GetDlgItem(IDC_ACTIVATION_CODE)->EnableWindow(!activated);
        m_ButtonOk.EnableWindow(!activated);
        if (!activated) {
            GotoDlgCtrl(GetDlgItem(IDC_ACTIVATION_CODE));
        }
    }
    delete message;
    return 0;
}

void ActivationDlg::OnClickManual(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (m_busy) {
        return;
    }
    if (mainDlg) {
        mainDlg->OnMenuAccountAdd();
    }
    DestroyWindow();
}

void ActivationDlg::OnCancel()
{
    // an activation in progress goes on; its result is shown in a notification
    provisioning.CancelActivationNotify(m_hWnd);
    DestroyWindow();
}

void ActivationDlg::PostNcDestroy()
{
    CThemeDialog::PostNcDestroy();
    if (s_open == this) {
        s_open = NULL;
    }
    delete this;
}
