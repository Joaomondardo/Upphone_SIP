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
#include "Provisioning.h"
#include "ProvisioningSupport.h"
#include "define.h"
#include "settings.h"
#include "mainDlg.h"
#include "langpack.h"

#define SECURITY_WIN32
#include <security.h>
#include <ctime>
#include <random>

#pragma comment(lib, "secur32.lib")

ProvisioningClient provisioning;

namespace {

using namespace Provisioning;

// Member of a JSON object without creating it (the non-const operator[] of jsoncpp inserts a null member).
const Json::Value& Get(const Json::Value& value, const char* key)
{
    return value[key];
}

// The jsoncpp of the project says isObject() (and isArray()) also for null: test the type itself.
bool IsObject(const Json::Value& value)
{
    return value.type() == Json::objectValue;
}

const int kRecoveryMinutes = 15;
const int kSchema = 1;
// codecs this build has (pjproject + bcg729, no Opus); the same list the server accepts
const TCHAR* const kAppCodecs[] = {
    _T("PCMA/8000/1"), _T("PCMU/8000/1"), _T("G729/8000/1"), _T("G722/16000/1"), _T("GSM/8000/1"),
    _T("iLBC/8000/1"), _T("speex/8000/1"), _T("speex/16000/1"), _T("speex/32000/1"),
};

time_t Now()
{
    return time(nullptr);
}

CString JStr(const Json::Value& value, const char* key)
{
    if (IsObject(value) && value.isMember(key) && value[key].isString()) {
        return FromUtf8(value[key].asString());
    }
    return CString();
}

int JInt(const Json::Value& value, const char* key, int fallback)
{
    if (IsObject(value) && value.isMember(key) && value[key].isIntegral()) {
        return value[key].asInt();
    }
    return fallback;
}

bool JBool(const Json::Value& value, const char* key, bool fallback)
{
    if (IsObject(value) && value.isMember(key) && value[key].isBool()) {
        return value[key].asBool();
    }
    return fallback;
}

// Times are kept as text: the jsoncpp of the project writes doubles with the decimal separator of the locale
// (a comma in Portuguese), which is not valid JSON, and its int has 32 bits.
time_t JTime(const Json::Value& value, const char* key)
{
    if (!IsObject(value) || !value.isMember(key)) {
        return 0;
    }
    const Json::Value& item = value[key];
    if (item.isString()) {
        return (time_t)_strtoi64(item.asString().c_str(), nullptr, 10);
    }
    return item.isIntegral() ? (time_t)item.asInt() : 0;
}

Json::Value TimeValue(time_t value)
{
    char buffer[32] = {};
    _i64toa_s((__int64)value, buffer, sizeof(buffer), 10);
    return Json::Value(std::string(buffer));
}

std::string WriteJson(const Json::Value& value)
{
    Json::FastWriter writer;
    return writer.write(value);
}

bool ValidDialCode(const CString& text)
{
    return text.GetLength() <= 20 && text.SpanIncluding(_T("0123456789*#+")) == text;
}

bool ValidAddress(const CString& text, bool required)
{
    if (text.IsEmpty()) {
        return !required;
    }
    return text.GetLength() <= 255 && text.FindOneOf(_T(" \t\r\n\"<>")) == -1;
}

int ActiveCalls()
{
    if (!mainDlg || !mainDlg->messagesDlg || !is_pjsua_running()) {
        return 0;
    }
    return mainDlg->messagesDlg->GetCallsCount();
}

// Registration of the account as the app sees it (the PBX is the official source).
void RegistrationState(bool blocked, CString& state, int& code, CString& reason)
{
    code = 0;
    reason.Empty();
    if (!is_pjsua_running() || !pjsua_acc_is_valid(account)) {
        state = blocked ? _T("disabled") : _T("unregistered");
        return;
    }
    pjsua_acc_info info;
    if (pjsua_acc_get_info(account, &info) != PJ_SUCCESS) {
        state = _T("unknown");
        return;
    }
    code = info.status;
    reason = MSIP::PjToStr(&info.status_text);
    if (!info.has_registration) {
        state = _T("unknown");
    }
    else if (code / 100 == 2 && info.expires > 0 && info.expires != PJSIP_EXPIRES_NOT_SPECIFIED) {
        state = _T("registered");
    }
    else if (code >= 300) {
        state = _T("failed");
    }
    else {
        state = _T("connecting");
    }
}

// Validates the configuration received from the server and converts it for the engine. A configuration this
// app cannot honor is refused (and reported), never applied partially.
bool ParseConfig(const Json::Value& config, ProvisionedConfig& out, CString& error)
{
    out = ProvisionedConfig();
    if (!IsObject(config)) {
        error = _T("configuração vazia");
        return false;
    }
    if (JInt(config, "schema", 0) != kSchema) {
        error = _T("versão da configuração não suportada por este app");
        return false;
    }
    const Json::Value& a = config["account"];
    const Json::Value& s = config["settings"];
    if (!IsObject(a) || !IsObject(s)) {
        error = _T("configuração incompleta");
        return false;
    }
    Account& account = out.account;
    account.label = JStr(a, "label");
    account.displayName = JStr(a, "display_name");
    account.username = JStr(a, "username");
    account.authID = JStr(a, "auth_id");
    account.password = JStr(a, "password");
    account.server = JStr(a, "server");
    account.proxy = JStr(a, "proxy");
    account.domain = JStr(a, "domain");
    account.transport = JStr(a, "transport");
    account.srtp = JStr(a, "srtp");
    account.publicAddr = JStr(a, "public_addr");
    account.registerRefresh = JInt(a, "register_refresh", 300);
    account.keepAlive = JInt(a, "keep_alive", 15);
    account.publish = JBool(a, "publish", true);
    account.ice = JBool(a, "ice", false);
    account.dialingPrefix = JStr(a, "dialing_prefix");
    account.dialPlan = JStr(a, "dial_plan");
    account.voicemailNumber = JStr(a, "voicemail_number");
    account.hideCID.Empty();
    account.allowRewrite = false;
    account.disableSessionTimer = false;
    account.rememberPassword = true;
    account.port = 0;
    if (account.domain.IsEmpty()) {
        account.domain = account.server;
    }
    if (account.label.IsEmpty()) {
        account.label = account.username;
    }

    if (account.username.IsEmpty() || account.username.GetLength() > 100 || account.username.FindOneOf(_T(" \t@:")) != -1) {
        error = _T("usuário SIP inválido");
        return false;
    }
    if (account.password.IsEmpty() || account.password.GetLength() > 128) {
        error = _T("senha SIP ausente");
        return false;
    }
    if (!ValidAddress(account.server, true) || !ValidAddress(account.domain, true) || !ValidAddress(account.publicAddr, false)) {
        error = _T("servidor SIP inválido");
        return false;
    }
    int pos = 0;
    CString proxy = account.proxy.Tokenize(_T(" "), pos);
    while (!proxy.IsEmpty()) {
        if (!ValidAddress(proxy, true)) {
            error = _T("proxy SIP inválido");
            return false;
        }
        proxy = account.proxy.Tokenize(_T(" "), pos);
    }
    if (account.transport != _T("udp") && account.transport != _T("tcp") && account.transport != _T("tls")) {
        error = _T("transporte SIP desconhecido");
        return false;
    }
#if !(defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0)
    if (account.transport == _T("tls")) {
        error = _T("esta versão do app não tem SIP sobre TLS");
        return false;
    }
#endif
    if (!account.srtp.IsEmpty() && account.srtp != _T("optional") && account.srtp != _T("mandatory")) {
        error = _T("modo SRTP desconhecido");
        return false;
    }
#if !(defined(PJMEDIA_HAS_SRTP) && PJMEDIA_HAS_SRTP != 0)
    if (!account.srtp.IsEmpty()) {
        error = _T("esta versão do app não tem SRTP");
        return false;
    }
#endif
    if (account.registerRefresh < 30 || account.registerRefresh > 3600 || account.keepAlive < 2 || account.keepAlive > 600) {
        error = _T("temporizadores SIP fora dos limites");
        return false;
    }
    if (!ValidDialCode(account.dialingPrefix) || !ValidDialCode(account.voicemailNumber) || account.dialPlan.GetLength() > 255) {
        error = _T("prefixo ou plano de discagem inválido");
        return false;
    }

    out.audioCodecs = JStr(s, "audio_codecs");
    int count = 0;
    pos = 0;
    CString codec = out.audioCodecs.Tokenize(_T(" "), pos);
    while (!codec.IsEmpty()) {
        bool known = false;
        for (const TCHAR* c : kAppCodecs) {
            known = known || codec == c;
        }
        if (!known) {
            error.Format(_T("codec não suportado por este app: %s"), codec.GetString());
            return false;
        }
        count++;
        codec = out.audioCodecs.Tokenize(_T(" "), pos);
    }
    if (!count) {
        error = _T("nenhum codec na configuração");
        return false;
    }
    out.dtmfMethod = JInt(s, "dtmf_method", 2);
    if (out.dtmfMethod < 0 || out.dtmfMethod > 3) {
        error = _T("método de DTMF desconhecido");
        return false;
    }
    out.stun = JStr(s, "stun");
    out.featureCodeAT = JStr(s, "feature_code_at");
    out.enableFeatureCodeAT = JBool(s, "enable_feature_code_at", false);
    out.echoTestNumber = JStr(s, "echo_test_number");
    out.debugLog = JBool(s, "debug_log", false);
    if (!ValidAddress(out.stun, false) || !ValidDialCode(out.featureCodeAT) || !ValidDialCode(out.echoTestNumber)) {
        error = _T("parâmetros de rede ou códigos de recurso inválidos");
        return false;
    }
    out.offlineGraceHours = max(0, JInt(config["management"], "offline_grace_hours", 0));
    out.clientName = JStr(config["client"], "name");
    out.userName = JStr(config["user"], "name");
    out.userEmail = JStr(config["user"], "email");
    out.supportContact = JStr(config["support"], "contact");
    out.revision = JInt(config, "revision", 0);
    out.valid = true;
    return true;
}

CString ServerMessage(const ProvJob* job)
{
    CString message = job->jsonOk ? JStr(job->json, "message") : CString();
    if (message.IsEmpty()) {
        if (job->response.status == 0) {
            message.Format(_T("Não foi possível falar com a Upp: %s."), job->response.error.GetString());
        }
        else {
            message.Format(_T("O servidor da Upp respondeu com erro %u."), job->response.status);
        }
    }
    return message;
}

CString ServerCode(const ProvJob* job)
{
    return job->jsonOk ? JStr(job->json, "error") : CString();
}

} // namespace

