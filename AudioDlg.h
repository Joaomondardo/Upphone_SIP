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

#include <pjsua-lib/pjsua.h>

// Upphone: choice and test of the microphone, speaker and ringing device, the only settings of the app the user
// changes (the technical ones come from the Upp platform). Shows the microphone level, plays the sounds of the
// app and records 5 seconds to play back. The tests are disabled during a call. Modeless; deletes itself.
class AudioDlg : public CThemeDialog
{
public:
	enum { IDD = IDD_AUDIO };
	static void Show(CWnd* pParent);
	static void CloseIfOpen(); // Upphone: release pjmedia ports/pools before the SIP stack is destroyed

protected:
	AudioDlg();
	static AudioDlg* s_open;
	CString m_savedInput;
	CString m_savedOutput;
	CString m_savedRing;
	CButtonEx m_ButtonSave;
	CButtonEx m_ButtonCancel;
	CButtonEx m_ButtonSpeaker;
	CButtonEx m_ButtonRing;
	CButtonEx m_ButtonRecord;
	int m_level = 0;
	bool m_error = false;
	// microphone level: the sound device stays open connected to a null port while the window is open
	pj_pool_t* m_pool = NULL;
	pjmedia_port* m_nullPort = NULL;
	pjsua_conf_port_id m_nullSlot = PJSUA_INVALID_ID;
	pjsua_recorder_id m_recorder = PJSUA_INVALID_ID;
	CString m_recordFile;
	int m_recordTicks = 0;
	int m_meterPause = 0;
	bool m_hiddenPause = false; // meter stopped while the window is hidden or minimized

	void FillDevices();
	CString Selected(int id) const;
	void ApplySelection();
	bool InCall() const;
	bool StartMeter();
	void StopMeter();
	void StopRecording(bool play);
	void SetStatus(const CString& text, bool error = false);
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnDestroy();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDeviceChanged();
	afx_msg void OnTestSpeaker();
	afx_msg void OnTestRing();
	afx_msg void OnRecord();
	DECLARE_MESSAGE_MAP()
};
