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
#include<set>
#include<map>
#include<unordered_set>
#include<unordered_map>

using std::vector;
using std::string;
using std::wstring;
using std::string_view;
using std::set;
using std::map;
using std::unordered_set;
using std::unordered_map;
using std::cout;
using std::cerr;
using std::endl;

#include "framework.h"
#include "resource.h"
#include "psapi.h"

#include <dxgi1_4.h>
#include <D3D11.h>
#include <d2d1_3.h>
#include <d2d1_3helper.h>
#include <dwrite_2.h>
#include <wincodec.h>
#include <imm.h>
#include <commdlg.h>
#include <shellapi.h>
#include <winspool.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwrite.lib" )
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib" )
#pragma comment(lib, "windowscodecs.lib" )
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib ,"imm32.lib")
#pragma comment(lib, "dwmapi.lib")

#include<opencv2/core.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>
#include<opencv2/highgui/highgui_c.h>


#define START_TIME_COUNT auto start_clock = std::chrono::system_clock::now()
#define END_TIME_COUNT auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_clock).count();\
                                            Utils::log("{}(): {} ms", __FUNCTION__, duration_ms)
#define END_TIME_COUNT_US auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_clock).count();\
                                            Utils::log("{}(): {} us", __FUNCTION__, duration_us)

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

    void operator+=(const Cood& t) {
        this->x += t.x;
        this->y += t.y;
    }

    void operator+=(const int i) {
        this->x += i;
        this->y += i;
    }

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

    bool operator==(const Cood& t) const {
        return (this->x == t.x) && (this->y == t.y);
    }

    void operator=(int n) {
        this->x = n;
        this->y = n;
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

enum class ActionENUM:int64_t {
    none = 0, newSize, slide, preImg, nextImg, zoomIn, zoomOut, toggleExif, toggleFullScreen, requitExit, normalFresh, rotateLeft, rotateRight, printImage
};

enum class CursorPos :int {
    leftUp = -3, leftDown = -2, leftEdge = -1, centerArea = 0, rightEdge = 1, rightDown = 2, rightUp = 3,
};

enum class ShowExtraUI :int {
    rotateLeftButton = -3, printer = -2, leftArrow = -1, none = 0, rightArrow = 1, reserve = 2, rotateRightButton = 3,
};

struct Action {
    ActionENUM action = ActionENUM::none;
    union {
        int x;
        int width;
    };
    union {
        int y;
        int height;
    };
};

struct WinSize {
    int width = 600;
    int height = 400;
    WinSize(){}
    WinSize(int w, int h) :width(w), height(h) {}

    bool operator==(const WinSize size) const {
        return this->width == size.width && this->height == size.height;
    }
    bool operator!=(const WinSize size) const {
        return this->width != size.width || this->height != size.height;
    }
    bool isZero() const {
        return this->width == 0 && this->height == 0;
    }
};

struct MatPack {
    cv::Mat* matPtr = nullptr;
    wstring* titleStrPtr = nullptr;
    void clear() {
        matPtr = nullptr;
        titleStrPtr = nullptr;
    }
};

class Utils {
public:

    template<typename... Args>
    static void log(const string_view fmt, Args&&... args) {
#ifndef NDEBUG
        static const bool TO_LOG_FILE = false;
        static FILE* fp = nullptr;

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::current_zone()->to_local(now);

        string str = std::format("[{:%H:%M:%S}] ", time)+
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
#else
        return;
#endif
    }

    static bool is_power_of_two(int64_t num) {
        return (num <= 0) ? 0 : ((num & (num - 1)) == 0);
    }

    static string bin2Hex(const void* bytes, const size_t len) {
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

    static std::wstring ansiToWstring(const std::string& str) {
        if (str.empty())return L"";

        int wcharLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), nullptr, 0);
        if (wcharLen == 0) return L"";

        std::wstring ret(wcharLen, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

        return ret;
    }

    static std::string wstringToAnsi(const std::wstring& wstr) {
        if (wstr.empty())return "";

        int byteLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
        if (byteLen == 0) return "";

        std::string ret(byteLen, 0);
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
        return ret;
    }

    //UTF8 to UTF16
    static std::wstring utf8ToWstring(const std::string& str) {
        if (str.empty())return L"";

        int wcharLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
        if (wcharLen == 0) return L"";

        std::wstring ret(wcharLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

        return ret;
    }

    //UTF16 to UTF8
    static std::string wstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty())return "";

        int byteLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
        if (byteLen == 0) return "";

        std::string ret(byteLen, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
        return ret;
    }

    static std::wstring latin1ToWstring(const std::string& str) {
        if (str.empty())return L"";

        int wcharLen = MultiByteToWideChar(1252, 0, str.c_str(), (int)str.length(), nullptr, 0);
        if (wcharLen == 0) return L"";

        std::wstring ret(wcharLen, 0);
        MultiByteToWideChar(1252, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

        return ret;
    }

    static std::string utf8ToAnsi(const std::string& str) {
        return wstringToAnsi(utf8ToWstring(str));
    }

    static std::string ansiToUtf8(const std::string& str) {
        return wstringToUtf8(ansiToWstring(str));
    }

    static rcFileInfo GetResource(unsigned int idi, const wchar_t* type) {
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

    static string size2Str(const size_t fileSize) {
        if (fileSize < 1024) return std::format("{} Bytes", fileSize);
        if (fileSize < 1024ULL * 1024) return std::format("{:.1f} KiB", fileSize / 1024.0);
        if (fileSize < 1024ULL * 1024 * 1024) return std::format("{:.1f} MiB", fileSize / (1024.0 * 1024));
        return std::format("{:.1f} GiB", fileSize / (1024.0 * 1024 * 1024));
    }

    static string timeStamp2Str(time_t timeStamp) {
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

    static WinSize getWindowSize(HWND hwnd) {
        RECT rect;
        if (GetClientRect(hwnd, &rect)) {
            return { rect.right,rect.bottom};
        }
        else {
            log("getWindowSize fail.");
            return { 0,0 };
        }
    }

    static void setCvWindowIcon(HINSTANCE hInstance, HWND hWnd, WORD wIconId) {
        if (IsWindow(hWnd)) {
            HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(wIconId));
            SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }

    static bool copyToClipboard(const std::wstring& text) {
        if (!OpenClipboard(nullptr)) {
            return false;
        }

        EmptyClipboard();

        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (!hGlob) {
            CloseClipboard();
            return false;
        }

        auto desPtr = GlobalLock(hGlob);
        if(desPtr)
            memcpy(desPtr, text.c_str(), (text.size() + 1) * sizeof(wchar_t));

        GlobalUnlock(hGlob);

        if (!SetClipboardData(CF_UNICODETEXT, hGlob)) {
            CloseClipboard();
            GlobalFree(hGlob);
            return false;
        }

        CloseClipboard();
        return true;
    }


    static void copyImageToClipboard(const cv::Mat& image) {
        if (image.empty()) {
            MessageBoxW(NULL, L"图像为空，无法复制到剪贴板", L"错误", MB_OK | MB_ICONERROR);
            return;
        }

        cv::Mat processedImage;

        // 转换为BGRA格式
        try {
            if (image.channels() == 1) {
                cv::cvtColor(image, processedImage, cv::COLOR_GRAY2BGRA);
            }
            else if (image.channels() == 3) {
                cv::cvtColor(image, processedImage, cv::COLOR_BGR2BGRA);
            }
            else if (image.channels() == 4) {
                processedImage = image.clone();
            }
            else {
                MessageBoxW(NULL, std::format(L"图像通道: {}", processedImage.channels()).c_str(), L"不支持的图像格式", MB_OK | MB_ICONERROR);
                return;
            }
        }
        catch (const cv::Exception& e) {
            MessageBoxW(NULL, Utils::utf8ToWstring(e.what()).c_str(), L"图像格式转换失败", MB_OK | MB_ICONERROR);
            return;
        }

        // 检查并缩放图像（保持宽高比）
        if (processedImage.cols > 16384 || processedImage.rows > 16384) {
            try {
                double scale = std::min(16384.0 / processedImage.cols, 16384.0 / processedImage.rows);
                int newWidth = static_cast<int>(processedImage.cols * scale);
                int newHeight = static_cast<int>(processedImage.rows * scale);
                cv::resize(processedImage, processedImage, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);
                MessageBoxW(NULL, std::format(L"图像分辨率太大，将缩放到 {}x{}", processedImage.cols, processedImage.rows).c_str(), L"提示", MB_OK | MB_ICONWARNING);
            }
            catch (const cv::Exception& e) {
                MessageBoxW(NULL, L"图像缩放失败", L"错误", MB_OK | MB_ICONERROR);
                return;
            }
        }

        const int width = processedImage.cols;
        const int height = processedImage.rows;


        // 打开剪贴板
        if (!OpenClipboard(NULL)) {
            MessageBoxW(NULL, L"无法打开剪贴板", L"错误", MB_OK | MB_ICONERROR);
            return;
        }

        // 清空剪贴板
        if (!EmptyClipboard()) {
            CloseClipboard();
            MessageBoxW(NULL, L"清空剪贴板失败", L"错误", MB_OK | MB_ICONERROR);
            return;
        }

        size_t dibStride = width * 4ULL;
        int imageDataSize = dibStride * height;

        // 创建CF_DIBV5格式数据
        int dibv5Size = sizeof(BITMAPV5HEADER) + imageDataSize;
        HGLOBAL hDibV5 = GlobalAlloc(GMEM_MOVEABLE, dibv5Size);
        if (hDibV5) {
            LPVOID pDibV5 = GlobalLock(hDibV5);
            if (pDibV5) {
                // 填充BITMAPV5HEADER
                BITMAPV5HEADER* pBmpV5Header = static_cast<BITMAPV5HEADER*>(pDibV5);
                ZeroMemory(pBmpV5Header, sizeof(BITMAPV5HEADER));

                pBmpV5Header->bV5Size = sizeof(BITMAPV5HEADER);
                pBmpV5Header->bV5Width = width;
                pBmpV5Header->bV5Height = height;
                pBmpV5Header->bV5Planes = 1;
                pBmpV5Header->bV5BitCount = 32;
                pBmpV5Header->bV5Compression = BI_BITFIELDS;
                pBmpV5Header->bV5SizeImage = imageDataSize;
                pBmpV5Header->bV5XPelsPerMeter = 0;
                pBmpV5Header->bV5YPelsPerMeter = 0;
                pBmpV5Header->bV5ClrUsed = 0;
                pBmpV5Header->bV5ClrImportant = 0;

                // 设置颜色掩码 (BGRA格式)
                pBmpV5Header->bV5RedMask = 0x00FF0000;
                pBmpV5Header->bV5GreenMask = 0x0000FF00;
                pBmpV5Header->bV5BlueMask = 0x000000FF;
                pBmpV5Header->bV5AlphaMask = 0xFF000000;

                // 设置颜色空间
                pBmpV5Header->bV5CSType = LCS_sRGB;
                pBmpV5Header->bV5Intent = LCS_GM_GRAPHICS;

                // 复制图像数据
                BYTE* pImageData = static_cast<BYTE*>(pDibV5) + sizeof(BITMAPV5HEADER);
                for (int y = 0; y < height; y++) {
                    const BYTE* srcRow = processedImage.ptr<BYTE>(height - 1 - y);
                    BYTE* dstRow = pImageData + y * dibStride;
                    memcpy(dstRow, srcRow, width * 4ULL);
                }

                GlobalUnlock(hDibV5);

                // 设置到剪贴板
                if (!SetClipboardData(CF_DIBV5, hDibV5)) {
                    GlobalFree(hDibV5);
                }
            }
            else {
                GlobalFree(hDibV5);
            }
        }

        // 将透明背景混合白色背景
        for (int y = 0; y < height; y++) {
            BYTE* srcRow = processedImage.ptr<BYTE>(y);

            for (int x = 0; x < width; x++) {
                BYTE b = srcRow[x * 4];
                BYTE g = srcRow[x * 4 + 1];
                BYTE r = srcRow[x * 4 + 2];
                BYTE a = srcRow[x * 4 + 3];

                if (a == 0) {
                    srcRow[x * 4] = 255;
                    srcRow[x * 4 + 1] = 255;
                    srcRow[x * 4 + 2] = 255;
                    srcRow[x * 4 + 3] = 255;
                }
                else if (a != 255) {
                    // Alpha混合白色背景 (255, 255, 255)
                    srcRow[x * 4] = static_cast<BYTE>((b * a + 255 * (255 - a) + 255) >> 8);     // B
                    srcRow[x * 4 + 1] = static_cast<BYTE>((g * a + 255 * (255 - a) + 255) >> 8); // G
                    srcRow[x * 4 + 2] = static_cast<BYTE>((r * a + 255 * (255 - a) + 255) >> 8); // R
                    srcRow[x * 4 + 3] = 255; // Alpha设为不透明
                }
            }
        }

        // 创建CF_DIB格式数据
        int dibSize = sizeof(BITMAPINFOHEADER) + imageDataSize;
        HGLOBAL hDib = GlobalAlloc(GMEM_MOVEABLE, dibSize);
        if (hDib) {
            LPVOID pDib = GlobalLock(hDib);
            if (pDib) {
                // 填充BITMAPINFOHEADER
                BITMAPINFOHEADER* pBmpHeader = static_cast<BITMAPINFOHEADER*>(pDib);
                ZeroMemory(pBmpHeader, sizeof(BITMAPINFOHEADER));

                pBmpHeader->biSize = sizeof(BITMAPINFOHEADER);
                pBmpHeader->biWidth = width;
                pBmpHeader->biHeight = height;
                pBmpHeader->biPlanes = 1;
                pBmpHeader->biBitCount = 32;
                pBmpHeader->biCompression = BI_RGB; // CF_DIB通常使用BI_RGB
                pBmpHeader->biSizeImage = imageDataSize;

                // 复制图像数据
                BYTE* pImageData = static_cast<BYTE*>(pDib) + sizeof(BITMAPINFOHEADER);
                for (int y = 0; y < height; y++) {
                    const BYTE* srcRow = processedImage.ptr<BYTE>(height - 1 - y);
                    BYTE* dstRow = pImageData + y * dibStride;
                    memcpy(dstRow, srcRow, width * 4ULL);
                }

                GlobalUnlock(hDib);

                // 设置到剪贴板
                if (!SetClipboardData(CF_DIB, hDib)) {
                    GlobalFree(hDib);
                }
            }
            else {
                GlobalFree(hDib);
            }
        }

        // 关闭剪贴板
        CloseClipboard();
    }


    // 将cv::Mat转换为HBITMAP
    static HBITMAP MatToHBITMAP(const cv::Mat& mat) {

        const int width = mat.cols;
        const int height = mat.rows;

        cv::Mat bgrMat;
        if (mat.channels() == 1) {
            cv::cvtColor(mat, bgrMat, cv::COLOR_GRAY2BGR);
        }
        else if (mat.channels() == 4) {
            bgrMat = cv::Mat(height, width, CV_8UC3);
            for (int y = 0; y < height; y++) {
                const BYTE* srcRow = mat.ptr<BYTE>(y);
                BYTE* desRow = bgrMat.ptr<BYTE>(y);

                for (int x = 0; x < width; x++) {
                    BYTE b = srcRow[x * 4];
                    BYTE g = srcRow[x * 4 + 1];
                    BYTE r = srcRow[x * 4 + 2];
                    BYTE a = srcRow[x * 4 + 3];

                    if (a == 0) {
                        desRow[x * 3] = 255;
                        desRow[x * 3 + 1] = 255;
                        desRow[x * 3 + 2] = 255;
                    }
                    else if (a == 255) {
                        desRow[x * 3] = b;
                        desRow[x * 3 + 1] = g;
                        desRow[x * 3 + 2] = r;
                    }
                    else {
                        // Alpha混合白色背景 (255, 255, 255)
                        desRow[x * 3] = static_cast<BYTE>((b * a + 255 * (255 - a) + 255) >> 8);     // B
                        desRow[x * 3 + 1] = static_cast<BYTE>((g * a + 255 * (255 - a) + 255) >> 8); // G
                        desRow[x * 3 + 2] = static_cast<BYTE>((r * a + 255 * (255 - a) + 255) >> 8); // R
                    }
                }
            }
        }

        const int stride = (width * 3 + 3) & ~3;  // 4字节对齐
        const size_t dataSize = static_cast<size_t>(stride) * height;

        // 准备BITMAPINFO结构
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;          // 24位RGB
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = dataSize;

        // 创建DIB并复制数据
        HDC hdcScreen = GetDC(NULL);
        void* pBits;
        HBITMAP hBitmap = CreateDIBSection(
            hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        ReleaseDC(NULL, hdcScreen);

        if (hBitmap) {
            for (int y = 0; y < height; y++) {
                const BYTE* srcRow = bgrMat.ptr<BYTE>(height - 1 - y);
                BYTE* dstRow = static_cast<BYTE*>(pBits) + y * stride;
                memcpy(dstRow, srcRow, stride);
            }
        }

        return hBitmap;
    }

    // 打印图像函数
    static bool PrintMatImage(const cv::Mat& image) {
        // 转换cv::Mat为HBITMAP
        HBITMAP hBitmap = MatToHBITMAP(image);
        if (!hBitmap) {
            return false;
        }

        // 初始化打印对话框
        PRINTDLG pd;
        ZeroMemory(&pd, sizeof(pd));
        pd.lStructSize = sizeof(pd);
        pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;

        // 显示打印对话框
        if (!PrintDlg(&pd)) {
            DeleteObject(hBitmap);
            return false;
        }

        // 准备文档信息
        DOCINFO di;
        ZeroMemory(&di, sizeof(di));
        di.cbSize = sizeof(di);
        di.lpszDocName = L"JarkViewer Printed Image";
        di.lpszOutput = nullptr;

        // 开始打印作业
        if (StartDoc(pd.hDC, &di) <= 0) {
            DeleteObject(hBitmap);
            DeleteDC(pd.hDC);
            return false;
        }

        // 开始新页面
        if (StartPage(pd.hDC) <= 0) {
            EndDoc(pd.hDC);
            DeleteObject(hBitmap);
            DeleteDC(pd.hDC);
            return false;
        }

        // 获取打印机和图像尺寸
        int pageWidth = GetDeviceCaps(pd.hDC, HORZRES);
        int pageHeight = GetDeviceCaps(pd.hDC, VERTRES);

        BITMAP bm;
        GetObject(hBitmap, sizeof(BITMAP), &bm);
        int imgWidth = bm.bmWidth;
        int imgHeight = bm.bmHeight;

        // 计算缩放比例（保持宽高比）
        double scale = std::min(
            static_cast<double>(pageWidth) / imgWidth,
            static_cast<double>(pageHeight) / imgHeight
        );

        scale *= (pageHeight > pageWidth ? 0.893 : 0.879); // 按标准A4边距 1.0-31.8/297  1.0-2.54/210

        int scaledWidth = static_cast<int>(imgWidth * scale);
        int scaledHeight = static_cast<int>(imgHeight * scale);

        // 居中位置
        int xPos = (pageWidth - scaledWidth) / 2;
        int yPos = (pageHeight - scaledHeight) / 2;

        // 创建内存DC
        HDC hdcMem = CreateCompatibleDC(pd.hDC);
        SelectObject(hdcMem, hBitmap);

        // 高质量缩放
        SetStretchBltMode(pd.hDC, HALFTONE);
        SetBrushOrgEx(pd.hDC, 0, 0, nullptr);

        // 绘制到打印机DC
        StretchBlt(
            pd.hDC, xPos, yPos, scaledWidth, scaledHeight,
            hdcMem, 0, 0, imgWidth, imgHeight, SRCCOPY
        );

        // 清理资源
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);

        // 结束页面和文档
        EndPage(pd.hDC);
        EndDoc(pd.hDC);
        DeleteDC(pd.hDC);

        return true;
    }


    template<class Interface>
    static inline void SafeRelease(Interface*& pInterfaceToRelease) {
        if (pInterfaceToRelease == nullptr)
            return;

        pInterfaceToRelease->Release();
        pInterfaceToRelease = nullptr;
    }


    // 从文件读取位图
    static HRESULT LoadBitmapFromFile(
        ID2D1DeviceContext* IN pRenderTarget,
        IWICImagingFactory2* IN pIWICFactory,
        PCWSTR IN uri,
        UINT OPTIONAL width,
        UINT OPTIONAL height,
        ID2D1Bitmap1** OUT ppBitmap)
    {
        IWICBitmapDecoder* pDecoder = nullptr;
        IWICBitmapFrameDecode* pSource = nullptr;
        IWICStream* pStream = nullptr;
        IWICFormatConverter* pConverter = nullptr;
        IWICBitmapScaler* pScaler = nullptr;

        HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
            uri,
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &pDecoder);

        if (SUCCEEDED(hr))
        {
            hr = pDecoder->GetFrame(0, &pSource);
        }
        if (SUCCEEDED(hr))
        {
            hr = pIWICFactory->CreateFormatConverter(&pConverter);
        }

        if (SUCCEEDED(hr))
        {
            if (width != 0 || height != 0)
            {
                UINT originalWidth, originalHeight;
                hr = pSource->GetSize(&originalWidth, &originalHeight);
                if (SUCCEEDED(hr))
                {
                    if (width == 0)
                    {
                        FLOAT scalar = static_cast<FLOAT>(height) / static_cast<FLOAT>(originalHeight);
                        width = static_cast<UINT>(scalar * static_cast<FLOAT>(originalWidth));
                    }
                    else if (height == 0)
                    {
                        FLOAT scalar = static_cast<FLOAT>(width) / static_cast<FLOAT>(originalWidth);
                        height = static_cast<UINT>(scalar * static_cast<FLOAT>(originalHeight));
                    }

                    hr = pIWICFactory->CreateBitmapScaler(&pScaler);
                    if (SUCCEEDED(hr))
                    {
                        hr = pScaler->Initialize(
                            pSource,
                            width,
                            height,
                            WICBitmapInterpolationModeCubic);
                    }
                    if (SUCCEEDED(hr))
                    {
                        hr = pConverter->Initialize(
                            pScaler,
                            GUID_WICPixelFormat32bppPBGRA,
                            WICBitmapDitherTypeNone,
                            nullptr,
                            0.f,
                            WICBitmapPaletteTypeMedianCut);
                    }
                }
            }
            else
            {
                hr = pConverter->Initialize(
                    pSource,
                    GUID_WICPixelFormat32bppPBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.f,
                    WICBitmapPaletteTypeMedianCut);
            }
        }
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateBitmapFromWicBitmap(
                pConverter,
                nullptr,
                ppBitmap);
        }

        SafeRelease(pDecoder);
        SafeRelease(pSource);
        SafeRelease(pStream);
        SafeRelease(pConverter);
        SafeRelease(pScaler);

        return hr;
    }

    // 创建路径几何图形
    static ID2D1PathGeometry* GetPathGeometry(ID2D1Factory4* pD2DFactory, D2D1_POINT_2F* points, UINT pointsCount)
    {
        ID2D1PathGeometry* geometry = NULL;
        HRESULT hr = pD2DFactory->CreatePathGeometry(&geometry);

        if (SUCCEEDED(hr))
        {
            ID2D1GeometrySink* pSink = NULL;
            hr = geometry->Open(&pSink); // 获取Sink对象

            if (SUCCEEDED(hr))
            {
                pSink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);

                pSink->AddLines(points, pointsCount);

                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }

            pSink->Close(); // 关闭Sink对象

            return geometry;
        }

        return NULL;
    }

    static void ToggleFullScreen(HWND hwnd) {
        static RECT preRect{};
        static DWORD preStyle = 0;
        static DWORD preExStyle = 0;

        static bool isFullScreen = false;

        if (isFullScreen) {
            // 退出全屏模式，恢复之前的窗口状态
            SetWindowLong(hwnd, GWL_STYLE, preStyle);
            SetWindowLong(hwnd, GWL_EXSTYLE, preExStyle);
            SetWindowPos(hwnd, NULL, preRect.left, preRect.top,
                preRect.right - preRect.left,
                preRect.bottom - preRect.top,
                SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        else {
            // 保存当前窗口状态
            GetWindowRect(hwnd, &preRect);
            preStyle = GetWindowLong(hwnd, GWL_STYLE);
            preExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

            // 切换到全屏模式
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo = { sizeof(monitorInfo) };
            GetMonitorInfo(monitor, &monitorInfo);

            SetWindowLong(hwnd, GWL_STYLE, preStyle & ~(WS_CAPTION | WS_THICKFRAME));
            SetWindowLong(hwnd, GWL_EXSTYLE, preExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

            SetWindowPos(hwnd, HWND_TOP,
                monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_NOZORDER | SWP_FRAMECHANGED);
        }

        isFullScreen = !isFullScreen;
    }

    // 假设 canvas 完全没有透明像素
    static void overlayImg(cv::Mat& canvas, cv::Mat& img, int xOffset, int yOffset) {
        if (canvas.type() != CV_8UC4 || img.type() != CV_8UC4)
            return;

        int canvasHeight = canvas.rows;
        int canvasWidth = canvas.cols;
        int imgHeight = img.rows;
        int imgWidth = img.cols;

        for (int y = 0; y < imgHeight; y++) {
            if ((yOffset + y) <= 0 || canvasHeight <= (yOffset + y))
                continue;

            auto canvasPtr = (intUnion*)(canvas.ptr() + canvas.step1() * (yOffset + y));
            auto imgPtr = (intUnion*)(img.ptr() + img.step1() * y);
            for (int x = 0; x < imgWidth; x++) {
                if ((xOffset + x) <= 0 || canvasWidth <= (xOffset + x))
                    continue;

                auto& canvasPx = canvasPtr[xOffset + x];
                auto imgPx = imgPtr[x];
                uint32_t alpha = imgPx[3];

                if (alpha == 255) {
                    canvasPx = imgPx;
                }
                else if (alpha) {
                    uint32_t inv_alpha = 255 - alpha;
                    canvasPx = {
                        (uint8_t)(((canvasPx[0] * inv_alpha + imgPx[0] * alpha) + 128) >> 8), // +128 四舍五入
                        (uint8_t)(((canvasPx[1] * inv_alpha + imgPx[1] * alpha) + 128) >> 8),
                        (uint8_t)(((canvasPx[2] * inv_alpha + imgPx[2] * alpha) + 128) >> 8),
                        255 };
                }
            }
        }
    }

    static std::wstring SelectFile(HWND hWnd) {
        OPENFILENAMEW ofn;
        wchar_t szFile[1024] = { 0 };

        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.hwndOwner = hWnd;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        ofn.lpstrFilter = L"All Files\0*.*\0"; // 过滤器
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameW(&ofn) == TRUE) {
            return std::wstring(szFile);
        }
        else {
            return L"";
        }
    }

};