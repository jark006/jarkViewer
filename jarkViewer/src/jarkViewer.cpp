
#include "Utils.h"

#include "stbText.h"
#include "LRU.h"


// TODO GIF 等动图

//#define TEST

const int BG_GRID_WIDTH = 8;
//const cv::Vec4b BG_COLOR_DARK{ 40, 40, 40, 255 };
//const cv::Vec4b BG_COLOR_LIGHT{ 60, 60, 60, 255 };
//const cv::Vec4b BG_COLOR{ 70, 70, 70, 255 };

HWND mainHWND;
const uint32_t GRID_DARK = 0xFF282828;// { 40, 40, 40, 255 };
const uint32_t GRID_LIGHT = 0xFF3c3c3c;// { 60, 60, 60, 255 };
const uint32_t BG_COLOR = 0xFF464646;// { 70, 70, 70, 255 };

const int fpsMax = 120;
const auto target_duration = std::chrono::microseconds(1000000 / fpsMax);
auto last_end = std::chrono::high_resolution_clock::now();

const wstring appName = L"JarkViewer V1.2";
const string windowName = "mainWindows";
const int64_t ZOOM_BASE = 1 << 16; // 基础缩放
const int64_t ZOOM_MIN = 1 << 10;
const int64_t ZOOM_MAX = 1 << 22;
int64_t zoomFitWindow = 0;  //适应显示窗口宽高的缩放比例
cv::Mat mainImg;

bool isBusy = false;
bool needFresh = false;
bool showExif = false;
bool requitExit = false;

Cood mouse{}, hasDropCur{}, hasDropTarget{};
int zoomOperate = 0;     // 缩放操作
int switchOperate = 0;   // 切换图片操作
int64_t zoomTarget = 0;  // 设定的缩放比例
int64_t zoomCur = 0;     // 动画播放过程的缩放比例，动画完毕后的值等于zoomTarget


stbText stb; //给Mat绘制文字
cv::Rect winSize(0, 0, 200, 100);

//LRU<wstring, cv::Mat, 10> myLRU;
LRU<wstring, ImageNode> imageDB(Utils::loadImage);

const unordered_set<wstring> supportExt = {
    L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png",
    L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
    L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic" , 
    L".heic" , L".heif", L".avif" };


void onMouseHandle(int event, int x, int y, int flags, void* param);
void test();

static uint32_t getSrcPx(cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) {
    switch (srcImg.channels()) {
    case 3: {
        auto& srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

        intUnion ret=255;
        if (zoomCur < ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            auto& px0 = srcImg.at<cv::Vec3b>(srcY - 1, srcX - 1);
            auto& px1 = srcImg.at<cv::Vec3b>(srcY - 1, srcX);
            auto& px2 = srcImg.at<cv::Vec3b>(srcY, srcX - 1);
            for (int i = 0; i < 3; i++)
                ret[i] = (px0[i] + px1[i] + px2[i] + srcPx[i]) / 4;

            return ret.u32;
        }
        ret[0] = srcPx[0];
        ret[1] = srcPx[1];
        ret[2] = srcPx[2];
        return ret.u32;
    }
    case 4: {
        intUnion srcPx = srcImg.at<intUnion>(srcY, srcX);
        intUnion bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ?
            GRID_DARK : GRID_LIGHT;

        intUnion px;
        if (zoomCur < ZOOM_BASE && srcY > 0 && srcX > 0) {
            auto& px0 = srcImg.at<intUnion>(srcY - 1, srcX - 1);
            auto& px1 = srcImg.at<intUnion>(srcY - 1, srcX);
            auto& px2 = srcImg.at<intUnion>(srcY, srcX - 1);
            for (int i = 0; i < 4; i++)
                px[i] = (px0[i] + px1[i] + px2[i] + srcPx[i]) / 4;
        }
        else {
            px = srcPx;
        }

        if (px[3] == 0) return bgPx.u32;
        else if (px[3] == 255) return intUnion(px[0], px[1], px[2], 255).u32;;

        const int alpha = px[3];
        intUnion ret= alpha;
        for (int i = 0; i < 3; i++)
            ret[i] = (bgPx[i] * (255 - alpha) + px[i] * alpha) / 256;
        return ret.u32;
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
        return intUnion(srcPx, srcPx, srcPx, 255).u32;
    }
    }

    return ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? 
        GRID_DARK : GRID_LIGHT;
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

    const int srcH = srcImg.rows;
    const int srcW = srcImg.cols;

    if (zoomTarget == 0) {
        zoomFitWindow = (srcH > 0 && srcW > 0) ? 
            std::min(winSize.height * ZOOM_BASE / srcH, winSize.width * ZOOM_BASE / srcW) : 
            ZOOM_BASE;
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

            mainImg.at<uint32_t>(y, x) = (srcX >= 0 && srcX < srcW && srcY >= 0 && srcY < srcH) ?
                getSrcPx(srcImg, srcX, srcY, x, y) : BG_COLOR;
        }
}



