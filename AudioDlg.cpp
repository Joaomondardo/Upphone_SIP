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
#include "AudioDlg.h"
#include "mainDlg.h"
#include "langpack.h"
#include "Provisioning/Provisioning.h"

#include <cmath>

// Upphone: Audio window (see AudioDlg.h)

AudioDlg* AudioDlg::s_open = NULL;

static const UINT_PTR kLevelTimer = 1;
static const int kTicksPerSecond = 10;
static const int kRecordSeconds = 5;

void AudioDlg::Show(CWnd* pParent)
{
    if (s_open) {
        s_open->ShowWindow(SW_SHOWNORMAL);
        s_open->SetForegroundWindow();
        return;
    }
    AudioDlg* dlg = new AudioDlg();
    if (!dlg->Create(IDD, pParent)) {
        delete dlg;
        return;
    }
    s_open = dlg;
    // a window of its own, not owned by the main one: minimizing the main window does not hide it
    ::SetWindowLongPtr(dlg->m_hWnd, GWLP_HWNDPARENT, 0);
    dlg->CenterWindow(CWnd::GetDesktopWindow());
    dlg->ShowWindow(SW_SHOWNORMAL);
    dlg->SetForegroundWindow();
}

AudioDlg::AudioDlg()
    : CThemeDialog(AudioDlg::IDD, NULL)
{
}

void AudioDlg::CloseIfOpen()
{
    if (s_open) {
        s_open->OnCancel();
    }
}

void AudioDlg::DoDataExchange(CDataExchange* pDX)
{
    CThemeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDOK, m_ButtonSave);
    DDX_Control(pDX, IDCANCEL, m_ButtonCancel);
    DDX_Control(pDX, IDC_AUDIO_TEST_SPEAKER, m_ButtonSpeaker);
    DDX_Control(pDX, IDC_AUDIO_TEST_RING, m_ButtonRing);
    DDX_Control(pDX, IDC_AUDIO_RECORD, m_ButtonRecord);
}

BEGIN_MESSAGE_MAP(AudioDlg, CThemeDialog)
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_AUDIO_MIC, &AudioDlg::OnDeviceChanged)
    ON_CBN_SELCHANGE(IDC_AUDIO_SPEAKER, &AudioDlg::OnDeviceChanged)
    ON_CBN_SELCHANGE(IDC_AUDIO_RING, &AudioDlg::OnDeviceChanged)
    ON_BN_CLICKED(IDC_AUDIO_TEST_SPEAKER, &AudioDlg::OnTestSpeaker)
    ON_BN_CLICKED(IDC_AUDIO_TEST_RING, &AudioDlg::OnTestRing)
    ON_BN_CLICKED(IDC_AUDIO_RECORD, &AudioDlg::OnRecord)
END_MESSAGE_MAP()

BOOL AudioDlg::OnInitDialog()
{
    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(m_hWnd);

    CString caption;
    caption.Format(_T("%s - %s"), _T(_GLOBAL_NAME_VISIBLE), Translate(_T("Audio")));
    SetWindowText(caption);
    GetDlgItem(IDC_AUDIO_TEXT)->SetWindowText(Translate(_T("Choose the microphone and the speaker used in calls. Speak to see the level of the microphone.")));

    m_savedInput = accountSettings.audioInputDevice;
    m_savedOutput = accountSettings.audioOutputDevice;
    m_savedRing = accountSettings.audioRingDevice;

    // CButtonEx applies the colors in EnableWindow
    m_ButtonSave.m_FaceColor = _GLOBAL_BRAND_ACCENT;
    m_ButtonSave.m_TextColor = RGB(255, 255, 255);
    m_ButtonSave.EnableWindow(TRUE);
    CButtonEx* buttons[] = { &m_ButtonCancel, &m_ButtonSpeaker, &m_ButtonRing, &m_ButtonRecord };
    for (CButtonEx* button : buttons) {
        button->m_FaceColor = _GLOBAL_THEME_SURFACE;
        button->m_TextColor = _GLOBAL_THEME_TEXT;
        button->EnableWindow(TRUE);
    }

    FillDevices();
    if (InCall()) {
        SetStatus(Translate(_T("During a call the tests are disabled; the choice is used in the next call.")));
    }
    else {
        StartMeter();
    }
    SetTimer(kLevelTimer, 1000 / kTicksPerSecond, NULL);
    return TRUE;
}

