#pragma once

// https://github.com/nothings/stb
#include "Utils.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"


class stbText {
public:
    const uint32_t IDR_TTF_DEFAULT = IDR_MSYHMONO_TTF;

    stbText(const wchar_t* filePath) {
        FILE* fontFile = _wfopen(filePath, L"rb");
        if (fontFile == nullptr) {
            Utils::log("Can not open font file!");
            Init(IDR_TTF_DEFAULT, L"TTF");
            return;
        }
        fseek(fontFile, 0, SEEK_END);
        auto fileSize = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);

        fontFileBuffer.resize(fileSize);

        fread(fontFileBuffer.data(), 1, fileSize, fontFile);
        fclose(fontFile);

        if (!stbtt_InitFont(&info, fontFileBuffer.data(), 0)) {
            Utils::log("stb init font failed");
            Init(IDR_TTF_DEFAULT, L"TTF");
            return;
        }

        auto newBufferSize = 2ULL * fontSize * fontSize;
        wordBuff.resize(newBufferSize);
        memset(wordBuff.data(), 0, newBufferSize);

        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    }

    stbText() {
        Init(IDR_TTF_DEFAULT, L"TTF");
    }

    stbText(unsigned int idi, const wchar_t* type) {
        Init(idi, type);
    }

    ~stbText() {}

    void setLineGap(float percent) {
        lineGapPercent = percent;
    }

    void setSize(int newSize) {
        fontSize = newSize > 2048 ? 2048 : newSize;

        auto newBufferSize = 2ULL * fontSize * fontSize;
        wordBuff.resize(newBufferSize);
        memset(wordBuff.data(), 0, newBufferSize);

        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    }


    void saveTest(const char* pngPath, int code) {
        stbtt_MakeCodepointBitmap(&info, wordBuff.data(), fontSize, fontSize, fontSize, scale, scale, code);
        stbi_write_png(pngPath, fontSize, fontSize, 1, wordBuff.data(), fontSize);
    }

    // str : UTF-8
    void putText(cv::Mat& img, const int x, const int y, const char* str, const cv::Vec4b& color) {
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


    void putAlignCenter(cv::Mat& img, RECT r, const char* str, const cv::Vec4b& color) {
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
        H *= fontSize;
        W = sizeAndGap * W / 2;

        const int x = r.left + (r.right - r.left - W) / 2;
        const int y = r.top + (r.bottom - r.top - H) / 2;

        putText(img, x, y, str, color);
    }

    void putAlignLeft(cv::Mat& img, RECT r, const char* str, const cv::Vec4b& color) {
        int codePoint = '?';
        int xOffset = r.left, yOffset = r.top;
        int areaWidth = r.right - r.left;
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

            if (codePoint == '\n' || (xOffset + fontSize) > r.right) {
                yOffset += int(fontSize * (1 + lineGapPercent));
                if (yOffset + fontSize > r.bottom) {
                    r.left += areaWidth;
                    r.right += areaWidth;
                    yOffset = r.top;
                }
                xOffset = r.left;
                if (codePoint == '\n')
                    continue;
            }

            xOffset += putWord(img, xOffset, yOffset, codePoint, color);
        }
    }

private:

    float scale = 0.1f;
    float lineGapPercent = 0.1f;
    stbtt_fontinfo info{};

    int fontSize = 16;

    vector<uint8_t> wordBuff;
    vector<uint8_t> fontFileBuffer;

    rcFileInfo rc;

    void Init(unsigned int idi, const wchar_t* type) {
        rc = Utils::GetResource(idi, type);

        if (!stbtt_InitFont(&info, rc.addr, 0)) {
            Utils::log("stb init font failed");
            Init(IDR_TTF_DEFAULT, L"TTF");
            return;
        }

        auto newBufferSize = 2ULL * fontSize * fontSize;
        wordBuff.resize(newBufferSize);
        memset(wordBuff.data(), 0, newBufferSize);

        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    }

    int putWord(cv::Mat& img, int x, int y, const int codePoint, const cv::Vec4b& color) {
        int c_x0, c_y0, c_x1, c_y1;
        stbtt_GetCodepointBitmapBox(&info, codePoint, scale, scale, &c_x0, &c_y0, &c_x1, &c_y1);

        int wordWidth = c_x1 - c_x0;
        int wordHigh = c_y1 - c_y0;
        stbtt_MakeCodepointBitmap(&info, wordBuff.data(), wordWidth, wordHigh, fontSize, scale, scale, codePoint);

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
                int alpha = wordBuff[yy * fontSize + xx] * color[3] / 255;
                if (alpha)
                    orgColor = {
                        (uint8_t)((orgColor[0] * (255 - alpha) + color[0] * alpha) / 255),
                        (uint8_t)((orgColor[1] * (255 - alpha) + color[1] * alpha) / 255),
                        (uint8_t)((orgColor[2] * (255 - alpha) + color[2] * alpha) / 255),
                        255 };
            }
        }

        const int size = int(fontSize * (1 + lineGapPercent));
        return codePoint < 128 ? (size / 2) : size;
    }
};