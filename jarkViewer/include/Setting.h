#pragma once

#include "jarkUtils.h"
#include "TextDrawer.h"
#include "D2D1App.h"
#include "FileAssociationManager.h"

extern const wstring appBuildInfo;
extern const wstring jarkLink;
extern const wstring GithubLink;
extern const wstring BaiduLink;
extern const wstring LanzouLink;

struct reguralTabCheckBox {
    cv::Rect rect{};
    string_view text;
    bool* valuePtr = nullptr;
};

struct reguralTabRadio {
    cv::Rect rect{};
    std::vector<string_view> text;
    int* valuePtr = nullptr;
};

struct SettingParams {
    const char* windowsName = nullptr;
    bool isParamsChange = false;
    bool reserve1 = false;
    bool reserve2 = false;
    bool reserve3 = false;

    int curTabIdx = 0; // 0:常规  1:文件关联  2:帮助  3:关于
    std::vector<string>* allSupportExt = nullptr;
    std::set<string>* checkedExt = nullptr;
    std::vector<reguralTabCheckBox>* reguralTabCheckBoxList;
    std::vector<reguralTabRadio>* reguralTabRadioList;
};

class Setting {
private:
    static const int winWidth = 1000;
    static const int winHeight = 800;
    static const int tabHeight = 50;

    const string windowsNameAnsi = jarkUtils::utf8ToAnsi("设置");
    const char* windowsName = windowsNameAnsi.c_str();

    SettingParams params{};
    TextDrawer textDrawer;
    cv::Mat winCanvas, tabTitleMat, helpPage, aboutPage;

    static inline SettingParameter* settingParameter = nullptr;

    static inline volatile bool requestExitFlag = false;
    std::vector<string> allSupportExt;
    std::set<string> checkedExt;

    std::vector<reguralTabCheckBox> reguralTabCheckBoxList;
    std::vector<reguralTabRadio> reguralTabRadioList;

    void Init() {
        textDrawer.setSize(24);
        winCanvas = cv::Mat(winHeight, winWidth, CV_8UC4, cv::Scalar(240, 240, 240, 240));

        params.windowsName = windowsNameAnsi.c_str();
        params.allSupportExt = &allSupportExt;
        params.checkedExt = &checkedExt;
        params.reguralTabCheckBoxList = &reguralTabCheckBoxList;
        params.reguralTabRadioList = &reguralTabRadioList;


        rcFileInfo rc;
        rc = jarkUtils::GetResource(IDB_PNG_SETTING_RES, L"PNG");
        auto settingRes = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.ptr), cv::IMREAD_UNCHANGED);

        tabTitleMat = settingRes({ 0, 0, 400, 50 }).clone();
        helpPage = settingRes({ 0, 100, 1000, 750 }).clone();
        aboutPage = settingRes({ 0, 850, 1000, 750 }).clone();

        // ReguralTab
        reguralTabCheckBoxList = {
            { {50, 100, 200, 50}, "旋转动画", &settingParameter->isAllowRotateAnimation },
            { {50, 150, 200, 50}, "缩放动画", &settingParameter->isAllowZoomAnimation },
        };
        reguralTabRadioList = {
            {{50, 200, 600, 50}, {"切图动画", "无动画", "上下滑动", "左右滑动"}, &settingParameter->switchImageAnimationMode },
        };

        // AssociateTab
        std::set<wstring> allSupportExtW;
        allSupportExtW.insert(ImageDatabase::supportExt.begin(), ImageDatabase::supportExt.end());
        allSupportExtW.insert(ImageDatabase::supportRaw.begin(), ImageDatabase::supportRaw.end());
        for (const auto& ext : allSupportExtW)
            allSupportExt.push_back(jarkUtils::wstringToUtf8(ext.substr(1)));

        auto checkedExtVec = jarkUtils::splitString(settingParameter->extCheckedListStr, ",");
        auto filtered = checkedExtVec | std::views::filter([](const std::string& s) { return !s.empty(); });
        checkedExt.clear();
        if (!filtered.empty())
            checkedExt.insert(filtered.begin(), filtered.end());
    }