void AudioDlg::FillDevices()
{
    CComboBox* combos[] = {
        (CComboBox*)GetDlgItem(IDC_AUDIO_MIC), (CComboBox*)GetDlgItem(IDC_AUDIO_SPEAKER), (CComboBox*)GetDlgItem(IDC_AUDIO_RING) };
    const CString* current[] = { &accountSettings.audioInputDevice, &accountSettings.audioOutputDevice, &accountSettings.audioRingDevice };
    for (CComboBox* combo : combos) {
        combo->ResetContent();
        combo->AddString(Translate(_T("Windows default")));
    }
    if (is_pjsua_running()) {
        unsigned count = PJMEDIA_AUD_MAX_DEVS;
        pjmedia_aud_dev_info info[PJMEDIA_AUD_MAX_DEVS];
        if (pjsua_enum_aud_devs(info, &count) == PJ_SUCCESS) {
            for (unsigned i = 0; i < count; i++) {
                CString name = MSIP::Utf8DecodeUni(info[i].name);
                if (info[i].input_count && combos[0]->FindStringExact(-1, name) == CB_ERR) {
                    combos[0]->AddString(name);
                }
                if (info[i].output_count) {
                    for (int c = 1; c < 3; c++) {
                        if (combos[c]->FindStringExact(-1, name) == CB_ERR) {
                            combos[c]->AddString(name);
                        }
                    }
                }
            }
        }
    }
    bool missing = false;
    for (int c = 0; c < 3; c++) {
        int index = current[c]->IsEmpty() ? 0 : combos[c]->FindStringExact(0, *current[c]);
        if (index == CB_ERR) {
            missing = true;
            index = 0;
        }
        combos[c]->SetCurSel(index);
    }
    if (missing) {
        SetStatus(Translate(_T("A device chosen before was not found; the Windows default is selected.")), true);
    }
}

CString AudioDlg::Selected(int id) const
{
    CComboBox* combo = (CComboBox*)GetDlgItem(id);
    int index = combo->GetCurSel();
    CString text;
    if (index > 0) {
        combo->GetLBText(index, text);
    }
    return text;
}

void AudioDlg::ApplySelection()
{
    accountSettings.audioInputDevice = Selected(IDC_AUDIO_MIC);
    accountSettings.audioOutputDevice = Selected(IDC_AUDIO_SPEAKER);
    accountSettings.audioRingDevice = Selected(IDC_AUDIO_RING);
    if (mainDlg && is_pjsua_running()) {
        mainDlg->UpdateSoundDevicesIds();
    }
}

bool AudioDlg::InCall() const
{
    return mainDlg && mainDlg->messagesDlg && mainDlg->messagesDlg->GetCallsCount() > 0;
}

bool AudioDlg::StartMeter()
{
    if (!is_pjsua_running() || InCall()) {
        return false;
    }
    ApplySelection();
    pjsua_snd_dev_param param;
    pjsua_snd_dev_param_default(&param);
    param.capture_dev = msip_audio_input;
    param.playback_dev = msip_audio_output;
    if (pjsua_set_snd_dev2(&param) != PJ_SUCCESS) {
        CString text = Translate(_T("Could not open the chosen microphone or speaker."));
        SetStatus(text, true);
        provisioning.ReportEvent("audio_error", "error", text);
        return false;
    }
    if (m_nullSlot == PJSUA_INVALID_ID) {
        // a port that only listens keeps the device open, so the level of the microphone can be read
        pjsua_conf_port_info info;
        if (pjsua_conf_get_port_info(0, &info) != PJ_SUCCESS) {
            return false;
        }
        m_pool = pjsua_pool_create("upp_audio_test", 512, 512);
        if (!m_pool || pjmedia_null_port_create(m_pool, info.clock_rate, info.channel_count, info.samples_per_frame,
                info.bits_per_sample, &m_nullPort) != PJ_SUCCESS
            || pjsua_conf_add_port(m_pool, m_nullPort, &m_nullSlot) != PJ_SUCCESS) {
            StopMeter();
            return false;
        }
        pjsua_conf_connect(0, m_nullSlot);
    }
    return true;
}