// ---------------------------------------------------------------------------------------------------- startup

void ProvisioningClient::Init()
{
    m_machineId = MachineId();
    m_storePath = accountSettings.pathRoaming + _T("provisioning.dat");
    LoadBootstrap();

    std::string data;
    if (!ProtectedRead(m_storePath, data)) {
        if (::PathFileExists(m_storePath)) {
            Log(_T("provisioning.dat não pôde ser lido (copiado de outro usuário ou computador?): ignorado"));
            ProtectedDelete(m_storePath);
        }
        return;
    }
    Json::Reader reader;
    Json::Value store;
    bool parsed = !data.empty() && reader.parse(data, store, false) && IsObject(store);
    if (!data.empty()) {
        SecureZeroMemory(&data[0], data.size());
    }
    if (!parsed) {
        Log(_T("provisioning.dat corrompido: ignorado"));
        return;
    }
    m_store = store;
    if (JStr(m_store, "device_token").IsEmpty()) {
        return; // only an interrupted activation (pending_activation)
    }
    CString storedMachine = JStr(m_store, "machine_id");
    if (!storedMachine.IsEmpty() && !m_machineId.IsEmpty() && storedMachine != m_machineId) {
        // the same Windows profile in another computer (cloned image): a new activation is needed
        Log(_T("provisioning.dat de outro computador (imagem clonada?): apagado, é preciso ativar de novo"));
        ProtectedDelete(m_storePath);
        m_store = Json::Value();
        return;
    }
    m_provisioned = true;
    m_lastContact = JTime(m_store, "last_contact");
    // Upphone: a restart must not lift a suspension received from the platform.
    m_blockedText = JStr(m_store, "blocked");
    m_update = Get(m_store, "update");
    m_pollInterval = max(30, JInt(m_store, "poll_interval", 60));
    const Json::Value& rejected = Get(m_store, "rejected");
    if (IsObject(rejected)) {
        m_rejectedEtag = JStr(rejected, "etag");
        m_rejectedRevision = JInt(rejected, "revision", 0);
        m_rejectedError = JStr(rejected, "error");
    }
    // a configuration received during a call in the previous session: no call is active now
    if (IsObject(Get(m_store, "pending"))) {
        Json::Value previousStore = m_store;
        Json::Value pending = Get(m_store, "pending");
        m_store["config"] = pending["config"];
        m_store["config_etag"] = pending["etag"];
        m_store["config_revision"] = pending["revision"];
        m_store.removeMember("pending");
        if (!SaveStore()) {
            m_store = previousStore;
        }
    }
    CString error;
    if (!ParseConfig(Get(m_store, "config"), m_config, error)) {
        Log(_T("configuração guardada não pode ser usada: ") + error);
    }
    // Upphone: check the offline limit before the first SIP registration, not only after a failed heartbeat.
    if (m_blockedText.IsEmpty() && m_config.valid && m_config.offlineGraceHours > 0 && m_lastContact > 0
        && Now() - m_lastContact > (time_t)m_config.offlineGraceHours * 3600) {
        CString text;
        text.Format(_T("Sem contato com a Upp há mais de %d horas. Confira a conexão com a internet."), m_config.offlineGraceHours);
        SetBlocked(text);
    }
    ApplySettingsOverrides(&accountSettings);
    ApplyAccount(&accountSettings);
    Log(_T("app iniciado com o ramal provisionado ") + m_config.account.label);
}

