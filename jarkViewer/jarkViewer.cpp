
#include "Utils.h"

#include "stbText.h"
#include "LRU.h"
#include "exif.h"

/*
opencv编译选项 编译静态库
https://github.com/opencv/opencv/releases
https://github.com/opencv/opencv_contrib/releases

编译选项 type Release
去掉  test ts js python pdb objc share_libs
勾选  opencv_world  with_freetype  enable_nonfree

windows_w32.cpp 在 (1951行) case WM_SETCURSOR: 下面替换成

static HCURSOR hCursor = LoadCursor(NULL, IDC_ARROW);
SetCursor(hCursor);

*/

// TODO GIF 等动图
// avif heic  https://github.com/strukturag/libheif


const int BG_GRID_WIDTH = 8;
const cv::Vec4b BG_COLOR_DARK{ 40, 40, 40, 255 };
const cv::Vec4b BG_COLOR_LIGHT{ 60, 60, 60, 255 };
const cv::Vec4b BG_COLOR{ 70, 70, 70, 255 };

HWND mainHWND;
const wchar_t* appName = L"JarkViewer V1.2";
const string windowName = "mainWindows";
const int64_t ZOOM_BASE = 1 << 16; // 基础缩放
const int64_t ZOOM_MIN = 1 << 10;
const int64_t ZOOM_MAX = 1 << 22;
int64_t zoomFitWindow = 0;  //适应显示窗口宽高的缩放比例
cv::Mat mainImg;

bool isBusy = false;
bool needFresh = false;
bool showExif = false;

Cood mouse{}, hasDropCur{}, hasDropTarget{};
int zoomOperate = 0;     // 缩放操作
int switchOperate = 0;   // 切换图片操作
int64_t zoomTarget = 0;  // 设定的缩放比例
int64_t zoomCur = 0;     // 动画播放过程的缩放比例，动画完毕后的值等于zoomTarget

int curFileIdx = -1;        //文件在路径列表的索引
vector<wstring> imgFileList; //工作目录下所有图像文件路径

stbText stb; //给Mat绘制文字
cv::Rect winSize;

LRU<wstring, cv::Mat, 10> myLRU;

