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
    none = 0, newSize, slide, preImg, nextImg, zoomIn, zoomOut, toggleExif, toggleFullScreen, requitExit, normalFresh, rotateLeft, rotateRight
};

enum class CursorPos :int {
    leftUp = -3, leftDown = -2, leftEdge = -1, centerArea = 0, rightEdge = 1, rightDown = 2, rightUp = 3,
};

enum class ShowExtraUI :int {
    rotateLeftButton = -2, leftArrow = -1, none = 0, rightArrow = 1, rotateRightButton = 2,
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

                auto imgPx = imgPtr[x];
                int alpha = imgPx[3];
                if (alpha) {
                    auto& canvasPx = canvasPtr[xOffset + x];
                    canvasPx = {
                        (uint8_t)((canvasPx[0] * (255 - alpha) + imgPx[0] * alpha) / 255),
                        (uint8_t)((canvasPx[1] * (255 - alpha) + imgPx[1] * alpha) / 255),
                        (uint8_t)((canvasPx[2] * (255 - alpha) + imgPx[2] * alpha) / 255),
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