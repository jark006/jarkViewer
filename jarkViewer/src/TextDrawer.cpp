#pragma once
#include "TextDrawer.h"

// https://github.com/nothings/stb
// 整个工程只能一个源文件定义 STB_TRUETYPE_IMPLEMENTATION， 其他地方只需include
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void TextDrawer::setLineGap(float percent) {
    lineGapPercent = percent;
}

void TextDrawer::setSize(int newSize) {
    fontSize = newSize > 2048 ? 2048 : (newSize < 16 ? 16 : newSize);

    auto newBufferSize = 2ULL * fontSize * fontSize;
    wordBuff.resize(newBufferSize);
    memset(wordBuff.data(), 0, newBufferSize);
    asciiCache.clear();
    asciiCache.resize(256);
}

// str : UTF-8
void TextDrawer::putText(cv::Mat& img, const int x, const int y, const char* str, const cv::Vec4b& color) {
    if (!hasInit) {
        Init(IDR_TTF_DEFAULT, L"TTF");
        hasInit = true;
    }
    if (scale == 0)
        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);

    int codePoint = '?';
    int xOffset = x, yOffset = y;
    const auto len = strlen(str);
    size_t i = 0;
    while (i < len)
    {
        if ((str[i] & 0x80) == 0) {
            codePoint = str[i];
            i++;
        }
        else if ((str[i] & 0xe0) == 0xc0) { // 110x'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x1f) << 6) | (str[i + 1] & 0x3f);
            i += 2;
        }
        else if ((str[i] & 0xf0) == 0xe0) { // 1110'xxxx 10xx'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x0f) << 12) | ((str[i + 1] & 0x3f) << 6) | (str[i + 2] & 0x3f);
            i += 3;
        }
        else if ((str[i] & 0xf8) == 0xf0) { // 1111'0xxx 10xx'xxxx 10xx'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x07) << 18) | ((str[i + 1] & 0x3f) << 12) | ((str[i + 2] & 0x3f) << 6) | (str[i + 3] & 0x3f);
            i += 4;
        }
        else {
            codePoint = '?';
            i++;
        }

        if (codePoint == '\n') {
            yOffset += int(fontSize * (1 + lineGapPercent));
            xOffset = x;
        }
        else {
            xOffset += putWord(img, xOffset, yOffset, codePoint, color);
        }
    }
}

//Rect {x, y, width, height}
void TextDrawer::putAlignCenter(cv::Mat& img, cv::Rect rect, const char* str, const cv::Vec4b& color) {
    int codePoint = '?';
    int H = 1, W = 0, W_cnt = 0;
    const auto len = strlen(str);
    size_t i = 0;
    while (i < len) {
        if ((str[i] & 0x80) == 0) {
            codePoint = str[i];
            i++;
        }
        else if ((str[i] & 0xe0) == 0xc0) { // 110x'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x1f) << 6) | (str[i + 1] & 0x3f);
            i += 2;
        }
        else if ((str[i] & 0xf0) == 0xe0) { // 1110'xxxx 10xx'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x0f) << 12) | ((str[i + 1] & 0x3f) << 6) | (str[i + 2] & 0x3f);
            i += 3;
        }
        else if ((str[i] & 0xf8) == 0xf0) { // 1111'0xxx 10xx'xxxx 10xx'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x07) << 18) | ((str[i + 1] & 0x3f) << 12) | ((str[i + 2] & 0x3f) << 6) | (str[i + 3] & 0x3f);
            i += 4;
        }
        else {
            codePoint = '?';
            i++;
        }

        if (codePoint == '\n') {
            H++;
            if (W_cnt > W) {
                W = W_cnt;
                W_cnt = 0;
            }
        }
        else {
            W_cnt += (codePoint < 256 ? 1 : 2);
        }
    }

    if (W_cnt > W)
        W = W_cnt;

    const int sizeAndGap = int(fontSize * (1 + lineGapPercent));// Mono Font
    H *= sizeAndGap;
    W = sizeAndGap * W / 2;

    const int x = rect.x + (rect.width - W) / 2;
    const int y = rect.y + (rect.height - H) / 2;

    putText(img, x, y, str, color);
}

