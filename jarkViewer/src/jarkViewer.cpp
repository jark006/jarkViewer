
#include "Utils.h"

#include "stbText.h"
#include "ImageDatabase.h"

/*
TODO
1. libpsd有BUG，部分PSD解码异常
2. exif 的旋转信息
3. avif crop 无法解码 kimono.crop.avif
4. 重构程序结构
*/

const int BG_GRID_WIDTH = 8;
const uint32_t BG_COLOR = 0x46;
const uint32_t GRID_DARK = 0xFF282828;
const uint32_t GRID_LIGHT = 0xFF3C3C3C;

const int fpsMax = 120;
const auto target_duration = std::chrono::microseconds(1000000 / fpsMax);
auto last_end = std::chrono::high_resolution_clock::now();

const wstring appName = L"JarkViewer V1.4";
const string windowName = "mainWindows";

const vector<int64_t> ZOOM_LIST = {
    1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16, 
    1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22,
};
const int64_t ZOOM_BASE = ZOOM_LIST[ZOOM_LIST.size()/2]; // 100%缩放

bool needFresh = false;
bool showExif = false;
int zoomOperate = 0;        // 缩放操作
int switchOperate = 0;      // 切换图片操作
int64_t zoomTarget = 0;     // 设定的缩放比例
int64_t zoomCur = 0;        // 动画播放过程的缩放比例，动画完毕后的值等于zoomTarget

WinSize winSize(200, 100);
Cood dropCur, dropTarget;

void onMouseHandle(int event, int x, int y, int flags, void* param);
void test();

