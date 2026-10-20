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

#include "global.h"
#include "json.h"
#include "ProvisioningHttp.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct AccountSettings;

// Upphone: provisioning of the account by the Upp platform (docs/PROVISIONAMENTO.md).
//
// The app is activated with a one-time code; the server returns a token of this computer and the SIP
// configuration, kept in a file protected with DPAPI (provisioning.dat) and never in Upphone.ini. While the app
// runs it sends a heartbeat (registration, applied configuration, version) and applies the configuration
// changed in the panel, waiting for the end of a call when the change restarts the account or the audio.
// All HTTP requests run in a worker thread; everything else runs in the UI thread.

enum ProvJobType { PROV_JOB_ACTIVATE, PROV_JOB_STATUS, PROV_JOB_CONFIG, PROV_JOB_LOGS, PROV_JOB_LOGOUT };

struct ProvJob {
    ProvJobType type = PROV_JOB_STATUS;
    CString method;
    CString url;
    CString headers;
    std::string body;
    bool collectLogs = false; // the worker collects the logs into body
    bool userInitiated = false;
    DWORD timeoutMs = 30000;
    Provisioning::HttpResponse response;
    Json::Value json;
    bool jsonOk = false;
};

// Configuration received from the server, validated.
struct ProvisionedConfig {
    bool valid = false;
    int revision = 0;
    Account account;
    CString audioCodecs;
    int dtmfMethod = 2;
    CString stun;
    CString featureCodeAT;
    bool enableFeatureCodeAT = false;
    bool debugLog = false;
    CString echoTestNumber;
    int offlineGraceHours = 0;
    CString clientName;
    CString userName;
    CString userEmail;
    CString supportContact;
};

// What "Minha conta" shows.
struct ProvisioningInfo {
    bool provisioned = false;
    ProvisionedConfig config;
    CString environment;
    CString server;
    CString deviceId;
    time_t activatedAt = 0;
    time_t lastContact = 0;
    CString managementText; // connection with the Upp platform
    int managementLevel = 0; // 0 ok, 1 warning, 2 error
    CString configText;
    int configLevel = 0;
    CString blockedText;
};

class ProvisioningClient
{
public:
    // Before the main window (end of AccountSettings::Init): reads the protected file and the installer bootstrap.
    // Read-only: it also runs in a second start that only passes its command line (tel: link, /answer) to the app
    // already open and exits; what has to be written, deleted or logged waits for Start.
    void Init();
    // Settings and account that come from the server (called by AccountSettings::SettingsLoad and Init).
    void ApplySettingsOverrides(AccountSettings* settings) const;
    void ApplyAccount(AccountSettings* settings) const;
    // Main window created and pjsua running: starts the worker and the heartbeat.
    void Start(HWND mainWnd);
    void Shutdown();

    bool IsProvisioned() const { return m_provisioned; }
    // provisioned, but the configuration kept is not usable by this app (refused): no account, no dialog
    bool HasUsableConfig() const { return m_provisioned && m_config.valid; }
    bool IsServiceBlocked() const { return !m_blockedText.IsEmpty(); }
    const CString& BlockedText() const { return m_blockedText; }

    // Activation (ActivationDlg). false and error filled when the code is refused before reaching the server.
    // The result goes to notifyWnd: UM_PROVISIONING_ACTIVATION, wParam 1 ok / 0 error, lParam CString* message.
    bool StartActivation(const CString& code, HWND notifyWnd, CString& error);
    bool IsActivating() const { return m_activating; }
    void CancelActivationNotify(HWND notifyWnd);
    // Code left for this user by the installer, or by an activation interrupted before the answer arrived.
    CString PendingCode() const;

    void OnTimer();
    void OnResult(ProvJob* job);
    void OnRegistrationChanged();
    void OnCallsEnded();

    void RequestLogout();
    void RefreshConfig();
    bool SendDiagnostics(CString& message);
    // event of the app sent with the next heartbeat (types accepted by the server: started, error, audio_error...)
    void ReportEvent(const char* type, const char* level, const CString& message);

    ProvisioningInfo GetInfo() const;
    // Version offered by the Upp platform (null when there is none)
    const Json::Value& UpdateOffer() const { return m_update; }

private:
    // store (provisioning.dat)
    bool SaveStore(CString* error = nullptr);
    void LoadBootstrap();
    void ConsumeBootstrap();
    CString ServerFor(const CString& environment) const;
    CString Headers(bool withToken, LPCTSTR contentType = _T("application/json; charset=utf-8")) const;

    // configuration
    enum ApplyResult { APPLY_DONE, APPLY_PENDING, APPLY_REJECTED };
    ApplyResult ApplyConfig(const Json::Value& config, const CString& etag, int revision, CString& error);
    void ApplyToEngine(const ProvisionedConfig& config, bool restartStack, bool codecsChanged, bool logChanged, bool accountChanged);
    void ReconfigureLog(bool enable);

    // requests
    void Enqueue(ProvJob* job);
    void WorkerLoop();
    void SendStatus();
    void FetchConfig();
    void Schedule(UINT ms);
    UINT NextDelay() const;

    void HandleActivation(ProvJob* job);
    void HandleStatus(ProvJob* job);
    void HandleConfig(ProvJob* job);
    bool HandleCommonErrors(ProvJob* job); // revoked, blocked... true when handled
    void ManagementError(ProvJob* job);
    void ManagementOk();
    void SetBlocked(const CString& text);
    void ClearBlocked();
    void Deprovision(const CString& message);
    void NotifyActivation(bool ok, const CString& message);
    void QueueEvent(const char* type, const char* level, const CString& message);

    // state (UI thread)
    HWND m_mainWnd = NULL;
    CString m_storePath;
    Json::Value m_store;
    bool m_provisioned = false;
    ProvisionedConfig m_config;
    CString m_machineId;
    CString m_serverEtag;
    CString m_rejectedEtag;
    int m_rejectedRevision = 0;
    CString m_rejectedError;
    time_t m_lastContact = 0;
    int m_failures = 0;
    int m_pollInterval = 60;
    CString m_managementError;
    CString m_blockedText;
    bool m_blockAfterCall = false;
    CString m_lastRegState;
    int m_lastRegCode = -1;
    bool m_statusInFlight = false;
    bool m_configInFlight = false;
    time_t m_lastDiagnostics = 0;
    Json::Value m_events = Json::Value(Json::arrayValue);
    Json::Value m_update;

    // activation
    std::atomic<bool> m_activating{ false };
    HWND m_activationWnd = NULL;
    CString m_activationCode;
    CString m_activationEnv;
    CString m_activationServer;
    CString m_requestKey;
    bool m_activationFromBootstrap = false;

    // left by Init for Start (only the instance that stays open writes the file and the log)
    std::vector<CString> m_initLog;
    CString m_deleteStoreReason;
    bool m_saveStoreOnStart = false;
    void InitLog(const CString& message) { m_initLog.push_back(message); }

    // installer bootstrap (%ProgramData%\Upphone SIP\ativacao.ini)
    CString m_bootstrapFile;
    CString m_bootstrapCode;
    CString m_bootstrapServer;

    // worker
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<ProvJob*> m_queue;
    std::atomic<bool> m_stop{ false };
    bool m_started = false;
};

extern ProvisioningClient provisioning;
