#pragma once
#include "jarkUtils.h"
#include "exifParse.h"
#include "LRU.h"

#include "libraw/libraw.h"
#include "libheif/heif.h"
#include "avif/avif.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "jxl/types.h"
#include "gif_lib.h"
#include "webp/decode.h"
#include "webp/demux.h"
#include "png.h"
#include "pngstruct.h"
#include "psdsdk.h"
#include "lunasvg.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"
#include "libbpg.h"
#include "minizip/unzip.h"
#include "videoDecoder.h"

#include "stb_image.h"
#include "qoi.h"

#pragma comment(lib, "IlmImf.lib")
#pragma comment(lib, "ippiw.lib")
#pragma comment(lib, "ippicvmt.lib")
#pragma comment(lib, "libjpeg-turbo.lib")
#pragma comment(lib, "libopenjp2.lib")
#pragma comment(lib, "libpng.lib")
#pragma comment(lib, "libprotobuf.lib")
#pragma comment(lib, "libtiff.lib")
#pragma comment(lib, "libwebp.lib")
#pragma comment(lib, "opencv_world4100.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "ittnotify.lib")
#pragma comment(lib, "heif.lib")
#pragma comment(lib, "libde265.lib")
#pragma comment(lib, "x265-static.lib")

// avif
#pragma comment(lib, "avif.lib")
#pragma comment(lib, "yuv.lib")
#pragma comment(lib, "dav1d.lib")
#pragma comment(lib, "aom.lib")

#pragma comment(lib, "gif.lib")
#pragma comment(lib, "raw.lib")
#pragma comment(lib, "OpenGL32.Lib")
#pragma comment(lib, "GlU32.Lib")
#pragma comment(lib, "freeglut.lib")
#pragma comment(lib, "lcms2.lib")
#pragma comment(lib, "jasper.lib")
#pragma comment(lib, "brotlicommon.lib")
#pragma comment(lib, "brotlidec.lib")
#pragma comment(lib, "brotlienc.lib")
#pragma comment(lib, "charset.lib")
#pragma comment(lib, "exiv2.lib")
#pragma comment(lib, "iconv.lib")
#pragma comment(lib, "inih.lib")
#pragma comment(lib, "INIReader.lib")
#pragma comment(lib, "intl.lib")
#pragma comment(lib, "libexpatMT.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "jxl.lib")
#pragma comment(lib, "jxl_cms.lib")
#pragma comment(lib, "jxl_threads.lib")
#pragma comment(lib, "hwy.lib")
#pragma comment(lib, "libpng16.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "FreeImagePlus.lib")
#pragma comment(lib, "pixman-1.lib")
#pragma comment(lib, "fontconfig.lib")
#pragma comment(lib, "Psd_MT.lib")
#pragma comment(lib, "lunasvg.lib")
#pragma comment(lib, "plutovg.lib")
#pragma comment(lib, "webp2.lib")
#pragma comment(lib, "imageio.lib")
#pragma comment(lib, "minizip.lib")
#pragma comment(lib, "bz2.lib")

// meson setup builddir --backend=vs --buildtype release -Dloaders="all" -Dtools="all"
//#pragma comment(lib, "thorvg-0.lib")

// .\gswin64c.exe -dNOPAUSE -dBATCH -sDEVICE=png16m -r300 -sOutputFile=d:\aa.png "D:\Downloads\test\perth.eps"

class ImageDatabase :public LRU<wstring, Frames> {
public:

    static inline const unordered_set<wstring> supportExt{
        L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png", L".apng",
        L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
        L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
        L".heic", L".heif", L".avif", L".avifs", L".gif", L".jxl",
        L".ico", L".icon", L".psd", L".tga", L".svg", L".jfif",
        L".jxr", L".wp2", L".pfm", L".bpg", L".livp", L".qoi",
    };

    static inline const unordered_set<wstring> supportRaw{
        L".crw", L".cr2", L".cr3", // Canon
        L".arw", L".srf", L".sr2", // Sony
        L".raw", L".dng", // Leica
        L".nef", // Nikon
        L".pef", // Pentax
        L".orf", // Olympus
        L".rw2", // Panasonic
        L".raf", // Fujifilm
        L".kdc", // Kodak
        L".x3f", // Sigma
        L".mrw", // Minolta
        L".3fr", // Hasselblad
        L".ari", // ARRIRAW
        L".bay", // Casio
        L".cap", // Phase One
        L".dcr", // Kodak
        L".dcs", // Kodak
        L".drf", // DNG+
        L".eip", // Enhanced Image Package, Phase One
        L".erf", // Epson
        L".fff", // Imacon/Hasselblad
        L".gpr", // GoPro
        L".iiq", // Phase One
        L".k25", // Kodak
        L".mdc", // Minolta
        L".mef", // Mamiya
        L".mos", // Leaf
        L".nrw", // Nikon
        L".ptx", // Pentax
        L".r3d", // Red Digital Cinema
        L".rwl", // Leica
        L".rwz", // Leica
        L".srw", // Samsung
    };