static void processMainImg(const wstring& path, cv::Mat& mainImg) {
    isBusy = true;

    auto& imgNode = imageDB.get(path);
    cv::Mat& srcImg = imgNode.img;

    processSrc(srcImg, mainImg);

    if (showExif) {
        const int padding = 10;
        RECT r{ padding, padding, winSize.width-padding, winSize.height-padding};
        stb.putLeft(mainImg, r, imgNode.exifStr.c_str(), { 255, 255, 255, 255 });
    }

    isBusy = false;
}



static int myMain(const wstring filePath, HINSTANCE hInstance) {
    namespace fs = std::filesystem;

#ifdef TEST
    test();
    exit(0);
#endif // TEST

    int curFileIdx = -1;         //文件在路径列表的索引
    vector<wstring> imgFileList; //工作目录下所有图像文件路径

    cv::namedWindow(windowName, cv::WindowFlags::WINDOW_NORMAL);  // WINDOW_AUTOSIZE  WINDOW_NORMAL
    mainHWND = Utils::getCvWindow(windowName.c_str());
    ShowWindow(mainHWND, SW_MAXIMIZE); //最大化窗口
    if (hInstance) Utils::setCvWindowIcon(hInstance, mainHWND, IDI_JARKVIEWER);

    cv::setMouseCallback(windowName, onMouseHandle);

    fs::path fullPath = fs::absolute(filePath);
    wstring fileName = fullPath.filename().wstring();

    if (fs::exists(fs::directory_entry(fullPath))) {
        auto workDir = fullPath.parent_path();

        for (const auto& entry : fs::directory_iterator(workDir)) {
            if (!entry.is_regular_file())continue;

            std::wstring ext = entry.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

            if (supportExt.contains(ext)){
                imgFileList.push_back(fs::absolute(entry.path()).wstring());
                if (curFileIdx == -1 && fileName == entry.path().filename())
                    curFileIdx = (int)imgFileList.size() - 1;
            }
        }
    }
    else {
        curFileIdx = -1;
    }

    if (curFileIdx == -1) {
        if (filePath.empty()) { //直接打开软件，没有传入参数
            imgFileList.emplace_back(appName);
            curFileIdx = (int)imgFileList.size() - 1;
            imageDB.put(appName, { Utils::getHomeMat(), "请在图像文件右键使用本软件打开" });
        }
        else { // 打开的文件不支持，默认加到尾部
            imgFileList.emplace_back(fullPath.wstring());
            curFileIdx = (int)imgFileList.size() - 1;
            imageDB.put(fullPath.wstring(), { Utils::getDefaultMat(), "图像格式不支持或已删除" });
        }
    }

    while (true) {
        auto ws = cv::getWindowImageRect(windowName);
        if ((ws.width != winSize.width || ws.height != winSize.height) && ws.width != 0 && ws.height != 0) {
            needFresh = true;
            if (winSize.width)
                hasDropTarget.x = hasDropTarget.x * ws.width / winSize.width;
            if (winSize.height)
                hasDropTarget.y = hasDropTarget.y * ws.height / winSize.height;

            winSize = ws;

            mainImg = cv::Mat(ws.height, ws.width, CV_8UC4);
        }

        if (needFresh) {
            needFresh = false;

            if (switchOperate) {
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

            processMainImg(imgFileList[curFileIdx], mainImg);
            cv::imshow(windowName, mainImg);

            wstring str = std::format(L" [{}/{}] {}% ",
                curFileIdx + 1, imgFileList.size(),
                zoomCur * 100ULL / ZOOM_BASE)
                + imgFileList[curFileIdx];
            SetWindowTextW(mainHWND, str.c_str());

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
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    if(argc == 1)
        return myMain(L"", nullptr);

    wstring filePath(*argv[1] == L'\"' ? argv[1] + 1 : argv[1]);
    if (filePath.length() && filePath[filePath.length() - 1] == L'\"')
        filePath.pop_back();
    return myMain(filePath, nullptr);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow) {

    wstring filePath(*lpCmdLine == '\0' ? L"" : (*lpCmdLine == '\"' ? lpCmdLine + 1 : lpCmdLine));
    if (filePath.length() && filePath[filePath.length() - 1] == L'\"')
        filePath.pop_back();

    return myMain(filePath, hInstance);
}


void onMouseHandle(int event, int x, int y, int flags, void* param) {
    using namespace std::chrono;

    static Cood mousePress;
    static bool isPresing = false;
    static bool cursorIsView = true; // 鼠标位置，若在看图区域，滚轮就缩放，其他区域滚轮就切换图片
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


void test() {

}