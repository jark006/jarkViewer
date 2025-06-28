#pragma once
#include "jarkUtils.h"

// https://github.com/nothings/stb
// 整个工程只能一个源文件定义 STB_TRUETYPE_IMPLEMENTATION， 其他地方只需include
#include "stb_truetype.h"


class TextDrawer {
public:
    const uint32_t IDR_TTF_DEFAULT = IDR_MSYHMONO_TTF;
    std::vector<std::vector<uint8_t>> asciiCache;

    TextDrawer() {}
    ~TextDrawer() {}

    void setLineGap(float percent);
    void setSize(int newSize);

    // str : UTF-8
    void putText(cv::Mat& img, const int x, const int y, const char* str, const cv::Vec4b& color);

    //Rect {x, y, width, height}
    void putAlignCenter(cv::Mat& img, cv::Rect rect, const char* str, const cv::Vec4b& color);

    //Rect {x, y, width, height}
    void putAlignLeft(cv::Mat& img, cv::Rect rect, const char* str, const cv::Vec4b& color);

private:
    bool hasInit = false;
    float scale = 0;
    float lineGapPercent = 0.1f;
    stbtt_fontinfo info{};

    int fontSize = 16;

    vector<uint8_t> wordBuff;
    vector<uint8_t> fontFileBuffer;

    rcFileInfo rc;

    void Init(unsigned int idi, const wchar_t* type);
    int putWord(cv::Mat& img, int x, int y, const int codePoint, const cv::Vec4b& color);
};