void AudioDlg::StopMeter()
{
    if (m_nullSlot != PJSUA_INVALID_ID && is_pjsua_running()) {
        pjsua_conf_disconnect(0, m_nullSlot);
        pjsua_conf_remove_port(m_nullSlot);
    }
    m_nullSlot = PJSUA_INVALID_ID;
    if (m_nullPort) {
        pjmedia_port_destroy(m_nullPort);
        m_nullPort = NULL;
    }
    if (m_pool) {
        pj_pool_release(m_pool);
        m_pool = NULL;
    }
    m_level = 0;
    CWnd* meter = GetDlgItem(IDC_AUDIO_MIC_LEVEL);
    if (meter) {
        meter->Invalidate();
    }
}

void AudioDlg::StopRecording(bool play)
{
    if (m_recorder == PJSUA_INVALID_ID) {
        return;
    }
    if (is_pjsua_running()) {
        pjsua_recorder_destroy(m_recorder);
    }
    m_recorder = PJSUA_INVALID_ID;
    m_ButtonRecord.SetWindowText(Translate(_T("Record and play")));
    if (play && mainDlg && is_pjsua_running() && !InCall()) {
        SetStatus(Translate(_T("Playing the recording in the speaker...")));
        mainDlg->PlayerPlay(m_recordFile, true, true);
    }
}

void AudioDlg::SetStatus(const CString& text, bool error)
{
    m_error = error;
    CWnd* status = GetDlgItem(IDC_AUDIO_STATUS);
    status->SetWindowText(text);
    status->Invalidate();
}

void AudioDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent != kLevelTimer) {
        CThemeDialog::OnTimer(nIDEvent);
        return;
    }
    bool inCall = InCall();
    if (inCall && (m_nullSlot != PJSUA_INVALID_ID || m_recorder != PJSUA_INVALID_ID)) {
        // a call started: the call owns the sound device
        StopRecording(false);
        StopMeter();
        SetStatus(Translate(_T("During a call the tests are disabled; the choice is used in the next call.")));
    }
    // the microphone stays open only while the window is on the screen
    bool visible = IsWindowVisible() && !IsIconic();
    if (!visible && (m_nullSlot != PJSUA_INVALID_ID || m_recorder != PJSUA_INVALID_ID)) {
        StopRecording(false);
        StopMeter();
        m_hiddenPause = true;
    }
    else if (visible && m_hiddenPause && !inCall) {
        m_hiddenPause = false;
        StartMeter();
    }
    m_ButtonSpeaker.EnableWindow(!inCall);
    m_ButtonRing.EnableWindow(!inCall);
    m_ButtonRecord.EnableWindow(!inCall);
    if (m_recorder != PJSUA_INVALID_ID && ++m_recordTicks >= kRecordSeconds * kTicksPerSecond) {
        StopRecording(true);
    }
    if (m_meterPause > 0 && --m_meterPause == 0 && !inCall) {
        StartMeter();
    }
    int level = 0;
    if (m_nullSlot != PJSUA_INVALID_ID && is_pjsua_running()) {
        unsigned tx = 0;
        unsigned rx = 0;
        // slot 0 is the sound device: what it receives is the microphone
        if (pjsua_conf_get_signal_level(0, &tx, &rx) == PJ_SUCCESS) {
            level = (int)rx;
        }
    }
    if (level != m_level) {
        m_level = level;
        GetDlgItem(IDC_AUDIO_MIC_LEVEL)->Invalidate();
    }
}

void AudioDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl != IDC_AUDIO_MIC_LEVEL) {
        CThemeDialog::OnDrawItem(nIDCtl, lpDrawItemStruct);
        return;
    }
    CDC* dc = CDC::FromHandle(lpDrawItemStruct->hDC);
    CRect rect = lpDrawItemStruct->rcItem;
    dc->FillSolidRect(rect, _GLOBAL_THEME_SURFACE);
    // square root: speech is low most of the time and the bar would barely move on a linear scale
    double ratio = sqrt(min(255, max(0, m_level)) / 255.0);
    CRect bar = rect;
    bar.right = bar.left + (int)(rect.Width() * ratio);
    if (bar.right > bar.left) {
        dc->FillSolidRect(bar, _GLOBAL_BRAND_ACCENT);
    }
    CBrush border(_GLOBAL_THEME_BORDER);
    dc->FrameRect(rect, &border);
}

