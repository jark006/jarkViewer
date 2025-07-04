#include "jarkUtils.h"

#include "TextDrawer.h"
#include "ImageDatabase.h"
#include "Printer.h"
#include "Setting.h"

#include "D2D1App.h"
#include <wrl.h>

// 启用OMP播放动画时 CPU 使用率太高
//#define ENABLE_OMP

#ifdef ENABLE_OMP
#include "omp.h"
#endif

/* TODO
1. 在鼠标光标位置缩放
1. 部分AVIF图像仍不能正常解码 AVIF_RESULT_BMFF_PARSE_FAILED
*/

std::wstring_view appName = L"JarkViewer v1.27";
std::wstring_view appBuildInfo = L"v1.27 (Build 20250705)";
std::wstring_view jarkLink = L"https://github.com/jark006";
std::wstring_view GithubLink = L"https://github.com/jark006/jarkViewer";
std::wstring_view BaiduLink = L"https://pan.baidu.com/s/1ka7p__WVw2du3mnOfqWceQ?pwd=6666"; // 密码 6666
std::wstring_view LanzouLink = L"https://jark006.lanzout.com/b0ko7mczg"; // 密码 6666



struct CurImageParameter {
    static const inline std::vector<int64_t> ZOOM_LIST{
    1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16,
    1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22,
    };
    static constexpr int64_t ZOOM_BASE = (1 << 16); // 100%缩放

    int64_t zoomTarget;     // 设定的缩放比例
    int64_t zoomCur;        // 动画播放过程的缩放比例，动画完毕后的值等于zoomTarget
    int curFrameIdx;        // 小于0则单张静态图像，否则为动画当前帧索引
    int curFrameIdxMax;     // 若是动画则为帧数量
    int curFrameDelay;      // 当前帧延迟
    Cood slideCur, slideTarget;
    std::shared_ptr<Frames> framesPtr;

    vector<int64_t> zoomList;
    int zoomIndex = 0;
    bool isAnimation = false;
    bool isAnimationPause = false;
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
        isAnimationPause = false;

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
            if (!jarkUtils::is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
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

    void updateZoomList(int winWidth = 0, int winHeight = 0) {
        if (winHeight == 0 || winWidth == 0 || framesPtr == nullptr)
            return;

        //适应显示窗口宽高的缩放比例
        int64_t zoomFitWindow = (rotation == 0 || rotation == 2)?
            std::min(winWidth * ZOOM_BASE / width, winHeight * ZOOM_BASE / height):
            std::min(winWidth * ZOOM_BASE / height, winHeight * ZOOM_BASE / width);

        zoomList = ZOOM_LIST;
        if (!jarkUtils::is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
            zoomList.push_back(zoomFitWindow);
        else {
            if (zoomIndex >= zoomList.size())
                zoomIndex = (int)zoomList.size() - 1;
        }
        std::sort(zoomList.begin(), zoomList.end());
    }

    void slideTargetRotateLeft() {
        slideTarget = { slideTarget.y, -slideTarget.x };
        slideCur = slideTarget;
    }

    void slideTargetRotateRight() {
        slideTarget = { -slideTarget.y, slideTarget.x };
        slideCur = slideTarget;
    }
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
    cv::Mat imgData, leftArrow, rightArrow, leftRotate, rightRotate, printer, setting, animationBarPlaying, animationBarPausing;

    ExtraUIRes() {
        rcFileInfo rc;

        rc = jarkUtils::GetResource(IDB_PNG_MAIN_RES, L"PNG");
        auto mainRes = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.ptr), cv::IMREAD_UNCHANGED);

        leftRotate = mainRes({ 0, 0, 50, 50 }).clone();
        rightRotate = mainRes({ 50, 0, 50, 50 }).clone();

        printer = mainRes({ 0, 50, 50, 50 }).clone();
        setting = mainRes({ 50, 50, 50, 50 }).clone();

        leftArrow = mainRes({ 100, 0, 50, 100 }).clone();
        rightArrow = mainRes({ 150, 0, 50, 100 }).clone();

        animationBarPlaying = mainRes({ 0, 100, 200, 50 }).clone();
        animationBarPausing = mainRes({ 0, 150, 200, 50 }).clone();
    }
    ~ExtraUIRes() {}
};

class JarkViewerApp : public D2D1App {
public:
    static const int BG_GRID_WIDTH = 16;
    static const uint32_t BG_COLOR = 0x46;
    static const uint32_t GRID_DARK = 0xFF282828;
    static const uint32_t GRID_LIGHT = 0xFF3C3C3C;

    static inline bool isLowZoom = false;

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

