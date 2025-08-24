#pragma once

#include "jarkUtils.h"
#include "TextDrawer.h"
#include "D2D1App.h"
#include "FileAssociationManager.h"

extern std::wstring_view appVersion;
extern std::wstring_view jarkLink;
extern std::wstring_view GithubLink;
extern std::wstring_view BaiduLink;
extern std::wstring_view LanzouLink;

struct labelBox {
    cv::Rect rect{};
    string_view text;
};
struct generalTabCheckBox {
    cv::Rect rect{};
    string_view text;
    bool* valuePtr = nullptr;
};

struct generalTabRadio {
    cv::Rect rect{};
    std::vector<string_view> text;
    int* valuePtr = nullptr;
};

class Setting {
private:
    static const int winWidth = 1000;
    static const int winHeight = 800;
    static const int tabHeight = 50;

    static inline string windowsNameAnsi;
    static inline volatile bool requestExitFlag = false;

    static inline std::vector<string> allSupportExt;
    static inline std::set<string> checkedExt;
    static inline std::vector<generalTabCheckBox> generalTabCheckBoxList;
    static inline std::vector<generalTabRadio> generalTabRadioList;
    static inline std::vector<labelBox> labelList;

    TextDrawer textDrawer;
    cv::Mat winCanvas, settingRes, tabTitleMat, helpPage, aboutPage;

    void Init(int tabIdx = 0) {
        textDrawer.setSize(24);
        windowsNameAnsi = jarkUtils::utf8ToAnsi("设置");
        winCanvas = cv::Mat(winHeight, winWidth, CV_8UC4, cv::Scalar(240, 240, 240, 240));
        curTabIdx = tabIdx;

        rcFileInfo rc;
        rc = jarkUtils::GetResource(IDB_PNG_SETTING_RES, L"PNG");
        settingRes = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.ptr), cv::IMREAD_UNCHANGED);

        tabTitleMat = settingRes({ 0, 0, 400, 50 });
        helpPage = settingRes({ 0, 100, 1000, 750 });
        aboutPage = settingRes({ 0, 850, 1000, 750 });

        // GeneralTab
        if (generalTabCheckBoxList.empty()) {
            generalTabCheckBoxList = {
                { {50, 100, 180, 50}, "旋转动画", &GlobalVar::settingParameter.isAllowRotateAnimation },
                { {50, 150, 180, 50}, "缩放动画", &GlobalVar::settingParameter.isAllowZoomAnimation },
                { {50, 200, 760, 50}, "平移图像加速 (拖动图像时优化渲染速度，图像会微微失真)", &GlobalVar::settingParameter.isOptimizeSlide },
            };
        }
        if (generalTabRadioList.empty()) {
            generalTabRadioList = {
                {{50, 300, 600, 50}, {"切图动画", "无动画", "上下滑动", "左右滑动"}, &GlobalVar::settingParameter.switchImageAnimationMode },
                {{50, 360, 600, 50}, {"界面主题", "跟随系统", "浅色", "深色"}, &GlobalVar::settingParameter.UI_Mode },
            };
        }

        // AssociateTab
        if (allSupportExt.empty()) {
            std::set<wstring> allSupportExtW;
            allSupportExtW.insert(ImageDatabase::supportExt.begin(), ImageDatabase::supportExt.end());
            allSupportExtW.insert(ImageDatabase::supportRaw.begin(), ImageDatabase::supportRaw.end());
            for (const auto& ext : allSupportExtW)
                allSupportExt.push_back(jarkUtils::wstringToUtf8(ext));
        }

        auto checkedExtVec = jarkUtils::splitString(GlobalVar::settingParameter.extCheckedListStr, ",");
        auto filtered = checkedExtVec | std::views::filter([](const std::string& s) { return !s.empty(); });
        checkedExt.clear();
        if (!filtered.empty())
            checkedExt.insert(filtered.begin(), filtered.end());
    }

