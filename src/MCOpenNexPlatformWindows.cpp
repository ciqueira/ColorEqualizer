#include "MCOpenNexPlatform.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <cwchar>
#include <string>

namespace {

std::wstring utf8ToWide(const char *value) {
  if (!value || value[0] == '\0') {
    return {};
  }
  const int length =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
  if (length <= 1) {
    return {};
  }
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
                          &result[0], length) == 0) {
    return {};
  }
  result.resize(static_cast<std::size_t>(length - 1));
  return result;
}

bool startsWith(const std::wstring &value, const wchar_t *prefix) {
  const std::wstring expected(prefix);
  return value.size() >= expected.size() &&
         _wcsnicmp(value.c_str(), expected.c_str(), expected.size()) == 0;
}

bool isSupportedUrl(const std::wstring &url) {
  return startsWith(url, L"https://") || startsWith(url, L"mcnexus://");
}

bool hasMCNexusProtocolHandler() {
  HKEY key = nullptr;
  const LSTATUS opened = RegOpenKeyExW(
      HKEY_CLASSES_ROOT, L"mcnexus\\shell\\open\\command", 0, KEY_READ, &key);
  if (opened != ERROR_SUCCESS) {
    return false;
  }
  RegCloseKey(key);
  return true;
}

} // namespace

namespace mcopen {

bool canOpenUrl(const char *value) {
  const std::wstring url = utf8ToWide(value);
  if (!isSupportedUrl(url)) {
    return false;
  }
  return startsWith(url, L"https://") || hasMCNexusProtocolHandler();
}

bool openUrl(const char *value) {
  const std::wstring url = utf8ToWide(value);
  if (!isSupportedUrl(url)) {
    return false;
  }
  const HINSTANCE result =
      ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr,
                    SW_SHOWNORMAL);
  return reinterpret_cast<intptr_t>(result) > 32;
}

bool openMCNexusApplication() {
  constexpr const wchar_t *candidates[] = {
      L"%ProgramFiles%\\MCNexus\\MCNexus.exe",
      L"%ProgramFiles(x86)%\\MCNexus\\MCNexus.exe",
      L"%LocalAppData%\\Programs\\MCNexus\\MCNexus.exe"};
  for (const wchar_t *candidate : candidates) {
    wchar_t expanded[MAX_PATH] = {};
    const DWORD length =
        ExpandEnvironmentStringsW(candidate, expanded, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
      continue;
    }
    const DWORD attributes = GetFileAttributesW(expanded);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
      continue;
    }
    const HINSTANCE result =
        ShellExecuteW(nullptr, L"open", expanded, nullptr, nullptr,
                      SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) > 32) {
      return true;
    }
  }
  return false;
}

} // namespace mcopen
