
#include "Utils.h"

#include "stbText.h"
#include "LRU.h"


const int BG_GRID_WIDTH = 8;
const cv::Vec4b BG_COLOR_DARK{ 40, 40, 40, 255 };
const cv::Vec4b BG_COLOR_LIGHT{ 60, 60, 60, 255 };
const cv::Vec4b BG_COLOR{ 70, 70, 70, 255 };

HWND mainHWND;
const string appName = "JarkViewer";
const string windowName = "mainWindows";
const string milliSecStr = Utils::utf8ToAnsi("毫秒");
const int64_t ZOOM_BASE = 1 << 16; // 基础缩放
const int64_t ZOOM_MIN = 1 << 10;
const int64_t ZOOM_MAX = 1 << 22;

cv::Mat mainImg;

bool isBusy = false;
bool needFresh = false;

Cood mouse{}, hasDrop{};
int zoomOperate = 0;     // 缩放
int switchOperate = 0;   // 切图
int64_t zoomValue = 0;   // 缩放比例

int curFileIdx = -1; //文件在路径列表的索引
vector<string> imgFileList;

stbText stb;
cv::Mat defaultMat, homeMat;
cv::Rect winSize;

LRU<string, cv::Mat, 10> myLRU;

const set<string> supportExt = {
    ".jpg", ".jp2", ".jpeg", ".jpe", ".bmp", ".dib", ".png",
    ".pbm", ".pgm", ".ppm",".pxm",".pnm",".sr", ".ras",
    ".exr", ".tiff", ".tif", ".webp", ".hdr", ".pic", };

int getWidth(cv::Mat& mat) { return mat.cols; }
int getHeight(cv::Mat& mat) { return mat.rows; }

void onMouseHandle(int event, int x, int y, int flags, void* param);
void mainCycle(std::filesystem::path& fullPath);

template<typename... Args>
void log(const string_view fmt, Args&&... args) {

    auto ts = time(nullptr) + 8 * 3600ULL;// UTC+8
    int HH = (ts / 3600) % 24;
    int MM = (ts / 60) % 60;
    int SS = ts % 60;
    cout << std::format("[{:02d}:{:02d}:{:02d}] ", HH, MM, SS) << std::vformat(fmt, std::make_format_args(args...)) << endl;
}


cv::Vec4b getSrcPx(cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) {
    switch (srcImg.channels()) {
    case 3: {
        auto& px = srcImg.at<cv::Vec3b>(srcY, srcX);
        return { px[0], px[1], px[2], 255 };
    }
    case 1: {
        auto& px = srcImg.at<uchar>(srcY, srcX);
        return cv::Vec4b{ px, px, px, 255 };
    }
    case 4: {
        auto& px = srcImg.at<cv::Vec4b>(srcY, srcX);
        cv::Vec4b bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? BG_COLOR_DARK : BG_COLOR_LIGHT;

        if (px[3] == 255) return px;
        else if (px[3] == 0) return bgPx;

        const int alpha = px[3];
        bgPx[3] = alpha;
        for (int i = 0; i < 3; i++)
            bgPx[i] = (bgPx[i] * (255 - alpha) + px[i] * alpha) / (255);
        return bgPx;
    }
    }

    return ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? BG_COLOR_DARK : BG_COLOR_LIGHT;
}

