#include "Utils.h"

#include "stbText.h"
#include "ImageDatabase.h"

#include "D2D1App.h"
#include <wrl.h>

// 启用OMP播放动画时 CPU 使用率太高
//#define ENABLE_OMP

#ifdef ENABLE_OMP
#include "omp.h"
#endif

/* TODO
1. eps格式
1. 在鼠标光标位置缩放
1. 部分AVIF图像仍不能正常解码 AVIF_RESULT_BMFF_PARSE_FAILED
1. svg动画
1. xpm和bpm格式
*/

const wstring appName = L"JarkViewer v1.25";



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
    int zoomIndex = 0;
    bool isAnimation = false;
    int width = 0;
    int height = 0;
    int rotation = 0; // 旋转： 0正常， 1逆90度， 2：180度， 3顺90度

    CurImageParameter() {
        Init();
    }

    void Init(int winWidth = 0, int winHeight = 0) {

        curFrameIdx = 0;
        curFrameDelay = 0;

        slideCur = 0;
        slideTarget = 0;
        rotation = 0;

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

    void handleNewSize(int winWidth = 0, int winHeight = 0) {
        if (winHeight == 0 || winWidth == 0 || framesPtr == nullptr)
            return;

        //适应显示窗口宽高的缩放比例
        int64_t zoomFitWindow = std::min(winWidth * ZOOM_BASE / width, winHeight * ZOOM_BASE / height);

        zoomList = ZOOM_LIST;
        if (!Utils::is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
            zoomList.push_back(zoomFitWindow);
        else {
            if (zoomIndex >= zoomList.size())
                zoomIndex = (int)zoomList.size() - 1;
        }
        std::sort(zoomList.begin(), zoomList.end());
    }

    void slideTargetRotateLeft() {
        slideTarget = { slideTarget.y, -slideTarget.x };
    }

    void slideTargetRotateRight() {
        slideTarget = { -slideTarget.y, slideTarget.x };
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

        if (!queue.empty() && action.action == ActionENUM::slide) {
            Action& back = queue.back();

            if (back.action == ActionENUM::slide) {
                back.x += action.x;
                back.y += action.y;
            }
            else {
                queue.push(action);
            }
        }
        else {
            queue.push(action);
        }
    }

    Action get() {
        std::unique_lock<std::mutex> lock(mtx);

        if (queue.empty())
            return { ActionENUM::none };

        Action res = queue.front();
        queue.pop();
        return res;
    }
};

class ExtraUIRes
{
public:
    cv::Mat imgData, leftArrow, RightArrow, leftRotate, rightRotate, printer;

    ExtraUIRes() {
        rcFileInfo rc;

        rc = Utils::GetResource(IDB_PNG2, L"PNG");
        imgData = cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
        leftArrow = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);

        rc = Utils::GetResource(IDB_PNG3, L"PNG");
        imgData = cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
        leftRotate = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);

        rc = Utils::GetResource(IDB_PNG4, L"PNG");
        imgData = cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
        RightArrow = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);

        rc = Utils::GetResource(IDB_PNG5, L"PNG");
        imgData = cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
        rightRotate = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);

        rc = Utils::GetResource(IDB_PNG6, L"PNG");
        imgData = cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
        printer = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
    }
    ~ExtraUIRes() {}
};

class JarkViewerApp : public D2D1App {
public:
    const int BG_GRID_WIDTH = 16;
    const uint32_t BG_COLOR = 0x46;
    const uint32_t GRID_DARK = 0xFF282828;
    const uint32_t GRID_LIGHT = 0xFF3C3C3C;

    OperateQueue operateQueue;

    CursorPos cursorPos = CursorPos::centerArea;
    CursorPos cursorPosLast = CursorPos::centerArea;
    ShowExtraUI extraUIFlag = ShowExtraUI::none;
    bool mouseIsPressing = false;
    bool ctrlIsPressing = false;
    bool smoothShift = false;
    bool showExif = false;
    Cood mousePos, mousePressPos;
    int winWidth = 800;
    int winHeight = 600;
    bool hasInitWinSize = false;

    ImageDatabase imgDB;

    int curFileIdx = -1;         // 文件在路径列表的索引
    vector<wstring> imgFileList; // 工作目录下所有图像文件路径