static uint32_t getSrcPx(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) {
    switch (srcImg.channels()) {
    case 3: {
        cv::Vec3b srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

        intUnion ret=255;
        if (zoomCur < ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            cv::Vec3b px0 = srcImg.at<cv::Vec3b>(srcY - 1, srcX - 1);
            cv::Vec3b px1 = srcImg.at<cv::Vec3b>(srcY - 1, srcX);
            cv::Vec3b px2 = srcImg.at<cv::Vec3b>(srcY, srcX - 1);
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
            intUnion srcPx1 = srcImg.at<intUnion>(srcY - 1, srcX - 1);
            intUnion srcPx2 = srcImg.at<intUnion>(srcY - 1, srcX);
            intUnion srcPx3 = srcImg.at<intUnion>(srcY, srcX - 1);
            for (int i = 0; i < 4; i++)
                px[i] = (srcPx1[i] + srcPx2[i] + srcPx3[i] + srcPx[i]) / 4;
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
    }

    return ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? 
        GRID_DARK : GRID_LIGHT;
}

static bool is_power_of_two(int64_t num) {
    return (num <= 0) ? 0 : ((num & (num - 1)) == 0);
}

static void drawCanvas(const cv::Mat& srcImg, cv::Mat& canvas) {
    static vector<int64_t> zoomList = ZOOM_LIST;
    static int zoomIndex = 0;

    const int srcH = srcImg.rows;
    const int srcW = srcImg.cols;

    if (srcH <= 0 || srcW <= 0)
        return;

    if (zoomTarget == 0) {
        //适应显示窗口宽高的缩放比例
        int64_t zoomFitWindow = std::min(winSize.width * ZOOM_BASE / srcW, winSize.height * ZOOM_BASE / srcH);
        zoomTarget = (srcH > winSize.height || srcW > winSize.width) ? zoomFitWindow : ZOOM_BASE;
        zoomCur = zoomTarget;

        zoomList = ZOOM_LIST;
        if (!is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
            zoomList.push_back(zoomFitWindow);
        std::sort(zoomList.begin(), zoomList.end());
        auto it = std::find(zoomList.begin(), zoomList.end(), zoomTarget);
        zoomIndex = (it != zoomList.end()) ? (int)std::distance(zoomList.begin(), it) : (int)(ZOOM_LIST.size() / 2);
    }

    if (zoomOperate) {
        if (zoomOperate > 0) {
            if (zoomIndex < zoomList.size() - 1)
                zoomIndex++;
        }
        else {
            if (zoomIndex > 0)
                zoomIndex--;
        }

        zoomOperate = 0;
        auto zoomNext = zoomList[zoomIndex];
        if (zoomTarget && zoomNext != zoomTarget) {
            dropTarget.x = (int)(zoomNext * dropTarget.x / zoomTarget);
            dropTarget.y = (int)(zoomNext * dropTarget.y / zoomTarget);
        }
        zoomTarget = zoomNext;
    }

    const int deltaW = dropCur.x + (int)((winSize.width - srcW * zoomCur / ZOOM_BASE) / 2);
    const int deltaH = dropCur.y + (int)((winSize.height - srcH * zoomCur / ZOOM_BASE) / 2);

    int xStart = deltaW < 0 ? 0 : deltaW;
    int yStart = deltaH < 0 ? 0 : deltaH;
    int xEnd = (int)(srcW * zoomCur / ZOOM_BASE + deltaW);
    int yEnd = (int)(srcH * zoomCur / ZOOM_BASE + deltaH);
    if (xEnd > winSize.width) xEnd = winSize.width;
    if (yEnd > winSize.height) yEnd = winSize.height;

    memset(canvas.ptr(), BG_COLOR, 4ULL * winSize.height * winSize.width);
    // 1360*768 1-10ms
    auto ptr = (uint32_t*)canvas.ptr();
    for (int y = yStart; y < yEnd; y++)
        for (int x = xStart; x < xEnd; x++) {
            const int srcX = (int)((x - deltaW) * ZOOM_BASE / zoomCur);
            const int srcY = (int)((y - deltaH) * ZOOM_BASE / zoomCur);
            if (0 <= srcX && srcX < srcW && 0 <= srcY && srcY < srcH)
                ptr[y * winSize.width + x] = getSrcPx(srcImg, srcX, srcY, x, y);
        }
}

static int myMain(const wstring filePath, HINSTANCE hInstance) {
    namespace fs = std::filesystem;

    test();

    ImageDatabase imgDB;

    int curFrameIdx = 0;         // 小于0则单张静态图像，否则为动画当前帧索引
    int curFrameIdxMax = 0;      // 若是动画则为帧数量
    int curFrameDelay = 0;       // 当前帧延迟
    int curFileIdx = -1;         // 文件在路径列表的索引
    vector<wstring> imgFileList; // 工作目录下所有图像文件路径

    stbText stb;                 // 给Mat绘制文字
    cv::Mat mainCanvas;          // 窗口内容画布

    cv::namedWindow(windowName, cv::WindowFlags::WINDOW_NORMAL);  // WINDOW_AUTOSIZE  WINDOW_NORMAL
    auto mainHWND = Utils::getCvWindow(windowName.c_str());
    ShowWindow(mainHWND, SW_MAXIMIZE); //最大化窗口
    if (hInstance) Utils::setCvWindowIcon(hInstance, mainHWND, IDI_JARKVIEWER);

    cv::setMouseCallback(windowName, onMouseHandle);

    fs::path fullPath = fs::absolute(filePath);
    wstring fileName = fullPath.filename().wstring();

    auto workDir = fullPath.parent_path();
    if (fs::exists(workDir)) {
        for (const auto& entry : fs::directory_iterator(workDir)) {
            if (!entry.is_regular_file())continue;

            std::wstring ext = entry.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

            if (ImageDatabase::supportExt.contains(ext) || ImageDatabase::supportRaw.contains(ext)) {
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
            imgDB().put(appName, {{{ImageDatabase::getHomeMat(), 0}}, "请在图像文件右键使用本软件打开"});

        }
        else { // 打开的文件不支持，默认加到尾部
            imgFileList.emplace_back(fullPath.wstring());
            curFileIdx = (int)imgFileList.size() - 1;
            imgDB().put(fullPath.wstring(), {{{ImageDatabase::getDefaultMat(), 0}}, "图像格式不支持或已删除"});
        }
    }

    while (true) {
        auto newSize = Utils::getWindowSize(mainHWND);
        if (newSize != winSize && !newSize.isZero()) {
            needFresh = true;
            if (winSize.width)
                dropTarget.x = dropTarget.x * newSize.width / winSize.width;
            if (winSize.height)
                dropTarget.y = dropTarget.y * newSize.height / winSize.height;

            winSize = newSize;

            mainCanvas = cv::Mat(newSize.height, newSize.width, CV_8UC4);
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

                dropCur = 0;
                dropTarget = 0;

                zoomCur = 0;
                zoomTarget = 0;

                curFrameIdx = 0;
                curFrameDelay = 0;
            }

            const auto& frames = imgDB().get(imgFileList[curFileIdx]);
            const auto& [srcImg, delay] = frames.imgList[curFrameIdx];

            curFrameIdxMax = (int)frames.imgList.size()-1;
            if (curFrameIdxMax > 0) { // Animation
                curFrameDelay = (delay <= 0 ? 10 : delay);
            }

            drawCanvas(srcImg, mainCanvas);

            if (showExif) {
                const int padding = 10;
                RECT r{ padding, padding, winSize.width/4 - padding, winSize.height - padding };
                stb.putAlignLeft(mainCanvas, r, frames.exifStr.c_str(), { 255, 255, 255, 255 }); // 长文本 8ms
            }

            cv::imshow(windowName, mainCanvas);

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
                    hasDropInit = dropCur;
                }
                else {
                    progressCnt += ((progressMax - progressCnt) / 2);
                    if (progressCnt >= (progressMax - 1)) {
                        zoomCur = zoomTarget;
                        dropCur = dropTarget;
                    }
                    else {
                        zoomCur = zoomInit + (zoomTarget - zoomInit) * progressCnt / progressMax;
                        dropCur = hasDropInit + (dropTarget - hasDropInit) * progressCnt / progressMax;
                    }
                }
            }
        }

        if (curFrameIdx < 0) {
            if (cv::waitKey(5) == 27) //ESC
                break;
        }
        else {
            static int delayRemain = 0;

            if (cv::waitKey(5) == 27) //ESC
                break;

            delayRemain -= 17;
            if (delayRemain <= 0) {
                delayRemain = curFrameDelay;
                curFrameIdx++;
                needFresh = true;
                if (curFrameIdx > curFrameIdxMax)
                    curFrameIdx = 0;
            }
        }

        if (!IsWindowVisible(mainHWND)) {
            Utils::log("IsWindowVisible false, exit.");
            break;
        }
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

    static Cood mouse, mousePress;
    static bool isPresing = false;
    static bool cursorIsView = true; // 鼠标位置，若在看图区域，滚轮就缩放，其他区域滚轮就切换图片
    static bool fullScreen = false;
    static auto lastTimestamp = system_clock::now();

    switch (event){
    case cv::EVENT_MOUSEMOVE: {
        mouse = { x, y };
        if (winSize.width >= 500)
            cursorIsView = (50 < x) && (x < (winSize.width - 50)); // 在窗口中间
        else
            cursorIsView = ((winSize.width / 5) < x) && (x < (winSize.width * 4 / 5));

        if (isPresing) {
            dropTarget = mouse - mousePress;
            dropCur = dropTarget;
            needFresh = true;
        }
    }break;

    case cv::EVENT_LBUTTONDOWN: {//左键按下
        mousePress = mouse - dropTarget;
        isPresing = true;
    }break;

    case cv::EVENT_LBUTTONUP: {//左键抬起
        isPresing = false;
        auto now = system_clock::now();
        auto delta = duration_cast<milliseconds>(now - lastTimestamp).count();
        if (50 < delta && delta < 500) { // 500ms
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

    case cv::EVENT_MOUSEWHEEL: { // 滚轮
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
    return;
    //std::ifstream file("D:\\Downloads\\ico.ico", std::ios::binary);
    //auto buf = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    //cv::Mat img(height, width, CV_8UC4, (void*)imageData);
    //cv::imshow("asd", img);
    //cv::waitKey(0);
    exit(0);
}