void ProvisioningClient::ApplySettingsOverrides(AccountSettings* settings) const
{
    if (!m_provisioned || !m_config.valid) {
        return;
    }
    settings->audioCodecs = m_config.audioCodecs;
    settings->DTMFMethod = m_config.dtmfMethod;
    settings->stun = m_config.stun;
    settings->enableSTUN = !m_config.stun.IsEmpty();
    settings->featureCodeAT = m_config.featureCodeAT;
    settings->enableFeatureCodeAT = m_config.enableFeatureCodeAT;
    settings->enableLog = m_config.debugLog;
}

void ProvisioningClient::ApplyAccount(AccountSettings* settings) const
{
    if (!m_provisioned || !m_config.valid) {
        return;
    }
    settings->accountId = 1;
    settings->account = m_config.account;
}

void ProvisioningClient::Start(HWND mainWnd)
{
    m_mainWnd = mainWnd;
    if (!m_started) {
        m_started = true;
        m_stop = false;
        m_thread = std::thread(&ProvisioningClient::WorkerLoop, this);
    }
    if (m_provisioned) {
        CString message;
        message.Format(_T("Upphone SIP %s iniciado"), _T(_GLOBAL_VERSION));
        QueueEvent("started", "info", message);
        Schedule(3000); // after the first registration attempt
    }
}

void ProvisioningClient::Shutdown()
{
    if (!m_started) {
        return;
    }
    ::KillTimer(m_mainWnd, IDT_TIMER_PROVISIONING);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
        for (ProvJob* job : m_queue) {
            delete job;
        }
        m_queue.clear();
    }
    m_cv.notify_all();
    // a request in progress may take up to its timeout: the process does not wait for it
    if (m_thread.joinable()) {
        m_thread.detach();
    }
    m_started = false;
}

bool ProvisioningClient::SaveStore(CString* error)
{
    m_store["schema"] = kSchema;
    std::string data = WriteJson(m_store);
    CString failure;
    bool ok = ProtectedWrite(m_storePath, data, &failure);
    SecureZeroMemory(&data[0], data.size());
    if (!ok) {
        Log(_T("não foi possível gravar provisioning.dat: ") + failure);
        if (error) {
            *error = failure;
        }
    }
    return ok;
}

void ProvisioningClient::LoadBootstrap()
{
    TCHAR programData[MAX_PATH] = {};
    if (!SHGetSpecialFolderPath(NULL, programData, CSIDL_COMMON_APPDATA, FALSE)) {
        return;
    }
    m_bootstrapFile.Format(_T("%s\\%s\\ativacao.ini"), programData, _T(_GLOBAL_NAME_NICE));
    if (!::PathFileExists(m_bootstrapFile)) {
        return;
    }
    TCHAR buffer[512] = {};
    GetPrivateProfileString(_T("Ativacao"), _T("usuario"), NULL, buffer, _countof(buffer), m_bootstrapFile);
    CString user = buffer;
    if (!user.IsEmpty()) {
        // the technician chose who activates: another Windows user ignores the code
        TCHAR name[256] = {};
        ULONG size = _countof(name);
        CString current;
        if (GetUserNameEx(NameSamCompatible, name, &size)) {
            current = name;
        }
        DWORD shortSize = _countof(name);
        CString shortName = GetUserName(name, &shortSize) ? name : _T("");
        if (user.CompareNoCase(current) != 0 && user.CompareNoCase(shortName) != 0) {
            Log(_T("código do instalador é de outro usuário do Windows: ignorado"));
            return;
        }
    }
    GetPrivateProfileString(_T("Ativacao"), _T("codigo"), NULL, buffer, _countof(buffer), m_bootstrapFile);
    CString code = NormalizeCode(buffer);
    SecureZeroMemory(buffer, sizeof(buffer));
    TCHAR done[64] = {};
    GetPrivateProfileString(_T("Provisioning"), _T("bootstrapDone"), NULL, done, _countof(done), accountSettings.iniFile);
    if (code.IsEmpty() || code.Right(4) == done) {
        return;
    }
    m_bootstrapCode = code;
    GetPrivateProfileString(_T("Ativacao"), _T("servidor"), NULL, buffer, _countof(buffer), m_bootstrapFile);
    m_bootstrapServer = buffer;
    m_bootstrapServer.Trim();
    Log(_T("código de ativação deixado pelo instalador: …") + code.Right(4));
}

void ProvisioningClient::ConsumeBootstrap()
{
    if (m_bootstrapCode.IsEmpty()) {
        return;
    }
    // without permission to delete the file, at least this user does not try the same code again
    if (!DeleteFile(m_bootstrapFile)) {
        WritePrivateProfileString(_T("Provisioning"), _T("bootstrapDone"), m_bootstrapCode.Right(4), accountSettings.iniFile);
    }
    m_bootstrapCode.Empty();
}

CString ProvisioningClient::PendingCode() const
{
    const Json::Value& pending = Get(m_store, "pending_activation");
    if (!m_provisioned && IsObject(pending) && JTime(pending, "started") > Now() - kRecoveryMinutes * 60) {
        return FormatCode(JStr(pending, "code"));
    }
    return m_bootstrapCode.IsEmpty() ? CString() : FormatCode(m_bootstrapCode);
}

