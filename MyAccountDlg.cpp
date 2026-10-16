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
#include "MyAccountDlg.h"
#include "AudioDlg.h"
#include "mainDlg.h"
#include "langpack.h"
#include "Provisioning/Provisioning.h"
#include "Provisioning/ProvisioningSupport.h"

// Upphone: "Minha conta" (see MyAccountDlg.h)

MyAccountDlg* MyAccountDlg::s_open = NULL;

static const UINT_PTR kRefreshTimer = 1;
static const int kLabels[6] = { IDC_MYACC_LABEL1, IDC_MYACC_LABEL2, IDC_MYACC_LABEL3, IDC_MYACC_LABEL4, IDC_MYACC_LABEL5, IDC_MYACC_LABEL6 };
static const int kValues[6] = { IDC_MYACC_VALUE1, IDC_MYACC_VALUE2, IDC_MYACC_VALUE3, IDC_MYACC_VALUE4, IDC_MYACC_VALUE5, IDC_MYACC_VALUE6 };

void MyAccountDlg::Show(CWnd* pParent)
{
    if (!provisioning.IsProvisioned()) {
        return;
    }
    if (s_open) {
        s_open->ShowWindow(SW_SHOWNORMAL);
        s_open->SetForegroundWindow();
        return;
    }
    MyAccountDlg* dlg = new MyAccountDlg();
    if (!dlg->Create(IDD, pParent)) {
        delete dlg;
        return;
    }
    s_open = dlg;
    dlg->CenterWindow(CWnd::GetDesktopWindow());
    dlg->ShowWindow(SW_SHOWNORMAL);
    dlg->SetForegroundWindow();
}

void MyAccountDlg::CloseIfOpen()
{
    if (s_open) {
        s_open->DestroyWindow();
    }
}

MyAccountDlg::MyAccountDlg()
    : CThemeDialog(MyAccountDlg::IDD, NULL)
{
}

void MyAccountDlg::DoDataExchange(CDataExchange* pDX)
{
    CThemeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MYACC_AUDIO, m_ButtonAudio);
    DDX_Control(pDX, IDC_MYACC_ECHO, m_ButtonEcho);
    DDX_Control(pDX, IDC_MYACC_DIAG, m_ButtonDiag);
    DDX_Control(pDX, IDC_MYACC_RELOAD, m_ButtonReload);
    DDX_Control(pDX, IDC_MYACC_LOGOUT, m_ButtonLogout);
    DDX_Control(pDX, IDCANCEL, m_ButtonClose);
}

BEGIN_MESSAGE_MAP(MyAccountDlg, CThemeDialog)
    ON_WM_CTLCOLOR()
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_MYACC_AUDIO, &MyAccountDlg::OnAudio)
    ON_BN_CLICKED(IDC_MYACC_ECHO, &MyAccountDlg::OnEcho)
    ON_BN_CLICKED(IDC_MYACC_DIAG, &MyAccountDlg::OnDiagnostics)
    ON_BN_CLICKED(IDC_MYACC_RELOAD, &MyAccountDlg::OnReload)
    ON_BN_CLICKED(IDC_MYACC_LOGOUT, &MyAccountDlg::OnLogout)
END_MESSAGE_MAP()

BOOL MyAccountDlg::OnInitDialog()
{
    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(m_hWnd);

    CString caption;
    caption.Format(_T("%s - %s"), _T(_GLOBAL_NAME_VISIBLE), Translate(_T("My Account")));
    SetWindowText(caption);

    LOGFONT lf;
    GetFont()->GetLogFont(&lf);
    LOGFONT base = lf;
    lf.lfHeight = -MulDiv(15, dpiY, 96);
    lf.lfWeight = FW_BOLD;
    m_fontTitle.CreateFontIndirect(&lf);
    GetDlgItem(IDC_MYACC_NAME)->SetFont(&m_fontTitle);
    base.lfWeight = FW_SEMIBOLD;
    m_fontLabel.CreateFontIndirect(&base);
    for (int id : kLabels) {
        GetDlgItem(id)->SetFont(&m_fontLabel);
    }

    int iconSize = MulDiv(32, dpiY, 96);
    m_hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MAINFRAME), IMAGE_ICON, iconSize, iconSize, LR_SHARED);
    CStatic* icon = (CStatic*)GetDlgItem(IDC_MYACC_ICON);
    icon->SetWindowPos(NULL, 0, 0, iconSize, iconSize, SWP_NOMOVE | SWP_NOZORDER);
    icon->SetIcon(m_hIcon);

    // CButtonEx applies the colors in EnableWindow
    CButtonEx* buttons[] = { &m_ButtonAudio, &m_ButtonEcho, &m_ButtonDiag, &m_ButtonReload, &m_ButtonLogout };
    for (CButtonEx* button : buttons) {
        button->m_FaceColor = _GLOBAL_THEME_SURFACE;
        button->m_TextColor = _GLOBAL_THEME_TEXT;
        button->EnableWindow(TRUE);
    }
    m_ButtonClose.m_FaceColor = _GLOBAL_BRAND_ACCENT;
    m_ButtonClose.m_TextColor = RGB(255, 255, 255);
    m_ButtonClose.EnableWindow(TRUE);

    Update();
    SetTimer(kRefreshTimer, 1000, NULL);
    GotoDlgCtrl(&m_ButtonClose);
    return FALSE;
}

