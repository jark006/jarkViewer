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
#include<stdexcept>
#include <ranges>

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
#include <dwmapi.h>

#include <mfapi.h>
#include <mfidl.h>
#include <shlwapi.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>

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

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#include<opencv2/core.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>


#define TIME_COUNT_START auto start_clock = std::chrono::system_clock::now()
#define TIME_COUNT_END jarkUtils::log("{}(): {} ms", __FUNCTION__, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_clock).count())
#define TIME_COUNT_END_US jarkUtils::log("{}(): {} us", __FUNCTION__, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_clock).count())
#define TIME_COUNT_END_NS jarkUtils::log("{}(): {} ns", __FUNCTION__, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - start_clock).count())

struct SettingParameter {
    uint8_t header[32];
    RECT rect{};                           // 窗口大小位置
    uint32_t showCmd = SW_MAXIMIZE;        // 窗口模式

    int printerBrightness = 100;           // 亮度调整 (0 ~ 200)
    int printerContrast = 100;             // 对比度调整 (0 ~ 200)
    int printercolorMode = 1;              // 颜色模式
    bool printerInvertColors = false;      // 是否反相
    bool printerBalancedBrightness = false;// 是否均衡亮度 文档优化

    bool reserve1 = false;
    bool reserve2 = false;

    bool isAllowRotateAnimation = true;
    bool isAllowZoomAnimation = true;
    bool reserve3 = false;
    bool reserve4 = false;
    int switchImageAnimationMode = 0;       // 0: 无动画  1:上下滑动  2:左右滑动

    int pptOrder = 0;                       // 幻灯片模式  0: 顺序  1:逆序  2:随机
    int pptTimeout = 5;                     // 幻灯片模式  切换间隔 1 ~ 300 秒

    uint32_t reserve[803];

    // 常见格式
    char extCheckedListStr[800] = "apng,avif,bmp,bpg,gif,heic,heif,ico,jfif,jp2,jpe,jpeg,jpg,jxl,jxr,livp,png,qoi,svg,tga,tif,tiff,webp,wp2";

};

static_assert(sizeof(SettingParameter) == 4096, "sizeof(SettingParameter) != 4096");

struct rcFileInfo {
    uint8_t* ptr = nullptr;
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
    none = 0, newSize, slide, preImg, nextImg, zoomIn, zoomOut, toggleExif, toggleFullScreen, requestExit, normalFresh,
    rotateLeft, rotateRight, printImage, setting,
};

enum class CursorPos :int {
    centerArea = 0, leftUp, leftDown, leftEdge, rightEdge, rightDown, rightUp, centerTop,
};

enum class ShowExtraUI :int {
    none = 0, rotateLeftButton, printer, leftArrow, rightArrow, setting, rotateRightButton, animationBar
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

class jarkUtils {
public:

    template<typename... Args>
    static void log(const std::string_view fmt, Args&&... args) {
#ifndef NDEBUG
        const bool TO_LOG_FILE = false;
        FILE* fp = nullptr;

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::current_zone()->to_local(now);

        string str = std::format("[{:%H:%M:%S}] ", time) +
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

    static bool is_power_of_two(int64_t num);

    static string bin2Hex(const void* bytes, const size_t len);

    static std::wstring ansiToWstring(const std::string& str);

    static std::string wstringToAnsi(const std::wstring& wstr);

    //UTF8 to UTF16
    static std::wstring utf8ToWstring(const std::string& str);

    //UTF16 to UTF8
    static std::string wstringToUtf8(const std::wstring& wstr);

    static std::wstring latin1ToWstring(const std::string& str);

    static std::string utf8ToAnsi(const std::string& str);

    static std::string ansiToUtf8(const std::string& str);

    static rcFileInfo GetResource(unsigned int idi, const wchar_t* type);

    static string size2Str(const size_t fileSize);

    static string timeStamp2Str(time_t timeStamp);

    static WinSize getWindowSize(HWND hwnd);

    // 设置窗口图标
    static void setWindowIcon(HWND hWnd, WORD wIconId);
    
    // 禁止窗口调整尺寸
    static void disableWindowResize(HWND hwnd);

    static bool copyToClipboard(const std::wstring& text);

    static bool limitSizeTo16K(cv::Mat& image);

    // Alpha透明通道混合白色背景
    static void flattenRGBAonWhite(cv::Mat& image);

    static void copyImageToClipboard(const cv::Mat& image);

    // 创建路径几何图形
    static ID2D1PathGeometry* GetPathGeometry(ID2D1Factory4* pD2DFactory, D2D1_POINT_2F* points, UINT pointsCount);

    static void ToggleFullScreen(HWND hwnd);

    // 假设 canvas 完全没有透明像素
    static void overlayImg(cv::Mat& canvas, cv::Mat& img, int xOffset, int yOffset);

    // 选取文件
    static std::wstring SelectFile(HWND hWnd);

    // 图像另存为 选取文件路径 ANSI/GBK
    static std::string saveImageDialog();

    static void openUrl(const wchar_t* url);

    static std::vector<std::string> splitString(std::string_view str, std::string_view delim);

    static void stringReplace(std::string& src, std::string_view oldBlock, std::string_view newBlock);

    static std::vector<std::wstring> splitWstring(std::wstring_view str, std::wstring_view delim);

    static void wstringReplace(std::wstring& src, std::wstring_view oldBlock, std::wstring_view newBlock);

    static void activateWindow(HWND hwnd);
};