#pragma once
#include "jarkUtils.h"
#include <shlwapi.h>
#include <shlobj.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

class FileAssociationManager {
private:
    std::wstring m_appPath;
    std::wstring m_appName;
    std::wstring m_progId;

    // 获取当前程序的完整路径
    std::wstring GetCurrentAppPath() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::wstring(path);
    }

    // 设置注册表值
    bool SetRegistryValue(HKEY hKey, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& value) {
        HKEY hSubKey;
        DWORD disposition;

        LONG result = RegCreateKeyExW(hKey, subKey.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE,
            nullptr, &hSubKey, &disposition);

        if (result != ERROR_SUCCESS) {
            return false;
        }

        result = RegSetValueExW(hSubKey, valueName.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t)));

        RegCloseKey(hSubKey);
        return result == ERROR_SUCCESS;
    }

    // 删除注册表键
    bool DeleteRegistryKey(HKEY hKey, const std::wstring& subKey) {
        return RegDeleteKeyW(hKey, subKey.c_str()) == ERROR_SUCCESS;
    }

    // 删除注册表值
    bool DeleteRegistryValue(HKEY hKey, const std::wstring& subKey, const std::wstring& valueName) {
        HKEY hSubKey;
        LONG result = RegOpenKeyExW(hKey, subKey.c_str(), 0, KEY_WRITE, &hSubKey);

        if (result != ERROR_SUCCESS) {
            return false;
        }

        result = RegDeleteValueW(hSubKey, valueName.c_str());
        RegCloseKey(hSubKey);

        return result == ERROR_SUCCESS;
    }

    // 检查是否已经关联到当前程序
    bool IsAssociatedWithCurrentApp(const std::wstring& extension) {
        // 检查类关联
        std::wstring extKey = L"Software\\Classes\\." + extension;

        HKEY hKey;
        LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, extKey.c_str(), 0, KEY_READ, &hKey);

        if (result != ERROR_SUCCESS) {
            return false;
        }

        wchar_t progId[256];
        DWORD bufferSize = sizeof(progId);
        result = RegQueryValueExW(hKey, L"", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(progId), &bufferSize);

        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS) {
            return std::wstring(progId) == m_progId;
        }

        return false;
    }

    // 注册程序ID（不设置图标，保持系统缩略图功能）
    bool RegisterProgId() {
        std::wstring progIdKey = L"Software\\Classes\\" + m_progId;

        // 设置程序描述
        if (!SetRegistryValue(HKEY_CURRENT_USER, progIdKey, L"", m_appName + L" Image File")) {
            return false;
        }

        // 不设置 DefaultIcon，让系统保持原有的缩略图显示功能

        // 设置打开命令
        std::wstring commandKey = progIdKey + L"\\shell\\open\\command";
        std::wstring commandValue = L"\"" + m_appPath + L"\" \"%1\"";
        if (!SetRegistryValue(HKEY_CURRENT_USER, commandKey, L"", commandValue)) {
            return false;
        }

        return true;
    }

    // 关联文件扩展名
    bool AssociateExtension(const std::wstring& extension) {
        std::wstring extKey = L"Software\\Classes\\." + extension;

        // 设置文件类型关联
        if (!SetRegistryValue(HKEY_CURRENT_USER, extKey, L"", m_progId)) {
            return false;
        }

        // 不再设置 UserChoice，因为它受到系统保护
        // Windows 会根据用户的实际选择来更新 UserChoice

        return true;
    }

    // 取消关联文件扩展名
    bool UnassociateExtension(const std::wstring& extension) {
        // 检查是否关联到当前程序
        if (!IsAssociatedWithCurrentApp(extension)) {
            return true; // 如果没有关联到当前程序，直接返回成功
        }

        // 删除类关联
        std::wstring extKey = L"Software\\Classes\\." + extension;
        DeleteRegistryValue(HKEY_CURRENT_USER, extKey, L"");

        return true;
    }

    // 通知系统文件关联已更改
    void NotifySystemChange() {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

public:
    FileAssociationManager() {
        m_appPath = GetCurrentAppPath();
        m_appName = L"jarkViewer";
        m_progId = L"jarkViewer.ImageFile";
    }

    // 管理文件关联的主函数
    bool ManageFileAssociations(const std::vector<std::wstring>& extChecked,
        const std::vector<std::wstring>& extUnchecked) {

        // 首先注册程序ID
        if (!RegisterProgId()) {
            return false;
        }

        bool allSucceeded = true;

        // 关联需要的扩展名
        for (const auto& ext : extChecked) {
            if (!AssociateExtension(ext)) {
                allSucceeded = false;
            }
        }

        // 取消不需要的扩展名关联
        for (const auto& ext : extUnchecked) {
            if (!UnassociateExtension(ext)) {
                allSucceeded = false;
            }
        }

        // 通知系统更改
        NotifySystemChange();

        return allSucceeded;
    }

    // 检查扩展名是否已关联到当前程序
    bool IsExtensionAssociated(const std::wstring& extension) {
        return IsAssociatedWithCurrentApp(extension);
    }

    // 获取当前程序路径
    std::wstring GetAppPath() const {
        return m_appPath;
    }
};

