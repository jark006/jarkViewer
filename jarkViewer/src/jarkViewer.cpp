
#include "Utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stbText.h"
#include "ImageDatabase.h"

#include "D2D1App.h"
#include <wrl.h>

/* TODO
1. svg内嵌base64 image
3. eps

*/

const wstring appName = L"JarkViewer v1.15";



struct CurImageParameter {
    static const vector<int64_t> ZOOM_LIST;
    static const int64_t ZOOM_BASE = (1 << 16); // 100%缩放

    int64_t zoomTarget;     // 设定的缩放比例
    int64_t zoomCur;        // 动画播放过程的缩放比例，动画完毕后的值等于zoomTarget
    int curFrameIdx;        // 小于0则单张静态图像，否则为动画当前帧索引
    int curFrameIdxMax;     // 若是动画则为帧数量
    int curFrameDelay;      // 当前帧延迟
    Cood slideCur, slideTarget;
    Frames* framesPtr = nullptr;

    vector<int64_t> zoomList;
    int zoomIndex=0;
    bool isAnimation = false;
    int width = 0;
    int height = 0;

    CurImageParameter() {
        Init();
    }

    void Init(int winWidth = 0, int winHeight = 0) {

        curFrameIdx = 0;
        curFrameDelay = 0;

        slideCur = 0;
        slideTarget = 0;

        if (framesPtr) {
            isAnimation = framesPtr->imgList.size() > 1;
            curFrameIdxMax = (int)framesPtr->imgList.size() - 1;
            width = framesPtr->imgList[0].img.cols;
            height = framesPtr->imgList[0].img.rows;

            //适应显示窗口宽高的缩放比例
            int64_t zoomFitWindow = std::min(winWidth * ZOOM_BASE / width, winHeight * ZOOM_BASE / height);
            zoomTarget = (height > winHeight || width > winWidth) ? zoomFitWindow : ZOOM_BASE;
            zoomCur = zoomTarget;

            zoomList = ZOOM_LIST;
            if (!Utils::is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
                zoomList.push_back(zoomFitWindow);
            std::sort(zoomList.begin(), zoomList.end());
            auto it = std::find(zoomList.begin(), zoomList.end(), zoomTarget);
            zoomIndex = (it != zoomList.end()) ? (int)std::distance(zoomList.begin(), it) : (int)(ZOOM_LIST.size() / 2);
        }
        else {
            isAnimation = false;
            curFrameIdxMax = 0;
            width = 0;
            height = 0;

            zoomList = ZOOM_LIST;
            zoomIndex = (int)(ZOOM_LIST.size() / 2);
            zoomTarget = ZOOM_BASE;
            zoomCur = ZOOM_BASE;
        }
    }
};

const vector<int64_t> CurImageParameter::ZOOM_LIST = {
    1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16,
    1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22,
};

class OperateQueue {
private:
    std::queue<Action> queue;
    std::mutex mtx;

public:
    void push(Action action) {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push(action);
    }

    Action get() {
        std::unique_lock<std::mutex> lock(mtx);

        if (queue.empty())
            return { ActionENUM::unknow };

        Action res = queue.front();
        queue.pop();
        return res;
    }
};

class D2D1Template : public D2D1App {
public:
    const int BG_GRID_WIDTH = 16;
    const uint32_t BG_COLOR = 0x46;
    const uint32_t GRID_DARK = 0xFF282828;
    const uint32_t GRID_LIGHT = 0xFF3C3C3C;

    OperateQueue operateQueue;
    ID2D1SolidColorBrush* m_pBrush = nullptr;// 单色画刷
    IDWriteTextFormat* m_pTextFormat = nullptr;// 字体格式

    bool cursorIsView = true;
    bool mouseIsPressing = false;
    bool showExif = false;
    Cood mousePos, mousePressPos;
    int winWidth = 800;
    int winHeight = 600;

    ImageDatabase imgDB;

    int curFileIdx = -1;         // 文件在路径列表的索引
    vector<wstring> imgFileList; // 工作目录下所有图像文件路径