CString ProvisioningClient::ServerFor(const CString& environment) const
{
    TCHAR buffer[512] = {};
    // 1. Upphone.ini [Provisioning] server= (portable version, development)
    GetPrivateProfileString(_T("Provisioning"), _T("server"), NULL, buffer, _countof(buffer), accountSettings.iniFile);
    CString server = buffer;
    // 2. installer: /SERVIDOR= (HKLM\Software\Upphone SIP, ProvisioningServer)
    if (server.IsEmpty()) {
        CRegKey key;
        CString path;
        path.Format(_T("Software\\%s"), _T(_GLOBAL_NAME_NICE));
        HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
        for (HKEY root : roots) {
            if (server.IsEmpty() && key.Open(root, path, KEY_READ) == ERROR_SUCCESS) {
                ULONG chars = _countof(buffer);
                if (key.QueryStringValue(_T("ProvisioningServer"), buffer, &chars) == ERROR_SUCCESS) {
                    server = buffer;
                }
                key.Close();
            }
        }
    }
    // 3. the code left by the installer
    if (server.IsEmpty()) {
        server = m_bootstrapServer;
    }
    // 4. the address of the environment of the code
    if (server.IsEmpty()) {
        if (environment == _T("prod")) {
            server = _T(_GLOBAL_PROVISIONING_PROD);
        }
        else if (environment == _T("homolog")) {
            server = _T(_GLOBAL_PROVISIONING_HOMOLOG);
        }
    }
    server.Trim();
    server.TrimRight(_T('/'));
    return server;
}

CString ProvisioningClient::Headers(bool withToken, LPCTSTR contentType) const
{
    CString headers;
    headers.Format(_T("Content-Type: %s\r\nAccept: application/json\r\nX-Request-ID: %s\r\nX-Upp-Machine: %s\r\n"),
        contentType, RandomHex(16).GetString(), m_machineId.GetString());
    if (withToken) {
        headers.AppendFormat(_T("Authorization: Bearer %s\r\n"), JStr(m_store, "device_token").GetString());
    }
    return headers;
}

// ---------------------------------------------------------------------------------------------------- activation

bool ProvisioningClient::StartActivation(const CString& code, HWND notifyWnd, CString& error)
{
    if (m_activating) {
        error = _T("A ativação já está em andamento.");
        return false;
    }
    if (m_provisioned) {
        error = _T("Este computador já está ativado. Use \"Sair da conta\" em Minha conta antes de ativar outro ramal.");
        return false;
    }
    CString normalized = NormalizeCode(code);
    if (normalized.IsEmpty()) {
        error = _T("O código tem 16 letras e números, no formato XXXX-XXXX-XXXX-XXXX.");
        return false;
    }
    if (!CodeChecksumOk(normalized)) {
        error = _T("Código digitado incorretamente. Confira os caracteres.");
        return false;
    }
    CString environment = CodeEnvironment(normalized);
    if (environment.IsEmpty()) {
        error = _T("Código inválido.");
        return false;
    }
    CString server = ServerFor(environment);
    if (server.IsEmpty()) {
        error.Format(_T("O servidor de %s não está configurado neste computador."), EnvironmentName(environment).GetString());
        return false;
    }
    CString reason;
    if (!IsAllowedServerUrl(server, &reason)) {
        error = _T("Servidor de provisionamento recusado: ") + reason + _T(".");
        return false;
    }
    // The same code of an activation whose answer was lost: the same key, so the server answers again with
    // the same device instead of refusing the code that was already used.
    const Json::Value& pending = Get(m_store, "pending_activation");
    if (IsObject(pending) && JStr(pending, "code") == normalized && JTime(pending, "started") > Now() - kRecoveryMinutes * 60
        && !JStr(pending, "request_key").IsEmpty()) {
        m_requestKey = JStr(pending, "request_key");
    }
    else {
        m_requestKey = RandomHex(32);
        Json::Value pendingActivation(Json::objectValue);
        pendingActivation["code"] = ToUtf8(normalized);
        pendingActivation["request_key"] = ToUtf8(m_requestKey);
        pendingActivation["started"] = TimeValue(Now());
        m_store = Json::Value(Json::objectValue);
        m_store["pending_activation"] = pendingActivation;
        SaveStore();
    }
    m_activationCode = normalized;
    m_activationEnv = environment;
    m_activationServer = server;
    m_activationWnd = notifyWnd;
    m_activationFromBootstrap = normalized == m_bootstrapCode;

    Json::Value device(Json::objectValue);
    device["hostname"] = ToUtf8(Hostname());
    device["os"] = ToUtf8(OsVersion());
    device["app_version"] = _GLOBAL_VERSION;
    device["arch"] = ToUtf8(Architecture());
    device["machine_id"] = ToUtf8(m_machineId);
    Json::Value capabilities(Json::arrayValue);
#if defined(PJMEDIA_HAS_SRTP) && PJMEDIA_HAS_SRTP != 0
    capabilities.append("srtp");
#endif
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0
    capabilities.append("tls");
#endif
    capabilities.append("g729");
    device["capabilities"] = capabilities;
    Json::Value body(Json::objectValue);
    body["code"] = ToUtf8(normalized);
    body["request_key"] = ToUtf8(m_requestKey);
    body["device"] = device;

    ProvJob* job = new ProvJob();
    job->type = PROV_JOB_ACTIVATE;
    job->method = _T("POST");
    job->url = server + _T("/api/v1/activation");
    job->headers = Headers(false);
    job->body = WriteJson(body);
    m_activating = true;
    Log(_T("ativação iniciada com o código …") + normalized.Right(4) + _T(" em ") + server);
    Enqueue(job);
    return true;
}

void ProvisioningClient::CancelActivationNotify(HWND notifyWnd)
{
    if (m_activationWnd == notifyWnd) {
        m_activationWnd = NULL;
    }
}

void ProvisioningClient::NotifyActivation(bool ok, const CString& message)
{
    HWND wnd = m_activationWnd;
    m_activationWnd = NULL;
    if (wnd && ::IsWindow(wnd)) {
        ::PostMessage(wnd, UM_PROVISIONING_ACTIVATION, ok ? 1 : 0, (LPARAM)new CString(message));
    }
    else if (mainDlg) {
        mainDlg->BaloonPopup(_T(_GLOBAL_NAME_NICE), message, ok ? NIIF_INFO : NIIF_WARNING);
    }
}

