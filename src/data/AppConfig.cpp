#include "AppConfig.h"

#include <windows.h>
#include <shlobj.h>

namespace splite
{

std::wstring AppConfig::GetConfigPath()
{
    wchar_t localAppData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                SHGFP_TYPE_CURRENT, localAppData)))
    {
        return L"SPlite.ini";
    }

    std::wstring directory = std::wstring(localAppData) + L"\\SPlite";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\settings.ini";
}

AppConfig AppConfig::Load()
{
    AppConfig config;
    const std::wstring path = GetConfigPath();
    config.topMost = GetPrivateProfileIntW(L"window", L"topMost", 1, path.c_str()) != 0;
    return config;
}

bool AppConfig::Save() const
{
    const std::wstring path = GetConfigPath();
    return WritePrivateProfileStringW(L"window", L"topMost",
                                      topMost ? L"1" : L"0", path.c_str()) != FALSE;
}

} // namespace splite
