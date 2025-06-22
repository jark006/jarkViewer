#pragma once

#include "Utils.h"

// 全局变量存储UI状态
struct PrintParams {
    //double topMargin = 5.0;       // 上边距百分比
    //double bottomMargin = 5.0;    // 下边距百分比
    //double leftMargin = 5.0;      // 左边距百分比
    //double rightMargin = 5.0;     // 右边距百分比
    //int layoutMode = 0;           // 0=适应, 1=填充, 2=原始比例
    int brightness = 100;         // 亮度调整 (0 ~ 200)
    int contrast = 100;           // 对比度调整 (0 ~ 200)
    int colorMode = 1;            // 颜色模式 0:彩色  1:黑白  2:文档优化 亮度均衡
    bool invertColors = false;    // 是否反相

    bool isParamsChange = false;
    bool confirmed = false;       // 用户点击确认
    bool saveToFile = false;      // 保存到文件
    cv::Mat previewImage;         // 预览图像

    const char* windowsName = "Print Preview";
};

class Printer {
private:
    const char* windowsName = "Print Preview";
    PrintParams params{};
    cv::Mat buttonPrint, buttonDocument, buttonNormal, buttonInvert, buttonColor, buttonGray;
    SettingParameter *settingParameter = nullptr;

    void Init() {
        rcFileInfo rc;
        rc = Utils::GetResource(IDB_PNG_BUTTON_PRINTER, L"PNG");
        buttonPrint = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);
        rc = Utils::GetResource(IDB_PNG_BUTTON_DOC, L"PNG");
        buttonDocument = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);
        rc = Utils::GetResource(IDB_PNG_BUTTON_NORMAL, L"PNG");
        buttonNormal = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);
        rc = Utils::GetResource(IDB_PNG_BUTTON_INVERT, L"PNG");
        buttonInvert = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);
        rc = Utils::GetResource(IDB_PNG_BUTTON_COLOR, L"PNG");
        buttonColor = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);
        rc = Utils::GetResource(IDB_PNG_BUTTON_GRAY, L"PNG");
        buttonGray = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);

        params.brightness = settingParameter->printerBrightness;
        params.contrast = settingParameter->printerContrast;
        params.colorMode = settingParameter->printercolorMode;
        params.invertColors = settingParameter->printerInvertColors;
        
        // 异常情况则恢复默认值
        if (params.brightness < 0 || params.brightness > 200 || params.contrast < 0 || params.brightness > 200 ||
            params.colorMode < 0 || params.colorMode >2) {
            params.brightness = 100;
            params.contrast = 100;
            params.colorMode = 1;
            params.invertColors = false;
        }
    }