void ProvisioningClient::HandleActivation(ProvJob* job)
{
    m_activating = false;
    if (job->response.status == 201 && job->jsonOk) {
        const Json::Value& json = job->json;
        CString environment = JStr(json, "environment");
        if (environment != m_activationEnv) {
            Log(_T("ativação recusada: o servidor é de outro ambiente (") + environment + _T(")"));
            NotifyActivation(false, _T("O servidor respondeu de outro ambiente. Confira o servidor configurado neste computador."));
            return;
        }
        Json::Value store(Json::objectValue);
        store["server_url"] = ToUtf8(m_activationServer);
        store["environment"] = ToUtf8(environment);
        store["device_id"] = Get(json, "device_id");
        store["device_token"] = Get(json, "device_token");
        store["machine_id"] = ToUtf8(m_machineId);
        store["activated_at"] = TimeValue(Now());
        store["last_contact"] = TimeValue(Now());
        store["poll_interval"] = JInt(json, "poll_interval", 60);
        store["update"] = Get(json, "update");
        m_store = store;
        m_lastContact = Now();
        m_pollInterval = max(30, JInt(json, "poll_interval", 60));
        m_update = Get(json, "update");
        m_provisioned = true;
        CString failure;
        if (!SaveStore(&failure)) {
            // without DPAPI the credentials are not kept anywhere: the activation is undone
            ProvJob* logout = new ProvJob();
            logout->type = PROV_JOB_LOGOUT;
            logout->method = _T("POST");
            logout->url = m_activationServer + _T("/api/v1/device/logout");
            logout->headers = Headers(true);
            logout->timeoutMs = 10000;
            Enqueue(logout);
            m_provisioned = false;
            m_store = Json::Value();
            NotifyActivation(false, _T("Não foi possível proteger as credenciais neste computador (") + failure +
                _T("). A ativação foi desfeita; fale com o suporte da Upp."));
            return;
        }
        // the manual account of this computer, if any, gives way to the provisioned one
        for (int i = 1; i <= 10; i++) {
            accountSettings.AccountDelete(i);
        }
        if (m_activationFromBootstrap) {
            ConsumeBootstrap();
        }
        CString error;
        ApplyResult result = ApplyConfig(Get(json, "config"), JStr(json, "config_etag"), JInt(json, "config_revision", 0), error);
        Log(_T("ativado: dispositivo ") + JStr(json, "device_id"));
        Schedule(1500);
        CString message;
        if (result == APPLY_REJECTED) {
            message = _T("O computador foi ativado, mas a configuração do ramal não é suportada por esta versão do app (") +
                error + _T("). A Upp já foi avisada.");
            NotifyActivation(false, message);
        }
        else {
            message.Format(_T("Ramal %s ativado neste computador."), m_config.account.label.GetString());
            NotifyActivation(true, message);
        }
        if (mainDlg) {
            mainDlg->PostMessage(UM_PROVISIONING_UPDATE, 0, 0);
        }
        return;
    }
    CString code = ServerCode(job);
    CString message = ServerMessage(job);
    if (job->response.status == 429) {
        message = _T("Muitas tentativas de ativação. Aguarde um minuto e tente de novo.");
    }
    if (!code.IsEmpty()) {
        message.AppendFormat(_T(" (código UPP-%s)"), code.GetString());
    }
    else if (job->response.status == 0) {
        message.AppendFormat(_T(" (%s)"), job->response.supportCode.GetString());
    }
    Log(_T("ativação recusada: ") + message);
    // a definitive answer: the code is not tried again (the key of a lost answer stays for the network errors)
    if (job->response.status >= 400 && job->response.status < 500 && job->response.status != 429) {
        m_store.removeMember("pending_activation");
        ProtectedDelete(m_storePath);
        if (m_activationFromBootstrap) {
            ConsumeBootstrap();
        }
    }
    NotifyActivation(false, message);
}

// ---------------------------------------------------------------------------------------------------- configuration

ProvisioningClient::ApplyResult ProvisioningClient::ApplyConfig(const Json::Value& config, const CString& etag, int revision, CString& error)
{
    ProvisionedConfig parsed;
    if (!ParseConfig(config, parsed, error)) {
        m_rejectedEtag = etag;
        m_rejectedRevision = revision;
        m_rejectedError = error;
        Json::Value rejected(Json::objectValue);
        rejected["etag"] = ToUtf8(etag);
        rejected["revision"] = revision;
        rejected["error"] = ToUtf8(error);
        m_store["rejected"] = rejected;
        SaveStore();
        Log(_T("configuração recusada: ") + error);
        return APPLY_REJECTED;
    }
    if (JStr(config, "environment") != JStr(m_store, "environment")) {
        error = _T("configuração de outro ambiente");
        m_rejectedEtag = etag;
        m_rejectedRevision = revision;
        m_rejectedError = error;
        Log(_T("configuração recusada: ") + error);
        return APPLY_REJECTED;
    }
    bool accountChanged = !m_config.valid || accountSettings.account != parsed.account || accountSettings.accountId != 1;
    bool codecsChanged = accountSettings.audioCodecs != parsed.audioCodecs;
    bool restartStack = accountSettings.stun != parsed.stun;
    bool logChanged = accountSettings.enableLog != parsed.debugLog;
    bool restarts = accountChanged || codecsChanged || restartStack;
    if (restarts && ActiveCalls() > 0) {
        // the change restarts the account or the audio: it waits for the end of the call
        Json::Value pending(Json::objectValue);
        pending["config"] = config;
        pending["etag"] = ToUtf8(etag);
        pending["revision"] = revision;
        Json::Value previousStore = m_store;
        m_store["pending"] = pending;
        if (!SaveStore()) {
            m_store = previousStore;
            error = _T("não foi possível gravar a configuração pendente neste computador");
            return APPLY_REJECTED;
        }
        Log(_T("configuração recebida durante uma chamada: aplicada ao fim da chamada"));
        return APPLY_PENDING;
    }
    // Upphone: keep the applied revision unchanged if the protected write fails. The next heartbeat must
    // report the configuration still running and fetch this revision again, rather than acknowledge it.
    Json::Value previousStore = m_store;
    m_store["config"] = config;
    m_store["config_etag"] = ToUtf8(etag);
    m_store["config_revision"] = revision;
    m_store.removeMember("pending");
    m_store.removeMember("rejected");
    if (!SaveStore()) {
        m_store = previousStore;
        error = _T("não foi possível gravar a configuração neste computador");
        return APPLY_REJECTED;
    }
    m_rejectedEtag.Empty();
    m_rejectedError.Empty();
    m_config = parsed;
    ApplyToEngine(parsed, restartStack, codecsChanged, logChanged, accountChanged);
    CString text;
    text.Format(_T("configuração (revisão %d) aplicada"), revision);
    Log(text);
    return APPLY_DONE;
}

void ProvisioningClient::ApplyToEngine(const ProvisionedConfig& config, bool restartStack, bool codecsChanged, bool logChanged, bool accountChanged)
{
    ApplySettingsOverrides(&accountSettings);
    if (!mainDlg || !is_pjsua_running()) {
        ApplyAccount(&accountSettings);
        return;
    }
    if (restartStack) {
        // STUN is part of the global SIP configuration: the stack restarts (never during a call)
        mainDlg->PJDestroy();
        ApplyAccount(&accountSettings);
        mainDlg->PJCreate();
        if (!IsServiceBlocked()) {
            mainDlg->PJAccountAdd();
        }
        mainDlg->OnAccountChanged();
        return;
    }
    if (codecsChanged) {
        mainDlg->PJAudioCodecs();
    }
    if (logChanged) {
        ReconfigureLog(config.debugLog);
    }
    if (accountChanged || !pjsua_acc_is_valid(account)) {
        mainDlg->PJAccountDelete(true);
        ApplyAccount(&accountSettings);
        if (!IsServiceBlocked()) {
            mainDlg->PJAccountAdd();
        }
        mainDlg->OnAccountChanged();
    }
}