//Rect {x, y, width, height}
void TextDrawer::putAlignLeft(cv::Mat& img, cv::Rect rect, const char* str, const cv::Vec4b& color) {
    if (!hasInit) {
        Init(IDR_TTF_DEFAULT, L"TTF");
        hasInit = true;
    }
    if (scale == 0)
        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);

    int codePoint = '?';
    int xOffset = rect.x, yOffset = rect.y;
    int areaWidth = rect.width;
    const auto len = strlen(str);
    size_t i = 0;
    while (i < len) {
        if ((str[i] & 0x80) == 0) {
            codePoint = str[i];
            i++;
        }
        else if ((str[i] & 0xe0) == 0xc0) { // 110x'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x1f) << 6) | (str[i + 1] & 0x3f);
            i += 2;
        }
        else if ((str[i] & 0xf0) == 0xe0) { // 1110'xxxx 10xx'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x0f) << 12) | ((str[i + 1] & 0x3f) << 6) | (str[i + 2] & 0x3f);
            i += 3;
        }
        else if ((str[i] & 0xf8) == 0xf0) { // 1111'0xxx 10xx'xxxx 10xx'xxxx 10xx'xxxx
            codePoint = ((str[i] & 0x07) << 18) | ((str[i + 1] & 0x3f) << 12) | ((str[i + 2] & 0x3f) << 6) | (str[i + 3] & 0x3f);
            i += 4;
        }
        else {
            codePoint = '?';
            i++;
        }

        if (codePoint == '\n' || (xOffset + fontSize) > (rect.x+rect.width)) {
            yOffset += int(fontSize * (1 + lineGapPercent));
            if (yOffset + fontSize > (rect.y+rect.height)) {
                rect.x += rect.width;
                yOffset = rect.y;

                if (rect.x >= img.cols) {
                    return;
                }
            }
            xOffset = rect.x;
            if (codePoint == '\n')
                continue;
        }

        xOffset += putWord(img, xOffset, yOffset, codePoint, color);
    }
}

void TextDrawer::Init(unsigned int idi, const wchar_t* type) {
    rc = jarkUtils::GetResource(idi, type);

    if (!stbtt_InitFont(&info, rc.ptr, 0)) {
        jarkUtils::log("stbtt_InitFont failed");
        if (idi != IDR_TTF_DEFAULT) {
            jarkUtils::log("Reset to IDR_TTF_DEFAULT");
            Init(IDR_TTF_DEFAULT, L"TTF");
        }
        return;
    }

    auto newBufferSize = 2ULL * fontSize * fontSize;
    wordBuff.resize(newBufferSize);
    memset(wordBuff.data(), 0, newBufferSize);
    asciiCache.clear();
    asciiCache.resize(256);
}

int TextDrawer::putWord(cv::Mat& img, int x, int y, const int codePoint, const cv::Vec4b& color) {
    int c_x0, c_y0, c_x1, c_y1;
    stbtt_GetCodepointBitmapBox(&info, codePoint, scale, scale, &c_x0, &c_y0, &c_x1, &c_y1);

    int wordWidth = c_x1 - c_x0;
    int wordHigh = c_y1 - c_y0;

    uint8_t* wordBuffPtr = nullptr;
    if (codePoint < 256) {
        if (asciiCache[codePoint].empty()) {
            asciiCache[codePoint].resize(wordBuff.size());
            stbtt_MakeCodepointBitmap(&info, asciiCache[codePoint].data(), wordWidth, wordHigh, fontSize, scale, scale, codePoint);
        }
        wordBuffPtr = asciiCache[codePoint].data();
    }
    else {
        stbtt_MakeCodepointBitmap(&info, wordBuff.data(), wordWidth, wordHigh, fontSize, scale, scale, codePoint);
        wordBuffPtr = wordBuff.data();
    }

    y += fontSize + c_y0;
    x += c_x0;

    for (int yy = 0; yy < wordHigh; yy++) {
        if (y + yy >= img.rows)
            break;

        auto ptr = (intUnion*)(img.ptr() + img.step1() * (y + yy));

        for (int xx = 0; xx < wordWidth; xx++) {
            if (x + xx >= img.cols)
                break;

            auto& orgColor = ptr[x + xx];
            int alpha = wordBuffPtr[yy * fontSize + xx] * color[3] / 255;
            if (alpha)
                orgColor = {
                    (uint8_t)((orgColor[0] * (255 - alpha) + color[0] * alpha + 255) >> 8),
                    (uint8_t)((orgColor[1] * (255 - alpha) + color[1] * alpha + 255) >> 8),
                    (uint8_t)((orgColor[2] * (255 - alpha) + color[2] * alpha + 255) >> 8),
                    255 };
        }
    }

    const int size = int(fontSize * (1 + lineGapPercent));
    return codePoint < 256 ? (size / 2) : size;
}
