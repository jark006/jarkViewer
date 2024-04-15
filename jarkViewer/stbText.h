#pragma once

//————————————————
//版权声明：本文为CSDN博主「天上下橙雨」的原创文章，遵循CC 4.0 BY - SA版权协议，转载请附上原文出处链接及本声明。
//原文链接：https://blog.csdn.net/weixin_40026797/article/details/112755090

#include "Utils.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" /* http://nothings.org/stb/stb_image_write.h */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h" /* http://nothings.org/stb/stb_truetype.h */


class stbText {
public:
    //const uint32_t IDR_TTF_DEFAULT = IDR_XIHEI_TTF;
    const uint32_t IDR_TTF_DEFAULT = IDR_MSYHMONO_TTF;

    stbText(const char* filePath) {
        FILE* fontFile = fopen(filePath, "rb");
        if (fontFile == nullptr)
        {
            Utils::log("Can not open font file!");
            Init(IDR_TTF_DEFAULT, L"TTF");
            return;
        }
        fseek(fontFile, 0, SEEK_END);
        auto fileSize = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);

        fontBuffer = (uint8_t*)malloc(fileSize);
        if (fontBuffer == nullptr) {
            Utils::log("Can not malloc fontBuffer!");
            Init(IDR_TTF_DEFAULT, L"TTF");
            return;
        }

        fread(fontBuffer, 1, fileSize, fontFile);
        fclose(fontFile);

        /* 初始化字体 */
        if (!stbtt_InitFont(&info, fontBuffer, 0)) {
            Utils::log("stb init font failed");
            Init(IDR_TTF_DEFAULT, L"TTF");
            return;
        }

        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    }

    stbText() {
        Init(IDR_TTF_DEFAULT, L"TTF");
    }

    // 需要把字体文件加到资源文件里
    stbText(unsigned int idi, const wchar_t* type) {
        Init(idi, type);
    }

    ~stbText() {
        if (wordBuff && wordBuff != wordBuffDefault)
            free(wordBuff);

        if (fontBuffer)
            free(fontBuffer);
    }

    void setLineGap(float percent) {
        lineGapPercent = percent;
    }

    void setSize(int newSize) {
        if (wordBuff && wordBuff != wordBuffDefault)
            free(wordBuff);

        if (newSize <= fontSize) {
            fontSize = newSize;
            wordBuff = wordBuffDefault;
        }
        else {
            if (newSize > 2048)
                newSize = 2048;

            wordBuff = (uint8_t*)malloc((size_t)newSize * newSize * 2);
            if (wordBuff) {
                fontSize = newSize;
            }
            else {
                fontSize = fontSizeDefault;
                wordBuff = wordBuffDefault;
            }
        }

        memset(wordBuff, 0, (size_t)fontSize * fontSize * 2);

        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    }


    void saveTest(const char* pngPath, int code) {
        stbtt_MakeCodepointBitmap(&info, wordBuff, fontSize, fontSize, fontSize, scale, scale, code);
        stbi_write_png(pngPath, fontSize, fontSize, 1, wordBuff, fontSize);
    }

    // str : UTF-8
    void putText(cv::Mat& img, const int x, const int y, const char* str, const cv::Scalar& color) {
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
                yOffset += int(fontSizeDefault * (1 + lineGapPercent));
                xOffset = x;
            }
            else {
                xOffset += putWord(img, xOffset, yOffset, codePoint, color);
            }
        }
    }

private:

    const static int fontSizeDefault = 24;
    uint8_t wordBuffDefault[fontSizeDefault * fontSizeDefault * 2] = { 0 };

    float scale = 0.1f;
    float lineGapPercent = 0.1f; // 左右间距 字符宽度的百分比
    stbtt_fontinfo info{};

    int fontSize = fontSizeDefault;
    uint8_t* wordBuff = wordBuffDefault;
    uint8_t* fontBuffer = nullptr;

    rcFileInfo rc;

    void Init(unsigned int idi, const wchar_t* type) {
        rc = Utils::GetResource(idi, type);

        /* 初始化字体 */
        if (!stbtt_InitFont(&info, rc.addr, 0))
            Utils::log("stb init font failed");

        scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    }

    int putWord(cv::Mat& img, int x, int y, const int codePoint, const cv::Scalar& color) {
        int c_x0, c_y0, c_x1, c_y1;
        stbtt_GetCodepointBitmapBox(&info, codePoint, scale, scale, &c_x0, &c_y0, &c_x1, &c_y1);

        int wordWidth = c_x1 - c_x0;
        int wordHigh = c_y1 - c_y0;
        stbtt_MakeCodepointBitmap(&info, wordBuff, wordWidth, wordHigh, fontSize, scale, scale, codePoint);

        y += fontSize + c_y0;
        x += c_x0;

        for (int yy = 0; yy < wordHigh; yy++) {
            if (y + yy >= img.rows)break;
            for (int xx = 0; xx < wordWidth; xx++) {
                if (x + xx >= img.cols)break;
                auto& orgColor = img.at<cv::Vec4b>(y + yy, x + xx);
                auto alpha = wordBuff[yy * fontSize + xx] * orgColor[3] * color[3] / (256 * 256 * 256.0);

                orgColor = {
                    (uint8_t)(orgColor[0] * (1 - alpha) + color[0] * alpha),
                    (uint8_t)(orgColor[1] * (1 - alpha) + color[1] * alpha),
                    (uint8_t)(orgColor[2] * (1 - alpha) + color[2] * alpha),
                    255
                };
            }
        }
        //return int((wordWidth <= (fontSize / 3) ? (fontSize / 3) : wordWidth) * (1+lineGapPercent)); //不等宽字体

        const int size = int(fontSize * (1 + lineGapPercent));//等宽字体
        return codePoint < 128 ? (size / 2) : size;
    }
};