public:
    Printer(const cv::Mat& image, SettingParameter* settingParameter) : settingParameter(settingParameter) {
        Init();
        PrintMatImage(image);
    }

    ~Printer() {
        settingParameter->printerBrightness = params.brightness;
        settingParameter->printerContrast = params.contrast;
        settingParameter->printercolorMode = params.colorMode;
        settingParameter->printerInvertColors = params.invertColors;
    }

    // 灰度/BGR/BGRA统一到BGR
    cv::Mat matToBGR(const cv::Mat& image) {
        if (image.empty())
            return {};

        cv::Mat bgrMat;
        if (image.channels() == 1) {
            cv::cvtColor(image, bgrMat, cv::COLOR_GRAY2BGR);
        }
        else if (image.channels() == 3) {
            bgrMat = image.clone();
        }
        else if (image.channels() == 4) {
            // Alpha混合白色背景 (255, 255, 255)
            const int width = image.cols;
            const int height = image.rows;
            bgrMat = cv::Mat(height, width, CV_8UC3);
            for (int y = 0; y < height; y++) {
                const BYTE* srcRow = image.ptr<BYTE>(y);
                BYTE* desRow = bgrMat.ptr<BYTE>(y);

                for (int x = 0; x < width; x++) {
                    BYTE b = srcRow[x * 4];
                    BYTE g = srcRow[x * 4 + 1];
                    BYTE r = srcRow[x * 4 + 2];
                    BYTE a = srcRow[x * 4 + 3];

                    if (a == 0) {
                        desRow[x * 3] = 255;
                        desRow[x * 3 + 1] = 255;
                        desRow[x * 3 + 2] = 255;
                    }
                    else if (a == 255) {
                        desRow[x * 3] = b;
                        desRow[x * 3 + 1] = g;
                        desRow[x * 3 + 2] = r;
                    }
                    else {
                        desRow[x * 3] = static_cast<BYTE>((b * a + 255 * (255 - a) + 255) >> 8);     // B
                        desRow[x * 3 + 1] = static_cast<BYTE>((g * a + 255 * (255 - a) + 255) >> 8); // G
                        desRow[x * 3 + 2] = static_cast<BYTE>((r * a + 255 * (255 - a) + 255) >> 8); // R
                    }
                }
            }
        }

        return bgrMat;
    }

    // 均衡全图亮度 再调整亮度对比度 适合打印文档
    static cv::Mat balancedImageBrightness(const cv::Mat& input_img) {
        if (input_img.type() != CV_8UC3) {
            MessageBoxW(nullptr, L"balancedImageBrightness转换图像错误: 只接受BGR/CV_8UC3类型图像", L"错误", MB_OK | MB_ICONERROR);
            return {};
        }

        int kernel_size = std::max(input_img.cols, input_img.rows) / 20;
        if (kernel_size < 3)
            kernel_size = 3;

        // 确保核大小为奇数
        kernel_size |= 1;

        // 转换为灰度图像
        cv::Mat gray;
        if (input_img.channels() == 3) {
            cvtColor(input_img, gray, cv::COLOR_BGR2GRAY);
        }
        else {
            gray = input_img.clone();
        }

        // 估计背景（使用大核模糊）
        cv::Mat background;
        GaussianBlur(gray, background, cv::Size(kernel_size, kernel_size), 0);

        // 从原始图像中减去背景
        cv::Mat corrected;
        // 相当于：corrected = gray * 1 + background * (-1) + 128
        addWeighted(gray, 1.0, background, -1.0, 128, corrected);

        // 增强对比度（归一化到0-255范围）
        normalize(corrected, corrected, 0, 255, cv::NORM_MINMAX);
        corrected.convertTo(corrected, CV_8U);

        cvtColor(corrected, corrected, cv::COLOR_GRAY2BGR);
        return corrected;
    }


    // 将cv::Mat转换为HBITMAP
    HBITMAP MatToHBITMAP(const cv::Mat& image) {
        if (image.empty()) {
            MessageBoxW(nullptr, L"MatToHBITMAP转换图像错误: 空图像", L"错误", MB_OK | MB_ICONERROR);
            return nullptr;
        }
        if (image.type() != CV_8UC3) {
            MessageBoxW(nullptr, L"MatToHBITMAP转换图像错误: 只接受BGR/CV_8UC3类型图像", L"错误", MB_OK | MB_ICONERROR);
            return nullptr;
        }

        const int width = image.cols;
        const int height = image.rows;

        const int stride = (width * 3 + 3) & ~3;  // 4字节对齐
        const size_t dataSize = static_cast<size_t>(stride) * height;

        // 准备BITMAPINFO结构
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;          // 24位RGB
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = dataSize;

        // 创建DIB并复制数据
        HDC hdcScreen = GetDC(nullptr);
        void* pBits;
        HBITMAP hBitmap = CreateDIBSection(
            hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        ReleaseDC(nullptr, hdcScreen);

        if (hBitmap) {
            for (int y = 0; y < height; y++) {
                const BYTE* srcRow = image.ptr<BYTE>(height - 1 - y);
                BYTE* dstRow = static_cast<BYTE*>(pBits) + y * stride;
                memcpy(dstRow, srcRow, stride);
            }
        }

        return hBitmap;
    }

    static void adjustBrightnessContrast(cv::Mat& src, int brightnessInt, int contrastInt) {
        if (src.empty() || src.type() != CV_8UC3)
            return;

        if (brightnessInt < 1)
            brightnessInt = 1;
        else if (brightnessInt > 199)
            brightnessInt = 199;

        if (contrastInt < 0)
            contrastInt = 0;
        else if (contrastInt > 200)
            contrastInt = 200;

        // brightness: 0 ~ 200 映射到 0 ~ 2.0
        // contrast:   0 ~ 200 映射到 0 ~ 2.0
        double brightness = brightnessInt / 100.0;
        double contrast = contrastInt / 100.0;

        brightness = pow(2.0 - brightness, 3); // 增大对比度比例
        contrast = pow(contrast, 3); // 增大对比度比例

        for (int y = 0; y < src.rows; y++) {
            for (int x = 0; x < src.cols; x++) {
                cv::Vec3b pixel = src.at<cv::Vec3b>(y, x);
                for (int c = 0; c < 3; c++) {
                    // 以128为中心进行对比度调整
                    double adjusted = (pixel[c] - 128.0) * contrast + 128.0;

                    // 添加亮度偏移
                    adjusted = pow(adjusted / 255.0, brightness) * 255;

                    // 确保值在0-255范围内
                    pixel[c] = cv::saturate_cast<uchar>(adjusted);
                }
                src.at<cv::Vec3b>(y, x) = pixel;
            }
        }
    }

    // 图像处理 调整对比度 彩色 黑白
    static void ApplyImageAdjustments(cv::Mat& image, int brightness, int contrast, int colorMode, bool invertColors) {
        if (image.empty()) return;

        if (invertColors) {
            cv::bitwise_not(image, image);
        }

        if (colorMode == 1) {
            cv::Mat gray;
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(gray, image, cv::COLOR_GRAY2BGR);
        }else if (colorMode == 2) {
            image = balancedImageBrightness(image);
        }

        // brightness: 0 ~ 200, 100是中间值，亮度不增不减
        // contrast:   0 ~ 200, 100是中间值，对比度不增不减
        adjustBrightnessContrast(image, brightness, contrast);
    }


    // OpenCV 回调函数
    void refreshPreview(void* userdata) {
        PrintParams* params = static_cast<PrintParams*>(userdata);

        cv::Mat adjusted = params->previewImage.clone();
        ApplyImageAdjustments(adjusted, params->brightness, params->contrast, params->colorMode, params->invertColors);

        // 扩展为正方形画布
        int outputSize = std::max(adjusted.rows, adjusted.cols);
        int x = (outputSize - adjusted.cols) / 2;
        int y = (outputSize - adjusted.rows) / 2;

        // 画布高度+50 放置底栏按钮
        cv::Mat squareMat(outputSize + 50, outputSize, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::Mat roi = squareMat(cv::Rect(x, y, adjusted.cols, adjusted.rows));
        adjusted.copyTo(roi);

        cv::cvtColor(squareMat, squareMat, cv::COLOR_BGR2BGRA);

        Utils::overlayImg(squareMat, params->colorMode ? (params->colorMode == 1 ? buttonGray : buttonDocument) : buttonColor, 100, 800);
        Utils::overlayImg(squareMat, params->invertColors ? buttonInvert : buttonNormal, 400, 800);
        Utils::overlayImg(squareMat, buttonPrint, squareMat.cols - buttonPrint.cols, squareMat.rows - buttonPrint.rows);
        cv::imshow(windowsName, squareMat);
    }


    // 打印前预处理
    cv::Mat ImagePreprocessingForPrint(const cv::Mat& image) {
        if (image.empty() || image.type() != CV_8UC3) {
            return {};
        }

        // 创建预览图像
        const int previewMaxSize = 800;

        double scale = (double)previewMaxSize / std::max(image.rows, image.cols);
        cv::resize(image, params.previewImage, cv::Size(), scale, scale);

        // 若长宽差距很极端，超长或超宽，缩放可能异常
        if (params.previewImage.empty() || params.previewImage.rows <= 0 || params.previewImage.cols <= 0) {
            return {};
        }

        // 创建UI窗口
        cv::namedWindow(windowsName, cv::WINDOW_AUTOSIZE);
        cv::resizeWindow(windowsName, previewMaxSize, static_cast<int>(previewMaxSize * 1.2));

        cv::setMouseCallback(windowsName, [](int event, int x, int y, int flags, void* userdata) {
            if (event == cv::EVENT_LBUTTONUP) {
                if ((100 < x) && (x < 200) && (800 < y)) { // 彩色
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    if (params->colorMode != 0) {
                        if (params->colorMode == 2) { // 若之前是文档优化，则恢复默认亮度对比度
                            params->brightness = 100;
                            params->contrast = 100;
                        }
                        params->colorMode = 0;
                        cv::setTrackbarPos("Brightness", params->windowsName, params->brightness);
                        cv::setTrackbarPos("Contrast", params->windowsName, params->contrast);
                        params->isParamsChange = true;
                    }
                }

                if ((200 < x) && (x < 300) && (800 < y)) { // 黑白
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    if (params->colorMode != 1) {
                        if (params->colorMode == 2) { // 若之前是文档优化，则恢复默认亮度对比度
                            params->brightness = 100;
                            params->contrast = 100;
                        }
                        params->colorMode = 1;
                        cv::setTrackbarPos("Brightness", params->windowsName, params->brightness);
                        cv::setTrackbarPos("Contrast", params->windowsName, params->contrast);
                        params->isParamsChange = true;
                    }
                }

                if ((300 < x) && (x < 400) && (800 < y)) { // 文档优化
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    if (params->colorMode != 2) {
                        params->colorMode = 2;
                        params->brightness = 160;
                        params->contrast = 180;
                        cv::setTrackbarPos("Brightness", params->windowsName, params->brightness);
                        cv::setTrackbarPos("Contrast", params->windowsName, params->contrast);
                        params->isParamsChange = true;
                    }
                }

                if ((400 < x) && (x < 500) && (800 < y)) { // 正色
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    if (params->invertColors) {
                        params->invertColors = false;
                        params->isParamsChange = true;
                    }
                }
                if ((500 < x) && (x < 600) && (800 < y)) { // 反色
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    if (!params->invertColors) {
                        params->invertColors = true;
                        params->isParamsChange = true;
                    }
                }
                if ((600 < x) && (x < 700) && (800 < y)) { //另存为
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    params->saveToFile = true;
                }
                if ((700 < x) && (x < 800) && (800 < y)) { // 确定按钮
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    params->confirmed = true;
                    params->isParamsChange = true;
                    cv::destroyWindow("Print Preview");
                }
            }
            }, &params);

        cv::createTrackbar("Brightness", windowsName, nullptr, 200, [](int value, void* userdata) {
            PrintParams* params = static_cast<PrintParams*>(userdata);
            params->brightness = value;
            params->isParamsChange = true;
            }, &params);
        cv::setTrackbarPos("Brightness", windowsName, params.brightness);

        cv::createTrackbar("Contrast", windowsName, nullptr, 200, [](int value, void* userdata) {
            PrintParams* params = static_cast<PrintParams*>(userdata);
            params->contrast = value;
            params->isParamsChange = true;
            }, &params);
        cv::setTrackbarPos("Contrast", windowsName, params.contrast);

        // 初始预览
        refreshPreview(&params);

        // 事件循环
        while (cv::getWindowProperty(windowsName, cv::WND_PROP_VISIBLE) > 0) {
            if (params.isParamsChange) {
                params.isParamsChange = false;
                refreshPreview(&params);
            }
            int key = cv::waitKey(30);
            if (params.confirmed) break;

            // 保存到文件
            if (params.saveToFile) {
                params.saveToFile = false;

                std::thread saveImageThread([](cv::Mat image, PrintParams params) {
                    auto path = Utils::saveImageDialog();
                    if (path.length() > 2) {
                        ApplyImageAdjustments(image, params.brightness, params.contrast, params.colorMode, params.invertColors);
                        cv::imwrite(path.c_str(), image);
                    }
                    }, image, params);
                saveImageThread.detach();
            }
        }

        // 用户取消处理
        if (!params.confirmed) return {};

        // 应用参数到原始图像
        cv::Mat finalImage = image.clone();
        ApplyImageAdjustments(finalImage, params.brightness, params.contrast, params.colorMode, params.invertColors);
        return finalImage;
    }


    // 打印图像函数
    bool PrintMatImage(const cv::Mat& image) {
        auto bgrMat = matToBGR(image);

        if (bgrMat.empty()) {
            MessageBoxW(nullptr, L"图像转换到BGR发生错误", L"错误", MB_OK | MB_ICONERROR);
            return false;
        }

        if (!Utils::limitSizeTo16K(bgrMat)) {
            MessageBoxW(nullptr, L"调整图像尺寸发生错误", L"错误", MB_OK | MB_ICONERROR);
            return false;
        }

        auto adjustMat = ImagePreprocessingForPrint(bgrMat);

        if (adjustMat.empty()) { // 取消打印
            return true;
        }

        // 初始化打印对话框
        PRINTDLGW pd{};
        pd.lStructSize = sizeof(pd);
        pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;

        // 显示打印对话框
        if (!PrintDlgW(&pd))
            return false;
        if (!pd.hDC)
            return false;

        // 准备文档信息
        DOCINFOW di{};
        di.cbSize = sizeof(di);
        di.lpszDocName = L"JarkViewer Printed Image";
        di.lpszOutput = nullptr;

        // 开始打印作业
        if (StartDocW(pd.hDC, &di) <= 0) {
            DeleteDC(pd.hDC);
            return false;
        }

        // 开始新页面
        if (StartPage(pd.hDC) <= 0) {
            EndDoc(pd.hDC);
            DeleteDC(pd.hDC);
            return false;
        }

        // 获取打印机和图像尺寸
        int pageWidth = GetDeviceCaps(pd.hDC, HORZRES);
        int pageHeight = GetDeviceCaps(pd.hDC, VERTRES);

        HBITMAP hBitmap = MatToHBITMAP(adjustMat);
        if (!hBitmap) {
            return false;
        }

        BITMAP bm{};
        GetObjectW(hBitmap, sizeof(BITMAP), &bm);
        int imgWidth = bm.bmWidth;
        int imgHeight = bm.bmHeight;

        // 计算缩放比例（保持宽高比）
        double scale = std::min(
            static_cast<double>(pageWidth) / imgWidth,
            static_cast<double>(pageHeight) / imgHeight
        );

        scale *= (pageHeight > pageWidth ? 0.893 : 0.879); // 按标准A4边距 1.0-31.8/297  1.0-2.54/210

        int scaledWidth = static_cast<int>(imgWidth * scale);
        int scaledHeight = static_cast<int>(imgHeight * scale);

        // 居中位置
        int xPos = (pageWidth - scaledWidth) / 2;
        int yPos = (pageHeight - scaledHeight) / 2;

        // 创建内存DC
        HDC hdcMem = CreateCompatibleDC(pd.hDC);
        SelectObject(hdcMem, hBitmap);

        // 高质量缩放
        SetStretchBltMode(pd.hDC, HALFTONE);
        SetBrushOrgEx(pd.hDC, 0, 0, nullptr);

        // 绘制到打印机DC
        StretchBlt(
            pd.hDC, xPos, yPos, scaledWidth, scaledHeight,
            hdcMem, 0, 0, imgWidth, imgHeight, SRCCOPY
        );

        // 清理资源
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);

        // 结束页面和文档
        EndPage(pd.hDC);
        EndDoc(pd.hDC);
        DeleteDC(pd.hDC);

        return true;
    }

};