void ProvisioningClient::ReconfigureLog(bool enable)
{
    pjsua_logging_config cfg;
    pjsua_logging_config_default(&cfg);
    cfg.decor |= PJ_LOG_HAS_CR;
    char* buffer = nullptr;
    if (enable) {
        buffer = MSIP::WideCharToPjStr(accountSettings.logFile);
        cfg.log_filename = pj_str(buffer);
    }
    pjsua_reconfigure_logging(&cfg);
    free(buffer);
    Log(enable ? _T("log SIP detalhado ligado pela Upp") : _T("log SIP detalhado desligado"));
}

void ProvisioningClient::OnCallsEnded()
{
    if (!m_provisioned || ActiveCalls() > 0) {
        return;
    }
    if (m_blockAfterCall) {
        m_blockAfterCall = false;
        if (IsServiceBlocked() && mainDlg) {
            mainDlg->PJAccountDelete(true);
            mainDlg->UpdateWindowText();
        }
    }
    if (IsObject(Get(m_store, "pending"))) {
        Json::Value pending = Get(m_store, "pending");
        CString error;
        ApplyConfig(Get(pending, "config"), JStr(pending, "etag"), JInt(pending, "revision", 0), error);
        Schedule(1000);
    }
}

void ProvisioningClient::RefreshConfig()
{
    if (!m_provisioned) {
        return;
    }
    m_rejectedEtag.Empty(); // asked by the user: try again even a configuration refused before
    FetchConfig();
}

// ---------------------------------------------------------------------------------------------------- heartbeat

void ProvisioningClient::Schedule(UINT ms)
{
    if (m_mainWnd) {
        ::SetTimer(m_mainWnd, IDT_TIMER_PROVISIONING, ms, NULL);
    }
}

UINT ProvisioningClient::NextDelay() const
{
    static std::mt19937 random((unsigned)GetTickCount());
    // up to 20% of random variation: thousands of computers do not call the server at the same second
    std::uniform_int_distribution<int> jitter(80, 120);
    double seconds = m_pollInterval;
    if (m_failures > 0) {
        seconds = min(30.0 * (1 << min(m_failures - 1, 5)), 600.0);
    }
    return (UINT)(seconds * 1000 * jitter(random) / 100);
}

void ProvisioningClient::OnTimer()
{
    ::KillTimer(m_mainWnd, IDT_TIMER_PROVISIONING);
    if (!m_provisioned) {
        return;
    }
    OnCallsEnded();
    if (!m_statusInFlight) {
        SendStatus();
    }
    Schedule(NextDelay());
}

void ProvisioningClient::OnRegistrationChanged()
{
    if (!m_provisioned) {
        return;
    }
    CString state;
    int code;
    CString reason;
    RegistrationState(IsServiceBlocked(), state, code, reason);
    if (state != m_lastRegState || (state == _T("failed") && code != m_lastRegCode)) {
        Schedule(1500); // tells the server soon
    }
}

void ProvisioningClient::ReportEvent(const char* type, const char* level, const CString& message)
{
    if (m_provisioned) {
        QueueEvent(type, level, message);
        Log(CString(type) + _T(": ") + message);
    }
}

void ProvisioningClient::QueueEvent(const char* type, const char* level, const CString& message)
{
    if (m_events.size() >= 20) {
        return;
    }
    Json::Value item(Json::objectValue);
    item["type"] = type;
    item["level"] = level;
    item["message"] = ToUtf8(message);
    m_events.append(item);
}

void ProvisioningClient::SendStatus()
{
    Json::Value body(Json::objectValue);
    body["app_version"] = _GLOBAL_VERSION;
    body["os"] = ToUtf8(OsVersion());
    body["hostname"] = ToUtf8(Hostname());
    body["arch"] = ToUtf8(Architecture());
    Json::Value report(Json::objectValue);
    const Json::Value& pending = Get(m_store, "pending");
    if (IsObject(pending)) {
        report["revision"] = Get(pending, "revision");
        report["etag"] = Get(pending, "etag");
        report["state"] = "pending";
        report["error"] = "aguardando o fim da chamada";
    }
    else if (!m_rejectedEtag.IsEmpty()) {
        report["revision"] = m_rejectedRevision;
        report["etag"] = ToUtf8(m_rejectedEtag);
        report["state"] = "rejected";
        report["error"] = ToUtf8(m_rejectedError);
    }
    else if (m_config.valid) {
        report["revision"] = JInt(m_store, "config_revision", 0);
        report["etag"] = Get(m_store, "config_etag");
        report["state"] = "applied";
    }
    body["config"] = report;
    CString state;
    int code;
    CString reason;
    RegistrationState(IsServiceBlocked(), state, code, reason);
    m_lastRegState = state;
    m_lastRegCode = code;
    Json::Value registration(Json::objectValue);
    registration["state"] = ToUtf8(state);
    registration["code"] = code;
    registration["reason"] = ToUtf8(reason);
    body["registration"] = registration;
    body["in_call"] = ActiveCalls() > 0;
    body["events"] = m_events;
    m_events = Json::Value(Json::arrayValue);

    ProvJob* job = new ProvJob();
    job->type = PROV_JOB_STATUS;
    job->method = _T("POST");
    job->url = JStr(m_store, "server_url") + _T("/api/v1/device/status");
    job->headers = Headers(true);
    job->body = WriteJson(body);
    m_statusInFlight = true;
    Enqueue(job);
}

void ProvisioningClient::FetchConfig()
{
    if (m_configInFlight) {
        return;
    }
    ProvJob* job = new ProvJob();
    job->type = PROV_JOB_CONFIG;
    job->method = _T("GET");
    job->url = JStr(m_store, "server_url") + _T("/api/v1/device/config");
    job->headers = Headers(true);
    m_configInFlight = true;
    Enqueue(job);
}

