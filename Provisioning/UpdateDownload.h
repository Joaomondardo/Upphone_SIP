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

// Update offered by the Upp platform: the app downloads the installer itself and runs it only after checking it
// (specification, RN09). Checks, in this order:
//   1. https address (certificate validated by Windows, see ProvisioningHttp);
//   2. SHA-256 equal to the one registered by the Upp in the panel (required: without it nothing is run);
//   3. version written in the installer equal to the offered one (a file of another version is refused);
//   4. Authenticode signature: when the file is signed it must be valid; with _GLOBAL_UPDATE_PUBLISHER defined,
//      it must be signed by that publisher (until the Upp has a certificate, unsigned installers are accepted
//      only because of the SHA-256 from the platform).
// The platform decides which version is offered (a lower one only when it pinned a version for the client).

namespace Provisioning {

struct UpdateDownloadResult {
    bool ok = false;
    CString path;     // installer checked, in a folder of its own in %TEMP%
    CString signer;   // publisher of the signature ("" = not signed)
    CString error;    // why it was refused, for the user
};

// Synchronous: call from a worker thread. version: offered version ("1.4.1").
UpdateDownloadResult DownloadUpdate(const CString& url, const CString& sha256, const CString& version);

// Runs the checked installer. The Inno Setup installer starts without elevation and then asks Windows for the
// administrator permission itself, so a "No" there is not seen here: process is the installer, to follow it with
// InstallerFailure. When the installation goes on, the installer closes the app and opens it again at the end
// (/ATUALIZACAO=1). error: why it did not start.
bool LaunchUpdate(const CString& path, HANDLE& process, CString& error);

// The installer ended while the app is still open: the update did not happen. Text for the user ("" when it ended
// with success, for example the same version again).
CString InstallerFailure(DWORD exitCode);

} // namespace Provisioning