    stbText stb;                 // 给Mat绘制文字
    cv::Mat mainCanvas;          // 窗口内容画布

    CurImageParameter curPar;

    D2D1Template() {
        m_wndCaption = appName;
    }

    ~D2D1Template() {
        Utils::SafeRelease(m_pBrush);
        Utils::SafeRelease(m_pTextFormat);
    }

    HRESULT Initialize(HINSTANCE hInstance, int nCmdShow, LPWSTR lpCmdLine) {
        namespace fs = std::filesystem;

        if (!SUCCEEDED(D2D1App::Initialize(hInstance, nCmdShow))) {
            return S_FALSE;
        }
        if (hInstance) Utils::setCvWindowIcon(hInstance, m_hWnd, IDI_JARKVIEWER);


        HRESULT hr = S_OK;

        // 创建字体格式
        if (m_pDWriteFactory != NULL) {
            hr = m_pDWriteFactory->CreateTextFormat(
                L"Sitka Text",
                NULL,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                68.0,
                L"en-us",
                &m_pTextFormat);
        }

        // 创建画刷
        if (m_pD2DDeviceContext != NULL && SUCCEEDED(hr)) {
            hr = m_pD2DDeviceContext->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::DarkBlue), &m_pBrush);
        }


        wstring filePath(*lpCmdLine == '\0' ? L"" : (*lpCmdLine == '\"' ? lpCmdLine + 1 : lpCmdLine));
        if (filePath.length() && filePath[filePath.length() - 1] == L'\"')
            filePath.pop_back();

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
                curFileIdx = 0;
                imgDB.put(appName, { {{ImageDatabase::getHomeMat(), 0}}, "请在图像文件右键使用本软件打开" });
            }
            else { // 打开的文件不支持，默认加到尾部
                imgFileList.emplace_back(fullPath.wstring());
                curFileIdx = (int)imgFileList.size() - 1;
                imgDB.put(fullPath.wstring(), { {{ImageDatabase::getDefaultMat(), 0}}, "图像格式不支持或已删除" });
            }
        }

        curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
        if (m_pD2DDeviceContext) {
            D2D1_SIZE_U displaySize = m_pD2DDeviceContext->GetPixelSize();
            curPar.Init(displaySize.width, displaySize.height);
        }
        else {
            curPar.Init(800, 600);
            Utils::log("m_pD2DDeviceContext nullptr");
        }

        return hr;
    }

    void OnMouseDown(WPARAM btnState, int x, int y) {
        switch ((long long)btnState)
        {
        case WM_LBUTTONDOWN: {//左键
            mouseIsPressing = true;
            mousePressPos = { x, y };
        }break;

        case WM_RBUTTONDOWN: {//右键
        }break;

        case WM_MBUTTONDOWN: {//中键

        }break;

        default:
            break;
        }
    }

    void OnMouseUp(WPARAM btnState, int x, int y) {
        using namespace std::chrono;
        static auto lastTimestamp = system_clock::now();

        switch ((long long)btnState)
        {
        case WM_LBUTTONUP: {//左键
            mouseIsPressing = false;

            auto now = system_clock::now();
            auto delta = duration_cast<milliseconds>(now - lastTimestamp).count();
            if (50 < delta && delta < 300) { // 50 ~ 300 ms
                Utils::ToggleFullScreen(m_hWnd);
            }
            lastTimestamp = now;
        }break;

        case WM_RBUTTONUP: {//右键
            exit(0);
        }break;

        case WM_MBUTTONUP: {//中键
            operateQueue.push({ ActionENUM::toggleExif });
        }break;

        default:
            break;
        }
    }

    void OnMouseMove(WPARAM btnState, int x, int y) {
        mousePos = { x, y };
        if (winWidth >= 500)
            cursorIsView = (50 < x) && (x < (winWidth - 50)); // 在窗口中间
        else
            cursorIsView = ((winWidth / 5) < x) && (x < (winWidth * 4 / 5));

        if (mouseIsPressing) {
            auto slideDelta = mousePos - mousePressPos;
            mousePressPos = mousePos;
            operateQueue.push({ ActionENUM::slide, slideDelta.x, slideDelta.y });
        }
    }

    void OnMouseWheel(UINT nFlags, short zDelta, int x, int y) {
        operateQueue.push({
            cursorIsView ?
            (zDelta < 0 ? ActionENUM::zoomIn : ActionENUM::zoomOut) :
            (zDelta < 0 ? ActionENUM::nextImg : ActionENUM::preImg)
            });
    }

    void OnKeyUp(WPARAM keyValue) {
        switch (keyValue)
        {
        case VK_SPACE: { // 按空格复制图像信息到剪贴板
            Utils::copyToClipboard(Utils::utf8ToWstring(imgDB.get(imgFileList[curFileIdx]).exifStr));
        }break;

        case VK_F11: {
            Utils::ToggleFullScreen(m_hWnd);
        }break;

        case 'W':
        case VK_UP: {
            operateQueue.push({ ActionENUM::zoomOut });
        }break;

        case 'S':
        case VK_DOWN: {
            operateQueue.push({ ActionENUM::zoomIn });
        }break;

        case 'A':
        case VK_PRIOR:
        case VK_LEFT: {
            operateQueue.push({ ActionENUM::preImg });
        }break;

        case 'D':
        case VK_NEXT:
        case VK_RIGHT: {
            operateQueue.push({ ActionENUM::nextImg });
        }break;

        default: {
            cout << "KeyValue: " << (int)keyValue << '\n';
        }break;
        }
    }

    void OnResize(UINT width, UINT height) {
        static int w = 0, h = 0;
        if (w == width && h == height)
            return;

        w = width;
        h = height;
        operateQueue.push({ ActionENUM::newSize, (int)width, (int)height });
    }

    uint32_t getSrcPx(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) const {
        switch (srcImg.channels()) {
        case 3: {
            cv::Vec3b srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

            intUnion ret = 255;
            if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
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
            auto srcPtr = (intUnion*)srcImg.ptr();
            int srcW = srcImg.cols;

            intUnion srcPx = srcPtr[srcW * srcY + srcX];
            intUnion bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ?
                GRID_DARK : GRID_LIGHT;

            intUnion px;
            if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) {
                intUnion srcPx1 = srcPtr[srcW * (srcY - 1) + srcX - 1];
                intUnion srcPx2 = srcPtr[srcW * (srcY - 1) + srcX];
                intUnion srcPx3 = srcPtr[srcW * (srcY)+srcX - 1];
                for (int i = 0; i < 4; i++)
                    px[i] = (srcPx1[i] + srcPx2[i] + srcPx3[i] + srcPx[i]) / 4;
            }
            else {
                px = srcPx;
            }

            if (px[3] == 0) return bgPx.u32;
            else if (px[3] == 255) return intUnion(px[0], px[1], px[2], 255).u32;;

            const int alpha = px[3];
            intUnion ret = alpha;
            for (int i = 0; i < 3; i++)
                ret[i] = (bgPx[i] * (255 - alpha) + px[i] * alpha) / 256;
            return ret.u32;
        }
        }

        return ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ?
            GRID_DARK : GRID_LIGHT;
    }


    void drawCanvas(const cv::Mat& srcImg, cv::Mat& canvas) const {

        const int srcH = srcImg.rows;
        const int srcW = srcImg.cols;

        const int canvasH = canvas.rows;
        const int canvasW = canvas.cols;

        if (srcH <= 0 || srcW <= 0)
            return;

        const int deltaW = curPar.slideCur.x + (int)((canvasW - srcW * curPar.zoomCur / curPar.ZOOM_BASE) / 2);
        const int deltaH = curPar.slideCur.y + (int)((canvasH - srcH * curPar.zoomCur / curPar.ZOOM_BASE) / 2);

        int xStart = deltaW < 0 ? 0 : deltaW;
        int yStart = deltaH < 0 ? 0 : deltaH;
        int xEnd = (int)(srcW * curPar.zoomCur / curPar.ZOOM_BASE + deltaW);
        int yEnd = (int)(srcH * curPar.zoomCur / curPar.ZOOM_BASE + deltaH);
        if (xEnd > canvasW) xEnd = canvasW;
        if (yEnd > canvasH) yEnd = canvasH;

        memset(canvas.ptr(), BG_COLOR, 4ULL * canvasH * canvasW);
        // 1360*768 1-10ms
        auto ptr = (uint32_t*)canvas.ptr();
        for (int y = yStart; y < yEnd; y++)
            for (int x = xStart; x < xEnd; x++) {
                const int srcX = (int)(((int64_t)x - deltaW) * curPar.ZOOM_BASE / curPar.zoomCur);
                const int srcY = (int)(((int64_t)y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                if (0 <= srcX && srcX < srcW && 0 <= srcY && srcY < srcH)
                    ptr[y * canvasW + x] = getSrcPx(srcImg, srcX, srcY, x, y);
            }
    }

    void DrawScene() {
        using namespace Microsoft::WRL;

        static D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
        );

        static D2D1_SIZE_U bitmapSize = D2D1::SizeU(600, 400); // 设置位图的宽度和高度

        if (m_pD2DDeviceContext == nullptr)
            return;

        auto operateAction = operateQueue.get();
        if (operateAction.action == ActionENUM::unknow) {
            if (curPar.zoomCur == curPar.zoomTarget &&
                curPar.slideCur == curPar.slideTarget &&
                !curPar.isAnimation) {
                Sleep(8);
                return;
            }
        }

        switch (operateAction.action)
        {
        case ActionENUM::newSize: {

            //D2D1_SIZE_U displaySize = {operateAction.width, operateAction.height};// m_pD2DDeviceContext->GetPixelSize();
            winWidth = operateAction.width;
            winHeight = operateAction.height;

            if (winWidth != mainCanvas.cols || winHeight != mainCanvas.rows) {
                mainCanvas = cv::Mat(winHeight, winWidth, CV_8UC4);
                bitmapSize = D2D1::SizeU(winWidth, winHeight);
                CreateWindowSizeDependentResources();
            }

            curPar.Init(winWidth, winHeight);
        } break;

        case ActionENUM::toggleFullScreen: {
            // todo
            D2D1_SIZE_U displaySize = m_pD2DDeviceContext->GetPixelSize();
            mainCanvas = cv::Mat(displaySize.height, displaySize.width, CV_8UC4);
        } break;

        case ActionENUM::preImg: {
            if (--curFileIdx < 0)
                curFileIdx = (int)imgFileList.size() - 1;
            curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
            curPar.Init(winWidth, winHeight);
        } break;

        case ActionENUM::nextImg: {
            if (++curFileIdx >= (int)imgFileList.size())
                curFileIdx = 0;
            curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
            curPar.Init(winWidth, winHeight);
        } break;

        case ActionENUM::slide: {
            curPar.slideTarget.x += operateAction.x;
            curPar.slideTarget.y += operateAction.y;
        } break;

        case ActionENUM::toggleExif: {
            showExif = !showExif;
        } break;

        case ActionENUM::zoomIn: {
            if (curPar.zoomIndex > 0)
                curPar.zoomIndex--;
            auto zoomNext = curPar.zoomList[curPar.zoomIndex];
            if (curPar.zoomTarget && zoomNext != curPar.zoomTarget) {
                curPar.slideTarget.x = (int)(zoomNext * curPar.slideTarget.x / curPar.zoomTarget);
                curPar.slideTarget.y = (int)(zoomNext * curPar.slideTarget.y / curPar.zoomTarget);
            }
            curPar.zoomTarget = zoomNext;
        } break;

        case ActionENUM::zoomOut: {
            if (curPar.zoomIndex < curPar.zoomList.size() - 1)
                curPar.zoomIndex++;
            auto zoomNext = curPar.zoomList[curPar.zoomIndex];
            if (curPar.zoomTarget && zoomNext != curPar.zoomTarget) {
                curPar.slideTarget.x = (int)(zoomNext * curPar.slideTarget.x / curPar.zoomTarget);
                curPar.slideTarget.y = (int)(zoomNext * curPar.slideTarget.y / curPar.zoomTarget);
            }
            curPar.zoomTarget = zoomNext;
        } break;

        case ActionENUM::requitExit: {
            exit(0);
        } break;

        default:
            break;
        }


        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        curPar.curFrameDelay = (delay <= 0 ? 10 : delay);
        
        const int progressMax = 8;
        static int progressCnt = progressMax;
        static int64_t zoomInit = 0;
        static Cood hasDropInit{};
        //if (curPar.zoomCur != curPar.zoomTarget || curPar.slideCur != curPar.slideTarget) { // 简单缩放动画
        //    if (progressCnt >= (progressMax - 1)) {
        //        progressCnt = 0;
        //        zoomInit = curPar.zoomCur;
        //        hasDropInit = curPar.slideCur;
        //    }
        //    else {
        //        progressCnt += ((progressMax - progressCnt) / 2);
        //        if (progressCnt >= (progressMax - 1)) {
        //            curPar.zoomCur = curPar.zoomTarget;
        //            curPar.slideCur = curPar.slideTarget;
        //        }
        //        else {
        //            curPar.zoomCur = zoomInit + (curPar.zoomTarget - zoomInit) * progressCnt / progressMax;
        //            curPar.slideCur = hasDropInit + (curPar.slideTarget - hasDropInit) * progressCnt / progressMax;
        //        }
        //    }
        //}
        curPar.zoomCur = curPar.zoomTarget;
        curPar.slideCur = curPar.slideTarget;

        drawCanvas(srcImg, mainCanvas);
        if (showExif) {
            const int padding = 10;
            const int rightEdge = (mainCanvas.cols - 2 * padding) / 4 + padding;
            RECT r{ padding, padding, rightEdge > 300 ? rightEdge : 300, mainCanvas.rows - padding };
            stb.putAlignLeft(mainCanvas, r, curPar.framesPtr->exifStr.c_str(), { 255, 255, 255, 255 }); // 长文本 8ms
        }

        wstring str = std::format(L" [{}/{}] {}% ",
            curFileIdx + 1, imgFileList.size(),
            curPar.zoomCur * 100ULL / curPar.ZOOM_BASE)
            + imgFileList[curFileIdx];
        SetWindowTextW(m_hWnd, str.c_str());

        ComPtr<ID2D1Bitmap1> pBitmap;
        m_pD2DDeviceContext->CreateBitmap(
            bitmapSize,
            mainCanvas.ptr(),
            mainCanvas.cols * 4,
            &bitmapProperties,
            &pBitmap
        );

        m_pD2DDeviceContext->BeginDraw();
        m_pD2DDeviceContext->DrawBitmap(pBitmap.Get());
        //m_pD2DDeviceContext->DrawText(
        //	L"Template project of Direct2D 1.1.",
        //	wcslen(L"Template project of Direct2D 1.1."),
        //	m_pTextFormat,
        //	D2D1::RectF(0, 0, 1080, 0),
        //	m_pBrush);
        m_pD2DDeviceContext->EndDraw();
        m_pSwapChain->Present(0, 0);


        if (curPar.isAnimation) {
            static int delayRemain = 0;

            delayRemain -= 16;
            if (delayRemain <= 0) {
                delayRemain = curPar.curFrameDelay;
                curPar.curFrameIdx++;
                if (curPar.curFrameIdx > curPar.curFrameIdxMax)
                    curPar.curFrameIdx = 0;
            }
            Sleep(8);
        }
    }

};

void test();

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nCmdShow)
{
#ifndef NDEBUG
    AllocConsole();
    FILE* stream;
    freopen_s(&stream, "CON", "w", stdout);//重定向输入流

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    test();

    Exiv2::enableBMFF();

    ::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    if (!SUCCEEDED(::CoInitialize(nullptr)))
        return 0;

    D2D1Template app;
    if (SUCCEEDED(app.Initialize(hInstance, nCmdShow, lpCmdLine)))
        app.Run();

    ::CoUninitialize();
    return 0;
}

void test() {
    return;

    std::ifstream file("D:\\Downloads\\test\\22.wp2", std::ios::binary);
    auto buf = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    exit(0);
}