void ProvisioningClient::OnResult(ProvJob* job)
{
    switch (job->type) {
    case PROV_JOB_ACTIVATE:
        HandleActivation(job);
        break;
    case PROV_JOB_STATUS:
        m_statusInFlight = false;
        if (m_provisioned) {
            HandleStatus(job);
        }
        break;
    case PROV_JOB_CONFIG:
        m_configInFlight = false;
        if (m_provisioned) {
            HandleConfig(job);
        }
        break;
    case PROV_JOB_LOGS:
        if (job->response.status == 201) {
            Log(_T("logs técnicos enviados à Upp"));
            if (job->userInitiated && mainDlg) {
                mainDlg->BaloonPopup(_T(_GLOBAL_NAME_NICE), _T("Diagnóstico enviado ao suporte da Upp."), NIIF_INFO);
            }
        }
        else if (job->userInitiated && mainDlg) {
            mainDlg->BaloonPopup(_T(_GLOBAL_NAME_NICE), _T("Não foi possível enviar o diagnóstico: ") + ServerMessage(job), NIIF_WARNING);
        }
        break;
    case PROV_JOB_LOGOUT:
        break;
    }
    // the job carried the token and maybe the configuration with the SIP password
    if (!job->body.empty()) {
        SecureZeroMemory(&job->body[0], job->body.size());
    }
    if (!job->response.body.empty()) {
        SecureZeroMemory(&job->response.body[0], job->response.body.size());
    }
    delete job;
}

bool ProvisioningClient::HandleCommonErrors(ProvJob* job)
{
    CString code = ServerCode(job);
    DWORD status = job->response.status;
    if (status == 401 && (code == _T("device_revoked") || code == _T("device_logged_out") || code == _T("identity_mismatch"))) {
        Log(_T("dispositivo desconectado pela Upp: ") + code);
        Deprovision(ServerMessage(job));
        return true;
    }
    if (status == 403 && (code == _T("device_blocked") || code == _T("extension_disabled") || code == _T("client_suspended"))) {
        m_lastContact = Now();
        m_store["last_contact"] = TimeValue(m_lastContact);
        m_failures = 0;
        m_managementError.Empty();
        SetBlocked(ServerMessage(job));
        return true;
    }
    return false;
}

void ProvisioningClient::ManagementError(ProvJob* job)
{
    m_failures++;
    CString code = ServerCode(job);
    m_managementError = ServerMessage(job);
    if (!code.IsEmpty()) {
        m_managementError.AppendFormat(_T(" (UPP-%s)"), code.GetString());
    }
    else {
        m_managementError.AppendFormat(_T(" (%s)"), job->response.supportCode.GetString());
    }
    if (m_failures == 1 || m_failures % 10 == 0) {
        Log(_T("falha ao falar com a Upp: ") + m_managementError);
    }
    // without contact for longer than the profile allows: the account stops until the contact comes back
    if (m_config.offlineGraceHours > 0 && m_lastContact > 0 && Now() - m_lastContact > (time_t)m_config.offlineGraceHours * 3600) {
        CString text;
        text.Format(_T("Sem contato com a Upp há mais de %d horas. Confira a conexão com a internet."), m_config.offlineGraceHours);
        SetBlocked(text);
    }
}

void ProvisioningClient::ManagementOk()
{
    m_lastContact = Now();
    m_store["last_contact"] = TimeValue(m_lastContact);
    m_failures = 0;
    m_managementError.Empty();
    ClearBlocked();
}

void ProvisioningClient::HandleStatus(ProvJob* job)
{
    if (HandleCommonErrors(job)) {
        return;
    }
    if (job->response.status != 200 || !job->jsonOk) {
        ManagementError(job);
        return;
    }
    const Json::Value& json = job->json;
    ManagementOk();
    m_pollInterval = max(30, JInt(json, "poll_interval", m_pollInterval));
    m_store["poll_interval"] = m_pollInterval;
    // token rotation: kept before it is used; the server accepts both until the new one arrives
    CString rotate = JStr(json, "rotate_token");
    bool save = false;
    if (!rotate.IsEmpty()) {
        Json::Value previousToken = Get(m_store, "device_token");
        m_store["device_token"] = ToUtf8(rotate);
        // Upphone: the server invalidates the previous token on first use of the new one. Never use the
        // replacement until it is durable, or the next app start would only have an invalid token.
        if (SaveStore()) {
            Log(_T("token do dispositivo renovado"));
        }
        else {
            m_store["device_token"] = previousToken;
            QueueEvent("error", "error", _T("Não foi possível guardar o novo token; o token anterior continua em uso."));
        }
    }
    // version of the app
    const Json::Value& update = Get(json, "update");
    if (WriteJson(update) != WriteJson(m_update)) {
        m_update = update;
        m_store["update"] = update;
        save = true;
        if (mainDlg) {
            mainDlg->PostMessage(UM_PROVISIONING_UPDATE, 0, 0);
        }
    }
    if (save || Now() - JTime(m_store, "saved_contact") > 3600) {
        m_store["saved_contact"] = TimeValue(Now());
        SaveStore();
    }
    // configuration changed in the panel
    m_serverEtag = JStr(json, "config_etag");
    CString applied = JStr(m_store, "config_etag");
    CString pending = JStr(Get(m_store, "pending"), "etag");
    if (!m_serverEtag.IsEmpty() && m_serverEtag != applied && m_serverEtag != pending && m_serverEtag != m_rejectedEtag) {
        FetchConfig();
    }
    else if (m_serverEtag == applied && !m_rejectedEtag.IsEmpty()) {
        m_rejectedEtag.Empty(); // the server went back to a configuration that works
        m_store.removeMember("rejected");
        SaveStore();
    }
    if (JBool(Get(json, "actions"), "upload_logs", false)) {
        ProvJob* logs = new ProvJob();
        logs->type = PROV_JOB_LOGS;
        logs->method = _T("POST");
        logs->url = JStr(m_store, "server_url") + _T("/api/v1/device/logs");
        logs->headers = Headers(true, _T("text/plain; charset=utf-8"));
        logs->collectLogs = true;
        Enqueue(logs);
    }
}

void ProvisioningClient::HandleConfig(ProvJob* job)
{
    if (HandleCommonErrors(job)) {
        return;
    }
    if (job->response.status != 200 || !job->jsonOk) {
        ManagementError(job);
        return;
    }
    ManagementOk();
    const Json::Value& json = job->json;
    CString error;
    ApplyResult result = ApplyConfig(Get(json, "config"), JStr(json, "config_etag"), JInt(json, "config_revision", 0), error);
    if (result == APPLY_REJECTED) {
        QueueEvent("config_error", "error", error);
    }
    Schedule(1000); // reports applied / pending / refused
}

void ProvisioningClient::SetBlocked(const CString& text)
{
    if (m_blockedText == text) {
        return;
    }
    m_blockedText = text;
    m_store["blocked"] = ToUtf8(text);
    SaveStore();
    Log(_T("uso do ramal suspenso: ") + text);
    if (!mainDlg) {
        return;
    }
    if (ActiveCalls() > 0) {
        m_blockAfterCall = true; // a call in progress is not cut
    }
    else {
        mainDlg->PJAccountDelete(true);
    }
    mainDlg->UpdateWindowText();
    mainDlg->BaloonPopup(_T(_GLOBAL_NAME_NICE), text, NIIF_WARNING);
}

