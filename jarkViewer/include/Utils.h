#pragma once

#include<iostream>
#include<format>
#include<algorithm>
#include<filesystem>
#include<chrono>
#include<mutex>
#include<semaphore>
#include<string>
#include<vector>
#include<unordered_set>
#include<unordered_map>

using std::vector;
using std::string;
using std::wstring;
using std::string_view;
using std::unordered_set;
using std::unordered_map;
using std::cout;
using std::endl;

#include "framework.h"
#include "resource.h"

#include<opencv2/core.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>
#include<opencv2/highgui/highgui_c.h>

#include "libraw/libraw.h"
#include "libheif/heif.h"
#include "avif/avif.h"
#include "gif_lib.h"
#include "psapi.h"

struct rcFileInfo {
    uint8_t* addr = nullptr;
    size_t size = 0;
};

union intUnion {
    uint32_t u32;
    uint8_t u8[4];
    intUnion() :u32(0) {}
    intUnion(uint32_t n) :u32(n) {}
    intUnion(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
        u8[0] = b0;
        u8[1] = b1;
        u8[2] = b2;
        u8[3] = b3;
    }
    uint8_t& operator[](const int i) {
        return u8[i];
    }
    void operator=(const int i) {
        u32 = i;
    }
    void operator=(const uint32_t i) {
        u32 = i;
    }
    void operator=(const intUnion i) {
        u32 = i.u32;
    }
};

struct Cood {
    int x = 0;
    int y = 0;

    Cood operator+(const Cood& t) const {
        Cood temp;
        temp.x = this->x + t.x;
        temp.y = this->y + t.y;
        return temp;
    }

    Cood operator-(const Cood& t) const {
        Cood temp;
        temp.x = this->x - t.x;
        temp.y = this->y - t.y;
        return temp;
    }

    Cood operator*(int i) const {
        Cood temp;
        temp.x = this->x * i;
        temp.y = this->y * i;
        return temp;
    }

    Cood operator/(int i) const {
        Cood temp;
        temp.x = this->x / i;
        temp.y = this->y / i;
        return temp;
    }
};

struct ImageNode {
    cv::Mat img;
    int delay;
};
struct Frames {
    std::vector<ImageNode> imgList;
    string exifStr;
};

struct GifData {
    const unsigned char* m_lpBuffer;
    size_t m_nBufferSize;
    size_t m_nPosition;
};

namespace Utils {

    static const bool TO_LOG_FILE = false;
    static FILE* fp = nullptr;

    template<typename... Args>
    static void log(const string_view fmt, Args&&... args) {
        auto ts = time(nullptr) + 8 * 3600ULL;// UTC+8
        int HH = (ts / 3600) % 24;
        int MM = (ts / 60) % 60;
        int SS = ts % 60;

        const string str = std::format("[{:02d}:{:02d}:{:02d}] ", HH, MM, SS) +
            std::vformat(fmt, std::make_format_args(args...)) + "\n";

        if (TO_LOG_FILE) {
            if (!fp) {
                fp = fopen("D:\\log.txt", "a");
                if (!fp)return;
            }
            fwrite(str.c_str(), 1, str.length(), fp);
            fflush(fp);
        }
        else {
            cout << str;
        }
    }

    string bin2Hex(const void* bytes, const size_t len) {
        auto charList = "0123456789ABCDEF";
        if (len == 0) return "";
        string res(len * 3 - 1, ' ');
        for (size_t i = 0; i < len; i++) {
            const uint8_t value = reinterpret_cast<const uint8_t*>(bytes)[i];
            res[i * 3] = charList[value >> 4];
            res[i * 3 + 1] = charList[value & 0x0f];
        }
        return res;
    }

    std::wstring ansiToWstring(const std::string& str) {
        if (str.empty())return L"";

        int wcharLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), nullptr, 0);
        if (wcharLen == 0) return L"";

        std::wstring ret(wcharLen, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

        return ret;
    }

    std::string wstringToAnsi(const std::wstring& wstr) {
        if (wstr.empty())return "";

        int byteLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
        if (byteLen == 0) return "";

        std::string ret(byteLen, 0);
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
        return ret;
    }

    //UTF8 to UTF16
    std::wstring utf8ToWstring(const std::string& str) {
        if (str.empty())return L"";

        int wcharLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
        if (wcharLen == 0) return L"";

        std::wstring ret(wcharLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

        return ret;
    }

    //UTF16 to UTF8
    std::string wstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty())return "";

        int byteLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
        if (byteLen == 0) return "";

        std::string ret(byteLen, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
        return ret;
    }

    std::string utf8ToAnsi(const std::string& str) {
        return wstringToAnsi(utf8ToWstring(str));
    }

    std::string ansiToUtf8(const std::string& str) {
        return wstringToUtf8(ansiToWstring(str));
    }

    rcFileInfo GetResource(unsigned int idi, const wchar_t* type) {
        rcFileInfo rc;

        HMODULE ghmodule = GetModuleHandle(NULL);
        if (ghmodule == NULL) {
            log("ghmodule null");
            return rc;
        }

        HRSRC hrsrc = FindResource(ghmodule, MAKEINTRESOURCE(idi), type);
        if (hrsrc == NULL) {
            log("hrstc null");
            return rc;
        }

        HGLOBAL hg = LoadResource(ghmodule, hrsrc);
        if (hg == NULL) {
            log("hg null");
            return rc;
        }

        rc.addr = (unsigned char*)(LockResource(hg));
        rc.size = SizeofResource(ghmodule, hrsrc);
        return rc;
    }

    string size2Str(const int fileSize) {
        if (fileSize < 1024) return std::format("{} Bytes", fileSize);
        if (fileSize < 1024 * 1024) return std::format("{:.1f} KiB", fileSize / 1024.0);
        if (fileSize < 1024 * 1024 * 1024) return std::format("{:.1f} MiB", fileSize / (1024.0 * 1024));
        return std::format("{:.1f} GiB", fileSize / (1024.0 * 1024 * 1024));
    }

    string timeStamp2Str(time_t timeStamp) {
        timeStamp += 8ULL * 3600; // UTC+8
        std::tm* ptm = std::gmtime(&timeStamp);
        std::stringstream ss;
        ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S UTC+8");
        return ss.str();
    }

    static HWND getCvWindow(LPCSTR lpWndName) {
        HWND hWnd = (HWND)cvGetWindowHandle(lpWndName);
        if (IsWindow(hWnd)) {
            HWND hParent = GetParent(hWnd);
            DWORD dwPid = 0;
            GetWindowThreadProcessId(hWnd, &dwPid);
            if (dwPid == GetCurrentProcessId()) {
                return hParent;
            }
        }
        return hWnd;
    }

    static void setCvWindowIcon(HINSTANCE hInstance, HWND hWnd, WORD wIconId) {
        if (IsWindow(hWnd)) {
            HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(wIconId));
            SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }
}