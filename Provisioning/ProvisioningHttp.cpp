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
#include "ProvisioningHttp.h"
#include "define.h"

#include <wininet.h>
#include <vector>

#pragma comment(lib, "wininet.lib")

// WinINet, the API the rest of the app uses (winhttp.h cannot be included together with afxinet.h, which the
// precompiled header brings). The server only accepts TLS 1.2 or newer.

namespace Provisioning {

static const size_t kMaxResponse = 4 * 1024 * 1024;

static bool IsLoopback(const CString& host)
{
    return host.CompareNoCase(_T("localhost")) == 0 || host == _T("127.0.0.1") || host == _T("::1") || host == _T("[::1]");
}

struct UrlParts {
    INTERNET_SCHEME scheme = INTERNET_SCHEME_HTTPS;
    CString host;
    INTERNET_PORT port = 0;
    CString path;
};

static bool CrackUrl(const CString& url, UrlParts& parts)
{
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    TCHAR host[256] = {};
    TCHAR path[2048] = {};
    TCHAR extra[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = _countof(path);
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = _countof(extra);
    if (!InternetCrackUrl(url, 0, 0, &uc)) {
        return false;
    }
    parts.scheme = uc.nScheme;
    parts.host = host;
    parts.port = uc.nPort;
    parts.path = CString(path) + extra;
    if (parts.path.IsEmpty()) {
        parts.path = _T("/");
    }
    return !parts.host.IsEmpty();
}

bool IsAllowedServerUrl(const CString& url, CString* reason)
{
    UrlParts parts;
    if (!CrackUrl(url, parts)) {
        if (reason) {
            *reason = _T("endereço do servidor inválido");
        }
        return false;
    }
    if (parts.scheme == INTERNET_SCHEME_HTTPS || (parts.scheme == INTERNET_SCHEME_HTTP && IsLoopback(parts.host))) {
        return true;
    }
    if (reason) {
        *reason = _T("o servidor de provisionamento precisa usar https://");
    }
    return false;
}

static CString DescribeError(DWORD error)
{
    switch (error) {
    case ERROR_INTERNET_NAME_NOT_RESOLVED:
        return _T("servidor não encontrado (DNS)");
    case ERROR_INTERNET_CANNOT_CONNECT:
    case ERROR_INTERNET_CONNECTION_ABORTED:
    case ERROR_INTERNET_CONNECTION_RESET:
        return _T("não foi possível conectar ao servidor");
    case ERROR_INTERNET_TIMEOUT:
        return _T("o servidor não respondeu a tempo");
    case ERROR_INTERNET_INVALID_CA:
    case ERROR_INTERNET_SEC_CERT_CN_INVALID:
    case ERROR_INTERNET_SEC_CERT_DATE_INVALID:
    case ERROR_INTERNET_SEC_CERT_ERRORS:
    case ERROR_INTERNET_SEC_INVALID_CERT:
    case ERROR_INTERNET_SEC_CERT_REVOKED:
    case ERROR_INTERNET_SECURITY_CHANNEL_ERROR:
        return _T("certificado do servidor inválido (conexão recusada por segurança)");
    case ERROR_INTERNET_PROXY_SERVER_UNREACHABLE:
    case ERROR_INTERNET_BAD_AUTO_PROXY_SCRIPT:
    case ERROR_INTERNET_UNABLE_TO_DOWNLOAD_SCRIPT:
        return _T("erro na configuração de proxy do Windows");
    }
    CString text;
    text.Format(_T("erro de rede %u"), error);
    return text;
}

HttpResponse HttpRequest(const CString& method, const CString& url, const CString& headers,
    const std::string& body, DWORD timeoutMs)
{
    HttpResponse response;
    UrlParts parts;
    if (!CrackUrl(url, parts)) {
        response.error = _T("endereço do servidor inválido");
        response.supportCode = _T("UPP-NET-URL");
        return response;
    }
    bool secure = parts.scheme == INTERNET_SCHEME_HTTPS;
    if (!secure && !IsLoopback(parts.host)) {
        response.error = _T("o servidor de provisionamento precisa usar https://");
        response.supportCode = _T("UPP-NET-HTTP");
        return response;
    }

    CString userAgent;
    userAgent.Format(_T("Upphone-SIP/%s"), _T(_GLOBAL_VERSION));
    // the proxy settings of Windows (including automatic configuration)
    HINTERNET session = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!session) {
        DWORD error = GetLastError();
        response.error = DescribeError(error);
        response.supportCode.Format(_T("UPP-NET-%u"), error);
        return response;
    }
    DWORD connectTimeout = 10000;
    InternetSetOption(session, INTERNET_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
    InternetSetOption(session, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOption(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    HINTERNET connect = InternetConnect(session, parts.host, parts.port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    // no INTERNET_FLAG_IGNORE_CERT_*: certificate errors are never ignored; no redirects (they could take the
    // token to another place); no cookies, cache or dialogs
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE
        | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_AUTH
        | (secure ? INTERNET_FLAG_SECURE : 0);
    LPCTSTR accept[] = { _T("application/json"), _T("*/*"), NULL };
    HINTERNET request = connect ? HttpOpenRequest(connect, method, parts.path, NULL, NULL, accept, flags, 0) : NULL;
    BOOL ok = request != NULL;
    if (ok) {
        ok = HttpSendRequest(request, headers.IsEmpty() ? NULL : (LPCTSTR)headers, headers.IsEmpty() ? 0 : (DWORD)-1L,
            body.empty() ? NULL : (LPVOID)body.data(), (DWORD)body.size());
    }
    if (!ok) {
        DWORD error = GetLastError();
        response.error = DescribeError(error);
        response.supportCode.Format(_T("UPP-NET-%u"), error);
    }
    else {
        DWORD status = 0;
        DWORD size = sizeof(status);
        HttpQueryInfo(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size, NULL);
        std::vector<char> buffer(16 * 1024);
        for (;;) {
            DWORD read = 0;
            if (!InternetReadFile(request, buffer.data(), (DWORD)buffer.size(), &read) || read == 0) {
                break;
            }
            response.body.append(buffer.data(), read);
            if (response.body.size() > kMaxResponse) {
                break;
            }
        }
        response.status = status;
        response.supportCode.Format(_T("UPP-HTTP-%u"), status);
    }
    if (request) {
        InternetCloseHandle(request);
    }
    if (connect) {
        InternetCloseHandle(connect);
    }
    InternetCloseHandle(session);
    return response;
}

} // namespace Provisioning