void ProvisioningClient::ClearBlocked()
{
    if (m_blockedText.IsEmpty()) {
        return;
    }
    // Upphone: only lift the persisted suspension after its removal has also been saved.
    m_store.removeMember("blocked");
    if (!SaveStore()) {
        m_store["blocked"] = ToUtf8(m_blockedText);
        return;
    }
    m_blockedText.Empty();
    m_blockAfterCall = false;
    Log(_T("uso do ramal liberado de novo"));
    if (mainDlg && is_pjsua_running() && m_config.valid && !pjsua_acc_is_valid(account)) {
        ApplyAccount(&accountSettings);
        mainDlg->PJAccountAdd();
        mainDlg->UpdateWindowText();
    }
}

// ---------------------------------------------------------------------------------------------------- logout

void ProvisioningClient::RequestLogout()
{
    if (!m_provisioned) {
        return;
    }
    ProvJob* job = new ProvJob();
    job->type = PROV_JOB_LOGOUT;
    job->method = _T("POST");
    job->url = JStr(m_store, "server_url") + _T("/api/v1/device/logout");
    job->headers = Headers(true);
    job->timeoutMs = 10000;
    Enqueue(job);
    Log(_T("o usuário saiu da conta"));
    Deprovision(_T("Você saiu da conta. Para usar o Upphone de novo, informe um código de ativação."));
}

void ProvisioningClient::Deprovision(const CString& message)
{
    ProtectedDelete(m_storePath);
    bool hadStun = !accountSettings.stun.IsEmpty();
    bool hadLog = accountSettings.enableLog;
    m_provisioned = false;
    m_config = ProvisionedConfig();
    m_store = Json::Value();
    m_update = Json::Value();
    m_blockedText.Empty();
    m_blockAfterCall = false;
    m_managementError.Empty();
    m_rejectedEtag.Empty();
    m_failures = 0;
    if (m_mainWnd) {
        ::KillTimer(m_mainWnd, IDT_TIMER_PROVISIONING);
    }
    // back to the fixed settings of the app, without an account
    accountSettings.ResetProvisionedFields();
    accountSettings.accountId = 0;
    accountSettings.account = Account();
    accountSettings.SettingsSave();
    if (mainDlg && is_pjsua_running()) {
        mainDlg->PJAccountDelete(true);
        if (hadStun) {
            mainDlg->PJDestroy();
            mainDlg->PJCreate();
        }
        else {
            mainDlg->PJAudioCodecs();
            if (hadLog) {
                ReconfigureLog(false);
            }
        }
        mainDlg->InitUI();
        mainDlg->OnAccountChanged();
        mainDlg->PostMessage(UM_PROVISIONING_DEPROVISIONED, 0, (LPARAM)new CString(message));
    }
}

bool ProvisioningClient::SendDiagnostics(CString& message)
{
    if (!m_provisioned) {
        message = _T("O diagnóstico é enviado só por um computador ativado.");
        return false;
    }
    if (Now() - m_lastDiagnostics < 60) {
        message = _T("Aguarde um minuto para enviar de novo.");
        return false;
    }
    m_lastDiagnostics = Now();
    ProvJob* job = new ProvJob();
    job->type = PROV_JOB_LOGS;
    job->method = _T("POST");
    job->url = JStr(m_store, "server_url") + _T("/api/v1/device/logs");
    job->headers = Headers(true, _T("text/plain; charset=utf-8"));
    job->collectLogs = true;
    job->userInitiated = true;
    Enqueue(job);
    message = _T("Enviando o diagnóstico...");
    return true;
}

// ---------------------------------------------------------------------------------------------------- worker

void ProvisioningClient::Enqueue(ProvJob* job)
{
    if (!m_started) {
        delete job;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(job);
    }
    m_cv.notify_one();
}

void ProvisioningClient::WorkerLoop()
{
    for (;;) {
        ProvJob* job = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
            if (m_stop) {
                return;
            }
            job = m_queue.front();
            m_queue.pop_front();
        }
        if (job->collectLogs) {
            job->body = CollectLogs(768 * 1024);
        }
        job->response = HttpRequest(job->method, job->url, job->headers, job->body, job->timeoutMs);
        if (!job->response.body.empty()) {
            Json::Reader reader;
            job->jsonOk = reader.parse(job->response.body, job->json, false) && IsObject(job->json);
        }
        if (m_stop || !m_mainWnd || !::PostMessage(m_mainWnd, UM_PROVISIONING_RESULT, 0, (LPARAM)job)) {
            delete job;
        }
    }
}

// ---------------------------------------------------------------------------------------------------- info

ProvisioningInfo ProvisioningClient::GetInfo() const
{
    ProvisioningInfo info;
    info.provisioned = m_provisioned;
    info.config = m_config;
    info.config.account.password.Empty();
    info.environment = JStr(m_store, "environment");
    info.server = JStr(m_store, "server_url");
    info.deviceId = JStr(m_store, "device_id");
    info.activatedAt = JTime(m_store, "activated_at");
    info.lastContact = m_lastContact;
    info.blockedText = m_blockedText;
    CString when;
    if (m_lastContact) {
        when = CTime(m_lastContact).Format(_T("%d/%m/%Y %H:%M"));
    }
    if (!m_managementError.IsEmpty()) {
        info.managementLevel = 2;
        info.managementText = m_managementError;
        if (!when.IsEmpty()) {
            info.managementText += _T(" Último contato: ") + when + _T(".");
        }
    }
    else if (m_lastContact) {
        info.managementText = _T("Conectado. Último contato: ") + when + _T(".");
    }
    else {
        info.managementText = _T("Aguardando o primeiro contato.");
        info.managementLevel = 1;
    }
    if (!m_rejectedEtag.IsEmpty()) {
        info.configLevel = 2;
        info.configText.Format(_T("Revisão %d recusada: %s."), m_rejectedRevision, m_rejectedError.GetString());
    }
    else if (IsObject(Get(m_store, "pending"))) {
        info.configLevel = 1;
        info.configText.Format(_T("Revisão %d recebida; será aplicada ao fim da chamada."), JInt(Get(m_store, "pending"), "revision", 0));
    }
    else if (m_config.valid) {
        info.configText.Format(_T("Revisão %d aplicada."), JInt(m_store, "config_revision", 0));
    }
    else {
        info.configLevel = 2;
        info.configText = _T("Sem configuração válida.");
    }
    return info;
}
