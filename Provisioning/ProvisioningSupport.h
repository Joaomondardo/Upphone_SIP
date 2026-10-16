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

// Upphone: helpers of the provisioning module (Provisioning.h): activation codes, identity of the computer,
// files protected with DPAPI and the technical log (always without secrets).
namespace Provisioning {

// Activation code: 16 characters of the Crockford base32 alphabet (no I, L, O, U). The first one is the
// environment (P production, H homologation, D development) and the last one is a check character.
// Returns the 16 characters in upper case, or an empty string when the format is wrong.
CString NormalizeCode(const CString& code);
bool CodeChecksumOk(const CString& normalized);
CString FormatCode(const CString& normalized); // XXXX-XXXX-XXXX-XXXX
CString CodeEnvironment(const CString& normalized); // "prod", "homolog", "dev" or ""
CString EnvironmentName(const CString& environment); // "produção"...

// Identity of this computer and Windows user: a copied configuration is refused by the server.
CString MachineId();
CString OsVersion(); // "Windows 10.0.19045"
CString Hostname();
CString Architecture(); // "x64" or "x86"
CString RandomHex(int bytes);

std::string ToUtf8(const CString& text);
CString FromUtf8(const std::string& text);

// File protected with DPAPI for the current Windows user, written atomically. The previous version is kept
// next to it (.prev) and used when the current one cannot be read.
bool ProtectedWrite(const CString& path, const std::string& data, CString* error);
bool ProtectedRead(const CString& path, std::string& data);
void ProtectedDelete(const CString& path);

// Technical log of the provisioning (provisioning_log.txt in the local data folder). Never log passwords,
// tokens or activation codes: only the last characters of a code.
void Log(const CString& message);
CString LogPath();
// Removes passwords, tokens and authentication headers (SIP Digest responses) from a log.
std::string SanitizeLog(const std::string& text);
// Last part of the provisioning log and of the SIP log, sanitized, for the support.
std::string CollectLogs(size_t maxBytes);

} // namespace Provisioning
