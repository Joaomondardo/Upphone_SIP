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
#include "UpdateDownload.h"
#include "ProvisioningHttp.h"
#include "define.h"

#include <bcrypt.h>
#include <softpub.h>
#include <wintrust.h>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "version.lib")

namespace Provisioning {

// the installers have about 5 MB; the limit only protects the memory against a wrong address
static const size_t kMaxInstaller = 200 * 1024 * 1024;
static const DWORD kDownloadTimeout = 10 * 60 * 1000;

static CString Sha256Hex(const std::string& data)
{
    CString hex;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    UCHAR digest[32];
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0) {
        if (BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0) {
            const char* p = data.data();
            size_t left = data.size();
            bool ok = true;
            while (ok && left) {
                ULONG chunk = (ULONG)min(left, (size_t)(64 * 1024 * 1024));
                ok = BCryptHashData(hash, (PUCHAR)p, chunk, 0) == 0;
                p += chunk;
                left -= chunk;
            }
            if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
                for (UCHAR b : digest) {
                    hex.AppendFormat(_T("%02x"), b);
                }
            }
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return hex;
}

// "1.4.1" -> 1.4.1.0, as Inno Setup writes VersionInfoVersion in the installer
static bool ParseVersion(const CString& text, WORD out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0;
    int part = 0;
    int pos = 0;
    CString token = text.Tokenize(_T("."), pos);
    while (!token.IsEmpty()) {
        if (part == 4 || token.SpanIncluding(_T("0123456789")) != token || token.GetLength() > 5) {
            return false;
        }
        out[part++] = (WORD)_ttoi(token);
        token = text.Tokenize(_T("."), pos);
    }
    return part > 0;
}

static bool FileVersion(const CString& path, WORD out[4])
{
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSize(path, &handle);
    if (!size) {
        return false;
    }
    std::vector<BYTE> info(size);
    VS_FIXEDFILEINFO* fixed = NULL;
    UINT length = 0;
    if (!GetFileVersionInfo(path, 0, size, info.data())
        || !VerQueryValue(info.data(), _T("\\"), (LPVOID*)&fixed, &length) || !fixed) {
        return false;
    }
    out[0] = HIWORD(fixed->dwFileVersionMS);
    out[1] = LOWORD(fixed->dwFileVersionMS);
    out[2] = HIWORD(fixed->dwFileVersionLS);
    out[3] = LOWORD(fixed->dwFileVersionLS);
    return true;
}

// ERROR_SUCCESS: valid signature (signer = publisher name); TRUST_E_NOSIGNATURE: not signed; other: invalid
static LONG CheckSignature(const CString& path, CString& signer)
{
    signer.Empty();
    WINTRUST_FILE_INFO file = { sizeof(file) };
    file.pcwszFilePath = path;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA data = { sizeof(data) };
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    // no WTD_SAFER_FLAG: with it a signed file that was changed is reported as "not signed" (tested)
    LONG result = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
    if (result == ERROR_SUCCESS) {
        CRYPT_PROVIDER_DATA* provider = WTHelperProvDataFromStateData(data.hWVTStateData);
        CRYPT_PROVIDER_SGNR* sgnr = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : NULL;
        CRYPT_PROVIDER_CERT* cert = sgnr ? WTHelperGetProvCertFromChain(sgnr, 0) : NULL;
        if (cert && cert->pCert) {
            TCHAR name[256] = { 0 };
            CertGetNameString(cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, _countof(name));
            signer = name;
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
    return result;
}

// the PE file carries an Authenticode signature (certificate table). A file with one is never treated as unsigned,
// whatever WinVerifyTrust answers.
static bool HasEmbeddedSignature(const std::string& data)
{
    if (data.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)data.data();
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0
        || (size_t)dos->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(WORD) > data.size()) {
        return false;
    }
    const BYTE* nt = (const BYTE*)data.data() + dos->e_lfanew;
    if (*(const DWORD*)nt != IMAGE_NT_SIGNATURE) {
        return false;
    }
    const BYTE* optional = nt + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    WORD magic = *(const WORD*)optional;
    size_t offset = optional - (const BYTE*)data.data();
    const IMAGE_DATA_DIRECTORY* security = NULL;
    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC && offset + sizeof(IMAGE_OPTIONAL_HEADER64) <= data.size()) {
        security = &((const IMAGE_OPTIONAL_HEADER64*)optional)->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY];
    }
    else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC && offset + sizeof(IMAGE_OPTIONAL_HEADER32) <= data.size()) {
        security = &((const IMAGE_OPTIONAL_HEADER32*)optional)->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY];
    }
    return security && security->VirtualAddress && security->Size;
}

static bool WriteFileAll(const CString& path, const std::string& data)
{
    HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file, data.data(), (DWORD)data.size(), &written, NULL) && written == data.size();
    CloseHandle(file);
    if (!ok) {
        DeleteFile(path);
    }
    return ok;
}