public:
    static inline volatile bool isWorking = false;

    Setting(const cv::Mat& image, SettingParameter* settingPar) {
        settingParameter = settingPar;
        requestExitFlag = false;
        isWorking = true;

        Init();
        windowsMainLoop();

        requestExitFlag = false;
        isWorking = false;
    }

    ~Setting() {}

    static void requestExit() {
        requestExitFlag = true;
    }

    void drawProgressBar(cv::Mat& image, cv::Rect rect, double progress) {
        cv::rectangle(image, rect, cv::Scalar(255, 0, 0, 255), 2);

        const int progresswidth = static_cast<int>(rect.width * progress);
        if (0 < progresswidth && progresswidth <= rect.width) {
            rect.width = progresswidth;
            cv::rectangle(image, rect, cv::Scalar(204, 72, 63, 255), -1);
        }
    }

    void refreshReguralTab() {
        cv::rectangle(winCanvas, { 0, tabHeight, winWidth, winHeight - tabHeight }, cv::Scalar(240, 240, 240, 255), -1);

        for (auto& cbox : reguralTabCheckBoxList) {
            cv::Rect rect({ cbox.rect.x + 8, cbox.rect.y + 8, cbox.rect.height - 16, cbox.rect.height - 16 }); //方形
            cv::rectangle(winCanvas, rect, cv::Scalar(0, 0, 0, 255), 4);
            if (*cbox.valuePtr) {
                rect = { cbox.rect.x + 12, cbox.rect.y + 12, cbox.rect.height - 24, cbox.rect.height - 24 }; //小方形
                cv::rectangle(winCanvas, rect, cv::Scalar(255, 200, 120, 255), -1);
            }

            rect = { cbox.rect.x + cbox.rect.height, cbox.rect.y, cbox.rect.width - cbox.rect.height, cbox.rect.height };
            textDrawer.putAlignCenter(winCanvas, rect, cbox.text.data(), cv::Scalar(0, 0, 0, 255));
        }

        for (auto& radio : reguralTabRadioList) {
            int idx = *radio.valuePtr;
            if (idx >= radio.text.size())
                idx = 0;

            int itemWidth = radio.rect.width / radio.text.size();
            cv::Rect rect1 = { radio.rect.x + itemWidth * (1 + idx) , radio.rect.y, itemWidth, radio.rect.height }; // 当前项背景框
            cv::rectangle(winCanvas, rect1, cv::Scalar(255, 230, 150, 255), -1);

            cv::Rect rect2 = { radio.rect.x + itemWidth , radio.rect.y, radio.rect.width - itemWidth, radio.rect.height }; //大框
            cv::rectangle(winCanvas, rect2, cv::Scalar(0, 0, 0, 255), 2);

            for (int i = 0; i < radio.text.size(); i++) {
                cv::Rect rect3 = { radio.rect.x + itemWidth * (i), radio.rect.y , itemWidth, radio.rect.height };
                textDrawer.putAlignCenter(winCanvas, rect3, radio.text[i].data(), cv::Scalar(0, 0, 0, 255));
            }
        }
    }

    void refreshAssociateTab() {
        cv::rectangle(winCanvas, { 0, tabHeight, winWidth, winHeight - tabHeight }, cv::Scalar(240, 240, 240, 255), -1);

        // 需和 handleAssociateTab 参数保持一致
        const int xOffset = 20, yOffset = 70;
        const int gridWidth = 80, gridHeight = 40;
        const int gridNumPerLine = 12;

        const std::string btnTextList[4] = { "默认选择", "全选", "全不选", "立即关联" };
        const cv::Rect btnRectList[4] = {
            { winWidth - 640, winHeight - 60, 160, 60 }, // defaultCheck
            { winWidth - 480, winHeight - 60, 160, 60 }, // allCheckBtnRect
            { winWidth - 320, winHeight - 60, 160, 60 }, // allClearBtnRect
            { winWidth - 160, winHeight - 60, 160, 60 }, // confirmBtnRect
        };

        int idx = 0;
        for (const auto& ext : allSupportExt) {
            int y = idx / gridNumPerLine;
            int x = idx % gridNumPerLine;
            cv::Rect rect({ xOffset + gridWidth * x, yOffset + gridHeight * y, gridWidth, gridHeight });
            if (checkedExt.contains(ext)) {
                cv::Rect rect2{ rect.x + 2, rect.y + 4, rect.width - 4, rect.height - 4 };
                cv::rectangle(winCanvas, rect2, cv::Scalar(255, 230, 150, 255), -1);
            }
            textDrawer.putAlignCenter(winCanvas, rect, ext.c_str(), cv::Scalar(0, 0, 0, 255));
            idx++;
        }

        for (int i = 0; i < 4; i++) {
            const cv::Rect& rect = btnRectList[i];
            cv::Rect rect2{ rect.x + 4, rect.y + 8, rect.width - 8, rect.height - 12 };
            cv::rectangle(winCanvas, rect2, cv::Scalar(200, 200, 200, 255), -1);
            textDrawer.putAlignCenter(winCanvas, rect, btnTextList[i].c_str(), cv::Scalar(0, 0, 0, 255));
        }
    }

    void refreshHelpTab() {
        jarkUtils::overlayImg(winCanvas, helpPage, 0, 50);
    }

    void refreshAboutTab() {
        jarkUtils::overlayImg(winCanvas, aboutPage, 0, 50);
        textDrawer.putAlignCenter(winCanvas, { 0, 580, 400, 40 }, jarkUtils::wstringToUtf8(appBuildInfo).c_str(), { 186, 38, 60, 255 });
    }

    void refreshUI() {
        // 绘制标签栏
        cv::rectangle(winCanvas, { 0, 0, winWidth, tabHeight }, cv::Scalar(200, 200, 200, 255), -1);
        cv::rectangle(winCanvas, { params.curTabIdx * 100, 0, 100, tabHeight }, cv::Scalar(240, 240, 240, 255), -1);
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

        if (event == cv::EVENT_LBUTTONUP) {
            for (auto& cbox : *params->reguralTabCheckBoxList) {
                if (isInside(x, y, cbox.rect)) {
                    *cbox.valuePtr = !(*cbox.valuePtr);
                    params->isParamsChange = true;
                }
            }

            for (auto& radio : *params->reguralTabRadioList) {
                if (isInside(x, y, radio.rect)) {
                    int itemWidth = radio.rect.width / radio.text.size();
                    int clickIdx = (x - radio.rect.x) / itemWidth - 1;
                    if (0 <= clickIdx && clickIdx < radio.text.size() - 1) {
                        *radio.valuePtr = clickIdx;
                        params->isParamsChange = true;
                    }
                }
            }
        }
    }

    static void finishReguralTab(void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);


    }

    static int getGridIndex(int x, int y, int xOffset = 50, int yOffset = 200,
        int gridW = 20, int gridH = 10, int colsPerRow = 10, int idxMax = -1) {  // 默认不检查 idxMax
        int relativeX = x - xOffset;
        int relativeY = y - yOffset;
        if (relativeX < 0 || relativeY < 0) {
            return -1; // 无效坐标
        }
        int col = relativeX / gridW;
        int row = relativeY / gridH;
        if (col >= colsPerRow) {
            return -1; // 超出列范围
        }
        int index = row * colsPerRow + col;
        if (idxMax >= 0 && index >= idxMax) {  // 如果 idxMax 有效，并且 index 超出
            return -1;
        }
        return index;
    }

    template<typename T>
    static void toggle(std::set<T>& s, const T& value) {
        if (!s.insert(value).second) {
            s.erase(value);
        }
    }

    static bool SetupFileAssociations(const std::vector<std::wstring>& extChecked,
        const std::vector<std::wstring>& extUnchecked) {

        FileAssociationManager manager;
        return manager.ManageFileAssociations(extChecked, extUnchecked);
    }

    static void handleAssociateTab(int event, int x, int y, int flags, void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);

        // 需和 refreshAssociateTab 参数保持一致
        const int xOffset = 20, yOffset = 70;
        const int gridWidth = 80, gridHeight = 40;
        const int gridNumPerLine = 12;
        const cv::Rect btnRectList[4] = {
            { winWidth - 640, winHeight - 60, 160, 60 }, // defaultCheck
            { winWidth - 480, winHeight - 60, 160, 60 }, // allCheckBtnRect
            { winWidth - 320, winHeight - 60, 160, 60 }, // allClearBtnRect
            { winWidth - 160, winHeight - 60, 160, 60 }, // confirmBtnRect
        };

        if (event == cv::EVENT_LBUTTONUP) {
            int gridIdx = getGridIndex(x, y, xOffset, yOffset, gridWidth, gridHeight, gridNumPerLine, params->allSupportExt->size());
            if (gridIdx >= 0) {
                const auto& targetExt = (*params->allSupportExt)[gridIdx];
                toggle(*(params->checkedExt), targetExt);
                params->isParamsChange = true;
            }
            else if (isInside(x, y, btnRectList[0])) { // 恢复默认勾选
                std::string_view defaultExtList{ "apng,avif,bmp,bpg,gif,heic,heif,ico,jfif,jp2,jpe,jpeg,jpg,jxl,jxr,livp,png,qoi,svg,tga,tif,tiff,webp,wp2" };
                memcpy(settingParameter->extCheckedListStr, defaultExtList.data(), defaultExtList.length() + 1);

                auto checkedExtVec = jarkUtils::splitString(settingParameter->extCheckedListStr, ",");
                auto filtered = checkedExtVec | std::views::filter([](const std::string& s) { return !s.empty(); });
                params->checkedExt->clear();
                if (!filtered.empty())
                    params->checkedExt->insert(filtered.begin(), filtered.end());

                params->isParamsChange = true;
            }
            else if (isInside(x, y, btnRectList[1])) { // 全选
                params->checkedExt->insert(params->allSupportExt->begin(), params->allSupportExt->end());
                params->isParamsChange = true;
            }
            else if (isInside(x, y, btnRectList[2])) { // 全不选
                params->checkedExt->clear();
                params->isParamsChange = true;
            }
            else if (isInside(x, y, btnRectList[3])) { // 立即关联
                std::vector<std::wstring> checkedExtW, unCheckedExtW;
                checkedExtW.reserve(params->checkedExt->size());
                for (const auto& ext : *(params->checkedExt)) {
                    checkedExtW.push_back(jarkUtils::utf8ToWstring(ext));
                }

                std::vector<std::string> uncheckedExt;
                uncheckedExt.reserve(params->allSupportExt->size() - params->checkedExt->size());
                for (const auto& ext : *(params->allSupportExt)) {
                    if (!params->checkedExt->contains(ext))
                        uncheckedExt.push_back(ext);
                }

                unCheckedExtW.reserve(uncheckedExt.size());
                for (const auto& ext : uncheckedExt) {
                    unCheckedExtW.push_back(jarkUtils::utf8ToWstring(ext));
                }

                if (SetupFileAssociations(checkedExtW, unCheckedExtW)) {
                    MessageBoxW(nullptr, L"文件关联设置成功！", L"jarkViewer", MB_OK | MB_ICONINFORMATION);
                }
                else {
                    MessageBoxW(nullptr, L"文件关联设置失败！", L"jarkViewer", MB_OK | MB_ICONERROR);
                }
            }
        }

    }

    static void finishAssociateTab(void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);

        std::string checkedList;
        for (const auto& ext : *(params->checkedExt)) {
            checkedList += ext;
            checkedList += ',';
        }
        if (checkedList.back() == ',')
            checkedList.pop_back();
        if (checkedList.empty())
            memset(settingParameter->extCheckedListStr, 0, 4);
        else
            memcpy(settingParameter->extCheckedListStr, checkedList.data(), checkedList.length() + 1);
    }

    static bool isInside(int x, int y, cv::Rect rect) {
        return rect.x < x && x < (rect.x + rect.width) && rect.y < y && y < (rect.y + rect.height);
    }

    static void handleAboutTab(int event, int x, int y, int flags, void* userdata) {
        SettingParams* params = static_cast<SettingParams*>(userdata);

        if (event == cv::EVENT_LBUTTONUP) {
            if (isInside(x, y, { 550, 100, 50, 50 })) {
                jarkUtils::openUrl(jarkLink.c_str());
            }
            if (isInside(x, y, { 440, 230, 520, 90 })) {
                jarkUtils::openUrl(GithubLink.c_str());
            }
            if (isInside(x, y, { 440, 335, 520, 110 })) {
                jarkUtils::openUrl(BaiduLink.c_str());
            }
            if (isInside(x, y, { 440, 460, 520, 90 })) {
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

        if (event == cv::EVENT_RBUTTONUP) { // 右键直接关闭打印窗口
            requestExit();
            return;
        }

        switch (params->curTabIdx) {
        case 0:handleReguralTab(event, x, y, flags, userdata); break;
        case 1:handleAssociateTab(event, x, y, flags, userdata); break;
        case 3:handleAboutTab(event, x, y, flags, userdata); break;
        }
    }

    void windowsMainLoop() {
        cv::namedWindow(windowsName, cv::WINDOW_AUTOSIZE);
        cv::resizeWindow(windowsName, winWidth, winHeight);
        cv::setMouseCallback(windowsName, mouseCallback, &params);

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

        switch (params.curTabIdx) {
        case 0: finishReguralTab(&params); break;
        case 1: finishAssociateTab(&params); break;
        }
    }
};