void MyAccountDlg::SetValue(int row, const CString& label, const CString& value, int level)
{
    CString current;
    GetDlgItem(kLabels[row])->GetWindowText(current);
    if (current != label) {
        GetDlgItem(kLabels[row])->SetWindowText(label);
    }
    GetDlgItem(kValues[row])->GetWindowText(current);
    if (current != value || m_levels[row] != level) {
        m_levels[row] = level;
        GetDlgItem(kValues[row])->SetWindowText(value);
        GetDlgItem(kValues[row])->Invalidate();
    }
}

static CString DeviceName(const CString& configured, bool input, int* level)
{
    if (configured.IsEmpty()) {
        return Translate(_T("Windows default"));
    }
    if (is_pjsua_running()) {
        unsigned count = PJMEDIA_AUD_MAX_DEVS;
        pjmedia_aud_dev_info info[PJMEDIA_AUD_MAX_DEVS];
        if (pjsua_enum_aud_devs(info, &count) == PJ_SUCCESS) {
            for (unsigned i = 0; i < count; i++) {
                if ((input ? info[i].input_count : info[i].output_count) && MSIP::Utf8DecodeUni(info[i].name) == configured) {
                    return configured;
                }
            }
        }
        *level = max(*level, 1);
        return configured + _T(" (") + Translate(_T("not found, using the Windows default")) + _T(")");
    }
    return configured;
}

bool MyAccountDlg::Update()
{
    ProvisioningInfo info = provisioning.GetInfo();
    if (!info.provisioned) {
        DestroyWindow(); // deletes this object
        return false;
    }
    const ProvisionedConfig& config = info.config;
    CString name = !config.userName.IsEmpty() ? config.userName
        : (!config.account.displayName.IsEmpty() ? config.account.displayName : config.account.label);
    CString current;
    GetDlgItem(IDC_MYACC_NAME)->GetWindowText(current);
    if (current != name) {
        GetDlgItem(IDC_MYACC_NAME)->SetWindowText(name);
    }
    CString subtitle = config.clientName;
    if (info.environment != _T("prod") && !info.environment.IsEmpty()) {
        subtitle.AppendFormat(_T(" - %s %s"), Translate(_T("environment")), Provisioning::EnvironmentName(info.environment).GetString());
    }
    GetDlgItem(IDC_MYACC_SUBTITLE)->GetWindowText(current);
    if (current != subtitle) {
        GetDlgItem(IDC_MYACC_SUBTITLE)->SetWindowText(subtitle);
    }

    CString value;
    value.Format(_T("%s (%s %s)"), config.account.label.GetString(), Translate(_T("SIP user")), config.account.username.GetString());
    SetValue(0, Translate(_T("Extension")), value, config.valid ? 0 : 2);

    value = Provisioning::Hostname();
    if (info.activatedAt) {
        value.AppendFormat(_T(" - %s %s"), Translate(_T("activated on")), CTime(info.activatedAt).Format(_T("%d/%m/%Y")).GetString());
    }
    SetValue(1, Translate(_T("This computer")), value, 0);

    if (!info.blockedText.IsEmpty()) {
        SetValue(2, Translate(_T("Connection with Upp")), info.blockedText, 2);
    }
    else {
        SetValue(2, Translate(_T("Connection with Upp")), info.managementText, info.managementLevel);
    }

    // registration as this computer sees it; the PBX is the official source
    int level = 0;
    bool registered = false;
    if (!is_pjsua_running()) {
        value = Translate(_T("Starting..."));
        level = 1;
    }
    else if (!pjsua_acc_is_valid(account)) {
        value = provisioning.IsServiceBlocked() ? Translate(_T("Disconnected: the use of the extension is suspended."))
            : Translate(_T("No account registered."));
        level = 2;
    }
    else {
        pjsua_acc_info acc;
        if (pjsua_acc_get_info(account, &acc) == PJ_SUCCESS) {
            if (acc.status / 100 == 2 && acc.expires > 0 && acc.expires != PJSIP_EXPIRES_NOT_SPECIFIED) {
                value.Format(_T("%s %s."), Translate(_T("Registered in")), config.account.server.GetString());
                registered = true;
            }
            else if (acc.status >= 300) {
                CString hint;
                if (acc.status == 401 || acc.status == 403) {
                    hint = Translate(_T("the PBX refused the password"));
                }
                else if (acc.status == 404) {
                    hint = Translate(_T("extension not found in the PBX"));
                }
                else if (acc.status == 408 || acc.status == 502 || acc.status == 503) {
                    hint = Translate(_T("the PBX did not answer"));
                }
                else {
                    hint = MSIP::PjToStr(&acc.status_text);
                }
                value.Format(_T("%s %d: %s (UPP-SIP-%d)."), Translate(_T("Failure")), acc.status, hint.GetString(), acc.status);
                level = 2;
            }
            else {
                value = Translate(_T("Connecting..."));
                level = 1;
            }
        }
    }
    SetValue(3, Translate(_T("Registration in the PBX")), value, level);

    SetValue(4, Translate(_T("Configuration")), info.configText, info.configLevel);

    level = 0;
    value.Format(_T("%s: %s - %s: %s"), Translate(_T("Microphone")), DeviceName(accountSettings.audioInputDevice, true, &level).GetString(),
        Translate(_T("Loudspeaker")), DeviceName(accountSettings.audioOutputDevice, false, &level).GetString());
    SetValue(5, Translate(_T("Audio")), value, level);

    CString support;
    if (!config.supportContact.IsEmpty()) {
        support.Format(_T("%s: %s"), Translate(_T("Upp support")), config.supportContact.GetString());
    }
    GetDlgItem(IDC_MYACC_SUPPORT)->GetWindowText(current);
    if (current != support) {
        GetDlgItem(IDC_MYACC_SUPPORT)->SetWindowText(support);
    }

    bool idle = !mainDlg || !mainDlg->messagesDlg || !mainDlg->messagesDlg->GetCallsCount();
    m_ButtonEcho.EnableWindow(!config.echoTestNumber.IsEmpty() && registered && idle);
    m_ButtonEcho.ShowWindow(config.echoTestNumber.IsEmpty() ? SW_HIDE : SW_SHOW);
    return true;
}

