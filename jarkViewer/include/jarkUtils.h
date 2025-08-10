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
#include<array>
#include<set>
#include<map>
#include<unordered_set>
#include<unordered_map>
#include<stdexcept>
#include<ranges>

using std::vector;
using std::string;
using std::wstring;
using std::string_view;
using std::wstring_view;
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
#include <shlobj.h>

#pragma comment(lib, "Psapi.lib")
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
    // 常见格式
    static inline std::string_view defaultExtList{ 
        "apng,avif,bmp,bpg,gif,heic,heif,ico,jfif,jp2,jpe,jpeg,jpg,jxl,jxr,livp,pbm,pfm,pgm,png,pnm,ppm,qoi,svg,tga,tif,tiff,webp,wp2" };

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
    bool isOptimizeSlide = true;            // 优化图像平移性能 （实为渲染工作量偷懒减半）
    bool reserve4 = false;
    int switchImageAnimationMode = 0;       // 0: 无动画  1:上下滑动  2:左右滑动

    int pptOrder = 0;                       // 幻灯片模式  0: 顺序  1:逆序  2:随机
    int pptTimeout = 5;                     // 幻灯片模式  切换间隔 1 ~ 300 秒

    int UI_Mode = 0;                        // 界面主题 0:跟随系统  1:浅色  2:深色

    uint32_t reserve[802];

    char extCheckedListStr[800];

    SettingParameter() {
        memcpy(extCheckedListStr, defaultExtList.data(), defaultExtList.length() + 1);
    }
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

template <>
struct std::formatter<Cood> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const Cood& cood, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "(x={}, y={})", cood.x, cood.y);
    }
};

enum class ImageFormat {
    None = 0,       // 解码失败
    Still,          // 静态图: jpg/bmp ...
    Animated,       // 动态图: gif/apng ...
    //LivePhoto       // 实况图: livp/MVIMG ...
};

struct ImageAsset {
    ImageFormat format;                 // 图像类型：静态/动图/实况
    cv::Mat primaryFrame;               // 静态图或实况的静态图
    std::vector<cv::Mat> frames;        // 动态图或实况的视频
    std::vector<int> frameDurations;    // 每帧时长
    string exifInfo;                    // 图像EXIF等信息
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
    static void log(std::string_view fmt, Args&&... args) {
#ifndef NDEBUG
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::current_zone()->to_local(now);
        auto str = std::format("[{:%H:%M:%S}] {}\n", time, std::vformat(fmt, std::make_format_args(args...)));
        std::cout << str;
#endif
    }

    template<typename... Args>
    static void log(std::wstring_view fmt, Args&&... args) {
#ifndef NDEBUG
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::current_zone()->to_local(now);
        auto str = std::format(L"[{:%H:%M:%S}] {}\n", time, std::vformat(fmt, std::make_wformat_args(args...)));
        std::cout << jarkUtils::wstringToUtf8(str);
#endif
    }

    static bool is_power_of_two(int64_t num);

    static string bin2Hex(const void* bytes, const size_t len);

    static std::wstring ansiToWstring(string_view str);

    static std::string wstringToAnsi(wstring_view wstr);

    static std::wstring utf8ToWstring(string_view str);

    static std::string wstringToUtf8(wstring_view wstr);

    static std::wstring latin1ToWstring(string_view str);

    static std::string utf8ToAnsi(string_view str);

    static std::string ansiToUtf8(string_view str);

    static rcFileInfo GetResource(unsigned int idi, const wchar_t* type);

    static string size2Str(const size_t fileSize);

    static string timeStamp2Str(time_t timeStamp);

    static WinSize getWindowSize(HWND hwnd);

    // 设置窗口图标
    static void setWindowIcon(HWND hWnd, WORD wIconId);
    
    // 禁止窗口调整尺寸
    static void disableWindowResize(HWND hwnd);

    static bool copyToClipboard(wstring_view text);

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

    static inline const char COMPILE_DATE_TIME[32] = {
        __DATE__[7],
        __DATE__[8],
        __DATE__[9],
        __DATE__[10],// YYYY year
        '-',

        // First month letter, Oct Nov Dec = '1' otherwise '0'
        (__DATE__[0] == 'O' || __DATE__[0] == 'N' || __DATE__[0] == 'D') ? '1' : '0',

        // Second month letter Jan, Jun or Jul
        (__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? '1'
        : ((__DATE__[2] == 'n') ? '6' : '7'))
        : (__DATE__[0] == 'F') ? '2'// Feb
        : (__DATE__[0] == 'M') ? (__DATE__[2] == 'r') ? '3' : '5'// Mar or May
        : (__DATE__[0] == 'A') ? (__DATE__[1] == 'p') ? '4' : '8'// Apr or Aug
        : (__DATE__[0] == 'S') ? '9'// Sep
        : (__DATE__[0] == 'O') ? '0'// Oct
        : (__DATE__[0] == 'N') ? '1'// Nov
        : (__DATE__[0] == 'D') ? '2'// Dec
        : 'X',

        '-',
        __DATE__[4] == ' ' ? '0' : __DATE__[4],// First day letter, replace space with digit
        __DATE__[5],// Second day letter
        ' ',
        __TIME__[0],
        __TIME__[1],
        __TIME__[2],
        __TIME__[3],
        __TIME__[4],
        __TIME__[5],
        __TIME__[6],
        __TIME__[7],
        '\0',
    };
};

class FunctionTimeCount {
public:
#ifndef NDEBUG
    string_view funcName;
    std::chrono::system_clock::time_point start_clock;

    FunctionTimeCount(string_view funcName) : funcName(funcName) {
        start_clock = std::chrono::system_clock::now();
    }

    ~FunctionTimeCount() {
        jarkUtils::log("{}(): {} ms", funcName, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_clock).count());
    }
#else
    FunctionTimeCount(string_view funcName) {}
#endif
};