const unordered_set<wstring> supportExt = {
    L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png",
    L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
    L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic" };

static int getWidth(cv::Mat& mat) { return mat.cols; }
static int getHeight(cv::Mat& mat) { return mat.rows; }

void onMouseHandle(int event, int x, int y, int flags, void* param);
std::wstring getExif(const wstring& path, cv::Mat& img);

static cv::Vec4b getSrcPx(cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) {
    switch (srcImg.channels()) {
    case 3: {
        auto& srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

        if (zoomCur < ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            auto& px0 = srcImg.at<cv::Vec3b>(srcY - 1, srcX - 1);
            auto& px1 = srcImg.at<cv::Vec3b>(srcY - 1, srcX);
            auto& px2 = srcImg.at<cv::Vec3b>(srcY, srcX - 1);
            cv::Vec4b ret;
            ret[3] = 255;
            for (int i = 0; i < 3; i++)
                ret[i] = (px0[i] + px1[i] + px2[i] + srcPx[i]) / 4;

            return ret;
        }

        return cv::Vec4b{ srcPx[0], srcPx[1], srcPx[2], 255 };
    }
    case 4: {
        auto& srcPx = srcImg.at<cv::Vec4b>(srcY, srcX);
        const cv::Vec4b& bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? BG_COLOR_DARK : BG_COLOR_LIGHT;

        cv::Vec4b px;
        if (zoomCur < ZOOM_BASE && srcY > 0 && srcX > 0) {
            auto& px0 = srcImg.at<cv::Vec4b>(srcY - 1, srcX - 1);
            auto& px1 = srcImg.at<cv::Vec4b>(srcY - 1, srcX);
            auto& px2 = srcImg.at<cv::Vec4b>(srcY, srcX - 1);
            for (int i = 0; i < 4; i++)
                px[i] = (px0[i] + px1[i] + px2[i] + srcPx[i]) / 4;
        }
        else {
            px = srcPx;
        }

        if (px[3] == 0) return bgPx;
        else if (px[3] == 255) return px;

        const int alpha = px[3];
        cv::Vec4b ret;
        ret[3] = alpha;
        for (int i = 0; i < 3; i++)
            ret[i] = (bgPx[i] * (255 - alpha) + px[i] * alpha) / 256;
        return ret;
    }
    case 1: {
        auto srcPx = srcImg.at<uchar>(srcY, srcX);

        if (zoomCur < ZOOM_BASE) {
            if (srcY > 0 && srcX > 0) { // 简单临近像素平均
                auto& px0 = srcImg.at<uchar>(srcY - 1, srcX - 1);
                auto& px1 = srcImg.at<uchar>(srcY - 1, srcX);
                auto& px2 = srcImg.at<uchar>(srcY, srcX - 1);
                srcPx = (px0 + px1 + px2 + srcPx) / 4;
            }
            else if (srcY == 0) {
                auto& px2 = srcImg.at<uchar>(srcY + 1, srcX);
                srcPx = (px2 + srcPx) / 2;
            }
            else {
                auto& px2 = srcImg.at<uchar>(srcY, srcX + 1);
                srcPx = (px2 + srcPx) / 2;
            }
        }
        return cv::Vec4b{ srcPx, srcPx, srcPx, 255 };
    }
    }

    return ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? BG_COLOR_DARK : BG_COLOR_LIGHT;
}

static int64 keep_msb_1_clear_others(int64 n) {
    if (n == 0)
        return 0;
    int msb = 63;
    while (((n >> msb) & 1) == 0)
        msb--;
    return 1LL << msb;
}

static void processSrc(cv::Mat& srcImg, cv::Mat& mainImg) {
    static int64_t zoomFitWindow = 0;  //适应显示窗口宽高的缩放比例

    const int srcH = getHeight(srcImg);
    const int srcW = getWidth(srcImg);

    if (zoomTarget == 0) {
        zoomFitWindow = (srcH > 0 && srcW > 0) ? std::min(winSize.height * ZOOM_BASE / srcH, winSize.width * ZOOM_BASE / srcW) : ZOOM_BASE;
        zoomTarget = (srcH > winSize.height || srcW > winSize.width) ? zoomFitWindow : ZOOM_BASE;
        zoomCur = zoomTarget;
    }

    if (zoomOperate > 0) {
        zoomOperate = 0;
        if (zoomTarget < ZOOM_MAX) {
            int64 zoomNext = zoomTarget * 2;
            if (zoomTarget == zoomFitWindow)
                zoomNext = keep_msb_1_clear_others(zoomTarget * 2);
            else if (zoomTarget < zoomFitWindow && zoomFitWindow < zoomNext)
                zoomNext = zoomFitWindow;

            if (zoomTarget) {
                hasDropTarget.x = (int)(zoomNext * hasDropTarget.x / zoomTarget);
                hasDropTarget.y = (int)(zoomNext * hasDropTarget.y / zoomTarget);
            }
            zoomTarget = zoomNext;
        }
    }
    else if (zoomOperate < 0) {
        zoomOperate = 0;
        if (zoomTarget > ZOOM_MIN) {
            int64 zoomNext = zoomTarget / 2;
            if (zoomTarget == zoomFitWindow) {
                if (zoomFitWindow != keep_msb_1_clear_others(zoomFitWindow))
                    zoomNext = keep_msb_1_clear_others(zoomFitWindow);
            }
            else if (zoomTarget > zoomFitWindow && zoomFitWindow > zoomNext)
                zoomNext = zoomFitWindow;

            if (zoomTarget) {
                hasDropTarget.x = (int)(zoomNext * hasDropTarget.x / zoomTarget);
                hasDropTarget.y = (int)(zoomNext * hasDropTarget.y / zoomTarget);
            }
            zoomTarget = zoomNext;
        }
    }

    const int64_t deltaW = hasDropCur.x + (winSize.width - srcW * zoomCur / ZOOM_BASE) / 2;
    const int64_t deltaH = hasDropCur.y + (winSize.height - srcH * zoomCur / ZOOM_BASE) / 2;

    for (int y = 0; y < winSize.height; y++)
        for (int x = 0; x < winSize.width; x++) {

            int srcX = (int)((x - deltaW) * ZOOM_BASE / zoomCur);
            int srcY = (int)((y - deltaH) * ZOOM_BASE / zoomCur);

            if (srcX >= 0 && srcX < srcW && srcY >= 0 && srcY < srcH)
                mainImg.at<cv::Vec4b>(y, x) = getSrcPx(srcImg, srcX, srcY, x, y);
            else
                mainImg.at<cv::Vec4b>(y, x) = BG_COLOR;
        }
}


static cv::Mat& getDefaultMat() {
    static cv::Mat defaultMat;
    static bool isInit = false;

    if (!isInit) {
        isInit = true;
        auto rc = Utils::GetResource(IDB_PNG, L"PNG");
        std::vector<char> imgData(rc.addr, rc.addr + rc.size);
        defaultMat = cv::imdecode(cv::Mat(imgData), cv::IMREAD_UNCHANGED);
    }
    return defaultMat;
}

static cv::Mat& getHomeMat() {
    static cv::Mat homeMat;
    static bool isInit = false;

    if (!isInit) {
        isInit = true;
        auto rc = Utils::GetResource(IDB_PNG1, L"PNG");
        std::vector<char> imgData(rc.addr, rc.addr + rc.size);
        homeMat = cv::imdecode(cv::Mat(imgData), cv::IMREAD_UNCHANGED);
    }
    return homeMat;
}

static void processMainImg(int curFileIdx, cv::Mat& mainImg) {
    isBusy = true;

    cv::Mat& srcImg = curFileIdx < 0 ? getHomeMat() : myLRU.get(imgFileList[curFileIdx], Utils::loadMat, getDefaultMat());

    processSrc(srcImg, mainImg);

    if (showExif && curFileIdx >= 0)
        stb.putText(mainImg, 10, 10, Utils::wstringToUtf8(getExif(imgFileList[curFileIdx], mainImg)).c_str(),
            { 255, 255, 255, 255 });

    if (curFileIdx >= 0) {
        wstring str = std::format(L" [{}/{}] {}% ",
            curFileIdx + 1, imgFileList.size(),
            zoomCur * 100ULL / ZOOM_BASE)
            + imgFileList[curFileIdx];
        SetWindowTextW(mainHWND, str.c_str());
    }
    else {
        SetWindowTextW(mainHWND, appName);
    }

    isBusy = false;
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

static int myMain(std::wstring& _fullPath, HINSTANCE hInstance) {
    using std::filesystem::path;
    using std::filesystem::directory_iterator;
    using std::filesystem::directory_entry;
    using std::filesystem::exists;

    cv::namedWindow(windowName, cv::WindowFlags::WINDOW_NORMAL);  // WINDOW_AUTOSIZE  WINDOW_NORMAL
    mainHWND = getCvWindow(windowName.c_str());
    ShowWindow(mainHWND, SW_MAXIMIZE); //最大化窗口
    if (hInstance) setCvWindowIcon(hInstance, mainHWND, IDI_JARKVIEWER);

    cv::setMouseCallback(windowName, onMouseHandle);

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    std::filesystem::path fullPath(_fullPath);
    if (exists(directory_entry(fullPath))) {
        auto workDir = fullPath.parent_path();

        for (auto const& dir_entry : directory_iterator{ workDir }) {
            if (!dir_entry.is_regular_file())continue;

            wstring extName = path(dir_entry).extension().wstring();
            for (auto& c : extName) c = std::tolower(c);

            if (supportExt.count(extName))
                imgFileList.emplace_back(workDir.wstring() + L"\\" + path(dir_entry).filename().wstring());
        }
        for (int i = 0; i < imgFileList.size(); i++) {
            if (imgFileList[i] == _fullPath) {
                curFileIdx = i;
                break;
            }
        }
    }
    else {
        curFileIdx = -1;
    }

    while (true) {
        auto ws = cv::getWindowImageRect(windowName);
        if ((ws.width != winSize.width || ws.height != winSize.height) && ws.width != 0 && ws.height != 0) {
            needFresh = true;
            zoomTarget = 0;
            if (winSize.width)
                hasDropTarget.x = hasDropTarget.x * ws.width / winSize.width;
            if (winSize.height)
                hasDropTarget.y = hasDropTarget.y * ws.height / winSize.height;

            winSize = ws;

            mainImg = cv::Mat(ws.height, ws.width, CV_8UC4);
        }

        if (needFresh) {
            needFresh = false;

            if (switchOperate && curFileIdx >= 0) {
                curFileIdx += switchOperate > 0 ? (-1) : (1);

                if (curFileIdx < 0)
                    curFileIdx = (int)imgFileList.size() - 1;
                else if (curFileIdx >= (int)imgFileList.size())
                    curFileIdx = 0;

                switchOperate = 0;
                zoomOperate = 0;
                hasDropCur = { 0, 0 };
                hasDropTarget = { 0, 0 };
                zoomTarget = 0;
                zoomCur = 0;
            }

            processMainImg(curFileIdx, mainImg);
            cv::imshow(windowName, mainImg);

            const int progressMax = 8;
            static int progressCnt = progressMax;
            static int64_t zoomInit = 0;
            static Cood hasDropInit{};
            if (zoomCur != zoomTarget) { // 简单缩放动画
                needFresh = true;

                if (progressCnt >= (progressMax - 1)) {
                    progressCnt = 0;
                    zoomInit = zoomCur;
                    hasDropInit = hasDropCur;
                }
                else {
                    progressCnt += ((progressMax - progressCnt) / 2);
                    if (progressCnt >= (progressMax - 1)) {
                        zoomCur = zoomTarget;
                        hasDropCur = hasDropTarget;
                    }
                    else {
                        zoomCur = zoomInit + (zoomTarget - zoomInit) * progressCnt / progressMax;
                        hasDropCur = hasDropInit + (hasDropTarget - hasDropInit) * progressCnt / progressMax;
                    }
                }
            }
        }

        if (cv::waitKey(5) == 27) //ESC
            break;

        if (!IsWindowVisible(mainHWND))
            break;
    }
    cv::destroyAllWindows();
    return 0;
}

// 在 属性-链接器-系统-子系统 更改模式
static int wmain(int argc, wchar_t* argv[]) {

    wstring fullPath(*argv[1] == L'\"' ? argv[1] + 1 : argv[1]);
    if (fullPath.length() && fullPath[fullPath.length() - 1] == L'\"')
        fullPath.pop_back();

    return myMain(fullPath, nullptr);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow) {

    wstring fullPath(*lpCmdLine == '\"' ? lpCmdLine + 1 : lpCmdLine);
    if (fullPath.length() && fullPath[fullPath.length() - 1] == L'\"')
        fullPath.pop_back();

    return myMain(fullPath, hInstance);
}


void onMouseHandle(int event, int x, int y, int flags, void* param) {
    using namespace std::chrono;

    static Cood mousePress;
    static bool isPresing = false;
    static bool cursorIsView = false; // 鼠标位置，若在看图区域，滚轮就缩放，其他区域滚轮就切换图片
    static bool fullScreen = false;
    static auto lastTimestamp = system_clock::now();

    switch (event)
    {
    case cv::EVENT_MOUSEMOVE: {
        mouse.x = x; //这里获取的是当前像素在源图像的坐标， 不是绘图窗口坐标
        mouse.y = y;
        if (winSize.width >= 500)
            cursorIsView = (50 < x) && (x < (winSize.width - 50)); // 在窗口中间
        else
            cursorIsView = ((winSize.width / 5) < x) && (x < (winSize.width * 4 / 5));

        if (isPresing) {
            hasDropTarget = mouse - mousePress;
            hasDropCur = hasDropTarget;
            needFresh = true;
        }
    }break;

        //左键按下
    case cv::EVENT_LBUTTONDOWN: {
        mousePress = mouse - hasDropTarget;
        isPresing = true;
        //Utils::log("press {} {}", mousePress.x, mousePress.y);
    }break;

        //左键抬起
    case cv::EVENT_LBUTTONUP: {
        isPresing = false;
        auto now = system_clock::now();
        auto delta = duration_cast<milliseconds>(now - lastTimestamp).count();
        if (delta < 500) { // 500ms
            cv::setWindowProperty(windowName, cv::WND_PROP_FULLSCREEN, fullScreen ? cv::WINDOW_NORMAL : cv::WINDOW_FULLSCREEN); // 全屏
            fullScreen = !fullScreen;
        }
        lastTimestamp = now;
    }break;

    case cv::EVENT_MBUTTONUP: {
        showExif = !showExif;
        needFresh = true;
    }break;

    case cv::EVENT_RBUTTONUP: {
        exit(0);
    }break;

        // 滚轮
    case cv::EVENT_MOUSEWHEEL: {
        if (isBusy) break;
        int mouseWheel = cv::getMouseWheelDelta(flags);
        if (cursorIsView) {//光标在 缩放 区域
            zoomOperate = mouseWheel;
        }
        else {  //光标在 切图 区域
            switchOperate = mouseWheel;
        }
        needFresh = true;
    }
    }
}

static std::wstring getExif(const wstring& path, cv::Mat& img) {
    using std::to_string;
    static unordered_map<wstring, wstring> exifMap;

    auto it = exifMap.find(path);
    if (it != exifMap.end())
        return it->second;

    if (exifMap.size() > 1000) // 不考虑LRU缓存, 多了直接清理
        exifMap.clear();

    auto res = std::format(L"  分辨率: {}x{}", getWidth(img), getHeight(img));

    auto ext = path.substr(path.length() - 4, 4);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](wchar_t c) { return std::tolower(c); }
    );
    if (ext != L".jpg" && ext != L"jpeg" && ext != L".jpe") {
        exifMap[path] = res;
        return res;
    }

    easyexif::EXIFInfo exifInfo(Utils::wstringToAnsi(path).c_str());
    if (!exifInfo.hasInfo) {
        exifMap[path] = res;
        return res;
    }

    if (exifInfo.ISOSpeedRatings)
        res += std::format(L"\n     ISO: {}", exifInfo.ISOSpeedRatings);

    if (exifInfo.FNumber)
        res += std::format(L"\n   F光圈: f/{}", exifInfo.FNumber);

    if (exifInfo.FocalLength)
        res += std::format(L"\n    焦距: {} mm", exifInfo.FocalLength);

    if (exifInfo.FocalLengthIn35mm)
        res += std::format(L"\n35mm焦距: {} mm", exifInfo.FocalLengthIn35mm);

    if (exifInfo.ExposureBiasValue)
        res += std::format(L"\n曝光误差: {} Ev", exifInfo.ExposureBiasValue);

    if (exifInfo.ExposureTime)
        res += std::format(L"\n曝光时长: {:.2f} s", exifInfo.ExposureTime);

    if (exifInfo.SubjectDistance)
        res += std::format(L"\n目标距离: {:.2f} m", exifInfo.SubjectDistance);

    if (exifInfo.GeoLocation.Longitude)
        res += std::format(L"\nGPS 经度: {:.6f}°", exifInfo.GeoLocation.Longitude);

    if (exifInfo.GeoLocation.Latitude)
        res += std::format(L"\nGPS 纬度: {:.6f}°", exifInfo.GeoLocation.Latitude);

    if (exifInfo.GeoLocation.Altitude)
        res += std::format(L"\n海拔高度: {:.2f} m", exifInfo.GeoLocation.Altitude);

    if (exifInfo.Copyright.length())
        res += std::format(L"\n    版权: {}", Utils::utf8ToWstring(exifInfo.Copyright).c_str());

    if (exifInfo.Software.length())
        res += std::format(L"\n    软件: {}", Utils::utf8ToWstring(exifInfo.Software).c_str());

    if (exifInfo.ImageDescription.length())
        res += std::format(L"\n    描述: {}", Utils::utf8ToWstring(exifInfo.ImageDescription).c_str());

    if (exifInfo.Flash)
        res += L"\n    闪光: 是";

    if (exifInfo.DateTime.length())
        res += std::format(L"\n创建时间: {}", Utils::utf8ToWstring(exifInfo.DateTime).c_str());

    if (exifInfo.DateTimeOriginal.length())
        res += std::format(L"\n原始时间: {}", Utils::utf8ToWstring(exifInfo.DateTimeOriginal).c_str());

    if (exifInfo.DateTimeDigitized.length())
        res += std::format(L"\n数字时间: {}", Utils::utf8ToWstring(exifInfo.DateTimeDigitized).c_str());

    if (exifInfo.Make.length() + exifInfo.Model.length())
        res += std::format(L"\n相机型号: {}", Utils::utf8ToWstring(exifInfo.Make + " " + exifInfo.Model).c_str());

    if (exifInfo.LensInfo.Make.length() + exifInfo.LensInfo.Model.length())
        res += std::format(L"\n镜头型号: {}", Utils::utf8ToWstring(exifInfo.LensInfo.Make + " " + exifInfo.LensInfo.Model).c_str());

    exifMap[path] = res;
    return res;
}
