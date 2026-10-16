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

#include <string>

namespace Provisioning {

struct HttpResponse {
    DWORD status = 0;       // HTTP status; 0 when the server could not be reached
    std::string body;       // UTF-8
    CString error;          // network error, when status is 0
    CString supportCode;    // short code for the support (UPP-NET-12029...)
};

// Synchronous HTTPS request, for the provisioning worker thread (never the UI thread).
// The server certificate is always validated (no exceptions), the Windows proxy settings are used and
// redirects are not followed. Plain http is accepted only for the local computer (development).
HttpResponse HttpRequest(const CString& method, const CString& url, const CString& headers,
    const std::string& body, DWORD timeoutMs = 30000);

// https:// (or http://localhost for development). reason: why it is not accepted.
bool IsAllowedServerUrl(const CString& url, CString* reason = nullptr);

} // namespace Provisioning