    ImageDatabase() {
        setCapacity(5);
    }

    cv::Mat errorTipsMat, homeMat;

    cv::Mat& getErrorTipsMat() {
        if (errorTipsMat.empty()) {
            auto rc = jarkUtils::GetResource(IDB_PNG_TIPS, L"PNG");
            cv::Mat imgData(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.ptr);
            errorTipsMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
        }
        return errorTipsMat;
    }


    cv::Mat& getHomeMat() {
        if (homeMat.empty()) {
            auto rc = jarkUtils::GetResource(IDB_PNG_HOME, L"PNG");
            cv::Mat imgData(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.ptr);
            homeMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
        }
        return homeMat;
    }


    static uint32_t swap_endian(uint32_t value) {
        return (value >> 24) |
            ((value >> 8) & 0x0000FF00) |
            ((value << 8) & 0x00FF0000) |
            (value << 24);
    }


    const std::string jxlStatusCode2String(JxlDecoderStatus status);
    std::vector<ImageNode> loadJXL(const wstring& path, const vector<uchar>& buf);
    std::string statusExplain(WP2Status status);
    std::vector<ImageNode> loadWP2(const wstring& path, const std::vector<uint8_t>& buf);
    std::vector<ImageNode> loadBPG(const std::wstring& path, const std::vector<uchar>& buf);
    std::vector<ImageNode> loadGif(const wstring& path, const vector<uchar>& buf);
    std::vector<ImageNode> loadWebp(const wstring& path, const std::vector<uint8_t>& buf);
    std::vector<ImageNode> loadApng(const std::wstring& path, const std::vector<uint8_t>& buf);


    // HEIC ONLY, AVIF not support
    // https://github.com/strukturag/libheif
    // vcpkg install libheif:x64-windows-static
    // vcpkg install libheif[hevc]:x64-windows-static
    cv::Mat loadHeic(const wstring& path, const vector<uchar>& buf);

    // vcpkg install libavif[core,aom,dav1d]:x64-windows-static
    // https://github.com/AOMediaCodec/libavif/issues/1451#issuecomment-1606903425
    // TODO 部分图像仍不能正常解码
    cv::Mat loadAvif(const wstring& path, const vector<uchar>& buf);
    cv::Mat loadRaw(const wstring& path, const vector<uchar>& buf);


    struct IconDirEntry {
        uint8_t width;
        uint8_t height;
        uint8_t colorCount;
        uint8_t reserved;
        uint16_t planes;
        uint16_t bitsPerPixel;
        uint32_t dataSize;
        uint32_t dataOffset;
    };

    struct DibHeader {
        uint32_t headerSize;
        int32_t width;
        int32_t height;
        uint16_t planes;
        uint16_t bitCount;
        uint32_t compression;
        uint32_t imageSize;
        int32_t xPelsPerMeter;
        int32_t yPelsPerMeter;
        uint32_t clrUsed;
        uint32_t clrImportant;
    };

    cv::Mat readDibFromMemory(const uint8_t* data, size_t size);

    // https://github.com/corkami/pics/blob/master/binary/ico_bmp.png
    std::tuple<cv::Mat, string> loadICO(const wstring& path, const vector<uchar>& buf);


    template <typename T, typename DataHolder>
    void* ExpandChannelToCanvas(psd::Allocator* allocator, const DataHolder* layer, const void* data, unsigned int canvasWidth, unsigned int canvasHeight)
    {
        T* canvasData = static_cast<T*>(allocator->Allocate(sizeof(T) * canvasWidth * canvasHeight, 16u));
        memset(canvasData, 0u, sizeof(T) * canvasWidth * canvasHeight);

        psd::imageUtil::CopyLayerData(static_cast<const T*>(data), canvasData, layer->left, layer->top, layer->right, layer->bottom, canvasWidth, canvasHeight);

        return canvasData;
    }