HBRUSH AudioDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH br = CThemeDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if (m_bAllowDark && pWnd) {
        switch (pWnd->GetDlgCtrlID()) {
        case IDC_AUDIO_TEXT:
            pDC->SetTextColor(_GLOBAL_THEME_TEXT2);
            break;
        case IDC_AUDIO_STATUS:
            pDC->SetTextColor(m_error ? _GLOBAL_THEME_DANGER : _GLOBAL_THEME_TEXT);
            break;
        }
    }
    return br;
}

void AudioDlg::OnDeviceChanged()
{
    StopRecording(false);
    StopMeter();
    ApplySelection();
    SetStatus(_T(""));
    if (!InCall()) {
        StartMeter();
    }
}

void AudioDlg::OnTestSpeaker()
{
    if (!mainDlg || InCall()) {
        return;
    }
    StopRecording(false);
    if (StartMeter()) {
        SetStatus(Translate(_T("Playing a sound in the speaker...")));
        mainDlg->PlayerPlay(mainDlg->SoundPath(_T("ringing.wav")), true, true);
    }
}

void AudioDlg::OnTestRing()
{
    if (!mainDlg || InCall()) {
        return;
    }
    StopRecording(false);
    // the ringing device may be another one: the meter stops while the ringtone plays
    StopMeter();
    ApplySelection();
    msip_set_sound_device(msip_audio_ring, true);
    SetStatus(Translate(_T("Playing the ringtone...")));
    mainDlg->PlayerPlay(mainDlg->SoundPath(_T("ringtone.wav")), true, false);
    m_meterPause = 4 * kTicksPerSecond;
}

void AudioDlg::OnRecord()
{
    if (!mainDlg || InCall()) {
        return;
    }
    if (m_recorder != PJSUA_INVALID_ID) {
        StopRecording(true);
        return;
    }
    if (!StartMeter()) {
        return;
    }
    TCHAR temp[MAX_PATH] = {};
    GetTempPath(MAX_PATH, temp);
    m_recordFile = CString(temp) + _T("upphone_teste_audio.wav");
    DeleteFile(m_recordFile);
    pj_str_t file = MSIP::StrToPjStr(m_recordFile);
    pj_status_t status = pjsua_recorder_create(&file, 0, NULL, -1, 0, &m_recorder);
    free(file.ptr);
    if (status != PJ_SUCCESS) {
        m_recorder = PJSUA_INVALID_ID;
        SetStatus(Translate(_T("Could not record from the microphone.")), true);
        return;
    }
    pjsua_conf_connect(0, pjsua_recorder_get_conf_port(m_recorder));
    m_recordTicks = 0;
    m_ButtonRecord.SetWindowText(Translate(_T("Stop")));
    SetStatus(Translate(_T("Recording 5 seconds: say something...")));
}

void AudioDlg::OnOK()
{
    StopRecording(false);
    StopMeter();
    ApplySelection();
    accountSettings.SettingsSave();
    DestroyWindow();
}

void AudioDlg::OnCancel()
{
    StopRecording(false);
    StopMeter();
    accountSettings.audioInputDevice = m_savedInput;
    accountSettings.audioOutputDevice = m_savedOutput;
    accountSettings.audioRingDevice = m_savedRing;
    if (mainDlg && is_pjsua_running()) {
        mainDlg->UpdateSoundDevicesIds();
    }
    DestroyWindow();
}

void AudioDlg::OnDestroy()
{
    KillTimer(kLevelTimer);
    StopRecording(false);
    StopMeter();
    if (!m_recordFile.IsEmpty()) {
        DeleteFile(m_recordFile);
    }
    CThemeDialog::OnDestroy();
}

void AudioDlg::PostNcDestroy()
{
    CThemeDialog::PostNcDestroy();
    if (s_open == this) {
        s_open = NULL;
    }
    delete this;
}
