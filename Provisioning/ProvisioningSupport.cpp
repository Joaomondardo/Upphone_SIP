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
#include "ProvisioningSupport.h"
#include "define.h"
#include "settings.h"

#include <bcrypt.h>
#include <mutex>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace Provisioning {

static const TCHAR kAlphabet[] = _T("0123456789ABCDEFGHJKMNPQRSTVWXYZ");
static const int kCodeLength = 16;
// entropy of the DPAPI blobs: a file of another program protected for the same user is not accepted
static const char kEntropy[] = "Upphone SIP provisioning v1";
static const LONGLONG kLogMaxSize = 512 * 1024;

CString NormalizeCode(const CString& code)
{
    CString text;
    for (int i = 0; i < code.GetLength(); i++) {
        TCHAR c = code[i];
        if (c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        c = (TCHAR)_totupper(c);
        if (c == 'O') {
            c = '0';
        }
        else if (c == 'I' || c == 'L') {
            c = '1';
        }
        if (!_tcschr(kAlphabet, c) || c == 0) {
            return _T("");
        }
        text += c;
    }
    return text.GetLength() == kCodeLength ? text : _T("");
}

bool CodeChecksumOk(const CString& normalized)
{
    if (normalized.GetLength() != kCodeLength) {
        return false;
    }
    int total = 0;
    for (int i = 0; i < kCodeLength - 1; i++) {
        const TCHAR* p = _tcschr(kAlphabet, normalized[i]);
        if (!p) {
            return false;
        }
        total += (i + 1) * (int)(p - kAlphabet);
    }
    return kAlphabet[total % 31] == normalized[kCodeLength - 1];
}

CString FormatCode(const CString& normalized)
{
    CString result;
    for (int i = 0; i < normalized.GetLength(); i++) {
        if (i && i % 4 == 0) {
            result += _T('-');
        }
        result += normalized[i];
    }
    return result;
}

CString CodeEnvironment(const CString& normalized)
{
    if (normalized.IsEmpty()) {
        return _T("");
    }
    switch (normalized[0]) {
    case 'P':
        return _T("prod");
    case 'H':
        return _T("homolog");
    case 'D':
        return _T("dev");
    }
    return _T("");
}

CString EnvironmentName(const CString& environment)
{
    if (environment == _T("prod")) {
        return _T("produção");
    }
    if (environment == _T("homolog")) {
        return _T("homologação");
    }
    if (environment == _T("dev")) {
        return _T("desenvolvimento");
    }
    return environment;
}

std::string ToUtf8(const CString& text)
{
    if (text.IsEmpty()) {
        return std::string();
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), NULL, 0, NULL, NULL);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), &result[0], size, NULL, NULL);
    return result;
}

CString FromUtf8(const std::string& text)
{
    if (text.empty()) {
        return CString();
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), NULL, 0);
    CString result;
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), result.GetBuffer(size), size);
    result.ReleaseBuffer(size);
    return result;
}