HBRUSH MyAccountDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH br = CThemeDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if (m_bAllowDark && pWnd) {
        int id = pWnd->GetDlgCtrlID();
        if (id == IDC_MYACC_SUBTITLE || id == IDC_MYACC_SUPPORT) {
            pDC->SetTextColor(_GLOBAL_THEME_TEXT2);
        }
        for (int i = 0; i < 6; i++) {
            if (id == kLabels[i]) {
                pDC->SetTextColor(_GLOBAL_THEME_TEXT2);
            }
            else if (id == kValues[i]) {
                pDC->SetTextColor(m_levels[i] == 2 ? _GLOBAL_THEME_DANGER : (m_levels[i] == 1 ? _GLOBAL_THEME_WARNING : _GLOBAL_THEME_TEXT));
            }
        }
    }
    return br;
}

void MyAccountDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kRefreshTimer) {
        Update(); // may close (and delete) the window: nothing after it
        return;
    }
    CThemeDialog::OnTimer(nIDEvent);
}

void MyAccountDlg::OnAudio()
{
    AudioDlg::Show(mainDlg);
}

void MyAccountDlg::OnEcho()
{
    ProvisioningInfo info = provisioning.GetInfo();
    if (mainDlg && !info.config.echoTestNumber.IsEmpty()) {
        mainDlg->MakeCall(info.config.echoTestNumber);
    }
}

void MyAccountDlg::OnDiagnostics()
{
    CString message;
    provisioning.SendDiagnostics(message);
    GetDlgItem(IDC_MYACC_SUPPORT)->SetWindowText(message);
}

void MyAccountDlg::OnReload()
{
    provisioning.RefreshConfig();
    GetDlgItem(IDC_MYACC_SUPPORT)->SetWindowText(Translate(_T("Looking for the configuration...")));
}

void MyAccountDlg::OnLogout()
{
    ProvisioningInfo info = provisioning.GetInfo();
    CString text;
    text.Format(Translate(_T("This computer stops using the extension %s. To use it again, a new activation code from Upp is needed.")),
        info.config.account.label.GetString());
    int pressed = 0;
    TaskDialog(m_hWnd, NULL, _T(_GLOBAL_NAME_NICE), Translate(_T("Sign out?")), text,
        TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON, &pressed);
    if (pressed != IDYES) {
        return;
    }
    if (mainDlg && mainDlg->messagesDlg && mainDlg->messagesDlg->GetCallsCount()) {
        msip_call_hangup_all();
    }
    provisioning.RequestLogout();
    DestroyWindow();
}

void MyAccountDlg::OnOK()
{
    DestroyWindow();
}

void MyAccountDlg::OnCancel()
{
    DestroyWindow();
}

void MyAccountDlg::PostNcDestroy()
{
    CThemeDialog::PostNcDestroy();
    if (s_open == this) {
        s_open = NULL;
    }
    delete this;
}