    TextDrawer textDrawer;                 // 给Mat绘制文字
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
            textDrawer.setSize(dpi < 168 ? 24 : 32);
        }
    }

    ~JarkViewerApp() {
    }

    HRESULT InitWindow(HINSTANCE hInstance) {
        if (!SUCCEEDED(D2D1App::Initialize(hInstance)))
            return S_FALSE;

        if (m_pD2DDeviceContext == nullptr)
            return S_FALSE;

        jarkUtils::setWindowIcon(m_hWnd, IDI_JARKVIEWER);

        return S_OK;
    }

    void initOpenFile(wstring filePath) {
        namespace fs = std::filesystem;

        curFileIdx = -1;
        imgFileList.clear();
        imgDB.clear();

        fs::path fullPath = fs::absolute(filePath);
        wstring openFileName = fullPath.filename().wstring();

        auto workDir = fullPath.parent_path();
        if (fs::exists(workDir)) {
            std::vector<std::wstring> fileNameList;
            for (const auto& entry : fs::directory_iterator(workDir)) {
                if (!entry.is_regular_file())continue;

                std::wstring ext = entry.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

                if (ImageDatabase::supportExt.contains(ext) || ImageDatabase::supportRaw.contains(ext)) {
                    fileNameList.push_back(entry.path().filename().wstring());
                    jarkUtils::log(L"add fileNameList: {}", fileNameList.back());
                }
            }

            // 自然排序 数字感知排序
            std::sort(fileNameList.begin(), fileNameList.end(), [](std::wstring_view a, std::wstring_view b) -> bool {
                return StrCmpLogicalW(a.data(), b.data()) < 0; });

            for (auto& fileName : fileNameList) {
                auto fullpath = (workDir / fileName).wstring();
                imgFileList.push_back(fullpath);
                jarkUtils::log(L"add fullpath imgFileList: {}", fullpath);
                if (curFileIdx == -1 && openFileName == fileName) {
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
                imgDB.put(wstring(appName), { {{imgDB.getHomeMat(), 0}}, "请在图像文件右键使用本软件打开" });
            }
            else { // 打开的文件不支持，默认加到尾部
                imgFileList.emplace_back(fullPath.wstring());
                curFileIdx = (int)imgFileList.size() - 1;
                imgDB.put(fullPath.wstring(), { {{imgDB.getErrorTipsMat(), 0}}, "图像格式不支持或已删除" });
            }
        }

        curPar.framesPtr = imgDB.getSafePtr(imgFileList[curFileIdx], imgFileList[(curFileIdx + 1) % imgFileList.size()]);
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
            else if (cursorPos == CursorPos::rightDown)
                operateQueue.push({ ActionENUM::setting });
            else if (cursorPos == CursorPos::centerTop) {
                // 按钮ID  0:上一帧  1:暂停/继续  2:下一帧  3:保存该帧
                int buttonIdx = (abs(winWidth / 2 - x) > 100 ? -1 : (x + 100 - winWidth / 2) / 50);
                if (curPar.isAnimation && buttonIdx >= 0) {
                    if (curPar.isAnimationPause) {
                        switch (buttonIdx)
                        {
                        case 0: {
                            if (--curPar.curFrameIdx < 0)
                                curPar.curFrameIdx = curPar.curFrameIdxMax;
                            operateQueue.push({ ActionENUM::normalFresh });
                        }break;
                        case 1: {
                            curPar.isAnimationPause = !curPar.isAnimationPause;
                            operateQueue.push({ ActionENUM::normalFresh });
                        }break;
                        case 2: {
                            if (++curPar.curFrameIdx > curPar.curFrameIdxMax)
                                curPar.curFrameIdx = 0;
                            operateQueue.push({ ActionENUM::normalFresh });
                        }break;
                        case 3: {
                            const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
                            std::thread saveImageThread([](cv::Mat image) {
                                auto path = jarkUtils::saveImageDialog();
                                if (path.length() > 2) {
                                    cv::imwrite(path.c_str(), image);
                                }
                                }, srcImg);
                            saveImageThread.detach();
                        }break;
                        }
                    }
                    else {
                        if (buttonIdx == 1) {
                            curPar.isAnimationPause = !curPar.isAnimationPause;
                            operateQueue.push({ ActionENUM::normalFresh });
                        }
                    }
                }
            }
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
                    jarkUtils::ToggleFullScreen(m_hWnd);
                }
            }
            operateQueue.push({ ActionENUM::normalFresh });
            return;
        }

        case WM_RBUTTONUP: {//右键
            operateQueue.push({ ActionENUM::requestExit });
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

            if (y < 50 && abs(x - winWidth / 2) < 100) {
                cursorPos = CursorPos::centerTop;
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

            case CursorPos::centerTop:
                if (curPar.isAnimation) {
                    extraUIFlag = ShowExtraUI::animationBar;
                    operateQueue.push({ ActionENUM::normalFresh });
                }
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
                extraUIFlag = ShowExtraUI::setting;
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
            operateQueue.push({ zDelta < 0 ? ActionENUM::zoomOut : ActionENUM::zoomIn });
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
            case 'O': { // Ctrl + O  打开图片
                wstring filePath = jarkUtils::SelectFile(m_hWnd);
                if (!filePath.empty()) {
                    initOpenFile(filePath);
                    operateQueue.push({ ActionENUM::normalFresh });
                }
            }break;

            //case 'S': { // Ctrl + S  动图 批量保存每一帧
            //    if (curPar.isAnimation) {
            //        const auto& imgs = curPar.framesPtr->imgList;
            //        wstring& filePath = imgFileList[curFileIdx];
            //        auto dotIdx = filePath.find_last_of(L".");
            //        if (dotIdx == string::npos)
            //            dotIdx = filePath.size();
            //        for (int i = 0; i < imgs.size(); i++) {
            //            cv::imwrite(jarkUtils::wstringToAnsi(std::format(L"{}_{:04}.png", filePath.substr(0, dotIdx), i + 1)), imgs[i].img);
            //        }
            //    }
            //}break;

            case 'C': { // Ctrl + C  复制到剪贴板
                const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
                jarkUtils::copyImageToClipboard(srcImg);
            }break;

            case 'P': { // Ctrl + P 打印
                operateQueue.push({ ActionENUM::printImage });
            }break;
            }
        }
        else {
            switch (keyValue)
            {

            case 'J': { // 上一帧
                if (curPar.isAnimation && curPar.isAnimationPause) {
                    curPar.curFrameIdx--;
                    if (curPar.curFrameIdx < 0)
                        curPar.curFrameIdx = curPar.curFrameIdxMax;
                    operateQueue.push({ ActionENUM::normalFresh });
                }
            }break;

            case 'K': { // 动图 暂停/继续
                if (curPar.isAnimation) {
                    curPar.isAnimationPause = !curPar.isAnimationPause;
                    operateQueue.push({ ActionENUM::normalFresh });
                }
            }break;

            case 'L': { // 下一帧
                if (curPar.isAnimation && curPar.isAnimationPause) {
                    curPar.curFrameIdx++;
                    if (curPar.curFrameIdx > curPar.curFrameIdxMax)
                        curPar.curFrameIdx = 0;
                    operateQueue.push({ ActionENUM::normalFresh });
                }
            }break;

            case VK_CONTROL: {
                ctrlIsPressing = true;
            }break;

            case 'C': { // 复制图像信息到剪贴板
                jarkUtils::copyToClipboard(jarkUtils::utf8ToWstring(imgDB.getSafePtr(imgFileList[curFileIdx])->exifStr));
            }break;

            case VK_F11: {
                jarkUtils::ToggleFullScreen(m_hWnd);
            }break;

            case 'Q': {
                operateQueue.push({ ActionENUM::rotateLeft });
            }break;

            case 'E': {
                operateQueue.push({ ActionENUM::rotateRight });
            }break;

            case 'W': {
                const int newTargetYMax = ((curPar.rotation == 0 or curPar.rotation == 2) ? curPar.height : curPar.width) * curPar.zoomTarget / 2 / curPar.ZOOM_BASE;
                int newTargetY = curPar.slideTarget.y + ((winHeight + winWidth) / 16);
                newTargetY = std::clamp(newTargetY, -newTargetYMax, newTargetYMax);
                curPar.slideTarget.y = newTargetY;
                smoothShift = true;
            }break;

            case 'S': {
                const int newTargetYMax = ((curPar.rotation == 0 or curPar.rotation == 2) ? curPar.height : curPar.width) * curPar.zoomTarget / 2 / curPar.ZOOM_BASE;
                int newTargetY = curPar.slideTarget.y - ((winHeight + winWidth) / 16);
                newTargetY = std::clamp(newTargetY, -newTargetYMax, newTargetYMax);
                curPar.slideTarget.y = newTargetY;
                smoothShift = true;
            }break;

            case 'A': {
                const int newTargetXMax = ((curPar.rotation == 0 || curPar.rotation == 2) ? curPar.width : curPar.height) * curPar.zoomTarget / 2 / curPar.ZOOM_BASE;
                int newTargetX = curPar.slideTarget.x + ((winHeight + winWidth) / 16);
                newTargetX = std::clamp(newTargetX, -newTargetXMax, newTargetXMax);
                curPar.slideTarget.x = newTargetX;
                smoothShift = true;
            }break;

            case 'D': {
                const int newTargetXMax = ((curPar.rotation == 0 || curPar.rotation == 2) ? curPar.width : curPar.height) * curPar.zoomTarget / 2 / curPar.ZOOM_BASE;
                int newTargetX = curPar.slideTarget.x - ((winHeight + winWidth) / 16);
                newTargetX = std::clamp(newTargetX, -newTargetXMax, newTargetXMax);
                curPar.slideTarget.x = newTargetX;
                smoothShift = true;
            }break;

            case VK_UP: {
                operateQueue.push({ ActionENUM::zoomIn });
            }break;

            case VK_DOWN: {
                operateQueue.push({ ActionENUM::zoomOut });
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
                operateQueue.push({ ActionENUM::requestExit });
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

    uint32_t getSrcPx1(const cv::Mat& srcImg, int srcX, int srcY) const {
        uchar srcPx = srcImg.at<uchar>(srcY, srcX);
        if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            const uchar px0 = srcImg.at<uchar>(srcY - 1, srcX - 1);
            const uchar px1 = srcImg.at<uchar>(srcY - 1, srcX);
            const uchar px2 = srcImg.at<uchar>(srcY, srcX - 1);
            srcPx = (px0 + px1 + px2 + srcPx) >> 2;
        }
        return srcPx | srcPx << 8 | srcPx << 16 | 255 << 24;
    }

    inline static uint32_t getSrcPx3(const cv::Mat& srcImg, int srcX, int srcY) {
        cv::Vec3b srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

        if (isLowZoom && srcY > 0 && srcX > 0) { // 简单临近像素平均
            const cv::Vec3b px1 = srcImg.at<cv::Vec3b>(srcY - 1, srcX - 1);
            const cv::Vec3b px2 = srcImg.at<cv::Vec3b>(srcY - 1, srcX);
            const cv::Vec3b px3 = srcImg.at<cv::Vec3b>(srcY, srcX - 1);
            for (int i = 0; i < 3; i++)
                srcPx[i] = (px1[i] + px2[i] + px3[i] + srcPx[i]) >> 2;
        }
        return *((uint32_t*)&srcPx) | (255 << 24);
    }

    inline static uint32_t getSrcPx4(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) {
        const intUnion* srcPtr = (intUnion*)srcImg.ptr();
        const int srcW = srcImg.cols;

        intUnion srcPx = srcPtr[srcW * srcY + srcX];

        if (isLowZoom && srcY > 0 && srcX > 0) { // 简单临近像素平均
            intUnion px1 = srcPtr[srcW * (srcY - 1) + srcX - 1];
            intUnion px2 = srcPtr[srcW * (srcY - 1) + srcX];
            intUnion px3 = srcPtr[srcW * srcY + srcX - 1];
            for (int i = 0; i < 4; i++)
                srcPx[i] = (px1[i] + px2[i] + px3[i] + srcPx[i]) >> 2;
        }

        if (srcPx[3] == 255) return srcPx.u32;

        intUnion bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ? GRID_DARK : GRID_LIGHT;
        if (srcPx[3] == 0) return bgPx.u32;

        const int alpha = srcPx[3];
        //intUnion ret = 255;
        //ret[0] = (bgPx[0] * (255 - alpha) + srcPx[0] * alpha + 255) >> 8;
        //ret[1] = (bgPx[1] * (255 - alpha) + srcPx[1] * alpha + 255) >> 8;
        //ret[2] = (bgPx[2] * (255 - alpha) + srcPx[2] * alpha + 255) >> 8;
        return 255 << 24 |
            (((bgPx[2] * (255 - alpha) + srcPx[2] * alpha + 255) & 0xff00) << 8) |
            ((bgPx[1] * (255 - alpha) + srcPx[1] * alpha + 255) & 0xff00) |
            ((bgPx[0] * (255 - alpha) + srcPx[0] * alpha + 255) >> 8);
    }

    void drawCanvas(const cv::Mat& srcImg, cv::Mat& canvas) const {
        int srcH, srcW;
        if (curPar.rotation == 0 || curPar.rotation == 2) {
            srcH = srcImg.rows;
            srcW = srcImg.cols;
        }
        else {
            srcH = srcImg.cols;
            srcW = srcImg.rows;
        }

        const int canvasH = canvas.rows;
        const int canvasW = canvas.cols;

        if (srcH <= 0 || srcW <= 0)
            return;

        // 源图和画板canvas均100%缩放且居中重合，此时随机取一个点，先只考虑水平方向
        // 该点与画板中心的距离，等于该点与源图中心的距离
        // 即 canvasW / 2 - x = srcW / 2 - srcX
        // 再考虑偏移量：canvasW / 2 - x = srcW / 2 - srcX - slide * srcW
        // 再考虑源图缩放：canvasW / 2 - x = (srcW / 2 - srcX - slide * srcW) * zoom
        // 即为源图和画板在特定位移和缩放的坐标变换公式
        // x = canvasW / 2.0 - (srcW / 2.0 - srcX - slide * srcW) * zoom
        // srcX = srcW / 2.0 - ((canvasW / 2.0 - x) / zoom + slide * srcW)

        //SlideParameter slideVal;// 偏移比例，窗口中心落在源图的点与源图中心距离占边长的比例，slideVal.x = delta.x/srcWidth
        //int xStart = std::round(canvasW / 2.0 - (srcW / 2.0 - curPar.slideVal.x * srcW) * curPar.zoomDouble);
        //int xEnd = std::round(canvasW / 2.0 - (-srcW / 2.0 - curPar.slideVal.x * srcW) * curPar.zoomDouble);
        //int yStart = std::round(canvasH / 2.0 - (srcH / 2.0 - curPar.slideVal.y * srcH) * curPar.zoomDouble);
        //int yEnd = std::round(canvasH / 2.0 - (-srcH / 2.0 - curPar.slideVal.y * srcH) * curPar.zoomDouble);

        //if (xStart < 0) xStart = 0;
        //if (yStart < 0) yStart = 0;
        //if (xEnd > canvasW) xEnd = canvasW;
        //if (yEnd > canvasH) yEnd = canvasH;

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

        const float zoomInvert = (float)curPar.ZOOM_BASE / curPar.zoomCur;
        isLowZoom = curPar.zoomCur < curPar.ZOOM_BASE;

        switch (srcImg.type()) {
        case CV_8UC4: {
#ifdef ENABLE_OMP
#pragma omp parallel for
#endif
            const intUnion* srcPtr = (intUnion*)srcImg.ptr();
            for (int y = yStart; y < yEnd; y++) {
                auto ptr = ((uint32_t*)canvas.ptr()) + y * canvasW;
                //int srcY = (int)((int64_t)(y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur); // 2K屏 50%缩放一帧34ms 100%缩放一帧14ms
                int srcY = (int)((y - deltaH) * zoomInvert); // 快一点  2K屏 50%缩放一帧28ms 100%缩放一帧14ms

                srcY = std::clamp(srcY, 0, srcH);

                switch (curPar.rotation) {
                case 0:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx4(srcImg, srcX, srcY, x, y);
                    }
                    break;
                case 1:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx4(srcImg, srcH - 1 - srcY, srcX, x, y);
                    }
                    break;
                case 2:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx4(srcImg, srcW - 1 - srcX, srcH - 1 - srcY, x, y);
                    }
                    break;
                default:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx4(srcImg, srcY, srcW - 1 - srcX, x, y);
                    }
                    break;
                }                

                 //如果正在拖动/缩放/平移时，则偷懒：每隔一行就直接用上一行数据
                if (settingPar.isOptimizeSlide && 
                    (mouseIsPressing || curPar.zoomCur > curPar.ZOOM_BASE || 
                    curPar.zoomCur != curPar.zoomTarget || curPar.slideCur != curPar.slideTarget)) {
                    //if ((y & 1) && (++y < yEnd)) {  // 必须固定奇数或偶数y行，否则透明图拖动/平移时背景格子上下单行像素抖动
                    //    memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    //}
                    //上下只能二选一
                    //if ((srcY & 1) && (++y < yEnd)) {  // 必须固定奇数或偶数srcY行，否则原图拖动/平移上下单行像素抖动
                    //    memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    //}
                    if (++y < yEnd) {  // 原图拖动/平移上下单行像素抖动
                        memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    }
                }
            }
        }break;

        case CV_8UC3: {
#ifdef ENABLE_OMP
#pragma omp parallel for
#endif
            for (int y = yStart; y < yEnd; y++) {
                auto ptr = ((uint32_t*)canvas.ptr()) + y * canvasW;
                //int srcY = (int)((int64_t)(y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                int srcY = (int)((y - deltaH) * zoomInvert);

                srcY = std::clamp(srcY, 0, srcH);

                switch (curPar.rotation) {
                case 0:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx3(srcImg, srcX, srcY);
                    }
                    break;
                case 1:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx3(srcImg, srcH - 1 - srcY, srcX);
                    }
                    break;
                case 2:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx3(srcImg, srcW - 1 - srcX, srcH - 1 - srcY);
                    }
                    break;
                default:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx3(srcImg, srcY, srcW - 1 - srcX);
                    }
                    break;
                }

                // 如果正在拖动/缩放/平移时，则偷懒：每隔一行就直接用上一行数据
                if (settingPar.isOptimizeSlide &&
                    (mouseIsPressing || curPar.zoomCur > curPar.ZOOM_BASE ||
                    curPar.zoomCur != curPar.zoomTarget || curPar.slideCur != curPar.slideTarget)) {
                    //if ((srcY & 1) && (++y < yEnd)) {  // 必须固定奇数或偶数srcY行，否则原图拖动/平移上下单行像素抖动
                    //    memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    //}
                    if (++y < yEnd) {  // 原图拖动/平移上下单行像素抖动
                        memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    }
                }
            }
        }break;

        case CV_8UC1: {
            for (int y = yStart; y < yEnd; y++) {
                auto ptr = ((uint32_t*)canvas.ptr()) + y * canvasW;
                //int srcY = (int)(((int64_t)y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                int srcY = (int)((y - deltaH) * zoomInvert);

                srcY = std::clamp(srcY, 0, srcH);

                switch (curPar.rotation) {
                case 0:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx1(srcImg, srcX, srcY);
                    }
                    break;
                case 1:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx1(srcImg, srcH - 1 - srcY, srcX);
                    }
                    break;
                case 2:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx1(srcImg, srcW - 1 - srcX, srcH - 1 - srcY);
                    }
                    break;
                default:
                    for (int x = xStart; x < xEnd; x++) {
                        int srcX = (x - deltaW) * zoomInvert;
                        srcX = std::clamp(srcX, 0, srcW - 1);
                        ptr[x] = getSrcPx1(srcImg, srcY, srcW - 1 - srcX);
                    }
                    break;
                }

                // 如果正在拖动/缩放/平移时，则偷懒：每隔一行就直接用上一行数据
                if (settingPar.isOptimizeSlide &&
                    (mouseIsPressing || curPar.zoomCur > curPar.ZOOM_BASE ||
                    curPar.zoomCur != curPar.zoomTarget || curPar.slideCur != curPar.slideTarget)) {
                    //if ((srcY & 1) && (++y < yEnd)) {  // 必须固定奇数或偶数srcY行，否则原图拖动/平移上下单行像素抖动
                    //    memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    //}
                    if (++y < yEnd) {  // 原图拖动/平移上下单行像素抖动
                        memcpy(((uint32_t*)canvas.ptr()) + y * canvasW, ptr, canvasW * 4ULL);
                    }
                }
            }
        }break;
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
        using namespace std::chrono;

        int maxEdge = (int)std::ceil(std::sqrt(winWidth * winWidth + winHeight * winHeight));
        if (maxEdge < 2)
            return;
        auto tmpCanvas = cv::Mat(maxEdge, maxEdge, CV_8UC4, cv::Vec4b(BG_COLOR, BG_COLOR, BG_COLOR));

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        drawCanvas(srcImg, tmpCanvas);
        cv::resize(tmpCanvas, tmpCanvas, cv::Size(tmpCanvas.cols / 2, tmpCanvas.cols / 2));

        for (int i = 0; i <= 90; i += ((100 - i) / 6)) {
            auto start_clock = system_clock::now();
            auto view = rotateImage(tmpCanvas, i)(cv::Rect((maxEdge - winWidth) / 4, (maxEdge - winHeight) / 4, winWidth / 2, winHeight / 2));
            cv::resize(view, mainCanvas, cv::Size(winWidth, winHeight), 0, 0, cv::INTER_NEAREST);
            drawExifInfo(mainCanvas);
            drawExtraUI(mainCanvas);

            updateMainCanvas();

            if (duration_cast<milliseconds>(system_clock::now() - start_clock).count() < 10)
                Sleep(1);
        }
    }

    void rotateRightAnimation() {
        using namespace std::chrono;

        int maxEdge = (int)std::ceil(std::sqrt(winWidth * winWidth + winHeight * winHeight));
        if (maxEdge < 2)
            return;
        auto tmpCanvas = cv::Mat(maxEdge, maxEdge, CV_8UC4, cv::Vec4b(BG_COLOR, BG_COLOR, BG_COLOR));

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        drawCanvas(srcImg, tmpCanvas);
        cv::resize(tmpCanvas, tmpCanvas, cv::Size(tmpCanvas.cols / 2, tmpCanvas.cols / 2));

        for (int i = 0; i >= -90; i -= ((100 + i) / 6)) {
            auto start_clock = system_clock::now();
            auto view = rotateImage(tmpCanvas, i)(cv::Rect((maxEdge - winWidth) / 4, (maxEdge - winHeight) / 4, winWidth / 2, winHeight / 2));
            cv::resize(view, mainCanvas, cv::Size(winWidth, winHeight), 0, 0, cv::INTER_NEAREST);
            drawExifInfo(mainCanvas);
            drawExtraUI(mainCanvas);

            updateMainCanvas();

            if (duration_cast<milliseconds>(system_clock::now() - start_clock).count() < 10)
                Sleep(1);
        }
    }

    // 添加水平运动模糊
    void applyHorizontalMotionBlur(cv::Mat& src, cv::Mat& dst, int kernelSize = 15, int direction = 1) {
        // 创建水平运动模糊核 (1行 x kernelSize列)
        cv::Mat kernel = cv::Mat::zeros(1, kernelSize, CV_32F);

        // 设置方向 (1.0 = 右移模糊, -1.0 = 左移模糊)
        int start = (direction > 0) ? 0 : kernelSize - 1;
        int end = (direction > 0) ? kernelSize : -1;
        int step = (direction > 0) ? 1 : -1;

        // 填充核 (线性衰减效果)
        float sum = 0.0f;
        for (int i = start; i != end; i += step) {
            float weight = 1.0f - std::abs(static_cast<float>(i - start) / (kernelSize - 1));
            kernel.at<float>(0, i) = weight;
            sum += weight;
        }
        // 归一化核
        kernel /= sum;
        // 应用水平卷积 (仅水平方向)
        cv::filter2D(src, dst, -1, kernel, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
    }

    // 添加竖直运动模糊
    void applyVerticalMotionBlur(cv::Mat& src, cv::Mat& dst, int kernelSize = 15, float direction = 1.0f) {
        // 创建水平运动模糊核 (1行 x kernelSize列)
        cv::Mat kernel = cv::Mat::zeros(kernelSize, 1, CV_32F);

        // 设置方向 (1.0 = 下移模糊, -1.0 = 上移模糊)
        int start = (direction > 0) ? 0 : kernelSize - 1;
        int end = (direction > 0) ? kernelSize : -1;
        int step = (direction > 0) ? 1 : -1;

        // 填充核 (线性衰减效果)
        float sum = 0.0f;
        for (int i = start; i != end; i += step) {
            float weight = 1.0f - std::abs(static_cast<float>(i - start) / (kernelSize - 1));
            kernel.at<float>(i, 0) = weight;
            sum += weight;
        }
        // 归一化核
        kernel /= sum;
        // 应用水平卷积 (仅水平方向)
        cv::filter2D(src, dst, -1, kernel, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
    }

    // 水平滑动 上一张图
    void mainCanvasSlideToPreAnimationHorizontal() {
        using namespace std::chrono;

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        auto nextmainCanvas = cv::Mat(mainCanvas.size(), mainCanvas.type());
        drawCanvas(srcImg, nextmainCanvas);
        drawExifInfo(nextmainCanvas);

        cv::Mat smallMainCanvas;
        cv::resize(mainCanvas, smallMainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);
        cv::resize(nextmainCanvas, nextmainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);

        cv::Mat panorama(nextmainCanvas.rows, nextmainCanvas.cols * 2, nextmainCanvas.type());
        nextmainCanvas.copyTo(panorama(cv::Rect(0, 0, smallMainCanvas.cols, smallMainCanvas.rows)));
        smallMainCanvas.copyTo(panorama(cv::Rect(smallMainCanvas.cols, 0, nextmainCanvas.cols, nextmainCanvas.rows)));

        cv::Mat blurred;
        applyHorizontalMotionBlur(panorama, blurred, 15, 1);
        panorama = blurred;

        const int frame_width = nextmainCanvas.cols;
        const int frame_height = nextmainCanvas.rows;
        for (int x = frame_width; x > 0; x -= ((frame_width * 1.5 - x) / 8)) {
            auto start_clock = system_clock::now();

            cv::Mat view = panorama(cv::Rect(x, 0, frame_width, frame_height));
            cv::resize(view, mainCanvas, cv::Size(winWidth, winHeight), 0, 0, cv::INTER_NEAREST);
            drawExtraUI(mainCanvas);

            updateMainCanvas();

            if (duration_cast<milliseconds>(system_clock::now() - start_clock).count() < 10)
                Sleep(1);
        }
    }

    // 水平滑动 下一张图
    void mainCanvasSlideToNextAnimationHorizontal() {
        using namespace std::chrono;

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        auto nextmainCanvas = cv::Mat(mainCanvas.size(), mainCanvas.type());
        drawCanvas(srcImg, nextmainCanvas);
        drawExifInfo(nextmainCanvas);

        cv::Mat smallMainCanvas;
        cv::resize(mainCanvas, smallMainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);
        cv::resize(nextmainCanvas, nextmainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);

        cv::Mat panorama(nextmainCanvas.rows, nextmainCanvas.cols * 2, nextmainCanvas.type());
        smallMainCanvas.copyTo(panorama(cv::Rect(0, 0, smallMainCanvas.cols, smallMainCanvas.rows)));
        nextmainCanvas.copyTo(panorama(cv::Rect(smallMainCanvas.cols, 0, nextmainCanvas.cols, nextmainCanvas.rows)));

        cv::Mat blurred;
        applyHorizontalMotionBlur(panorama, blurred, 15, -1);
        panorama = blurred;

        const int frame_width = nextmainCanvas.cols;
        const int frame_height = nextmainCanvas.rows;
        for (int x = 0; x <= frame_width; x += ((frame_width*1.5 - x) / 8)) {
            auto start_clock = system_clock::now();

            cv::Mat view = panorama(cv::Rect(x, 0, frame_width, frame_height));
            cv::resize(view, mainCanvas, cv::Size(winWidth, winHeight), 0, 0, cv::INTER_NEAREST);
            drawExtraUI(mainCanvas);

            updateMainCanvas();

            if (duration_cast<milliseconds>(system_clock::now() - start_clock).count() < 10)
                Sleep(1);
        }
    }

    // 竖直滑动 上一张图
    void mainCanvasSlideToPreAnimationVertical() {
        using namespace std::chrono;

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        auto nextmainCanvas = cv::Mat(mainCanvas.size(), mainCanvas.type());
        drawCanvas(srcImg, nextmainCanvas);
        drawExifInfo(nextmainCanvas);

        cv::Mat smallMainCanvas;
        cv::resize(mainCanvas, smallMainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);
        cv::resize(nextmainCanvas, nextmainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);

        cv::Mat panorama(nextmainCanvas.rows * 2, nextmainCanvas.cols, nextmainCanvas.type());
        nextmainCanvas.copyTo(panorama(cv::Rect(0, 0, smallMainCanvas.cols, smallMainCanvas.rows)));
        smallMainCanvas.copyTo(panorama(cv::Rect(0, smallMainCanvas.rows, nextmainCanvas.cols, nextmainCanvas.rows)));

        cv::Mat blurred;
        applyVerticalMotionBlur(panorama, blurred, 15, 1);
        panorama = blurred;

        const int frame_width = nextmainCanvas.cols;
        const int frame_height = nextmainCanvas.rows;
        for (int y = frame_height; y >= 0; y -= ((frame_height * 1.5 - y) / 8)) {
            auto start_clock = system_clock::now();

            cv::Mat view = panorama(cv::Rect(0, y, frame_width, frame_height));
            cv::resize(view, mainCanvas, cv::Size(winWidth, winHeight), 0, 0, cv::INTER_NEAREST);
            drawExtraUI(mainCanvas);

            updateMainCanvas();

            if (duration_cast<milliseconds>(system_clock::now() - start_clock).count() < 10)
                Sleep(1);
        }
    }
    
    // 竖直滑动 下一张图
    void mainCanvasSlideToNextAnimationVertical() {
        using namespace std::chrono;

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        auto nextmainCanvas = cv::Mat(mainCanvas.size(), mainCanvas.type());
        drawCanvas(srcImg, nextmainCanvas);
        drawExifInfo(nextmainCanvas);

        cv::Mat smallMainCanvas;
        cv::resize(mainCanvas, smallMainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);
        cv::resize(nextmainCanvas, nextmainCanvas, cv::Size(winWidth / 4, winHeight / 4), 0, 0, cv::INTER_NEAREST);

        cv::Mat panorama(nextmainCanvas.rows*2, nextmainCanvas.cols, nextmainCanvas.type());
        smallMainCanvas.copyTo(panorama(cv::Rect(0, 0, smallMainCanvas.cols, smallMainCanvas.rows)));
        nextmainCanvas.copyTo(panorama(cv::Rect(0, smallMainCanvas.rows, nextmainCanvas.cols, nextmainCanvas.rows)));

        cv::Mat blurred;
        applyVerticalMotionBlur(panorama, blurred, 15, -1);
        panorama = blurred;

        const int frame_width = nextmainCanvas.cols;
        const int frame_height = nextmainCanvas.rows;
        for (int y = 0; y <= frame_height; y += ((frame_height * 1.5 - y) / 8)) {
            auto start_clock = system_clock::now();

            cv::Mat view = panorama(cv::Rect(0, y, frame_width, frame_height));
            cv::resize(view, mainCanvas, cv::Size(winWidth, winHeight), 0, 0, cv::INTER_NEAREST);
            drawExtraUI(mainCanvas);

            updateMainCanvas();

            if (duration_cast<milliseconds>(system_clock::now() - start_clock).count() < 10)
                Sleep(1);
        }
    }

    void drawExifInfo(cv::Mat& canvas) {
        if (showExif) {
            const int padding = 10;
            const int areaWidth = (canvas.cols - 2 * padding) / 4;
            cv::Rect rect{ padding, padding, std::max(areaWidth, 400), canvas.rows - 2 * padding };
            textDrawer.putAlignLeft(canvas, rect, curPar.framesPtr->exifStr.c_str(), { 255, 255, 255, 255 }); // 长文本 8ms
        }
    }

    void drawExtraUI(cv::Mat& canvas) {
        int canvasHeight = canvas.rows;
        int canvasWidth = canvas.cols;

        //窗口尺寸太小则直接退出
        if (canvasWidth < 100 || canvasHeight < 100 || extraUIFlag == ShowExtraUI::none)
            return;

        switch (extraUIFlag)
        {
        case ShowExtraUI::rotateLeftButton: {
            auto& img = extraUIRes.leftRotate;
            jarkUtils::overlayImg(canvas, img, 0, (canvasHeight / 4 - img.rows) / 2);
        } break;

        case ShowExtraUI::leftArrow: {
            auto& img = extraUIRes.leftArrow;
            jarkUtils::overlayImg(canvas, img, 0, (canvasHeight - img.rows) / 2);
        } break;

        case ShowExtraUI::printer: {
            auto& img = extraUIRes.printer;
            jarkUtils::overlayImg(canvas, img, 0, (canvasHeight * 7 / 4 - img.rows) / 2);
        } break;

        case ShowExtraUI::setting: {
            auto& img = extraUIRes.setting;
            jarkUtils::overlayImg(canvas, img, canvasWidth - img.cols, (canvasHeight * 7 / 4 - img.rows) / 2);
        } break;

        case ShowExtraUI::rightArrow: {
            auto& img = extraUIRes.rightArrow;
            jarkUtils::overlayImg(canvas, img, canvasWidth - img.cols, (canvasHeight - img.rows) / 2);
        } break;

        case ShowExtraUI::rotateRightButton: {
            auto& img = extraUIRes.rightRotate;
            jarkUtils::overlayImg(canvas, img, canvasWidth - img.cols, (canvasHeight / 4 - img.rows) / 2);
        } break;

        case ShowExtraUI::animationBar: {
            auto& img = curPar.isAnimationPause ? extraUIRes.animationBarPausing : extraUIRes.animationBarPlaying;
            jarkUtils::overlayImg(canvas, img, (canvasWidth - img.cols)/2, 0);
        } break;
        }
    }

    void updateMainCanvas() {
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
    }


    int64_t delayRemain = 0;
    const std::chrono::milliseconds frameDuration{ 10 };
    std::chrono::steady_clock::time_point lastTimestamp = std::chrono::steady_clock::now();


    void DrawScene() {
        auto operateAction = operateQueue.get();
        if (operateAction.action == ActionENUM::none &&
            curPar.zoomCur == curPar.zoomTarget &&
            curPar.slideCur == curPar.slideTarget &&
            (!curPar.isAnimation || (curPar.isAnimation && curPar.isAnimationPause))) {

            Sleep(1); // Windows机制限制，实际时长最小只能 15.6ms
            return;
        }

        if (operateAction.action == ActionENUM::printImage) {
            if (Printer::isWorking) {
                jarkUtils::activateWindow(Printer::hwnd);
            }
            else {
                Setting::requestExit(); // OpenCV窗口暂时不能同时共存
                const auto& imgs = curPar.framesPtr->imgList;
                std::thread printerThread([](cv::Mat image, SettingParameter* settingParameter) {
                    Printer printer(image, settingParameter);
                    }, imgs[curPar.curFrameIdx].img, & settingPar);
                printerThread.detach();
            }
            return;
        }

        if (operateAction.action == ActionENUM::setting) {
            if (Setting::isWorking) {
                jarkUtils::activateWindow(Setting::hwnd);
            }
            else {
                Printer::requestExit(); // OpenCV窗口暂时不能同时共存
                const auto& imgs = curPar.framesPtr->imgList;
                std::thread settingThread([](cv::Mat image, SettingParameter* settingParameter) {
                    Setting setting(image, settingParameter);
                    }, imgs.front().img, &settingPar);
                settingThread.detach();
            }
            return;
        }

        // 以下action均需要刷新画面
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
                curPar.updateZoomList(winWidth, winHeight);
            }
            else {
                hasInitWinSize = true;
                curPar.Init(winWidth, winHeight);
            }
        } break;

        case ActionENUM::preImg: {
            if (imgFileList.size() <= 1)
                break;
            
            if (settingPar.switchImageAnimationMode) {// 开动画时才需要
                const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
                drawCanvas(srcImg, mainCanvas); //先更新无额外按钮UI的原图
                drawExifInfo(mainCanvas);
            }

            if (--curFileIdx < 0)
                curFileIdx = (int)imgFileList.size() - 1;
            curPar.framesPtr = imgDB.getSafePtr(imgFileList[curFileIdx], imgFileList[(curFileIdx + imgFileList.size() - 1) % imgFileList.size()]);
            curPar.Init(winWidth, winHeight);

            if (settingPar.switchImageAnimationMode == 1)
                mainCanvasSlideToPreAnimationVertical();      // 竖直滑动
            else if (settingPar.switchImageAnimationMode == 2)
                mainCanvasSlideToPreAnimationHorizontal();    // 水平滑动

            lastTimestamp = std::chrono::steady_clock::now();
            delayRemain = 0;
        } break;

        case ActionENUM::nextImg: {
            if (imgFileList.size() <= 1)
                break;
            
            if (settingPar.switchImageAnimationMode) {// 开动画时才需要
                const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
                drawCanvas(srcImg, mainCanvas); //先更新无额外按钮UI的原图
                drawExifInfo(mainCanvas);
            }

            if (++curFileIdx >= (int)imgFileList.size())
                curFileIdx = 0;
            curPar.framesPtr = imgDB.getSafePtr(imgFileList[curFileIdx], imgFileList[(curFileIdx + 1) % imgFileList.size()]);
            curPar.Init(winWidth, winHeight);

            if (settingPar.switchImageAnimationMode == 1)
                mainCanvasSlideToNextAnimationVertical();   // 竖直滑动
            else if (settingPar.switchImageAnimationMode == 2)
                mainCanvasSlideToNextAnimationHorizontal(); // 水平滑动

            lastTimestamp = std::chrono::steady_clock::now();
            delayRemain = 0;
        } break;

        case ActionENUM::slide: {
            // slideVal: -0.5 ~ 0.5
            //if (curPar.rotation == 0 || curPar.rotation == 2) {
            //    curPar.slideVal.x += operateAction.x / (curPar.width * curPar.zoomDouble);
            //    curPar.slideVal.y += operateAction.y / (curPar.height * curPar.zoomDouble);
            //}
            //else {
            //    curPar.slideVal.x += operateAction.x / (curPar.height * curPar.zoomDouble);
            //    curPar.slideVal.y += operateAction.y / (curPar.width * curPar.zoomDouble);
            //}
            // curPar.slideVal.x = std::clamp(curPar.slideVal.x, -0.5, 0.5);
            // curPar.slideVal.y = std::clamp(curPar.slideVal.y, -0.5, 0.5);

            int newTargetX = curPar.slideTarget.x + operateAction.x;
            int newTargetY = curPar.slideTarget.y + operateAction.y;

            const int newTargetXMax = ((curPar.rotation == 0 || curPar.rotation == 2) ? curPar.width : curPar.height) * curPar.zoomTarget / 2 / curPar.ZOOM_BASE;
            newTargetX = std::clamp(newTargetX, -newTargetXMax, newTargetXMax);

            const int newTargetYMax = ((curPar.rotation == 0 or curPar.rotation == 2) ? curPar.height : curPar.width) * curPar.zoomTarget / 2 / curPar.ZOOM_BASE;
            newTargetY = std::clamp(newTargetY, -newTargetYMax, newTargetYMax);

            curPar.slideTarget.x = newTargetX;
            curPar.slideTarget.y = newTargetY;
        } break;

        case ActionENUM::toggleExif: {
            showExif = !showExif;
        } break;

        case ActionENUM::zoomIn: {
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

        case ActionENUM::zoomOut: {
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

        case ActionENUM::rotateLeft: {
            if (settingPar.isAllowRotateAnimation) {
                rotateLeftAnimation();
            }
            curPar.rotation = (curPar.rotation + 1) & 0b11;
            curPar.slideTargetRotateLeft();
            curPar.updateZoomList(winWidth, winHeight);
        } break;

        case ActionENUM::rotateRight: {
            if (settingPar.isAllowRotateAnimation) {
                rotateRightAnimation();
            }
            curPar.rotation = (curPar.rotation + 4 - 1) & 0b11;
            curPar.slideTargetRotateRight();
            curPar.updateZoomList(winWidth, winHeight);
        } break;

        case ActionENUM::requestExit: {
            Printer::requestExit();
            Setting::requestExit();
            PostMessageW(m_hWnd, WM_DESTROY, 0, 0);
        } break;
        }

        if (curPar.zoomCur != curPar.zoomTarget || curPar.slideCur != curPar.slideTarget) {
            if (settingPar.isAllowZoomAnimation && smoothShift) { // 简单缩放动画
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

        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        curPar.curFrameDelay = (delay <= 0 ? 10 : delay);

        drawCanvas(srcImg, mainCanvas);
        drawExifInfo(mainCanvas);
        drawExtraUI(mainCanvas);

        if (curPar.isAnimation && curPar.isAnimationPause) {
            wstring str = std::format(L"逐帧浏览 [{}/{}] {}% ",
                curPar.curFrameIdx + 1, curPar.curFrameIdxMax + 1,
                curPar.zoomCur * 100ULL / curPar.ZOOM_BASE)
                + imgFileList[curFileIdx];
            if (curPar.rotation)
                str += (curPar.rotation == 1 ? L"  逆时针旋转90°" : (curPar.rotation == 3 ? L"  顺时针旋转90°" : (L"  旋转180°")));
            SetWindowTextW(m_hWnd, str.c_str());
        }
        else {
            wstring str = std::format(L" [{}/{}] {}% ",
                curFileIdx + 1, imgFileList.size(),
                curPar.zoomCur * 100ULL / curPar.ZOOM_BASE)
                + imgFileList[curFileIdx];
            if (curPar.rotation)
                str += (curPar.rotation == 1 ? L"  逆时针旋转90°" : (curPar.rotation == 3 ? L"  顺时针旋转90°" : (L"  旋转180°")));
            SetWindowTextW(m_hWnd, str.c_str());
        }

        updateMainCanvas();

        if (curPar.isAnimation && curPar.isAnimationPause == false) {
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
    else {
        MessageBoxW(NULL, L"窗口创建失败！", L"错误", MB_ICONERROR);
    }

    ::CoUninitialize();
    return 0;
}

void test() {
    return;

    //set<wstring> sortRaw(ImageDatabase::supportRaw.begin(), ImageDatabase::supportRaw.end());
    //for (auto& ext : sortRaw)
    //    std::wcout << ext.substr(1) << ' ';
    //std::wcout << endl;

    //set<wstring> sortExt(ImageDatabase::supportExt.begin(), ImageDatabase::supportExt.end());
    //for (auto& ext : sortExt)
    //    std::wcout << ext.substr(1) << ' ';
    //std::wcout << endl;

    std::ifstream file("D:\\Downloads\\test\\22.wp2", std::ios::binary);
    auto buf = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    exit(0);
}