    stbText stb;                 // 给Mat绘制文字
    cv::Mat mainCanvas;          // 窗口内容画布
    D2D1_SIZE_U bitmapSize = D2D1::SizeU(600, 400);

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
    );

    CurImageParameter curPar;
    ExtraUIRes extraUIRes;
    std::chrono::system_clock::time_point lastClickTimestamp;

    JarkViewerApp() {
        m_wndCaption = appName;

        UINT dpi = GetDpiForSystem(); // 100%: 96 150%: 144 200%: 192
        if (dpi >= 144) {
            stb.setSize(dpi < 168 ? 24 : 32);
        }
    }

    ~JarkViewerApp() {
    }

    HRESULT InitWindow(HINSTANCE hInstance) {
        if (!SUCCEEDED(D2D1App::Initialize(hInstance)))
            return S_FALSE;

        if (m_pD2DDeviceContext == nullptr) {
            MessageBoxW(NULL, L"窗口创建失败！", L"错误", MB_ICONERROR);
            exit(-1);
        }

        if (hInstance)
            Utils::setCvWindowIcon(hInstance, m_hWnd, IDI_JARKVIEWER);

        return S_OK;
    }

    void initOpenFile(wstring filePath) {
        namespace fs = std::filesystem;

        curFileIdx = -1;
        imgFileList.clear();
        imgDB.clear();

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
                imgDB.put(appName, { {{imgDB.getHomeMat(), 0}}, "请在图像文件右键使用本软件打开" });
            }
            else { // 打开的文件不支持，默认加到尾部
                imgFileList.emplace_back(fullPath.wstring());
                curFileIdx = (int)imgFileList.size() - 1;
                imgDB.put(fullPath.wstring(), { {{imgDB.getErrorTipsMat(), 0}}, "图像格式不支持或已删除" });
            }
        }

        curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
        curPar.Init(winWidth, winHeight);
    }

    void OnMouseDown(WPARAM btnState, int x, int y) {
        switch ((long long)btnState)
        {
        case WM_LBUTTONDOWN: {//左键
            if (cursorPos == CursorPos::centerArea)
                mouseIsPressing = true;

            mousePressPos = { x, y };

            if (cursorPos == CursorPos::leftEdge)
                operateQueue.push({ ActionENUM::preImg });
            else if (cursorPos == CursorPos::rightEdge)
                operateQueue.push({ ActionENUM::nextImg });
            else if (cursorPos == CursorPos::leftUp)
                operateQueue.push({ ActionENUM::rotateLeft });
            else if (cursorPos == CursorPos::rightUp)
                operateQueue.push({ ActionENUM::rotateRight });
            else if (cursorPos == CursorPos::leftDown)
                operateQueue.push({ ActionENUM::printImage });
            return;
        }

        case WM_RBUTTONDOWN: {//右键
            return;
        }

        case WM_MBUTTONDOWN: {//中键
            return;
        }

        default:
            return;
        }
    }

    void OnMouseUp(WPARAM btnState, int x, int y) {
        using namespace std::chrono;

        switch ((long long)btnState)
        {
        case WM_LBUTTONUP: {//左键
            mouseIsPressing = false;

            if (cursorPos == CursorPos::centerArea) {
                auto now = system_clock::now();
                auto elapsed = duration_cast<milliseconds>(now - lastClickTimestamp).count();
                lastClickTimestamp = now;

                if (10 < elapsed && elapsed < 300) { // 10 ~ 300 ms
                    Utils::ToggleFullScreen(m_hWnd);
                }
            }
            return;
        }

        case WM_RBUTTONUP: {//右键
            operateQueue.push({ ActionENUM::requitExit });
            return;
        }

        case WM_MBUTTONUP: {//中键
            operateQueue.push({ ActionENUM::toggleExif });
            return;
        }

        default:
            return;
        }
    }

    void OnMouseMove(WPARAM btnState, int x, int y) {
        mousePos = { x, y };

        if (mouseIsPressing) {
            cursorPos = CursorPos::centerArea;
        }
        else {
            if (winWidth >= 500) {
                if (0 <= x && x < 50) {
                    if (0 <= y && y < (winHeight / 4)) {
                        cursorPos = CursorPos::leftUp;
                    }
                    else if ((winHeight / 4) <= y && y < (winHeight * 3 / 4)) {
                        cursorPos = CursorPos::leftEdge;
                    }
                    else if ((winHeight * 3 / 4) <= y && y < (winHeight)) {
                        cursorPos = CursorPos::leftDown;
                    }
                }
                else if (((winWidth - 50) < x && x <= winWidth)) {
                    if (0 <= y && y < (winHeight / 4)) {
                        cursorPos = CursorPos::rightUp;
                    }
                    else if ((winHeight / 4) <= y && y < (winHeight * 3 / 4)) {
                        cursorPos = CursorPos::rightEdge;
                    }
                    else if ((winHeight * 3 / 4) <= y && y < (winHeight)) {
                        cursorPos = CursorPos::rightDown;
                    }
                }
                else {
                    cursorPos = CursorPos::centerArea;
                }
            }
            else {
                if (0 <= x && x < (winWidth / 4)) {
                    if (0 <= y && y < (winHeight / 4)) {
                        cursorPos = CursorPos::leftUp;
                    }
                    else if ((winHeight / 4) <= y && y < (winHeight * 3 / 4)) {
                        cursorPos = CursorPos::leftEdge;
                    }
                    else if ((winHeight * 3 / 4) <= y && y < (winHeight)) {
                        cursorPos = CursorPos::leftDown;
                    }
                }
                else if ((winWidth * 3 / 4) < x && x <= winWidth) {
                    if (0 <= y && y < (winHeight / 4)) {
                        cursorPos = CursorPos::rightUp;
                    }
                    else if ((winHeight / 4) <= y && y < (winHeight * 3 / 4)) {
                        cursorPos = CursorPos::rightEdge;
                    }
                    else if ((winHeight * 3 / 4) <= y && y < (winHeight)) {
                        cursorPos = CursorPos::rightDown;
                    }
                }
                else {
                    cursorPos = CursorPos::centerArea;
                }
            }
        }

        if (cursorPosLast != cursorPos) {
            switch (cursorPos)
            {
            case CursorPos::leftUp:
                extraUIFlag = ShowExtraUI::rotateLeftButton;
                operateQueue.push({ ActionENUM::normalFresh });
                break;

            case CursorPos::leftDown:
                extraUIFlag = ShowExtraUI::printer;
                operateQueue.push({ ActionENUM::normalFresh });
                break;

            case CursorPos::leftEdge:
                extraUIFlag = ShowExtraUI::leftArrow;
                operateQueue.push({ ActionENUM::normalFresh });
                break;

            case CursorPos::centerArea:
                extraUIFlag = ShowExtraUI::none;
                operateQueue.push({ ActionENUM::normalFresh });
                break;

            case CursorPos::rightEdge:
                extraUIFlag = ShowExtraUI::rightArrow;
                operateQueue.push({ ActionENUM::normalFresh });
                break;

            case CursorPos::rightDown:
                extraUIFlag = ShowExtraUI::none;
                operateQueue.push({ ActionENUM::normalFresh });
                break;

            case CursorPos::rightUp:
                extraUIFlag = ShowExtraUI::rotateRightButton;
                operateQueue.push({ ActionENUM::normalFresh });
                break;
            }

            cursorPosLast = cursorPos;
        }

        if (mouseIsPressing) {
            auto slideDelta = mousePos - mousePressPos;
            mousePressPos = mousePos;
            operateQueue.push({ ActionENUM::slide, slideDelta.x, slideDelta.y });
        }
    }

    void OnMouseLeave() {
        cursorPosLast = cursorPos = CursorPos::centerArea;
        extraUIFlag = ShowExtraUI::none;
        operateQueue.push({ ActionENUM::normalFresh });
    }

    void OnMouseWheel(UINT nFlags, short zDelta, int x, int y) {
        switch (cursorPos)
        {
        case CursorPos::centerArea:
            operateQueue.push({ zDelta < 0 ? ActionENUM::zoomIn : ActionENUM::zoomOut });
            break;

        case CursorPos::leftEdge:
        case CursorPos::rightEdge:
            operateQueue.push({ zDelta < 0 ? ActionENUM::nextImg : ActionENUM::preImg });
            break;

        case CursorPos::leftDown:
        case CursorPos::rightDown:
            break;

        case CursorPos::leftUp:
        case CursorPos::rightUp:
            operateQueue.push({ zDelta < 0 ? ActionENUM::rotateRight : ActionENUM::rotateLeft });
            break;
        }
    }

    void OnKeyDown(WPARAM keyValue) {
        if (ctrlIsPressing) {
            switch (keyValue)
            {
            case 'O': { // Ctrl + O
                wstring filePath = Utils::SelectFile(m_hWnd);
                if (!filePath.empty()) {
                    initOpenFile(filePath);
                    operateQueue.push({ ActionENUM::normalFresh });
                }
            }break;
            case 'S': { // Ctrl + S
                const auto& imgs = curPar.framesPtr->imgList;
                wstring& filePath = imgFileList[curFileIdx];
                if (imgs.size() > 1) {
                    auto dotIdx = filePath.find_last_of(L".");
                    if (dotIdx == string::npos)
                        dotIdx = filePath.size();
                    for (int i = 0; i < imgs.size(); i++) {
                        cv::imwrite(Utils::wstringToAnsi(std::format(L"{}_{:04}.png", filePath.substr(0, dotIdx), i + 1)), imgs[i].img);
                    }
                }
            }break;
            case 'C': {
                const auto& imgs = curPar.framesPtr->imgList;
                Utils::copyImageToClipboard(imgs.front().img);
            }break;
            case 'P': {
                const auto& imgs = curPar.framesPtr->imgList;
                Utils::PrintMatImage(imgs.front().img);
            }break;
            }
        }
        else {
            switch (keyValue)
            {
            case VK_CONTROL: {
                ctrlIsPressing = true;
            }break;

            case 'C': { // 复制图像信息到剪贴板
                Utils::copyToClipboard(Utils::utf8ToWstring(imgDB.get(imgFileList[curFileIdx]).exifStr));
            }break;

            case VK_F11: {
                Utils::ToggleFullScreen(m_hWnd);
            }break;

            case 'Q': {
                operateQueue.push({ ActionENUM::rotateLeft });
            }break;

            case 'E': {
                operateQueue.push({ ActionENUM::rotateRight });
            }break;

            case 'W': {
                int newTargetY = curPar.slideTarget.y + ((winHeight + winWidth) / 16);

                int newTargetYMax = ((curPar.rotation == 0 or curPar.rotation == 2) ? curPar.height : curPar.width) * curPar.zoomTarget / (curPar.ZOOM_BASE * 2);
                int newTargetYMin = -newTargetYMax;
                if (newTargetY > newTargetYMax) {
                    newTargetY = newTargetYMax;
                }
                else if (newTargetY < newTargetYMin) {
                    newTargetY = newTargetYMin;
                }
                curPar.slideTarget.y = newTargetY;
                smoothShift = true;
            }break;

            case 'S': {
                int newTargetY = curPar.slideTarget.y - ((winHeight + winWidth) / 16);

                int newTargetYMax = ((curPar.rotation == 0 or curPar.rotation == 2) ? curPar.height : curPar.width) * curPar.zoomTarget / (curPar.ZOOM_BASE * 2);
                int newTargetYMin = -newTargetYMax;
                if (newTargetY > newTargetYMax) {
                    newTargetY = newTargetYMax;
                }
                else if (newTargetY < newTargetYMin) {
                    newTargetY = newTargetYMin;
                }
                curPar.slideTarget.y = newTargetY;
                smoothShift = true;
            }break;

            case 'A': {
                int newTargetX = curPar.slideTarget.x + ((winHeight + winWidth) / 16);

                int newTargetXMax = ((curPar.rotation == 0 || curPar.rotation == 2) ? curPar.width : curPar.height) * curPar.zoomTarget / (curPar.ZOOM_BASE * 2);
                int newTargetXMin = -newTargetXMax;
                if (newTargetX > newTargetXMax) {
                    newTargetX = newTargetXMax;
                }
                else if (newTargetX < newTargetXMin) {
                    newTargetX = newTargetXMin;
                }
                curPar.slideTarget.x = newTargetX;
                smoothShift = true;
            }break;

            case 'D': {
                int newTargetX = curPar.slideTarget.x - ((winHeight + winWidth) / 16);

                int newTargetXMax = ((curPar.rotation == 0 || curPar.rotation == 2) ? curPar.width : curPar.height) * curPar.zoomTarget / (curPar.ZOOM_BASE * 2);
                int newTargetXMin = -newTargetXMax;
                if (newTargetX > newTargetXMax) {
                    newTargetX = newTargetXMax;
                }
                else if (newTargetX < newTargetXMin) {
                    newTargetX = newTargetXMin;
                }
                curPar.slideTarget.x = newTargetX;
                smoothShift = true;
            }break;

            case VK_UP: {
                operateQueue.push({ ActionENUM::zoomOut });
            }break;

            case VK_DOWN: {
                operateQueue.push({ ActionENUM::zoomIn });
            }break;

            case VK_PRIOR:
            case VK_LEFT: {
                operateQueue.push({ ActionENUM::preImg });
            }break;
                        
            case VK_SPACE:
            case VK_NEXT:
            case VK_RIGHT: {
                operateQueue.push({ ActionENUM::nextImg });
            }break;

            case 'I': {
                operateQueue.push({ ActionENUM::toggleExif });
            }break;

            case VK_ESCAPE: { // ESC
                operateQueue.push({ ActionENUM::requitExit });
            }break;

            default: {
#ifndef NDEBUG
                cout << "OnKeyDown KeyValue: " << (int)keyValue << '\n';
#endif // NDEBUG
            }break;
            }
        }
    }

    void OnKeyUp(WPARAM keyValue) {
        switch (keyValue)
        {
        case VK_CONTROL: {
            ctrlIsPressing = false;
        }break;

        default: {
#ifndef NDEBUG
            cout << "OnKeyUp KeyValue: " << (int)keyValue << '\n';
#endif // NDEBUG
        }break;
        }
    }

    void OnDropFiles(WPARAM wParam) {
        wstring path;
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

        if (0 < fileCount) { // 拖入多文件时，只接受第一个
            wchar_t filePath[4096] = {};
            DragQueryFileW(hDrop, 0, filePath, 4096);
            path = filePath;
        }
        DragFinish(hDrop);

        if (!path.empty()) {
            initOpenFile(path);
            operateQueue.push({ ActionENUM::normalFresh });
        }
    }

    void OnResize(UINT width, UINT height) {
        operateQueue.push({ ActionENUM::newSize, (int)width, (int)height });
    }

    uint32_t getSrcPx3(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) const {
        cv::Vec3b srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

        intUnion ret = 255;
        if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            cv::Vec3b px0 = srcImg.at<cv::Vec3b>(srcY - 1, srcX - 1);
            cv::Vec3b px1 = srcImg.at<cv::Vec3b>(srcY - 1, srcX);
            cv::Vec3b px2 = srcImg.at<cv::Vec3b>(srcY, srcX - 1);
            for (int i = 0; i < 3; i++)
                ret[i] = (px0[i] + px1[i] + px2[i] + srcPx[i]) >> 2;

            return ret.u32;
        }
        ret[0] = srcPx[0];
        ret[1] = srcPx[1];
        ret[2] = srcPx[2];
        return ret.u32;
    }

    uint32_t getSrcPx4(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) const {
        // 使用 xxx.ptr() 需注意 xxx.step1() 必须等于 xxx.cols*4
        auto srcPtr = (intUnion*)srcImg.ptr();
        int srcW = srcImg.cols;

        intUnion srcPx = srcPtr[srcW * srcY + srcX];
        intUnion bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ?
            GRID_DARK : GRID_LIGHT;

        if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            intUnion srcPx1 = srcPtr[srcW * (srcY - 1) + srcX - 1];
            intUnion srcPx2 = srcPtr[srcW * (srcY - 1) + srcX];
            intUnion srcPx3 = srcPtr[srcW * srcY + srcX - 1];
            for (int i = 0; i < 4; i++)
                srcPx[i] = (srcPx1[i] + srcPx2[i] + srcPx3[i] + srcPx[i]) >> 2;
        }

        if (srcPx[3] == 0) return bgPx.u32;
        if (srcPx[3] == 255) return srcPx.u32;

        const int alpha = srcPx[3];
        intUnion ret = 255;
        for (int i = 0; i < 3; i++)
            ret[i] = (bgPx[i] * (255 - alpha) + srcPx[i] * alpha + 128) >> 8;
        return ret.u32;
    }

    void drawCanvas(const cv::Mat& srcImg, cv::Mat& canvas) const {
        int srcH, srcW;
        if (curPar.rotation == 1 || curPar.rotation == 3) {
            srcH = srcImg.cols;
            srcW = srcImg.rows;
        }
        else {
            srcH = srcImg.rows;
            srcW = srcImg.cols;
        }

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

        // 使用 xxx.ptr() 需注意 xxx.step1() 必须等于 xxx.cols*4
        memset(canvas.ptr(), BG_COLOR, 4ULL * canvasH * canvasW);

        if (((srcH == 600 and srcW == 800) or (srcH == 800 and srcW == 600)) and *((uint32_t*)srcImg.ptr()) == 0xFF464646) {
            // 内置的用于提示的图像
        }
        else { // 普通图像  画边框
            if (0 < xStart and xStart < canvasW) {
                for (int y = std::max(yStart - 1, 0); 0 <= y and y < std::min(yEnd + 1, canvasH - 1); y++) {
                    ((uint32_t*)canvas.ptr())[y * canvasW + xStart - 1] = 0xFF000000;
                }
            }
            if (0 < xEnd and xEnd < canvasW) {
                for (int y = std::max(yStart - 1, 0); 0 <= y and y < std::min(yEnd + 1, canvasH - 1); y++) {
                    ((uint32_t*)canvas.ptr())[y * canvasW + xEnd] = 0xFF000000;
                }
            }

            if (0 < yStart and yStart < canvasH) {
                for (int x = std::max(xStart - 1, 0); 0 <= x and x < std::min(xEnd + 1, canvasW - 1); x++) {
                    ((uint32_t*)canvas.ptr())[(yStart - 1) * canvasW + x] = 0xFF000000;
                }
            }
            if (0 < yEnd and yEnd < canvasH) {
                for (int x = std::max(xStart - 1, 0); 0 <= x and x < std::min(xEnd + 1, canvasW - 1); x++) {
                    ((uint32_t*)canvas.ptr())[yEnd * canvasW + x] = 0xFF000000;
                }
            }
        }

        if (srcImg.type() == CV_8UC3) {
#ifdef ENABLE_OMP
#pragma omp parallel for
#endif
            for (int y = yStart; y < yEnd; y++) {
                auto ptr = ((uint32_t*)canvas.ptr()) + y * canvasW;
                for (int x = xStart; x < xEnd; x++) {
                    const int srcX = (int)(((int64_t)x - deltaW) * curPar.ZOOM_BASE / curPar.zoomCur);
                    const int srcY = (int)(((int64_t)y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                    if (0 <= srcX && srcX < srcW && 0 <= srcY && srcY < srcH) {
                        switch (curPar.rotation)
                        {
                        case 1:
                            ptr[x] = getSrcPx3(srcImg, srcH - 1 - srcY, srcX, x, y);
                            break;
                        case 2:
                            ptr[x] = getSrcPx3(srcImg, srcW - 1 - srcX, srcH - 1 - srcY, x, y);
                            break;
                        case 3:
                            ptr[x] = getSrcPx3(srcImg, srcY, srcW - 1 - srcX, x, y);
                            break;
                        default:
                            ptr[x] = getSrcPx3(srcImg, srcX, srcY, x, y);
                            break;
                        }
                    }
                }
            }
        }
        else if (srcImg.type() == CV_8UC4) {
#ifdef ENABLE_OMP
#pragma omp parallel for
#endif
            for (int y = yStart; y < yEnd; y++) {
                auto ptr = ((uint32_t*)canvas.ptr()) + y * canvasW;
                for (int x = xStart; x < xEnd; x++) {
                    const int srcX = (int)(((int64_t)x - deltaW) * curPar.ZOOM_BASE / curPar.zoomCur);
                    const int srcY = (int)(((int64_t)y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                    if (0 <= srcX && srcX < srcW && 0 <= srcY && srcY < srcH) {
                        switch (curPar.rotation)
                        {
                        case 1:
                            ptr[x] = getSrcPx4(srcImg, srcH - 1 - srcY, srcX, x, y);
                            break;
                        case 2:
                            ptr[x] = getSrcPx4(srcImg, srcW - 1 - srcX, srcH - 1 - srcY, x, y);
                            break;
                        case 3:
                            ptr[x] = getSrcPx4(srcImg, srcY, srcW - 1 - srcX, x, y);
                            break;
                        default:
                            ptr[x] = getSrcPx4(srcImg, srcX, srcY, x, y);
                            break;
                        }
                    }
                }
            }
        }
    }

    cv::Mat rotateImage(const cv::Mat& image, double angle) {
        int width = image.cols;
        int height = image.rows;
        cv::Point2f center(width / 2.0f, height / 2.0f);

        cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);

        cv::Mat rotatedImage;
        cv::warpAffine(image, rotatedImage, rotationMatrix, image.size(),
            cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(BG_COLOR, BG_COLOR, BG_COLOR));

        return rotatedImage;
    }

    // 取对角线作为新画布的宽高，绘制好内容再旋转，最后截取画布。
    // 若直接使用原尺寸画布进行旋转，宽或高其中较小的那边旋转到较长那边时，会缺失部分内容
    void rotateLeftAnimation() {
        int maxEdge = (int)std::ceil(std::sqrt(winWidth * winWidth + winHeight * winHeight));
        if (maxEdge < 2)
            return;
        auto tmpCanvas = cv::Mat(maxEdge, maxEdge, CV_8UC4, cv::Vec4b(BG_COLOR, BG_COLOR, BG_COLOR));

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        drawCanvas(srcImg, tmpCanvas);

        bool needDelay = (maxEdge <= 1920);
        for (int i = 0; i <= 90; i += ((100 - i) / 4)) {
            auto tmp = rotateImage(tmpCanvas, i)(cv::Rect((maxEdge - winWidth) / 2, (maxEdge - winHeight) / 2, winWidth, winHeight));
            handleExtraUI(tmp);

            m_pD2DDeviceContext->CreateBitmap(
                bitmapSize,
                tmp.ptr(),
                (UINT32)tmp.step,
                &bitmapProperties,
                &pBitmap
            );

            m_pD2DDeviceContext->BeginDraw();
            m_pD2DDeviceContext->DrawBitmap(pBitmap.Get());
            m_pD2DDeviceContext->EndDraw();
            m_pSwapChain->Present(0, 0);
            if (needDelay)
                Sleep(1);
        }
    }

    void rotateRightAnimation() {
        int maxEdge = (int)std::ceil(std::sqrt(winWidth * winWidth + winHeight * winHeight));
        if (maxEdge < 2)
            return;
        auto tmpCanvas = cv::Mat(maxEdge, maxEdge, CV_8UC4, cv::Vec4b(BG_COLOR, BG_COLOR, BG_COLOR));

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        drawCanvas(srcImg, tmpCanvas);

        bool needDelay = (maxEdge <= 1920);
        for (int i = 0; i >= -90; i += ((-100 - i) / 4)) {
            auto tmp = rotateImage(tmpCanvas, i)(cv::Rect((maxEdge - winWidth) / 2, (maxEdge - winHeight) / 2, winWidth, winHeight));
            handleExtraUI(tmp);

            m_pD2DDeviceContext->CreateBitmap(
                bitmapSize,
                tmp.ptr(),
                (UINT32)tmp.step,
                &bitmapProperties,
                &pBitmap
            );

            m_pD2DDeviceContext->BeginDraw();
            m_pD2DDeviceContext->DrawBitmap(pBitmap.Get());
            m_pD2DDeviceContext->EndDraw();
            m_pSwapChain->Present(0, 0);
            if (needDelay)
                Sleep(1);
        }

    }

    void handleExtraUI(cv::Mat& canvas) {

        if (showExif) {
            const int padding = 10;
            const int rightEdge = (canvas.cols - 2 * padding) / 4 + padding;
            RECT r{ padding, padding, rightEdge > 300 ? rightEdge : 300, canvas.rows - padding };
            stb.putAlignLeft(canvas, r, curPar.framesPtr->exifStr.c_str(), { 255, 255, 255, 255 }); // 长文本 8ms
        }

        int canvasHeight = canvas.rows;
        int canvasWidth = canvas.cols;

        switch (extraUIFlag)
        {
        case ShowExtraUI::rotateLeftButton: {
            if (canvasWidth > 100 && canvasHeight > 100) {
                auto& img = extraUIRes.leftRotate;
                int imgHeight = img.rows;
                int imgWidth = img.cols;
                Utils::overlayImg(canvas, img, 0, (canvasHeight / 4 - imgHeight) / 2);
            }
        } break;

        case ShowExtraUI::leftArrow: {
            if (canvasWidth > 100 && canvasHeight > 100) {
                auto& img = extraUIRes.leftArrow;
                int imgHeight = img.rows;
                int imgWidth = img.cols;
                Utils::overlayImg(canvas, img, 0, (canvasHeight - imgHeight) / 2);
            }
        } break;

        case ShowExtraUI::printer: {
            if (canvasWidth > 100 && canvasHeight > 100) {
                auto& img = extraUIRes.printer;
                int imgHeight = img.rows;
                int imgWidth = img.cols;
                Utils::overlayImg(canvas, img, 0, (canvasHeight * 7 / 4 - imgHeight) / 2);
            }
        } break;

        case ShowExtraUI::none:
            break;

        case ShowExtraUI::rightArrow: {
            if (canvasWidth > 100 && canvasHeight > 100) {
                auto& img = extraUIRes.RightArrow;
                int imgHeight = img.rows;
                int imgWidth = img.cols;
                Utils::overlayImg(canvas, img, canvasWidth - imgWidth, (canvasHeight - imgHeight) / 2);
            }
        } break;

        case ShowExtraUI::rotateRightButton: {
            if (canvasWidth > 100 && canvasHeight > 100) {
                auto& img = extraUIRes.rightRotate;
                int imgHeight = img.rows;
                int imgWidth = img.cols;
                Utils::overlayImg(canvas, img, canvasWidth - imgWidth, (canvasHeight / 4 - imgHeight) / 2);
            }
        } break;
        }
    }

    void DrawScene() {
        const auto frameDuration = std::chrono::milliseconds(10);

        static int64_t delayRemain = 0;
        static auto lastTimestamp = std::chrono::steady_clock::now();

        if (m_pD2DDeviceContext == nullptr)
            return;

        auto operateAction = operateQueue.get();
        if (operateAction.action == ActionENUM::none) {
            if (curPar.zoomCur == curPar.zoomTarget &&
                curPar.slideCur == curPar.slideTarget &&
                !curPar.isAnimation) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return;
            }
        }

        if (operateAction.action == ActionENUM::printImage) {
            const auto& imgs = curPar.framesPtr->imgList;
            Utils::PrintMatImage(imgs.front().img);
            return;
        }

        switch (operateAction.action)
        {
        case ActionENUM::newSize: {
            if (operateAction.width == 0 || operateAction.height == 0)
                break;

            if (winWidth == operateAction.width && winHeight == operateAction.height)
                break;

            winWidth = operateAction.width;
            winHeight = operateAction.height;

            if (winWidth != mainCanvas.cols || winHeight != mainCanvas.rows) {
                mainCanvas = cv::Mat(winHeight, winWidth, CV_8UC4);
                bitmapSize = D2D1::SizeU(winWidth, winHeight);
                CreateWindowSizeDependentResources();
            }

            if (hasInitWinSize) {
                curPar.handleNewSize(winWidth, winHeight);
            }
            else {
                hasInitWinSize = true;
                curPar.Init(winWidth, winHeight);
            }
        } break;

        case ActionENUM::preImg: {
            if (--curFileIdx < 0)
                curFileIdx = (int)imgFileList.size() - 1;
            curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
            curPar.Init(winWidth, winHeight);
            lastTimestamp = std::chrono::steady_clock::now();
            delayRemain = 0;
        } break;

        case ActionENUM::nextImg: {
            if (++curFileIdx >= (int)imgFileList.size())
                curFileIdx = 0;
            curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
            curPar.Init(winWidth, winHeight);
            lastTimestamp = std::chrono::steady_clock::now();
            delayRemain = 0;
        } break;

        case ActionENUM::slide: {
            int newTargetX = curPar.slideTarget.x + operateAction.x;
            int newTargetY = curPar.slideTarget.y + operateAction.y;

            int newTargetXMax = ((curPar.rotation == 0 || curPar.rotation == 2) ? curPar.width : curPar.height) * curPar.zoomTarget / (curPar.ZOOM_BASE * 2);
            int newTargetXMin = -newTargetXMax;
            if (newTargetX > newTargetXMax) {
                newTargetX = newTargetXMax;
            }
            else if (newTargetX < newTargetXMin) {
                newTargetX = newTargetXMin;
            }

            int newTargetYMax = ((curPar.rotation == 0 or curPar.rotation == 2) ? curPar.height : curPar.width) * curPar.zoomTarget / (curPar.ZOOM_BASE * 2);
            int newTargetYMin = -newTargetYMax;
            if (newTargetY > newTargetYMax) {
                newTargetY = newTargetYMax;
            }
            else if (newTargetY < newTargetYMin) {
                newTargetY = newTargetYMin;
            }

            curPar.slideTarget.x = newTargetX;
            curPar.slideTarget.y = newTargetY;

        } break;

        case ActionENUM::toggleExif: {
            showExif = !showExif;
        } break;

        case ActionENUM::zoomIn: {
            if (curPar.zoomIndex > 0) {
                curPar.zoomIndex--;

                auto zoomNext = curPar.zoomList[curPar.zoomIndex];
                if (curPar.zoomTarget && zoomNext != curPar.zoomTarget) {
                    curPar.slideTarget.x = (int)(zoomNext * curPar.slideTarget.x / curPar.zoomTarget);
                    curPar.slideTarget.y = (int)(zoomNext * curPar.slideTarget.y / curPar.zoomTarget);
                }
                curPar.zoomTarget = zoomNext;
                smoothShift = true;
            }
        } break;

        case ActionENUM::zoomOut: {
            if (curPar.zoomIndex < curPar.zoomList.size() - 1) {
                curPar.zoomIndex++;

                auto zoomNext = curPar.zoomList[curPar.zoomIndex];
                if (curPar.zoomTarget && zoomNext != curPar.zoomTarget) {
                    curPar.slideTarget.x = (int)(zoomNext * curPar.slideTarget.x / curPar.zoomTarget);
                    curPar.slideTarget.y = (int)(zoomNext * curPar.slideTarget.y / curPar.zoomTarget);
                }
                curPar.zoomTarget = zoomNext;
                smoothShift = true;
            }
        } break;

        case ActionENUM::rotateLeft: {
            rotateLeftAnimation();
            curPar.rotation = (curPar.rotation + 1) & 0b11;
            curPar.slideTargetRotateLeft();
        } break;

        case ActionENUM::rotateRight: {
            rotateRightAnimation();
            curPar.rotation = (curPar.rotation + 4 - 1) & 0b11;
            curPar.slideTargetRotateRight();
        } break;

        case ActionENUM::requitExit: {
            PostMessageW(m_hWnd, WM_DESTROY, 0, 0);
        } break;

        default:
            break;
        }


        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        curPar.curFrameDelay = (delay <= 0 ? 10 : delay);

        if (curPar.zoomCur != curPar.zoomTarget || curPar.slideCur != curPar.slideTarget) {
            if (smoothShift) { // 简单缩放动画
                const int progressMax = 1 << 8;
                static int progressCnt = progressMax;
                static int64_t zoomInit = 0;
                static int64_t zoomTargetInit = 0;
                static Cood slideInit{}, slideTargetInit{};

                //未开始进行动画 或 动画未完成就有新缩放操作
                if (progressCnt >= progressMax || zoomTargetInit != curPar.zoomTarget || slideTargetInit != curPar.slideTarget) {
                    progressCnt = 1;
                    zoomInit = curPar.zoomCur;
                    zoomTargetInit = curPar.zoomTarget;
                    slideInit = curPar.slideCur;
                    slideTargetInit = curPar.slideTarget;
                }
                else {
                    auto addDelta = ((progressMax - progressCnt) / 4);
                    if (addDelta <= 1) {
                        progressCnt = progressMax;
                        curPar.zoomCur = curPar.zoomTarget;
                        curPar.slideCur = curPar.slideTarget;
                        smoothShift = false;
                    }
                    else {
                        progressCnt += addDelta;
                        curPar.zoomCur = zoomInit + (curPar.zoomTarget - zoomInit) * progressCnt / progressMax;
                        curPar.slideCur = slideInit + (curPar.slideTarget - slideInit) * progressCnt / progressMax;
                    }
                }
            }
            else {
                curPar.zoomCur = curPar.zoomTarget;
                curPar.slideCur = curPar.slideTarget;
            }
        }

        drawCanvas(srcImg, mainCanvas);
        handleExtraUI(mainCanvas);

        wstring str = std::format(L" [{}/{}] {}% ",
            curFileIdx + 1, imgFileList.size(),
            curPar.zoomCur * 100ULL / curPar.ZOOM_BASE)
            + imgFileList[curFileIdx];
        if (curPar.rotation)
            str += (curPar.rotation == 1 ? L"  逆时针旋转90°" : (curPar.rotation == 3 ? L"  顺时针旋转90°" : (L"  旋转180°")));
        SetWindowTextW(m_hWnd, str.c_str());

        m_pD2DDeviceContext->CreateBitmap(
            bitmapSize,
            mainCanvas.ptr(),
            (UINT32)mainCanvas.step,
            &bitmapProperties,
            &pBitmap
        );

        m_pD2DDeviceContext->BeginDraw();
        m_pD2DDeviceContext->DrawBitmap(pBitmap.Get());
        m_pD2DDeviceContext->EndDraw();
        m_pSwapChain->Present(0, 0);


        if (curPar.isAnimation) {
            if (delayRemain <= 0)
                delayRemain = curPar.curFrameDelay;

            auto nowTimestamp = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(nowTimestamp - lastTimestamp);
            lastTimestamp = nowTimestamp;

            if (frameDuration > elapsed)
                std::this_thread::sleep_for(frameDuration - elapsed);

            delayRemain -= elapsed.count();
            if (delayRemain <= 0) {
                delayRemain = curPar.curFrameDelay;
                curPar.curFrameIdx++;
                if (curPar.curFrameIdx > curPar.curFrameIdxMax)
                    curPar.curFrameIdx = 0;
            }
        }
    }

};

void test();

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
#ifndef NDEBUG
    AllocConsole();
    FILE* stream;
    freopen_s(&stream, "CON", "w", stdout);//重定向标准输出流
    freopen_s(&stream, "CON", "w", stderr);//重定向错误输出流

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    test();

    Exiv2::enableBMFF();
    ::ImmDisableIME(GetCurrentThreadId()); // 禁用输入法，防止干扰按键操作

    ::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    if (!SUCCEEDED(::CoInitialize(nullptr)))
        return 0;

    wstring filePath = lpCmdLine;
    if (!filePath.empty() && filePath.front() == '\"') {
        filePath = filePath.substr(1);
    }
    if (!filePath.empty() && filePath.back() == '\"') {
        filePath.pop_back();
    }

    JarkViewerApp app;
    if (SUCCEEDED(app.InitWindow(hInstance))) {
        app.initOpenFile(filePath);
        app.Run();
    }

    ::CoUninitialize();
    return 0;
}

void test() {
    return;

    std::ifstream file("D:\\Downloads\\test\\22.wp2", std::ios::binary);
    auto buf = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    exit(0);
}