static CString Sha256Hex(const std::string& data)
{
    CString result;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    UCHAR digest[32] = {};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0) {
        if (BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0) {
            if (BCryptHashData(hash, (PUCHAR)data.data(), (ULONG)data.size(), 0) == 0
                && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
                for (UCHAR b : digest) {
                    result.AppendFormat(_T("%02x"), b);
                }
            }
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return result;
}

CString MachineId()
{
    CString guid;
    HKEY key;
    // KEY_WOW64_64KEY: the 32-bit Upphone on 64-bit Windows reads the same value as the 64-bit one
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Cryptography"), 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        TCHAR buffer[128] = {};
        DWORD size = sizeof(buffer) - sizeof(TCHAR);
        DWORD type = 0;
        if (RegQueryValueEx(key, _T("MachineGuid"), NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS && type == REG_SZ) {
            guid = buffer;
        }
        RegCloseKey(key);
    }
    CString sid = MSIP::GetSID();
    if (guid.IsEmpty() && sid.IsEmpty()) {
        return _T("");
    }
    return Sha256Hex(ToUtf8(_T("upphone|") + guid + _T("|") + sid));
}

CString OsVersion()
{
    typedef LONG(WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    HMODULE ntdll = GetModuleHandle(_T("ntdll.dll"));
    RtlGetVersionFn fn = ntdll ? (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    CString result = _T("Windows");
    if (fn && fn(&info) == 0) {
        result.AppendFormat(_T(" %u.%u.%u"), info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
    }
    return result;
}

CString Hostname()
{
    TCHAR buffer[256] = {};
    DWORD size = _countof(buffer);
    if (GetComputerNameEx(ComputerNameDnsHostname, buffer, &size)) {
        return buffer;
    }
    size = _countof(buffer);
    return GetComputerName(buffer, &size) ? buffer : _T("");
}

CString Architecture()
{
#if defined(_M_X64)
    return _T("x64");
#else
    return _T("x86");
#endif
}

CString RandomHex(int bytes)
{
    std::vector<UCHAR> buffer(bytes);
    CString result;
    if (BCryptGenRandom(NULL, buffer.data(), (ULONG)buffer.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return result;
    }
    for (UCHAR b : buffer) {
        result.AppendFormat(_T("%02x"), b);
    }
    return result;
}

static bool WriteAll(const CString& path, const BYTE* data, DWORD size)
{
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, size, &written, NULL) && written == size && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) {
        DeleteFile(path);
    }
    return ok != FALSE;
}

static bool ReadAll(const CString& path, std::vector<BYTE>& data, DWORD maxSize)
{
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart <= maxSize;
    if (ok) {
        data.resize((size_t)size.QuadPart);
        DWORD read = 0;
        ok = ReadFile(h, data.data(), (DWORD)data.size(), &read, NULL) && read == data.size();
    }
    CloseHandle(h);
    return ok;
}

bool ProtectedWrite(const CString& path, const std::string& data, CString* error)
{
    DATA_BLOB in = { (DWORD)data.size(), (BYTE*)data.data() };
    DATA_BLOB entropy = { (DWORD)(sizeof(kEntropy) - 1), (BYTE*)kEntropy };
    DATA_BLOB out = {};
    // Upphone: never fall back to a weaker protection; without DPAPI the credentials are not stored
    if (!CryptProtectData(&in, _T("Upphone SIP"), &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        if (error) {
            error->Format(_T("DPAPI %u"), GetLastError());
        }
        return false;
    }
    CString temp = path + _T(".tmp");
    bool ok = WriteAll(temp, out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (!ok) {
        if (error) {
            error->Format(_T("gravação %u"), GetLastError());
        }
        return false;
    }
    if (::PathFileExists(path)) {
        // the current file becomes the previous copy (.prev) and the new one takes its place
        if (ReplaceFile(path, temp, path + _T(".prev"), REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
            return true;
        }
    }
    if (MoveFileEx(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    if (error) {
        error->Format(_T("substituição %u"), GetLastError());
    }
    DeleteFile(temp);
    return false;
}

static bool ProtectedReadOne(const CString& path, std::string& data)
{
    std::vector<BYTE> blob;
    if (!ReadAll(path, blob, 4 * 1024 * 1024)) {
        return false;
    }
    DATA_BLOB in = { (DWORD)blob.size(), blob.data() };
    DATA_BLOB entropy = { (DWORD)(sizeof(kEntropy) - 1), (BYTE*)kEntropy };
    DATA_BLOB out = {};
    // fails for a file copied from another user or computer: it is not used
    if (!CryptUnprotectData(&in, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return false;
    }
    data.assign((const char*)out.pbData, out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}

bool ProtectedRead(const CString& path, std::string& data)
{
    return ProtectedReadOne(path, data) || ProtectedReadOne(path + _T(".prev"), data);
}

void ProtectedDelete(const CString& path)
{
    DeleteFile(path);
    DeleteFile(path + _T(".prev"));
    DeleteFile(path + _T(".tmp"));
}

// ---------------------------------------------------------------------------------------------------- log

static std::mutex g_logMutex;

CString LogPath()
{
    CString folder = accountSettings.pathLocal;
    if (folder.IsEmpty()) {
        folder = accountSettings.pathExe;
    }
    if (folder.Right(1) != _T("\\")) {
        folder += _T("\\");
    }
    return folder + _T("provisioning_log.txt");
}

void Log(const CString& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    CString path = LogPath();
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesEx(path, GetFileExInfoStandard, &fad)) {
        LONGLONG size = ((LONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        if (size > kLogMaxSize) {
            MoveFileEx(path, path + _T(".1"), MOVEFILE_REPLACE_EXISTING);
        }
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    CString line;
    line.Format(_T("%04u-%02u-%02u %02u:%02u:%02u %s\r\n"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, message.GetString());
    std::string text = SanitizeLog(ToUtf8(line));
    HANDLE h = CreateFile(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, text.data(), (DWORD)text.size(), &written, NULL);
        CloseHandle(h);
    }
}

static std::string Lower(const std::string& text)
{
    std::string result = text;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

static bool IsTokenChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

// replaces the value after "keyword" + "=" or ":" (with or without quotes) by [removido]
static void RedactValues(std::string& line, const std::string& keyword)
{
    size_t from = 0;
    for (;;) {
        std::string lower = Lower(line);
        size_t pos = lower.find(keyword, from);
        if (pos == std::string::npos) {
            return;
        }
        size_t i = pos + keyword.size();
        while (i < line.size() && (line[i] == '"' || line[i] == ' ')) {
            i++;
        }
        if (i >= line.size() || (line[i] != '=' && line[i] != ':')) {
            from = pos + keyword.size();
            continue;
        }
        i++;
        while (i < line.size() && line[i] == ' ') {
            i++;
        }
        size_t start = i;
        size_t end;
        if (i < line.size() && line[i] == '"') {
            end = line.find('"', i + 1);
            end = end == std::string::npos ? line.size() : end + 1;
        }
        else {
            end = start;
            while (end < line.size() && line[end] != ' ' && line[end] != ',' && line[end] != '&' && line[end] != ';' && line[end] != '\r') {
                end++;
            }
        }
        if (end > start) {
            line.replace(start, end - start, "[removido]");
        }
        from = start + 10;
    }
}

std::string SanitizeLog(const std::string& text)
{
    static const char* headers[] = { "authorization:", "authenticate:" };
    static const char* keywords[] = { "password", "senha", "passwd", "secret", "device_token", "rotate_token", "request_key", "\"code\"" };
    static const char* tokens[] = { "upd_", "upk_", "whsec_" };
    std::string result;
    result.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        end = end == std::string::npos ? text.size() : end + 1;
        std::string line = text.substr(pos, end - pos);
        pos = end;
        std::string lower = Lower(line);
        bool header = false;
        for (const char* h : headers) {
            size_t p = lower.find(h);
            if (p != std::string::npos) {
                // Authorization / Proxy-Authorization / WWW-Authenticate: the Digest response allows attacking
                // the SIP password offline, so the whole value goes
                line = line.substr(0, p + strlen(h)) + " [removido]" + (line.back() == '\n' ? "\n" : "");
                header = true;
                break;
            }
        }
        if (!header) {
            for (const char* k : keywords) {
                RedactValues(line, k);
            }
            for (const char* t : tokens) {
                size_t p = 0;
                while ((p = Lower(line).find(t, p)) != std::string::npos) {
                    size_t start = p + strlen(t);
                    size_t e = start;
                    while (e < line.size() && IsTokenChar(line[e])) {
                        e++;
                    }
                    if (e - start >= 10) {
                        line.replace(start, e - start, "[removido]");
                    }
                    p = start;
                }
            }
        }
        result += line;
    }
    return result;
}

static std::string FileTail(const CString& path, size_t maxBytes)
{
    std::string result;
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return result;
    }
    LARGE_INTEGER size = {};
    if (GetFileSizeEx(h, &size) && size.QuadPart > 0) {
        LONGLONG start = size.QuadPart > (LONGLONG)maxBytes ? size.QuadPart - (LONGLONG)maxBytes : 0;
        LARGE_INTEGER move;
        move.QuadPart = start;
        if (SetFilePointerEx(h, move, NULL, FILE_BEGIN)) {
            result.resize((size_t)(size.QuadPart - start));
            DWORD read = 0;
            if (ReadFile(h, &result[0], (DWORD)result.size(), &read, NULL)) {
                result.resize(read);
            }
            else {
                result.clear();
            }
        }
    }
    CloseHandle(h);
    return result;
}

std::string CollectLogs(size_t maxBytes)
{
    std::string text;
    CString header;
    SYSTEMTIME st;
    GetLocalTime(&st);
    header.Format(_T("Upphone SIP %s (%s) - %s - %04u-%02u-%02u %02u:%02u\r\n"), _T(_GLOBAL_VERSION), Architecture().GetString(),
        OsVersion().GetString(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    text += ToUtf8(header);
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        text += "\r\n===== provisioning_log.txt =====\r\n";
        text += FileTail(LogPath(), maxBytes / 2);
    }
    if (::PathFileExists(accountSettings.logFile)) {
        text += "\r\n===== log SIP =====\r\n";
        text += FileTail(accountSettings.logFile, maxBytes / 2);
    }
    return SanitizeLog(text);
}

} // namespace Provisioning