void processSrc(cv::Mat& srcImg, cv::Mat& mainImg) {
    static int64_t curImgZoomBase = 0;

    int srcH = getHeight(srcImg);
    int srcW = getWidth(srcImg);
    int mainH = getHeight(mainImg);
    int mainW = getWidth(mainImg);

    if (zoomValue == 0) {
        if (srcH > mainH || srcW > mainW) {
            curImgZoomBase = std::min(mainH * ZOOM_BASE / srcH, mainW * ZOOM_BASE / srcW);
            zoomValue = curImgZoomBase;
        }
        else {
            zoomValue = ZOOM_BASE;
        }
    }

    if (zoomOperate > 0) {
        zoomOperate = 0;
        if (zoomValue < ZOOM_MAX) {
            int64 lastZoom = zoomValue;

            if (zoomValue == curImgZoomBase) {
                int i = 0;
                while (zoomValue)
                {
                    zoomValue >>= 1;
                    i++;
                }
                zoomValue = 1;
                while (i--)
                {
                    zoomValue <<= 1;
                }
            }
            else {
                int64 nextZoom = zoomValue * 2;
                if (zoomValue < curImgZoomBase && nextZoom >= curImgZoomBase)
                    zoomValue = curImgZoomBase;
                else
                    zoomValue = nextZoom;
            }

            if (lastZoom) {
                hasDrop.x = (int)(zoomValue * hasDrop.x / lastZoom);
                hasDrop.y = (int)(zoomValue * hasDrop.y / lastZoom);
            }
        }
    }
    else if (zoomOperate < 0) {
        zoomOperate = 0;
        if (zoomValue > ZOOM_MIN) {
            int64 lastZoom = zoomValue;

            if (zoomValue == curImgZoomBase) {
                int i = 0;
                while (zoomValue)
                {
                    zoomValue >>= 1;
                    i++;
                }
                zoomValue = 1;
                while (i--)
                {
                    zoomValue <<= 1;
                }
                zoomValue >>= 1;
            }
            else {
                int64_t nextZoom = zoomValue / 2;
                if (zoomValue > curImgZoomBase && nextZoom <= curImgZoomBase)
                    zoomValue = curImgZoomBase;
                else
                    zoomValue = nextZoom;
            }

            if (lastZoom) {
                hasDrop.x = (int)(zoomValue * hasDrop.x / lastZoom);
                hasDrop.y = (int)(zoomValue * hasDrop.y / lastZoom);
            }
        }
    }

    const int64_t deltaW = hasDrop.x + (mainW - srcW * zoomValue / ZOOM_BASE) / 2;
    const int64_t deltaH = hasDrop.y + (mainH - srcH * zoomValue / ZOOM_BASE) / 2;

    for (int y = 0; y < mainH; y++)
        for (int x = 0; x < mainW; x++) {

            int srcX = (int)((x - deltaW) * ZOOM_BASE / zoomValue);
            int srcY = (int)((y - deltaH) * ZOOM_BASE / zoomValue);

            if (srcX >= 0 && srcX < srcW && srcY >= 0 && srcY < srcH)
                mainImg.at<cv::Vec4b>(y, x) = getSrcPx(srcImg, srcX, srcY, x, y);
            else
                mainImg.at<cv::Vec4b>(y, x) = BG_COLOR;
        }
}


void processMainImg(int curFileIdx, cv::Mat& mainImg) {
    static wstring millStr = Utils::utf8ToWstring("毫秒");
    isBusy = true;

    using namespace std::chrono;
    auto st = system_clock::now();

    cv::Mat& srcImg = curFileIdx < 0 ? homeMat : myLRU.get(imgFileList[curFileIdx], Utils::loadMat, defaultMat);

    processSrc(srcImg, mainImg);

    auto duration = duration_cast<microseconds>(system_clock::now() - st).count();
    if (curFileIdx >= 0) {
        if (duration >= 1000)
            SetWindowTextW(mainHWND, std::format(L"[{}/{}] [ {} {} ] {}% {}",
                curFileIdx + 1, imgFileList.size(),
                duration / 1000, millStr.c_str(), zoomValue * 100ULL / ZOOM_BASE, Utils::ansiToWstring(imgFileList[curFileIdx]).c_str()).c_str());
        else
            SetWindowTextW(mainHWND, std::format(L"[{}/{}] [ {:.2f} {} ] {}% {}",
                curFileIdx + 1, imgFileList.size(),
                duration / 1000.0, millStr.c_str(), zoomValue * 100ULL / ZOOM_BASE, Utils::ansiToWstring(imgFileList[curFileIdx]).c_str()).c_str());
    }
    else {
        SetWindowTextW(mainHWND, L"jarkViewer");
    }

    isBusy = false;
}