    void* ExpandChannelToCanvas(const psd::Document* document, psd::Allocator* allocator, psd::Layer* layer, psd::Channel* channel)
    {
        if (document->bitsPerChannel == 8)
            return ExpandChannelToCanvas<uint8_t>(allocator, layer, channel->data, document->width, document->height);
        else if (document->bitsPerChannel == 16)
            return ExpandChannelToCanvas<uint16_t>(allocator, layer, channel->data, document->width, document->height);
        else if (document->bitsPerChannel == 32)
            return ExpandChannelToCanvas<float32_t>(allocator, layer, channel->data, document->width, document->height);

        return nullptr;
    }


    template <typename T>
    void* ExpandMaskToCanvas(const psd::Document* document, psd::Allocator* allocator, T* mask)
    {
        if (document->bitsPerChannel == 8)
            return ExpandChannelToCanvas<uint8_t>(allocator, mask, mask->data, document->width, document->height);
        else if (document->bitsPerChannel == 16)
            return ExpandChannelToCanvas<uint16_t>(allocator, mask, mask->data, document->width, document->height);
        else if (document->bitsPerChannel == 32)
            return ExpandChannelToCanvas<float32_t>(allocator, mask, mask->data, document->width, document->height);

        return nullptr;
    }


    unsigned int FindChannel(psd::Layer* layer, int16_t channelType) {
        const int32_t CHANNEL_NOT_FOUND = UINT_MAX;

        for (unsigned int i = 0; i < layer->channelCount; ++i)
        {
            psd::Channel* channel = &layer->channels[i];
            if (channel->data && channel->type == channelType)
                return i;
        }

        return CHANNEL_NOT_FOUND;
    }

    template <typename T>
    struct TmpValue;

    // uint8_t的特化
    template <>
    struct TmpValue<uint8_t> {
        static constexpr uint8_t alphaMax = 255;
    };

    // uint16_t的特化
    template <>
    struct TmpValue<uint16_t> {
        static constexpr uint16_t alphaMax = 65535;
    };

    // float的特化
    template <>
    struct TmpValue<float> {
        static constexpr float alphaMax = 1.0f;
    };

    template <typename T>
    T* CreateInterleavedImage(psd::Allocator* allocator, const void* srcR, const void* srcG, const void* srcB, unsigned int width, unsigned int height)
    {
        T* image = static_cast<T*>(allocator->Allocate(4ULL * width * height * sizeof(T), 16u));

        const T* r = static_cast<const T*>(srcR);
        const T* g = static_cast<const T*>(srcG);
        const T* b = static_cast<const T*>(srcB);
        psd::imageUtil::InterleaveRGB(b, g, r, TmpValue<T>::alphaMax, image, width, height); // RGB -> BGR

        return image;
    }


    template <typename T>
    T* CreateInterleavedImage(psd::Allocator* allocator, const void* srcR, const void* srcG, const void* srcB, const void* srcA, unsigned int width, unsigned int height)
    {
        T* image = static_cast<T*>(allocator->Allocate(4ULL * width * height * sizeof(T), 16u));

        const T* r = static_cast<const T*>(srcR);
        const T* g = static_cast<const T*>(srcG);
        const T* b = static_cast<const T*>(srcB);
        const T* a = static_cast<const T*>(srcA);
        psd::imageUtil::InterleaveRGBA(b, g, r, a, image, width, height); // RGB -> BGR

        return image;
    }


    // https://github.com/MolecularMatters/psd_sdk
    cv::Mat loadPSD(const wstring& path, const vector<uchar>& buf);
    cv::Mat loadTGA_HDR(const wstring& path, const vector<uchar>& buf);
    cv::Mat loadSVG(const wstring& path, const vector<uchar>& buf);
    cv::Mat loadJXR(const wstring& path, const vector<uchar>& buf);
    cv::Mat loadMat(const wstring& path, const vector<uchar>& buf);
    bool parsePFMHeader(const vector<uchar>& buf, int& width, int& height, float& scaleFactor, bool& isColor, size_t& dataOffset);
    cv::Mat loadPFM(const wstring& path, const vector<uchar>& buf);
    cv::Mat loadQOI(const wstring& path, const vector<uchar>& buf);

    std::pair<std::vector<uint8_t>, std::string> unzipLivp(std::vector<uint8_t>& livpFileBuff);

    vector<cv::Mat> loadMats(const wstring& path, const vector<uchar>& buf);

    struct PngSource {
        const uint8_t* data;
        size_t size;
        int    offset;
    };


    void handleExifOrientation(int orientation, cv::Mat& img);
    Frames loader(const wstring& path);
};