UpdateDownloadResult DownloadUpdate(const CString& url, const CString& sha256, const CString& version)
{
    UpdateDownloadResult result;
    CString expected = sha256;
    expected.Trim();
    expected.MakeLower();
    WORD offered[4];
    if (expected.GetLength() != 64 || expected.SpanIncluding(_T("0123456789abcdef")) != expected) {
        result.error = _T("A Upp não informou o código de verificação (SHA-256) desta versão. Fale com o suporte.");
        return result;
    }
    if (!ParseVersion(version, offered)) {
        result.error = _T("Versão oferecida inválida.");
        return result;
    }
    CString reason;
    if (!IsAllowedServerUrl(url, &reason)) {
        result.error = _T("Endereço de download recusado: ") + reason;
        return result;
    }

    HttpResponse response = HttpRequest(_T("GET"), url, CString(), std::string(), kDownloadTimeout, kMaxInstaller, true);
    if (response.status != 200) {
        if (response.status == 0) {
            result.error.Format(_T("Não foi possível baixar a atualização: %s (%s)."), response.error.GetString(),
                response.supportCode.GetString());
        }
        else {
            result.error.Format(_T("O servidor do download respondeu %u."), response.status);
        }
        return result;
    }
    if (response.truncated) {
        result.error = _T("O arquivo de download é grande demais para ser o instalador.");
        return result;
    }
    // integrity before anything touches the disk as an executable
    if (Sha256Hex(response.body) != expected) {
        result.error = _T("O arquivo baixado não confere com o código de verificação da Upp. Nada foi instalado.");
        return result;
    }

    // a folder of its own in %TEMP% of the user
    TCHAR temp[MAX_PATH] = { 0 };
    GetTempPath(_countof(temp), temp);
    CString folder;
    folder.Format(_T("%sUpphone SIP update"), temp);
    CreateDirectory(folder, NULL);
    result.path.Format(_T("%s\\Upphone-SIP-%s-setup.exe"), folder.GetString(), version.GetString());
    if (!WriteFileAll(result.path, response.body)) {
        result.error = _T("Não foi possível gravar o instalador na pasta temporária.");
        return result;
    }

    WORD inFile[4];
    if (!FileVersion(result.path, inFile) || memcmp(inFile, offered, sizeof(inFile)) != 0) {
        DeleteFile(result.path);
        result.error = _T("O instalador baixado não é da versão oferecida. Nada foi instalado.");
        return result;
    }

    CString signer;
    LONG trust = CheckSignature(result.path, signer);
    CString publisher = _T(_GLOBAL_UPDATE_PUBLISHER);
    bool unsigned_ = trust == TRUST_E_NOSIGNATURE && !HasEmbeddedSignature(response.body);
    if (unsigned_ && publisher.IsEmpty()) {
        // no certificate yet: the SHA-256 registered by the Upp is what guarantees the file
    }
    else if (unsigned_) {
        DeleteFile(result.path);
        result.error = _T("O instalador não tem assinatura digital da Upp. Nada foi instalado.");
        return result;
    }
    else if (trust != ERROR_SUCCESS) {
        DeleteFile(result.path);
        result.error.Format(_T("A assinatura digital do instalador não é válida (0x%08X). Nada foi instalado."), (unsigned)trust);
        return result;
    }
    else if (!publisher.IsEmpty() && signer != publisher) {
        DeleteFile(result.path);
        result.error.Format(_T("O instalador é assinado por \"%s\", não pela Upp. Nada foi instalado."), signer.GetString());
        return result;
    }
    result.signer = signer;
    result.ok = true;
    return result;
}

bool LaunchUpdate(const CString& path, HANDLE& process, CString& error)
{
    // silent: the activation page is not shown (the computer is already activated) and the app is closed and
    // opened again by the installer
    process = NULL;
    SHELLEXECUTEINFO info = { sizeof(info) };
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = _T("open");
    info.lpFile = path;
    info.lpParameters = _T("/SILENT /SUPPRESSMSGBOXES /NORESTART /ATUALIZACAO=1");
    info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteEx(&info)) {
        process = info.hProcess;
        return true;
    }
    DWORD code = GetLastError();
    if (code == ERROR_CANCELLED) {
        error = _T("A instalação precisa da permissão de administrador do Windows. Tente de novo e confirme, ou peça à TI.");
    }
    else {
        error.Format(_T("Não foi possível iniciar o instalador (erro %u)."), code);
    }
    return false;
}

CString InstallerFailure(DWORD exitCode)
{
    CString text;
    if (exitCode == 0) {
        return text;
    }
    // Inno Setup: 2 = cancelled before installing, which includes "No" in the administrator permission of Windows
    // (measured in the test VM); 5 = cancelled during the installation
    if (exitCode == 2 || exitCode == 5) {
        text = _T("A atualização não foi instalada: a permissão de administrador do Windows não foi confirmada. ")
            _T("Tente de novo e clique em Sim, ou peça à TI.");
    }
    else {
        text.Format(_T("A atualização não foi instalada (o instalador terminou com o código %u)."), exitCode);
    }
    return text;
}

} // namespace Provisioning