int myMain(std::filesystem::path& fullPath) {
    using std::filesystem::path;
    using std::filesystem::directory_iterator;
    using std::filesystem::directory_entry;
    using std::filesystem::exists;

    cv::setMouseCallback(windowName, onMouseHandle);

    auto rc = Utils::GetResource(IDB_PNG, L"PNG");
    std::vector<char> imgData(rc.addr, rc.addr + rc.size);
    defaultMat = cv::imdecode(cv::Mat(imgData), cv::IMREAD_UNCHANGED);

    rc = Utils::GetResource(IDB_PNG1, L"PNG");
    std::vector<char> imgData2(rc.addr, rc.addr + rc.size);
    homeMat = cv::imdecode(cv::Mat(imgData2), cv::IMREAD_UNCHANGED);

    if (exists(directory_entry(fullPath))) {
        auto workDir = fullPath.parent_path();

        for (auto const& dir_entry : directory_iterator{ workDir }) {
            if (!dir_entry.is_regular_file())continue;

            string extName = path(dir_entry).extension().string();
            for (auto& c : extName) c = std::tolower(c);

            if (supportExt.count(extName))
                imgFileList.emplace_back(workDir.string() + "\\" + path(dir_entry).filename().string());
        }
        for (int i = 0; i < imgFileList.size(); i++) {
            if (imgFileList[i] == fullPath) {
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
            winSize = ws;

            mainImg = cv::Mat(ws.height, ws.width, CV_8UC4);

            needFresh = true;
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
                hasDrop = { 0, 0 };
                zoomValue = 0;
            }

            processMainImg(curFileIdx, mainImg);
            cv::imshow(windowName, mainImg);
        }

        if (cv::waitKey(5) == 27) //ESC
            break;


        if (!IsWindowVisible(mainHWND))
            break;
        //if (cv::getWindowProperty(windowName, cv::WND_PROP_VISIBLE) == 0) {
        //    break;
        //}
        //bool bVisible = (::GetWindowLong(mainHWND, GWL_STYLE) & WS_VISIBLE) != 0;
        //if (!bVisible)
        //    break;
    }
    cv::destroyAllWindows();
    return 0;
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


// 在 属性-链接器-系统-子系统 更改模式
int wmain(int argc, wchar_t* argv[]) {

    wstring str(*argv[1] == L'\"' ? argv[1] + 1 : argv[1]);
    if (str.length() > 2 && str[str.length() - 1] == '\"')
        str.pop_back();

    std::filesystem::path fullPath(str);

    cv::namedWindow(windowName, cv::WindowFlags::WINDOW_NORMAL);  // WINDOW_AUTOSIZE  WINDOW_NORMAL
    mainHWND = getCvWindow(windowName.c_str());

    system("CHCP 65001");
    log("启动控制台");

    return myMain(fullPath);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow) {

    wstring str(*lpCmdLine == '\"' ? lpCmdLine + 1 : lpCmdLine);
    if (str.length() > 2 && str[str.length() - 1] == '\"')
        str.pop_back();

    std::filesystem::path fullPath(str);

    cv::namedWindow(windowName, cv::WindowFlags::WINDOW_NORMAL);  // WINDOW_AUTOSIZE  WINDOW_NORMAL
    mainHWND = getCvWindow(windowName.c_str());
    ShowWindow(mainHWND, SW_MAXIMIZE); //最大化窗口
    setCvWindowIcon(hInstance, mainHWND, IDI_JARKVIEWER);

    return myMain(fullPath);
}


void onMouseHandle(int event, int x, int y, int flags, void* param) {
    using namespace std::chrono;

    static Cood mousePress;
    static bool isPresing = false;
    static bool cursorIsView = false;
    static bool fullScreen = false;
    static auto lastTimestamp = system_clock::now();

    //static HCURSOR hCursor = LoadCursor(NULL, IDC_ARROW);
    //SetCursor(hCursor);// 设置窗口光标

    switch (event)
    {
    case cv::EVENT_MOUSEMOVE: {
        mouse.x = x; //这里获取的是当前像素在源图像的坐标， 不是绘图窗口坐标
        mouse.y = y;
        cursorIsView = x < (winSize.width >= 1000 ? (winSize.width - 100) : (winSize.width * 4 / 5));
        if (isPresing) {
            hasDrop = mouse - mousePress;
            needFresh = true;
        }
    }break;

        //左键按下
    case cv::EVENT_LBUTTONDOWN: {
        mousePress = mouse - hasDrop;
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