public:
    static inline volatile HWND hwnd = nullptr;
    static inline volatile int curTabIdx = 0; // 0:常规  1:文件关联  2:帮助  3:关于
    static inline volatile bool isWorking = false;
    static inline volatile bool isNeedRefreshUI = false;

    Setting(int tabIdx = 0) {
        requestExitFlag = false;
        isWorking = true;

        Init(tabIdx);
        windowsMainLoop();

        requestExitFlag = false;
        isWorking = false;
        hwnd = nullptr;
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

    void refreshGeneralTab() {
        cv::rectangle(winCanvas, { 0, tabHeight, winWidth, winHeight - tabHeight }, cv::Scalar(240, 240, 240, 255), -1);

        for (auto& cbox : generalTabCheckBoxList) {
            cv::Rect rect({ cbox.rect.x + 8, cbox.rect.y + 8, cbox.rect.height - 16, cbox.rect.height - 16 }); //方形
            cv::rectangle(winCanvas, rect, cv::Scalar(0, 0, 0, 255), 4);
            if (*cbox.valuePtr) {
                rect = { cbox.rect.x + 14, cbox.rect.y + 14, cbox.rect.height - 28, cbox.rect.height - 28 }; //小方形
                cv::rectangle(winCanvas, rect, cv::Scalar(255, 200, 120, 255), -1);
            }

            rect = { cbox.rect.x + cbox.rect.height, cbox.rect.y, cbox.rect.width - cbox.rect.height, cbox.rect.height };
            textDrawer.putAlignCenter(winCanvas, rect, cbox.text.data(), cv::Scalar(0, 0, 0, 255));
        }

        for (auto& radio : generalTabRadioList) {
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

        for (auto& labelBox : labelList) {
            cv::Rect rect = {
                labelBox.rect.x + labelBox.rect.height,
                labelBox.rect.y,
                labelBox.rect.width - labelBox.rect.height,
                labelBox.rect.height };
            textDrawer.putAlignCenter(winCanvas, rect, labelBox.text.data(), cv::Scalar(0, 0, 0, 255));
        }
    }

    void refreshAssociateTab() {
        cv::rectangle(winCanvas, { 0, tabHeight, winWidth, winHeight - tabHeight }, cv::Scalar(240, 240, 240, 255), -1);

        // 需和 handleAssociateTab 参数保持一致
        const int xOffset = 20, yOffset = 70;
        const int gridWidth = 80, gridHeight = 40;
        const int gridNumPerLine = 12;

        const std::string btnTextList[4] = { "选择常用", "全选", "全不选", "立即关联" };
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
        textDrawer.putAlignCenter(winCanvas, { 0, 580, 400, 40 }, jarkUtils::wstringToUtf8(appVersion).c_str(), { 186, 38, 60, 255 });
        textDrawer.putAlignCenter(winCanvas, { 0, 670, 400, 40 }, "[Build time]", { 186, 38, 60, 255 });
        textDrawer.putAlignCenter(winCanvas, { 0, 700, 400, 40 }, jarkUtils::COMPILE_DATE_TIME, { 186, 38, 60, 255 });
    }

    void refreshUI() {
        // 绘制标签栏
        cv::rectangle(winCanvas, { 0, 0, winWidth, tabHeight }, cv::Scalar(200, 200, 200, 255), -1);
        cv::rectangle(winCanvas, { curTabIdx * 100, 0, 100, tabHeight }, cv::Scalar(240, 240, 240, 255), -1);
        jarkUtils::overlayImg(winCanvas, tabTitleMat, 0, 0);

        switch (curTabIdx) {
        case 0:refreshGeneralTab(); break;
        case 1:refreshAssociateTab(); break;
        case 2:refreshHelpTab(); break;
        default:refreshAboutTab(); break;
        }

        cv::imshow(windowsNameAnsi.c_str(), winCanvas);
    }

    static void handleGeneralTab(int event, int x, int y, int flags) {
        if (event == cv::EVENT_LBUTTONUP) {
            for (auto& cbox : generalTabCheckBoxList) {
                if (isInside(x, y, cbox.rect)) {
                    *cbox.valuePtr = !(*cbox.valuePtr);
                    isNeedRefreshUI = true;
                }
            }

            for (auto& radio : generalTabRadioList) {
                if (isInside(x, y, radio.rect)) {
                    int itemWidth = radio.rect.width / radio.text.size();
                    int clickIdx = (x - radio.rect.x) / itemWidth - 1;
                    if (0 <= clickIdx && clickIdx < radio.text.size() - 1) {
                        *radio.valuePtr = clickIdx;
                        isNeedRefreshUI = true;

                        if (radio.text.front() == "界面主题") {
                            GlobalVar::isNeedUpdateTheme = true;
                        }
                    }
                }
            }
        }
    }

    static void finishGeneralTab() {

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

    static void handleAssociateTab(int event, int x, int y, int flags) {

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
            int gridIdx = getGridIndex(x, y, xOffset, yOffset, gridWidth, gridHeight, gridNumPerLine, allSupportExt.size());
            if (gridIdx >= 0) {
                const auto& targetExt = (allSupportExt)[gridIdx];
                toggle(checkedExt, targetExt);
                isNeedRefreshUI = true;
            }
            else if (isInside(x, y, btnRectList[0])) { // 恢复默认勾选
                memcpy(GlobalVar::settingParameter.extCheckedListStr,
                    SettingParameter::defaultExtList.data(),
                    SettingParameter::defaultExtList.length() + 1);

                auto checkedExtVec = jarkUtils::splitString(GlobalVar::settingParameter.extCheckedListStr, ",");
                auto filtered = checkedExtVec | std::views::filter([](const std::string& s) { return !s.empty(); });
                checkedExt.clear();
                if (!filtered.empty())
                    checkedExt.insert(filtered.begin(), filtered.end());

                isNeedRefreshUI = true;
            }
            else if (isInside(x, y, btnRectList[1])) { // 全选
                checkedExt.insert(allSupportExt.begin(), allSupportExt.end());
                isNeedRefreshUI = true;
            }
            else if (isInside(x, y, btnRectList[2])) { // 全不选
                checkedExt.clear();
                isNeedRefreshUI = true;
            }
            else if (isInside(x, y, btnRectList[3])) { // 立即关联
                std::vector<std::wstring> checkedExtW, unCheckedExtW;
                checkedExtW.reserve(checkedExt.size());
                for (const auto& ext : checkedExt) {
                    checkedExtW.push_back(jarkUtils::utf8ToWstring(ext));
                }

                std::vector<std::string> uncheckedExt;
                uncheckedExt.reserve(allSupportExt.size() - checkedExt.size());
                for (const auto& ext : allSupportExt) {
                    if (!checkedExt.contains(ext))
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

    static void finishAssociateTab() {
        std::string checkedList;
        for (const auto& ext : checkedExt) {
            checkedList += ext;
            checkedList += ',';
        }
        if (checkedList.back() == ',')
            checkedList.pop_back();
        if (checkedList.empty())
            memset(GlobalVar::settingParameter.extCheckedListStr, 0, 4);
        else
            memcpy(GlobalVar::settingParameter.extCheckedListStr, checkedList.data(), checkedList.length() + 1);
    }

    static bool isInside(int x, int y, cv::Rect rect) {
        return rect.x < x && x < (rect.x + rect.width) && rect.y < y && y < (rect.y + rect.height);
    }

    static void handleAboutTab(int event, int x, int y, int flags) {
        if (event == cv::EVENT_LBUTTONUP) {
            if (isInside(x, y, { 440, 100, 300, 120 })) {
                jarkUtils::openUrl(jarkLink.data());
            }
            if (isInside(x, y, { 440, 280, 520, 90 })) {
                jarkUtils::openUrl(GithubLink.data());
            }
            if (isInside(x, y, { 440, 380, 520, 110 })) {
                jarkUtils::openUrl(BaiduLink.data());
            }
            if (isInside(x, y, { 440, 510, 520, 90 })) {
                jarkUtils::openUrl(LanzouLink.data());
            }
        }
    }

    static void mouseCallback(int event, int x, int y, int flags, void* userData) {
        if (event == cv::EVENT_LBUTTONUP && y < 50) {
            int newTabIdx = x / 100;
            if (newTabIdx <= 3 && newTabIdx != curTabIdx) {
                isNeedRefreshUI = true;
                switch (curTabIdx) {
                case 0: finishGeneralTab(); break;
                case 1: finishAssociateTab(); break;
                }
                curTabIdx = newTabIdx;
            }
        }

        if (event == cv::EVENT_RBUTTONUP) { // 右键直接关闭打印窗口
            requestExit();
            return;
        }

        switch (curTabIdx) {
        case 0:handleGeneralTab(event, x, y, flags); break;
        case 1:handleAssociateTab(event, x, y, flags); break;
        case 3:handleAboutTab(event, x, y, flags); break;
        }
    }

    void windowsMainLoop() {
        cv::namedWindow(windowsNameAnsi, cv::WINDOW_AUTOSIZE);
        cv::resizeWindow(windowsNameAnsi, winWidth, winHeight);
        cv::setMouseCallback(windowsNameAnsi, mouseCallback, nullptr);

        refreshUI();

        hwnd = FindWindowA(NULL, windowsNameAnsi.c_str());
        if (hwnd) {
            jarkUtils::disableWindowResize(hwnd);

            HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCE(IDI_JARKVIEWER));
            if (hIcon) {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }
            BOOL themeMode = GlobalVar::settingParameter.UI_Mode == 0 ? GlobalVar::isSystemDarkMode : (GlobalVar::settingParameter.UI_Mode == 1 ? 0 : 1);
            DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &themeMode, sizeof(BOOL));
        }

        while (cv::getWindowProperty(windowsNameAnsi, cv::WND_PROP_VISIBLE) > 0) {
            if (isNeedRefreshUI) {
                isNeedRefreshUI = false;
                refreshUI();
            }

            if (cv::waitKey(10) == 27) // ESC
                requestExit();
            if (requestExitFlag) {
                cv::destroyWindow(windowsNameAnsi);
                break;
            }
        }

        switch (curTabIdx) {
        case 0: finishGeneralTab(); break;
        case 1: finishAssociateTab(); break;
        }
    }
};