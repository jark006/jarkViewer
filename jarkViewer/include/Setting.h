#pragma once

#include "jarkUtils.h"
#include "TextDrawer.h"
#include "D2D1App.h"

extern const wstring appBuildInfo;
extern const wstring jarkLink;
extern const wstring GithubLink;
extern const wstring BaiduLink;
extern const wstring LanzouLink;

struct SettingParams {
    const char* windowsName = nullptr;
    bool isParamsChange = false;
    bool reserve1 = false;
    bool reserve2 = false;
    bool reserve3 = false;

    int curTabIdx = 0; // 0:常规  1:文件关联  2:帮助  3:关于

};

class Setting {
private:
    const int winWidth = 800;
    const int winHeight = 600;

    const string windowsNameAnsi = jarkUtils::utf8ToAnsi("设置");
    const char* windowsName = windowsNameAnsi.c_str();

    SettingParams params{};
    TextDrawer textDrawer;
    cv::Mat winCanvas, tabTitleMat, helpPage, aboutPage;

    SettingParameter* settingParameter = nullptr;

    static inline volatile bool requestExitFlag = false;


    void Init() {
        textDrawer.setSize(24);
        params.windowsName = windowsNameAnsi.c_str();

        rcFileInfo rc;
        rc = jarkUtils::GetResource(IDB_PNG_SETTING_RES, L"PNG");
        auto settingRes = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.ptr), cv::IMREAD_UNCHANGED);

        cv::Rect a{};

        tabTitleMat = settingRes({ 0, 0, 400, 50 }).clone();
        helpPage = settingRes({ 0, 100, 800, 550 }).clone();
        aboutPage = settingRes({ 0, 650, 800, 550 }).clone();
        
    }

public:
    static inline volatile bool isWorking = false;

    Setting(const cv::Mat& image, SettingParameter* settingParameter) : settingParameter(settingParameter) {
        requestExitFlag = false;
        isWorking = true;

        Init();

        winCanvas = cv::Mat(winHeight, winWidth, CV_8UC4, cv::Scalar(240, 240, 240, 240));

        cv::namedWindow(windowsName, cv::WINDOW_AUTOSIZE);
        cv::resizeWindow(windowsName, winWidth, winHeight);
        cv::setMouseCallback(windowsName, mouseCallback, &params);

        mainLoop();


        requestExitFlag = false;
        isWorking = false;
    }

    ~Setting() {}

    static void requestExit() {
        requestExitFlag = true;
    }

    void drawProgressBar(cv::Mat& image, int xBegin, int xEnd, int yBegin, int yEnd, double progress) {
        cv::rectangle(image,
            cv::Point(xBegin, yBegin),
            cv::Point(xEnd, yEnd),
            cv::Scalar(255, 0, 0, 255),
            2);

        int fillWidth = static_cast<int>((xEnd - xBegin) * progress);
        if (0 < fillWidth && fillWidth <= (xEnd - xBegin)) {
            cv::rectangle(image,
                cv::Point(xBegin, yBegin),
                cv::Point(xBegin + fillWidth, yEnd),
                cv::Scalar(204, 72, 63, 255),
                -1); // -1表示完全填充
        }
    }

    void refreshReguralTab() {
        cv::rectangle(winCanvas, { 0, 50, 800, 550 }, cv::Scalar(24, 72, 63, 255), -1);

    }

    void refreshAssociateTab() {
        cv::rectangle(winCanvas, {0, 50, 800, 550}, cv::Scalar(204, 72, 63, 255), -1);

    }

    void refreshHelpTab() {
        jarkUtils::overlayImg(winCanvas, helpPage, 0, 50);
    }

    void refreshAboutTab() {
        jarkUtils::overlayImg(winCanvas, aboutPage, 0, 50);
        textDrawer.putAlignCenter(winCanvas, { 0, 480, 360, 500 }, jarkUtils::wstringToUtf8(appBuildInfo).c_str(), { 186, 38, 60, 255 });
    }

    void refreshUI() {
        // 绘制标签栏
        cv::rectangle(winCanvas, cv::Point(0, 0), cv::Point(winWidth, 50), cv::Scalar(220, 220, 220, 255), -1);
        cv::rectangle(winCanvas, cv::Point(params.curTabIdx * 100, 0), cv::Point((params.curTabIdx + 1) * 100, 50), cv::Scalar(240, 240, 240, 255), -1);
        jarkUtils::overlayImg(winCanvas, tabTitleMat, 0, 0);

        switch (params.curTabIdx) {
        case 0:refreshReguralTab(); break;
        case 1:refreshAssociateTab(); break;
        case 2:refreshHelpTab(); break;
        default:refreshAboutTab(); break;
        }

        cv::imshow(windowsName, winCanvas);
    }

    static void handleReguralTab(int event, int x, int y, int flags, void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);


    }

    static void finishReguralTab(void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);


    }

    static void handleAssociateTab(int event, int x, int y, int flags, void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);


    }

    static void finishAssociateTab(void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);


    }

    static bool isInside(int x, int y, RECT rect) {
        return rect.left < x && x < rect.right && rect.top < y && y < rect.bottom;
    }

    static void handleAboutTab(int event, int x, int y, int flags, void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);

        if (event == cv::EVENT_LBUTTONUP) {
            if (isInside(x, y, {510, 140, 560, 200})) {
                jarkUtils::openUrl(jarkLink.c_str());
            }
            if (isInside(x, y, { 400, 280, 760, 350 })) {
                jarkUtils::openUrl(GithubLink.c_str());
            }
            if (isInside(x, y, { 400, 366, 760, 454 })) {
                jarkUtils::openUrl(BaiduLink.c_str());
            }
            if (isInside(x, y, { 400, 470, 760, 540 })) {
                jarkUtils::openUrl(LanzouLink.c_str());
            }
        }
    }

    static void mouseCallback(int event, int x, int y, int flags, void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);

        if (event == cv::EVENT_LBUTTONUP && y < 50) {
            int newTabIdx = x / 100;
            if (newTabIdx <= 3 && newTabIdx != params->curTabIdx) {
                params->isParamsChange = true;
                switch (params->curTabIdx) {
                case 0: finishReguralTab(userdata); break;
                case 1: finishAssociateTab(userdata); break;
                }
                params->curTabIdx = newTabIdx;
            }
        }

        if (event == cv::EVENT_RBUTTONUP){ // 右键直接关闭打印窗口
            requestExit();
            return;
        }

        switch (params->curTabIdx) {
        case 0:handleReguralTab(event, x, y, flags, userdata); break;
        case 1:handleAssociateTab(event, x, y, flags, userdata); break;
        case 3:handleAboutTab(event, x, y, flags, userdata); break;
        }
    }

    void mainLoop() {

        refreshUI();

        HWND hwnd = FindWindowA(NULL, windowsName);
        if (hwnd) {
            jarkUtils::disableWindowResize(hwnd);

            HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCE(IDI_JARKVIEWER));
            if (hIcon) {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }
            DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &D2D1App::isDarkMode, sizeof(BOOL));
        }

        while (cv::getWindowProperty(windowsName, cv::WND_PROP_VISIBLE) > 0) {
            if (params.isParamsChange) {
                params.isParamsChange = false;
                refreshUI();
            }

            int key = cv::waitKey(10);
            if (requestExitFlag) {
                cv::destroyWindow(params.windowsName);
                break;
            }
        }
    }
};