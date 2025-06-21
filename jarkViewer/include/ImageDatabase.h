#pragma once
#include "Utils.h"
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

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif // !STB_IMAGE_IMPLEMENTATION

#ifndef QOI_IMPLEMENTATION
#define QOI_IMPLEMENTATION
#include "qoi.h"
#endif

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

class ImageDatabase:public LRU<wstring, Frames> {
public:

    static inline const unordered_set<wstring> supportExt{
        L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png", L".apng",
        L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
        L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
        L".heic", L".heif", L".avif", L".avifs", L".gif", L".jxl",
        L".ico", L".icon", L".psd", L".tga", L".svg", L".jfif",
        L".jxr", L".wp2", L".pfm", L".bpg", L".livp", L".qoi",
    };

    static inline const unordered_set<wstring> supportRaw {
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
        setCapacity(3);
    }

    cv::Mat errorTipsMat, homeMat;

    cv::Mat& getErrorTipsMat() {
        if (errorTipsMat.empty()) {
            auto rc = Utils::GetResource(IDB_PNG, L"PNG");
            cv::Mat imgData(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
            errorTipsMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
        }
        return errorTipsMat;
    }

    cv::Mat& getHomeMat() {
        if (homeMat.empty()) {
            auto rc = Utils::GetResource(IDB_PNG1, L"PNG");
            cv::Mat imgData(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
            homeMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
        }
        return homeMat;
    }

    // HEIC ONLY, AVIF not support
    // https://github.com/strukturag/libheif
    // vcpkg install libheif:x64-windows-static
    // vcpkg install libheif[hevc]:x64-windows-static
    cv::Mat loadHeic(const wstring& path, const vector<uchar>& buf) {
        if (buf.empty())
            return cv::Mat();

        auto exifStr = std::format("路径: {}\n大小: {}",
            Utils::wstringToUtf8(path), Utils::size2Str(buf.size()));

        auto filetype_check = heif_check_filetype(buf.data(), 12);
        if (filetype_check == heif_filetype_no) {
            Utils::log("Input file is not an HEIF/AVIF file: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        if (filetype_check == heif_filetype_yes_unsupported) {
            Utils::log("Input file is an unsupported HEIF/AVIF file type: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        heif_context* ctx = heif_context_alloc();
        auto err = heif_context_read_from_memory_without_copy(ctx, buf.data(), buf.size(), nullptr);
        if (err.code) {
            Utils::log("heif_context_read_from_memory_without_copy error: {} {}", Utils::wstringToUtf8(path), err.message);
            return cv::Mat();
        }

        // get a handle to the primary image
        heif_image_handle* handle = nullptr;
        err = heif_context_get_primary_image_handle(ctx, &handle);
        if (err.code) {
            Utils::log("heif_context_get_primary_image_handle error: {} {}", Utils::wstringToUtf8(path), err.message);
            if (ctx) heif_context_free(ctx);
            if (handle) heif_image_handle_release(handle);
            return cv::Mat();
        }

        // decode the image and convert colorspace to RGB, saved as 24bit interleaved
        heif_image* img = nullptr;
        err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
        if (err.code) {
            Utils::log("Error: {}", Utils::wstringToUtf8(path));
            Utils::log("heif_decode_image error: {}", err.message);
            if (ctx) heif_context_free(ctx);
            if (handle) heif_image_handle_release(handle);
            if (img) heif_image_release(img);
            return cv::Mat();
        }

        int stride = 0;
        const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

        auto width = heif_image_handle_get_width(handle);
        auto height = heif_image_handle_get_height(handle);

        cv::Mat matImg;
        cv::cvtColor(cv::Mat(height, width, CV_8UC4, (uint8_t*)data, stride), matImg, cv::COLOR_RGBA2BGRA);

        // clean up resources
        heif_context_free(ctx);
        heif_image_release(img);
        heif_image_handle_release(handle);

        return matImg;
    }

    // vcpkg install libavif[core,aom,dav1d]:x64-windows-static
    // https://github.com/AOMediaCodec/libavif/issues/1451#issuecomment-1606903425
    // TODO 部分图像仍不能正常解码
    cv::Mat loadAvif(const wstring& path, const vector<uchar>& buf) {
        avifImage* image = avifImageCreateEmpty();
        if (image == nullptr) {
            Utils::log("avifImageCreateEmpty failure: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        avifDecoder* decoder = avifDecoderCreate();
        if (decoder == nullptr) {
            Utils::log("avifDecoderCreate failure: {}", Utils::wstringToUtf8(path));
            avifImageDestroy(image);
            return cv::Mat();
        }

        decoder->strictFlags = AVIF_STRICT_DISABLED;  // 严格模式下，老旧的不标准格式会解码失败

        avifResult result = avifDecoderReadMemory(decoder, image, buf.data(), buf.size());
        if (result != AVIF_RESULT_OK) {
            Utils::log("avifDecoderReadMemory failure: {} {}", Utils::wstringToUtf8(path), avifResultToString(result));
            avifImageDestroy(image);
            avifDecoderDestroy(decoder);
            return cv::Mat();
        }

        avifRGBImage rgb;
        avifRGBImageSetDefaults(&rgb, image);
        result = avifRGBImageAllocatePixels(&rgb);
        if (result != AVIF_RESULT_OK) {
            Utils::log("avifRGBImageAllocatePixels failure: {} {}", Utils::wstringToUtf8(path), avifResultToString(result));
            avifImageDestroy(image);
            avifDecoderDestroy(decoder);
            return cv::Mat();
        }

        rgb.format = AVIF_RGB_FORMAT_BGRA; // OpenCV is BGRA
        result = avifImageYUVToRGB(image, &rgb);
        if (result != AVIF_RESULT_OK) {
            Utils::log("avifImageYUVToRGB failure: {} {}", Utils::wstringToUtf8(path), avifResultToString(result));
            avifImageDestroy(image);
            avifDecoderDestroy(decoder);
            avifRGBImageFreePixels(&rgb);
            return cv::Mat();
        }

        avifImageDestroy(image);
        avifDecoderDestroy(decoder);

        auto ret = cv::Mat(rgb.height, rgb.width, CV_8UC4);
        if (rgb.depth == 8) {
            if (rgb.rowBytes == ret.step1() && rgb.rowBytes == rgb.width * 4) {
                memcpy(ret.ptr(), rgb.pixels, (size_t)rgb.width * rgb.height * 4);
            }
            else {
                size_t minStep = rgb.rowBytes < ret.step1() ? rgb.rowBytes : ret.step1();
                for (uint32_t y = 0; y < rgb.height; y++) {
                    memcpy(ret.ptr() + ret.step1() * y, rgb.pixels + rgb.rowBytes * y, minStep);
                }
            }
        }
        else {
            int bitShift = 2;
            switch (rgb.depth) {
            case 10: bitShift = 2; break;
            case 12: bitShift = 4; break;
            case 16: bitShift = 8; break;
            }

            for (uint32_t y = 0; y < rgb.height; y++) {
                const uint16_t* src = (uint16_t*)(rgb.pixels + rgb.rowBytes * y);
                uint8_t* dst = (uint8_t*)(ret.ptr() + ret.step1() * y);
                for (uint32_t x = 0; x < rgb.width * 4; x++) {
                    dst[x] = (uint8_t)(src[x] >> bitShift);
                }
            }
        }

        avifRGBImageFreePixels(&rgb);
        return ret;
    }

    cv::Mat loadRaw(const wstring& path, const vector<uchar>& buf) {
        if (buf.empty()) {
            Utils::log("Buf is empty: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        auto rawProcessor = std::make_unique<LibRaw>();

        int ret = rawProcessor->open_buffer(buf.data(), buf.size());
        if (ret != LIBRAW_SUCCESS) {
            Utils::log("Cannot open RAW file: {} {}", Utils::wstringToUtf8(path), libraw_strerror(ret));
            return cv::Mat();
        }

        ret = rawProcessor->unpack();
        if (ret != LIBRAW_SUCCESS) {
            Utils::log("Cannot unpack RAW file: {} {}", Utils::wstringToUtf8(path), libraw_strerror(ret));
            return cv::Mat();
        }

        ret = rawProcessor->dcraw_process();
        if (ret != LIBRAW_SUCCESS) {
            Utils::log("Cannot process RAW file: {} {}", Utils::wstringToUtf8(path), libraw_strerror(ret));
            return cv::Mat();
        }

        libraw_processed_image_t* image = rawProcessor->dcraw_make_mem_image(&ret);
        if (image == nullptr) {
            Utils::log("Cannot make image from RAW data: {} {}", Utils::wstringToUtf8(path), libraw_strerror(ret));
            return cv::Mat();
        }

        cv::Mat retImg;
        cv::cvtColor(cv::Mat(image->height, image->width, CV_8UC3, image->data), retImg, cv::COLOR_RGB2BGRA);

        // Clean up
        LibRaw::dcraw_clear_mem(image);
        rawProcessor->recycle();

        return retImg;
    }

    const std::string jxlStatusCode2String(JxlDecoderStatus status) {
        switch (status) {
        case JXL_DEC_SUCCESS:
            return "Success: Function call finished successfully or decoding is finished.";
        case JXL_DEC_ERROR:
            return "Error: An error occurred, e.g. invalid input file or out of memory.";
        case JXL_DEC_NEED_MORE_INPUT:
            return "Need more input: The decoder needs more input bytes to continue.";
        case JXL_DEC_NEED_PREVIEW_OUT_BUFFER:
            return "Need preview output buffer: The decoder requests setting a preview output buffer.";
        case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
            return "Need image output buffer: The decoder requests an output buffer for the full resolution image.";
        case JXL_DEC_JPEG_NEED_MORE_OUTPUT:
            return "JPEG needs more output: The JPEG reconstruction buffer is too small.";
        case JXL_DEC_BOX_NEED_MORE_OUTPUT:
            return "Box needs more output: The box contents output buffer is too small.";
        case JXL_DEC_BASIC_INFO:
            return "Basic info: Basic information such as image dimensions and extra channels is available.";
        case JXL_DEC_COLOR_ENCODING:
            return "Color encoding: Color encoding or ICC profile from the codestream header is available.";
        case JXL_DEC_PREVIEW_IMAGE:
            return "Preview image: A small preview frame has been decoded.";
        case JXL_DEC_FRAME:
            return "Frame: Beginning of a frame, frame header information is available.";
        case JXL_DEC_FULL_IMAGE:
            return "Full image: A full frame or layer has been decoded.";
        case JXL_DEC_JPEG_RECONSTRUCTION:
            return "JPEG reconstruction: JPEG reconstruction data is decoded.";
        case JXL_DEC_BOX:
            return "Box: The header of a box in the container format (BMFF) is decoded.";
        case JXL_DEC_FRAME_PROGRESSION:
            return "Frame progression: A progressive step in decoding the frame is reached.";
        default:
            return "Unknown status code.";
        }
    }


    std::vector<ImageNode> loadJXL(const wstring& path, const vector<uchar>& buf) {
        std::vector<ImageNode> frames;
        cv::Mat mat;

        // Multi-threaded parallel runner.
        auto runner = JxlResizableParallelRunnerMake(nullptr);

        auto dec = JxlDecoderMake(nullptr);
        JxlDecoderStatus status = JxlDecoderSubscribeEvents(dec.get(),
            JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE);
        if (JXL_DEC_SUCCESS != status) {
            Utils::log("JxlDecoderSubscribeEvents failed\n{}\n{}",
                Utils::wstringToUtf8(path),
                jxlStatusCode2String(status));
            return frames;
        }

        status = JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner, runner.get());
        if (JXL_DEC_SUCCESS != status) {
            Utils::log("JxlDecoderSetParallelRunner failed\n{}\n{}",
                Utils::wstringToUtf8(path),
                jxlStatusCode2String(status));
            return frames;
        }

        JxlBasicInfo info{};
        JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

        JxlDecoderSetInput(dec.get(), buf.data(), buf.size());
        JxlDecoderCloseInput(dec.get());

        int duration_ms = 0;
        for (;;) {
            status = JxlDecoderProcessInput(dec.get());

            if (status == JXL_DEC_ERROR) {
                Utils::log("Decoder error\n{}\n{}",
                    Utils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return frames;
            }
            else if (status == JXL_DEC_NEED_MORE_INPUT) {
                Utils::log("Error, already provided all input\n{}\n{}",
                    Utils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return frames;
            }
            else if (status == JXL_DEC_BASIC_INFO) {
                if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                    Utils::log("JxlDecoderGetBasicInfo failed\n{}\n{}",
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return frames;
                }

                duration_ms = info.animation.tps_numerator == 0 ? 0 : (info.animation.tps_denominator * 1000 / info.animation.tps_numerator);
                JxlResizableParallelRunnerSetThreads(
                    runner.get(),
                    JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
            }
            else if (status == JXL_DEC_COLOR_ENCODING) {
                // Get the ICC color profile of the pixel data
                //size_t icc_size;
                //if (JXL_DEC_SUCCESS !=
                //    JxlDecoderGetICCProfileSize(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                //        &icc_size)) {
                //    Utils::log("JxlDecoderGetICCProfileSize failed\n{}\n{}",
                //        Utils::wstringToUtf8(path),
                //        jxlStatusCode2String(status));
                //    return mat;
                //}
                //icc_profile.resize(icc_size);
                //if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                //    dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                //    icc_profile.data(), icc_profile.size())) {
                //    Utils::log("JxlDecoderGetColorAsICCProfile failed\n{}\n{}",
                //        Utils::wstringToUtf8(path),
                //        jxlStatusCode2String(status));
                //    return mat;
                //}
            }
            else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
                size_t buffer_size;

                status = JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size);
                if (JXL_DEC_SUCCESS != status) {
                    Utils::log("JxlDecoderImageOutBufferSize failed\n{}\n{}",
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return frames;
                }
                auto byteSizeRequire = 4ULL * info.xsize * info.ysize;
                if (buffer_size != byteSizeRequire) {
                    Utils::log("Invalid out buffer size {} {}\n{}\n{}",
                        buffer_size, byteSizeRequire,
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return frames;
                }
                mat = cv::Mat(info.ysize, info.xsize, CV_8UC4);
                status = JxlDecoderSetImageOutBuffer(dec.get(), &format, mat.ptr(), byteSizeRequire);
                if (JXL_DEC_SUCCESS != status) {
                    Utils::log("JxlDecoderSetImageOutBuffer failed\n{}\n{}",
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return frames;
                }
            }
            else if (status == JXL_DEC_FULL_IMAGE) {
                cv::cvtColor(mat, mat, cv::COLOR_BGRA2RGBA);
                frames.push_back({ mat.clone(), duration_ms });
            }
            else if (status == JXL_DEC_SUCCESS) {
                // All decoding successfully finished.
                // It's not required to call JxlDecoderReleaseInput(dec.get()) here since
                // the decoder will be destroyed.

                return frames;
            }
            else {
                Utils::log("Unknown decoder status\n{}\n{}",
                    Utils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return frames;
            }
        }
    }


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

    cv::Mat readDibFromMemory(const uint8_t* data, size_t size) {
        // 确保有足够的数据用于DIB头
        if (size < sizeof(DibHeader)) {
            Utils::log("Insufficient data for DIB header. {}", size);
            return cv::Mat();
        }

        // 读取DIB头
        DibHeader& header = *((DibHeader*)data);

        // 验证头大小是否符合预期
        if (header.headerSize != sizeof(DibHeader)) {
            Utils::log("Unsupported DIB header size {}", header.headerSize);
            return cv::Mat();
        }

        // 计算调色板大小（如果存在）
        int paletteSize = 0;
        if (header.bitCount <= 8) {
            paletteSize = (header.clrUsed == 0 ? 1 << header.bitCount : header.clrUsed) * 4;
        }

        // 确保有足够的数据用于调色板和像素数据
        //const size_t requireSize = (size_t)header.headerSize + paletteSize + header.imageSize; // header.imageSize偶尔大于size
        //if (size < requireSize) {
        //    Utils::log("Insufficient data for image.");
        //    Utils::log("requireSize {} givenSize {}", requireSize, size);
        //    return cv::Mat();
        //}

        // 读取调色板（如果存在）
        std::vector<cv::Vec4b> palette;
        if (paletteSize > 0) {
            palette.resize(paletteSize / 4);
            std::memcpy(palette.data(), data + sizeof(DibHeader), paletteSize);
        }

        // 指向像素数据的指针
        const uint8_t* pixelData = data + header.headerSize + paletteSize;

        // 检查是否存在透明度掩码
        bool hasAlphaMask = (header.height == 2 * header.width);
        int imageHeight = hasAlphaMask ? header.height / 2 : header.height;

        // 创建cv::Mat对象
        cv::Mat image(imageHeight, header.width, CV_8UC4);

        // 根据位深度处理像素数据
        switch (header.bitCount) {
        case 1: {
            // 处理1位图像
            int byteWidth = (header.width + 7) / 8; // 每行字节数
            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int byteIndex = (imageHeight - y - 1) * byteWidth + x / 8;
                    int bitIndex = x % 8;
                    int colorIndex = (pixelData[byteIndex] >> (7 - bitIndex)) & 0x01;
                    image.at<cv::Vec4b>(y, x) = cv::Vec4b(palette[colorIndex][0], palette[colorIndex][1], palette[colorIndex][2], 255);
                }
            }
            break;
        }
        case 4: {
            // 处理4位图像
            int byteWidth = (header.width + 1) / 2; // 每行字节数
            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int byteIndex = (imageHeight - y - 1) * byteWidth + x / 2;
                    int nibbleIndex = x % 2;
                    int colorIndex = (pixelData[byteIndex] >> (4 * (1 - nibbleIndex))) & 0x0F;
                    image.at<cv::Vec4b>(y, x) = cv::Vec4b(palette[colorIndex][0], palette[colorIndex][1], palette[colorIndex][2], 255);
                }
            }
            break;
        }
        case 8: {
            // 处理8位图像
            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int byteIndex = (imageHeight - y - 1) * header.width + x;
                    int colorIndex = pixelData[byteIndex];
                    image.at<cv::Vec4b>(y, x) = cv::Vec4b(palette[colorIndex][0], palette[colorIndex][1], palette[colorIndex][2], 255);
                }
            }
            break;
        }
        case 16: {
            // 处理16位图像
            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int pixelIndex = (imageHeight - y - 1) * header.width + x;
                    uint16_t pixel = ((uint16_t*)pixelData)[pixelIndex];
                    uint8_t r = (pixel >> 10) & 0x1F;
                    uint8_t g = (pixel >> 5) & 0x1F;
                    uint8_t b = pixel & 0x1F;
                    image.at<cv::Vec4b>(y, x) = cv::Vec4b(b << 3, g << 3, r << 3, 255);
                }
            }
            break;
        }
        case 24: {
            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int byteIndex = ((imageHeight - y - 1) * header.width + x) * 3;
                    uint8_t r = pixelData[byteIndex];
                    uint8_t g = pixelData[byteIndex + 1];
                    uint8_t b = pixelData[byteIndex + 2];
                    image.at<cv::Vec4b>(y, x) = cv::Vec4b(b, g, r, 255);
                }
            }
            break;
        }
        case 32: {
            memcpy(image.ptr(), pixelData, 4ULL * imageHeight * header.width);
            cv::flip(image, image, 0); // 上下翻转
            break;
        }
        default:
            Utils::log("Unsupported bit count {}", header.bitCount);
            return cv::Mat();
        }

        // 如果存在透明度掩码
        if (hasAlphaMask) {
            if (image.channels() == 3)
                cv::cvtColor(image, image, cv::COLOR_BGR2BGRA);

            const uint8_t* maskData = pixelData + header.width * header.width * header.bitCount / 8;

            int rowSize = (header.width + 31) / 32 * 4;
            int totalSize = rowSize * header.height;

            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int byteIndex = ((imageHeight - y - 1) * rowSize) + (x / 8);
                    int bitIndex = 7 - (x % 8);

                    // 读取透明掩码
                    int transparencyBit = (maskData[byteIndex] >> bitIndex) & 1;
                    if (transparencyBit == 1)
                        image.at<uint32_t>(y, x) = 0;

                    // 读取图像掩码
                    //int imageBit = (maskData[totalSize + byteIndex] >> bitIndex) & 1;
                    //if (imageBit == 0)
                    //    image.at<uint32_t>(y, x) = 0;
                }
            }
        }
        return image;
    }

    // https://github.com/corkami/pics/blob/master/binary/ico_bmp.png
    std::tuple<cv::Mat, string> loadICO(const wstring& path, const vector<uchar>& buf) {
        if (buf.size() < 6) {
            Utils::log("Invalid ICO file: {}", Utils::wstringToUtf8(path));
            return { cv::Mat(),"" };
        }

        uint16_t numImages = *reinterpret_cast<const uint16_t*>(&buf[4]);
        std::vector<IconDirEntry> entries;

        if (numImages > 255) {
            Utils::log("numImages Error: {} {}", Utils::wstringToUtf8(path), numImages);
            return { cv::Mat(),"" };
        }

        size_t offset = 6;
        for (int i = 0; i < numImages; ++i) {
            if (offset + sizeof(IconDirEntry) > buf.size()) {
                Utils::log("Invalid ICO file structure: {}", Utils::wstringToUtf8(path));
                return { cv::Mat(),"" };
            }

            entries.push_back(*reinterpret_cast<const IconDirEntry*>(&buf[offset]));
            offset += sizeof(IconDirEntry);
        }

        vector<cv::Mat> imgs;
        for (const auto& entry : entries) {
            int width = entry.width == 0 ? 256 : entry.width;
            int height = entry.height == 0 ? 256 : entry.height;

            const size_t endOffset = size_t(entry.dataOffset) + entry.dataSize;
            if (endOffset > buf.size()) {
                Utils::log("Invalid image data offset or size: {}", Utils::wstringToUtf8(path));
                Utils::log("endOffset {} fileBuf.size(): {}", endOffset, buf.size());
                Utils::log("entry.dataOffset {}", entry.dataOffset);
                Utils::log("entry.dataSize: {}", entry.dataSize);
                continue;
            }

            cv::Mat rawData(1, entry.dataSize, CV_8UC1, (uint8_t*)(buf.data() + entry.dataOffset));

            // PNG BMP
            if (memcmp(rawData.ptr(), "\x89PNG", 4) == 0 || memcmp(rawData.ptr(), "BM", 2) == 0) {
                imgs.push_back(cv::imdecode(rawData, cv::IMREAD_UNCHANGED));
                continue;
            }

            DibHeader* dibHeader = (DibHeader*)(buf.data() + entry.dataOffset);
            if (dibHeader->headerSize != 0x28)
                continue;

            imgs.push_back(readDibFromMemory((uint8_t*)(buf.data() + entry.dataOffset), entry.dataSize));
        }

        int totalWidth = 0;
        int maxHeight = 0;
        for (auto& img : imgs) {
            if (img.rows == 0 || img.cols == 0)
                continue;

            if (img.channels() == 1) {
                cv::cvtColor(img, img, cv::COLOR_GRAY2BGRA);
            }
            else if (img.channels() == 3) {
                cv::cvtColor(img, img, cv::COLOR_BGR2BGRA);
            }
            else if (img.channels() == 4) {
                // Already BGRA
            }
            else {
                Utils::log("Unsupported image format");
                img = getErrorTipsMat();
            }

            totalWidth += img.cols;
            maxHeight = std::max(maxHeight, img.rows);
        }

        if (totalWidth == 0)
            return { cv::Mat(),"" };

        cv::Mat result(maxHeight, totalWidth, CV_8UC4, cv::Scalar(0, 0, 0, 0));

        string resolutionStr = "\n分辨率:";
        int imageCnt = 0;
        int currentX = 0;
        for (const auto& img : imgs) {
            if (img.rows == 0 || img.cols == 0)
                continue;

            img.copyTo(result(cv::Rect(currentX, 0, img.cols, img.rows)));
            currentX += img.cols;

            resolutionStr += std::format(" {}x{}", img.cols, img.rows);
            imageCnt++;
        }
        resolutionStr += std::format("\n子图数量: {}", imageCnt);

        return { result,ExifParse::getSimpleInfo(path, result.cols, result.rows, buf.data(), buf.size()) + resolutionStr };
    }


    static const unsigned int CHANNEL_NOT_FOUND = UINT_MAX;

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


    unsigned int FindChannel(psd::Layer* layer, int16_t channelType)
    {
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
    cv::Mat loadPSD(const wstring& path, const vector<uchar>& buf) {
        cv::Mat img;

        psd::MallocAllocator allocator;
        psd::NativeFile file(&allocator);

        if (!file.OpenRead(path.c_str())) {
            Utils::log("Cannot open file {}", Utils::wstringToUtf8(path));
            return img;
        }

        psd::Document* document = CreateDocument(&file, &allocator);
        if (!document) {
            Utils::log("Cannot create document {}", Utils::wstringToUtf8(path));
            file.Close();
            return img;
        }

        // the sample only supports RGB colormode
        if (document->colorMode != psd::colorMode::RGB)
        {
            Utils::log("Document is not in RGB color mode {}", Utils::wstringToUtf8(path));
            DestroyDocument(document, &allocator);
            file.Close();
            return img;
        }

        // extract all layers and masks.
        bool hasTransparencyMask = false;
        psd::LayerMaskSection* layerMaskSection = ParseLayerMaskSection(document, &file, &allocator);
        if (layerMaskSection)
        {
            hasTransparencyMask = layerMaskSection->hasTransparencyMask;

            // extract all layers one by one. this should be done in parallel for maximum efficiency.
            for (unsigned int i = 0; i < layerMaskSection->layerCount; ++i)
            {
                psd::Layer* layer = &layerMaskSection->layers[i];
                ExtractLayer(document, &file, &allocator, layer);

                // check availability of R, G, B, and A channels.
                // we need to determine the indices of channels individually, because there is no guarantee that R is the first channel,
                // G is the second, B is the third, and so on.
                const unsigned int indexR = FindChannel(layer, psd::channelType::R);
                const unsigned int indexG = FindChannel(layer, psd::channelType::G);
                const unsigned int indexB = FindChannel(layer, psd::channelType::B);
                const unsigned int indexA = FindChannel(layer, psd::channelType::TRANSPARENCY_MASK);

                // note that channel data is only as big as the layer it belongs to, e.g. it can be smaller or bigger than the canvas,
                // depending on where it is positioned. therefore, we use the provided utility functions to expand/shrink the channel data
                // to the canvas size. of course, you can work with the channel data directly if you need to.
                void* canvasData[4] = {};
                unsigned int channelCount = 0u;
                if ((indexR != CHANNEL_NOT_FOUND) && (indexG != CHANNEL_NOT_FOUND) && (indexB != CHANNEL_NOT_FOUND))
                {
                    // RGB channels were found.
                    canvasData[0] = ExpandChannelToCanvas(document, &allocator, layer, &layer->channels[indexR]);
                    canvasData[1] = ExpandChannelToCanvas(document, &allocator, layer, &layer->channels[indexG]);
                    canvasData[2] = ExpandChannelToCanvas(document, &allocator, layer, &layer->channels[indexB]);
                    channelCount = 3u;

                    if (indexA != CHANNEL_NOT_FOUND)
                    {
                        // A channel was also found.
                        canvasData[3] = ExpandChannelToCanvas(document, &allocator, layer, &layer->channels[indexA]);
                        channelCount = 4u;
                    }
                }

                // interleave the different pieces of planar canvas data into one RGB or RGBA image, depending on what channels
                // we found, and what color mode the document is stored in.
                uint8_t* image8 = nullptr;
                uint16_t* image16 = nullptr;
                float32_t* image32 = nullptr;
                if (channelCount == 3u)
                {
                    if (document->bitsPerChannel == 8)
                    {
                        image8 = CreateInterleavedImage<uint8_t>(&allocator, canvasData[0], canvasData[1], canvasData[2], document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 16)
                    {
                        image16 = CreateInterleavedImage<uint16_t>(&allocator, canvasData[0], canvasData[1], canvasData[2], document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 32)
                    {
                        image32 = CreateInterleavedImage<float32_t>(&allocator, canvasData[0], canvasData[1], canvasData[2], document->width, document->height);
                    }
                }
                else if (channelCount == 4u)
                {
                    if (document->bitsPerChannel == 8)
                    {
                        image8 = CreateInterleavedImage<uint8_t>(&allocator, canvasData[0], canvasData[1], canvasData[2], canvasData[3], document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 16)
                    {
                        image16 = CreateInterleavedImage<uint16_t>(&allocator, canvasData[0], canvasData[1], canvasData[2], canvasData[3], document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 32)
                    {
                        image32 = CreateInterleavedImage<float32_t>(&allocator, canvasData[0], canvasData[1], canvasData[2], canvasData[3], document->width, document->height);
                    }
                }

                allocator.Free(canvasData[0]);
                allocator.Free(canvasData[1]);
                allocator.Free(canvasData[2]);
                allocator.Free(canvasData[3]);

                // get the layer name.
                // Unicode data is preferred because it is not truncated by Photoshop, but unfortunately it is optional.
                // fall back to the ASCII name in case no Unicode name was found.
                std::wstringstream layerName;
                if (layer->utf16Name)
                {
                    //In Windows wchar_t is utf16
                    PSD_STATIC_ASSERT(sizeof(wchar_t) == sizeof(uint16_t));
                    layerName << reinterpret_cast<wchar_t*>(layer->utf16Name);
                }
                else
                {
                    layerName << layer->name.c_str();
                }
            }

            DestroyLayerMaskSection(layerMaskSection, &allocator);
        }

        // extract the image data section, if available. the image data section stores the final, merged image, as well as additional
        // alpha channels. this is only available when saving the document with "Maximize Compatibility" turned on.
        if (document->imageDataSection.length != 0)
        {
            psd::ImageDataSection* imageData = ParseImageDataSection(document, &file, &allocator);
            if (imageData)
            {
                // interleave the planar image data into one RGB or RGBA image.
                // store the rest of the (alpha) channels and the transparency mask separately.
                const unsigned int imageCount = imageData->imageCount;

                // note that an image can have more than 3 channels, but still no transparency mask in case all extra channels
                // are actual alpha channels.
                bool isRgb = false;
                if (imageCount == 3)
                {
                    // imageData->images[0], imageData->images[1] and imageData->images[2] contain the R, G, and B channels of the merged image.
                    // they are always the size of the canvas/document, so we can interleave them using imageUtil::InterleaveRGB directly.
                    isRgb = true;
                }
                else if (imageCount >= 4)
                {
                    // check if we really have a transparency mask that belongs to the "main" merged image.
                    if (hasTransparencyMask)
                    {
                        // we have 4 or more images/channels, and a transparency mask.
                        // this means that images 0-3 are RGBA, respectively.
                        isRgb = false;
                    }
                    else
                    {
                        // we have 4 or more images stored in the document, but none of them is the transparency mask.
                        // this means we are dealing with RGB (!) data, and several additional alpha channels.
                        isRgb = true;
                    }
                }

                uint8_t* image8 = nullptr;
                uint16_t* image16 = nullptr;
                float32_t* image32 = nullptr;
                if (isRgb)
                {
                    // RGB
                    if (document->bitsPerChannel == 8)
                    {
                        image8 = CreateInterleavedImage<uint8_t>(&allocator, imageData->images[0].data, imageData->images[1].data, imageData->images[2].data, document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 16)
                    {
                        image16 = CreateInterleavedImage<uint16_t>(&allocator, imageData->images[0].data, imageData->images[1].data, imageData->images[2].data, document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 32)
                    {
                        image32 = CreateInterleavedImage<float32_t>(&allocator, imageData->images[0].data, imageData->images[1].data, imageData->images[2].data, document->width, document->height);
                    }
                }
                else
                {
                    // RGBA
                    if (document->bitsPerChannel == 8)
                    {
                        image8 = CreateInterleavedImage<uint8_t>(&allocator, imageData->images[0].data, imageData->images[1].data, imageData->images[2].data, imageData->images[3].data, document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 16)
                    {
                        image16 = CreateInterleavedImage<uint16_t>(&allocator, imageData->images[0].data, imageData->images[1].data, imageData->images[2].data, imageData->images[3].data, document->width, document->height);
                    }
                    else if (document->bitsPerChannel == 32)
                    {
                        image32 = CreateInterleavedImage<float32_t>(&allocator, imageData->images[0].data, imageData->images[1].data, imageData->images[2].data, imageData->images[3].data, document->width, document->height);
                    }
                }

                if (document->bitsPerChannel == 8) {
                    img = cv::Mat(document->height, document->width, CV_8UC4, image8).clone();
                }
                else if (document->bitsPerChannel == 16) {
                    cv::Mat(document->height, document->width, CV_16UC4, image16).convertTo(img, CV_8UC4, 1.0/256);
                }
                else if (document->bitsPerChannel == 32) {
                    cv::Mat(document->height, document->width, CV_32FC4, image32).convertTo(img, CV_8UC4, 255.0);
                }

                allocator.Free(image8);
                allocator.Free(image16);
                allocator.Free(image32);

                DestroyImageDataSection(imageData, &allocator);
            }
        }

        // don't forget to destroy the document, and close the file.
        DestroyDocument(document, &allocator);
        file.Close();

        return img;
    }

    cv::Mat loadTGA_HDR(const wstring& path, const vector<uchar>& buf) {
        int width, height, channels;

        // 使用stb_image从内存缓冲区加载图像
        uint8_t* img = stbi_load_from_memory(buf.data(), (int)buf.size(), &width, &height, &channels, 0);

        if (!img) {
            Utils::log("Failed to load image: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        // 确定OpenCV的色彩空间
        int cv_type;
        switch (channels) {
        case 1: cv_type = CV_8UC1; break;
        case 3: cv_type = CV_8UC3; break;
        case 4: cv_type = CV_8UC4; break;
        default:
            stbi_image_free(img);
            Utils::log("Unsupported number of channels:{} {}", channels, Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        auto result = cv::Mat(height, width, cv_type, img).clone();

        stbi_image_free(img);

        cv::cvtColor(result, result, channels == 1 ? cv::COLOR_GRAY2BGRA :
            (channels == 3 ? cv::COLOR_RGB2BGRA : cv::COLOR_RGBA2BGRA));

        return result;
    }

    cv::Mat loadSVG(const wstring& path, const vector<uchar>& buf){
        const int maxEdge = 3840;
        static bool isInitFont = false;

        if (!isInitFont) {
            isInitFont = true;
            auto rc = Utils::GetResource(IDR_MSYHMONO_TTF, L"TTF");
            lunasvg_add_font_face_from_data("", false, false, rc.addr, rc.size, nullptr, nullptr);
        }

        auto document = lunasvg::Document::loadFromData((const char*)buf.data(), buf.size());
        if (!document) {
            Utils::log("Failed to load SVG data {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        // 宽高比例
        const double AspectRatio = (document->height() == 0) ? 1 : (document->width() / document->height());
        int height, width;

        if (AspectRatio == 1) {
            height = width = maxEdge;
        }
        else if (AspectRatio > 1) {
            width = maxEdge;
            height = int(maxEdge / AspectRatio);
        }
        else {
            height = maxEdge;
            width = int(maxEdge * AspectRatio);
        }

        auto bitmap = document->renderToBitmap(width, height);
        if (bitmap.isNull()) {
            Utils::log("Failed to render SVG to bitmap {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        return cv::Mat(height, width, CV_8UC4, bitmap.data()).clone();
    }

    cv::Mat loadJXR(const wstring& path, const vector<uchar>& buf) {
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize COM library." << std::endl;
            return cv::Mat();
        }

        IWICImagingFactory* pFactory = NULL;
        hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&pFactory);
        if (FAILED(hr)) {
            std::cerr << "Failed to create WIC Imaging Factory." << std::endl;
            CoUninitialize();
            return cv::Mat();
        }

        IStream* pStream = NULL;
        hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        if (FAILED(hr)) {
            std::cerr << "Failed to create stream." << std::endl;
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        ULONG bytesWritten;
        hr = pStream->Write(buf.data(), (ULONG)buf.size(), &bytesWritten);
        if (FAILED(hr) || bytesWritten != buf.size()) {
            std::cerr << "Failed to write to stream." << std::endl;
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        LARGE_INTEGER li = { 0 };
        hr = pStream->Seek(li, STREAM_SEEK_SET, NULL);
        if (FAILED(hr)) {
            std::cerr << "Failed to seek stream." << std::endl;
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        IWICBitmapDecoder* pDecoder = NULL;
        hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (FAILED(hr)) {
            std::cerr << "Failed to create decoder from stream." << std::endl;
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        IWICBitmapFrameDecode* pFrame = NULL;
        hr = pDecoder->GetFrame(0, &pFrame);
        if (FAILED(hr)) {
            std::cerr << "Failed to get frame from decoder." << std::endl;
            pDecoder->Release();
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        UINT width, height;
        hr = pFrame->GetSize(&width, &height);
        if (FAILED(hr)) {
            std::cerr << "Failed to get image size." << std::endl;
            pFrame->Release();
            pDecoder->Release();
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        IWICFormatConverter* pConverter = NULL;
        hr = pFactory->CreateFormatConverter(&pConverter);
        if (FAILED(hr)) {
            std::cerr << "Failed to create format converter." << std::endl;
            pFrame->Release();
            pDecoder->Release();
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize format converter." << std::endl;
            pConverter->Release();
            pFrame->Release();
            pDecoder->Release();
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        cv::Mat mat(height, width, CV_8UC4);
        hr = pConverter->CopyPixels(NULL, width * 4, width * height * 4, mat.data);
        if (FAILED(hr)) {
            std::cerr << "Failed to copy pixels." << std::endl;
            pConverter->Release();
            pFrame->Release();
            pDecoder->Release();
            pStream->Release();
            pFactory->Release();
            CoUninitialize();
            return cv::Mat();
        }

        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        CoUninitialize();

        return mat;
    }

    std::vector<ImageNode> loadBPG(const std::wstring& path, const std::vector<uchar>& buf) {
        auto decoderContext = bpg_decoder_open();
        if (bpg_decoder_decode(decoderContext, buf.data(), (int)buf.size()) < 0) {
            Utils::log("cvMat cannot decode: {}", Utils::wstringToUtf8(path));
            return {};
        }

        std::vector<ImageNode> frames;
        BPGImageInfo img_info{};
        bpg_decoder_get_info(decoderContext, &img_info);

        auto width = img_info.width;
        auto height = img_info.height;

        cv::Mat frame(height, width, CV_8UC4);
        uint32_t* ptr = (uint32_t*)frame.data;

        if (img_info.has_animation) {
            while (true) {
                if (bpg_decoder_start(decoderContext, BPG_OUTPUT_FORMAT_RGBA32) == 0) {
                    for (uint32_t y = 0; y < height; y++) {
                        bpg_decoder_get_line(decoderContext, ptr + width * y);
                    }
                    cv::cvtColor(frame, frame, cv::COLOR_RGBA2BGRA);

                    int num, den;
                    bpg_decoder_get_frame_duration(decoderContext, &num, &den);
                    frames.push_back({ frame.clone(), den == 0 ? 100 : (num * 1000 / den) });
                }
                else {
                    break;
                }
            }
        }
        else {
            if (bpg_decoder_start(decoderContext, BPG_OUTPUT_FORMAT_RGBA32) == 0) {
                for (uint32_t y = 0; y < height; y++) {
                    bpg_decoder_get_line(decoderContext, ptr + width * y);
                }
                cv::cvtColor(frame, frame, cv::COLOR_RGBA2BGRA);
                frames.push_back({ frame, 0 });
            }
        }

        bpg_decoder_close(decoderContext);
        return frames;
    }

    vector<cv::Mat> loadMats(const wstring& path, const vector<uchar>& buf) {
        vector<cv::Mat> imgs;

        if (!cv::imdecodemulti(buf, cv::IMREAD_UNCHANGED, imgs)) {
            return imgs;
        }
        return imgs;
    }

    cv::Mat loadMat(const wstring& path, const vector<uchar>& buf) {
        cv::Mat img;
        try{
            img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
        }
        catch (cv::Exception e) {
            Utils::log("cvMat cannot decode: {} [{}]", Utils::wstringToUtf8(path), e.what());
            return cv::Mat();
        }

        if (img.empty()) {
            Utils::log("cvMat cannot decode: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        if (img.channels() == 1)
            cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);

        if (img.channels() != 3 && img.channels() != 4) {
            Utils::log("cvMat unsupport channel: {}", img.channels());
            return cv::Mat();
        }

        // enum { CV_8U=0, CV_8S=1, CV_16U=2, CV_16S=3, CV_32S=4, CV_32F=5, CV_64F=6 }
        if (img.depth() <= 1) {
            return img;
        }
        else if (img.depth() <= 3) {
            cv::Mat tmp;
            img.convertTo(tmp, CV_8U, 1.0 / 256);
            return tmp;
        }
        else if (img.depth() <= 5) {
            cv::Mat tmp;
            img.convertTo(tmp, CV_8U, 1.0 / 65536);
            return tmp;
        }
        Utils::log("Special: {}, img.depth(): {}, img.channels(): {}",
            Utils::wstringToUtf8(path), img.depth(), img.channels());
        return cv::Mat();
    }


    std::vector<ImageNode> loadGif(const wstring& path, const vector<uchar>& buf) {

        auto InternalRead_Mem = [](GifFileType* gif, GifByteType* buf, int len) -> int {
            if (len == 0)
                return 0;

            GifData* pData = (GifData*)gif->UserData;
            if (pData->m_nPosition > pData->m_nBufferSize)
                return 0;

            UINT nRead;
            if (pData->m_nPosition + len > pData->m_nBufferSize || pData->m_nPosition + len < pData->m_nPosition)
                nRead = (UINT)(pData->m_nBufferSize - pData->m_nPosition);
            else
                nRead = len;

            memcpy((BYTE*)buf, (BYTE*)pData->m_lpBuffer + pData->m_nPosition, nRead);
            pData->m_nPosition += nRead;

            return nRead;
            };

        std::vector<ImageNode> frames;

        GifData gifData{ buf.data(), buf.size(), 0 };

        int error;
        GifFileType* gif = DGifOpen((void*)&gifData, InternalRead_Mem, &error);

        if (gif == nullptr) {
            Utils::log("DGifOpen: Error: {} {}", Utils::wstringToUtf8(path), GifErrorString(error));
            return frames;
        }
        int retCode = DGifSlurp(gif);
        //if (retCode != GIF_OK) { // 部分GIF可能无法完全解码
        //    Utils::log("DGifSlurp Error: {} {}", Utils::wstringToUtf8(path), GifErrorString(gif->Error));
        //    DGifCloseFile(gif, &error);
        //    return frames;
        //}

        // 对画布尺寸进行修正：某些GIF的SWidth、SHeight小于帧的宽、高
        // https://www.cnblogs.com/stronghorse/p/16002884.html
        for (int i = 0; i < gif->ImageCount; i++) {
            const GifImageDesc& ImageDesc = gif->SavedImages[i].ImageDesc;
            if ((ImageDesc.Width + ImageDesc.Left) > gif->SWidth)
                gif->SWidth = ImageDesc.Width + ImageDesc.Left;
            if ((ImageDesc.Height + ImageDesc.Top) > gif->SHeight)
                gif->SHeight = ImageDesc.Height + ImageDesc.Top;
        }

        const auto pxNums = gif->SWidth * gif->SHeight;
        const auto byteSize = 4ULL * pxNums;
        vector<GifByteType> canvas(byteSize);
        vector<GifByteType> lastCanvas(byteSize);
        vector<GifByteType> tmpCanvas(byteSize);

        for (int i = 0; i < gif->ImageCount; ++i) {
            memset(tmpCanvas.data(), 0, byteSize);
            SavedImage& image = gif->SavedImages[i];
            GraphicsControlBlock gcb{};
            auto retCode = DGifSavedExtensionToGCB(gif, i, &gcb);
            if (retCode != GIF_OK) {
                gcb.TransparentColor = -1;
                gcb.DelayTime = 10;
            }

            auto colorMap = image.ImageDesc.ColorMap ? image.ImageDesc.ColorMap : gif->SColorMap;

            for (int y = 0; y < image.ImageDesc.Height; ++y) {
                for (int x = 0; x < image.ImageDesc.Width; ++x) {
                    int gifX = x + image.ImageDesc.Left;
                    int gifY = y + image.ImageDesc.Top;
                    GifByteType colorIndex = image.RasterBits[y * image.ImageDesc.Width + x];
                    if (colorIndex == gcb.TransparentColor) {
                        continue;
                    }
                    GifColorType& color = colorMap->Colors[colorIndex];
                    int pixelIdx = (gifY * gif->SWidth + gifX) * 4;
                    tmpCanvas[pixelIdx] = color.Blue;
                    tmpCanvas[pixelIdx + 1] = color.Green;
                    tmpCanvas[pixelIdx + 2] = color.Red;
                    tmpCanvas[pixelIdx + 3] = 255;
                }
            }

            if (i == 0) { // 首帧 全量帧
                memcpy(canvas.data(), tmpCanvas.data(), byteSize);
            }
            else {
                auto canvasPtr = (uint32_t*)canvas.data();
                auto tmpCanvasPtr = (uint32_t*)tmpCanvas.data();
                for (int idx = 0; idx < pxNums; idx++) {
                    if (tmpCanvasPtr[idx])
                        canvasPtr[idx] = tmpCanvasPtr[idx];
                }
            }

            frames.emplace_back(cv::Mat(gif->SHeight, gif->SWidth, CV_8UC4, canvas.data()).clone(),
                gcb.DelayTime * 10);

            if (gcb.DisposalMode <= DISPOSE_DO_NOT) { // 不处理
            }
            else if (gcb.DisposalMode == DISPOSE_BACKGROUND) { // 恢复到背景色
                memset(canvas.data(), 0, byteSize);
            }
            else if (gcb.DisposalMode == DISPOSE_PREVIOUS) { // 恢复到前一帧
                memcpy(canvas.data(), lastCanvas.data(), byteSize);
            }

            memcpy(lastCanvas.data(), canvas.data(), byteSize);
        }

        DGifCloseFile(gif, nullptr);

        if (frames.empty()) {
            Utils::log("Gif decode 0 frames: {}", Utils::wstringToUtf8(path));
        }

        return frames;
    }


    std::vector<ImageNode> loadWebp(const wstring& path, const std::vector<uint8_t>& buf) {
        std::vector<ImageNode> frames;

        WebPData webp_data{ buf.data(), buf.size() };
        WebPDemuxer* demux = WebPDemux(&webp_data);
        if (!demux) {
            Utils::log("Failed to create WebP demuxer: {}", Utils::wstringToUtf8(path));
            frames.push_back({ getErrorTipsMat(), 0 });
            return frames;
        }

        uint32_t canvas_width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
        uint32_t canvas_height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);

        WebPIterator iter;
        cv::Mat lastFrame(canvas_height, canvas_width, CV_8UC4, cv::Vec4b(0, 0, 0, 0)); // Initialize the global canvas with transparency
        cv::Mat canvas(canvas_height, canvas_width, CV_8UC4, cv::Vec4b(0, 0, 0, 0)); // Initialize the global canvas with transparency

        if (WebPDemuxGetFrame(demux, 1, &iter)) {
            do {
                WebPDecoderConfig config;
                if (!WebPInitDecoderConfig(&config)) {
                    WebPDemuxReleaseIterator(&iter);
                    WebPDemuxDelete(demux);

                    Utils::log("Failed to initialize WebP decoder config: {}", Utils::wstringToUtf8(path));
                    frames.push_back({ getErrorTipsMat(), 0 });
                    return frames;
                }

                config.output.colorspace = WEBP_CSP_MODE::MODE_bgrA;

                if (WebPDecode(iter.fragment.bytes, iter.fragment.size, &config) != VP8_STATUS_OK) {
                    WebPFreeDecBuffer(&config.output);
                    WebPDemuxReleaseIterator(&iter);
                    WebPDemuxDelete(demux);

                    Utils::log("Failed to decode WebP frame: {}", Utils::wstringToUtf8(path));
                    frames.push_back({ getErrorTipsMat(), 0 });
                    return frames;
                }

                cv::Mat frame(config.output.height, config.output.width, CV_8UC4, config.output.u.RGBA.rgba);
                cv::Mat roi = canvas(cv::Rect(iter.x_offset, iter.y_offset, config.output.width, config.output.height));
                frame.copyTo(roi);

                for (int y = 0; y < canvas.rows; ++y) {
                    for (int x = 0; x < canvas.cols; ++x) {
                        cv::Vec4b srcPixel = canvas.at<cv::Vec4b>(y, x);
                        if (srcPixel[3] != 0) {
                            lastFrame.at<cv::Vec4b>(y, x) = srcPixel;
                        }
                    }
                }

                frames.push_back({ lastFrame.clone(), iter.duration });

                WebPFreeDecBuffer(&config.output);
            } while (WebPDemuxNextFrame(&iter));
            WebPDemuxReleaseIterator(&iter);
        }

        WebPDemuxDelete(demux);
        return frames;
    }

    std::string statusExplain(WP2Status status) {
        switch (status) {
        case WP2_STATUS_OK:
            return "Operation completed successfully.";
        case WP2_STATUS_VERSION_MISMATCH:
            return "Version mismatch.";
        case WP2_STATUS_OUT_OF_MEMORY:
            return "Memory error allocating objects.";
        case WP2_STATUS_INVALID_PARAMETER:
            return "A parameter value is invalid.";
        case WP2_STATUS_NULL_PARAMETER:
            return "A pointer parameter is NULL.";
        case WP2_STATUS_BAD_DIMENSION:
            return "Picture has invalid width/height.";
        case WP2_STATUS_USER_ABORT:
            return "Abort request by user.";
        case WP2_STATUS_UNSUPPORTED_FEATURE:
            return "Unsupported feature.";
        case WP2_STATUS_BITSTREAM_ERROR:
            return "Bitstream has syntactic error.";
        case WP2_STATUS_NOT_ENOUGH_DATA:
            return "Premature EOF during decoding.";
        case WP2_STATUS_BAD_READ:
            return "Error while reading bytes.";
        case WP2_STATUS_NEURAL_DECODE_FAILURE:
            return "Neural decoder failed.";
        case WP2_STATUS_BITSTREAM_OUT_OF_MEMORY:
            return "Memory error while flushing bits.";
        case WP2_STATUS_INVALID_CONFIGURATION:
            return "Encoding configuration is invalid.";
        case WP2_STATUS_BAD_WRITE:
            return "Error while flushing bytes.";
        case WP2_STATUS_FILE_TOO_BIG:
            return "File is bigger than 4G.";
        case WP2_STATUS_INVALID_COLORSPACE:
            return "Encoder called with bad colorspace.";
        case WP2_STATUS_NEURAL_ENCODE_FAILURE:
            return "Neural encoder failed.";
        default:
            return "Unknown status.";
        }
    }

    static uint32_t swap_endian(uint32_t value) {
        return (value >> 24) |
            ((value >> 8) & 0x0000FF00) |
            ((value << 8) & 0x00FF0000) |
            (value << 24);
    }

    // https://chromium.googlesource.com/codecs/libwebp2  commit 96720e6410284ebebff2007d4d87d7557361b952  Date:   Mon Sep 9 18:11:04 2024 +0000
    // 网络找的不少wp2图像无法解码，使用 libwebp2 的 cwp2.exe 工具编码的 .wp2 图片可以正常解码
    std::vector<ImageNode> loadWP2(const wstring& path, const std::vector<uint8_t>& buf) {
        std::vector<ImageNode> frames;

        WP2::ArrayDecoder decoder(buf.data(), buf.size());
        uint32_t duration_ms = 0;

        while (decoder.ReadFrame(&duration_ms)) {
            auto& output_buffer = decoder.GetPixels();
            cv::Mat img; // Need BGRA or BGR

            switch (output_buffer.format())
            {
            case WP2_Argb_32:
            case WP2_ARGB_32:
            case WP2_XRGB_32: {// A-RGB -> BGR-A 大小端互转
                img = cv::Mat(output_buffer.height(), output_buffer.width(), CV_8UC4, (void*)output_buffer.GetRow(0), output_buffer.stride());
                auto srcPtr = (uint32_t*)img.ptr();
                auto pixelCount = (size_t)img.cols * img.rows;
                for (size_t i = 0; i < pixelCount; ++i) {
                    srcPtr[i] = swap_endian(srcPtr[i]);
                }
            }break;

            case WP2_rgbA_32:
            case WP2_RGBA_32:
            case WP2_RGBX_32: {
                img = cv::Mat(output_buffer.height(), output_buffer.width(), CV_8UC4, (void*)output_buffer.GetRow(0), output_buffer.stride());
                cv::cvtColor(img, img, cv::COLOR_RGBA2BGRA);
            }break;

            case WP2_bgrA_32:
            case WP2_BGRA_32:
            case WP2_BGRX_32: {
                img = cv::Mat(output_buffer.height(), output_buffer.width(), CV_8UC4, (void*)output_buffer.GetRow(0), output_buffer.stride());
            }break;

            case WP2_RGB_24: {
                img = cv::Mat(output_buffer.height(), output_buffer.width(), CV_8UC3, (void*)output_buffer.GetRow(0), output_buffer.stride());
                cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
            }break;

            case WP2_BGR_24: {
                img = cv::Mat(output_buffer.height(), output_buffer.width(), CV_8UC3, (void*)output_buffer.GetRow(0), output_buffer.stride());
            }break;

            case WP2_Argb_38: { // HDR format: 8 bits for A, 10 bits per RGB.
                img = cv::Mat(output_buffer.height(), output_buffer.width(), CV_8UC4); // 8-bit BGRA

                for (uint32_t y = 0; y < output_buffer.height(); ++y) {
                    const uint8_t* src_row = (const uint8_t*)output_buffer.GetRow(y);
                    cv::Vec4b* dst_row = img.ptr<cv::Vec4b>(y);

                    for (uint32_t x = 0; x < output_buffer.width(); ++x) {
                        // src_row contains 5 bytes per pixel: A (8 bits), R (10 bits), G (10 bits), B (10 bits)
                        const uint8_t A = src_row[0];           // 8 bits for alpha
                        const uint16_t R = ((src_row[1] << 2) | (src_row[2] >> 6)); // 10 bits for red
                        const uint16_t G = ((src_row[2] & 0x3F) << 4) | (src_row[3] >> 4); // 10 bits for green
                        const uint16_t B = ((src_row[3] & 0x0F) << 6) | (src_row[4] >> 2); // 10 bits for blue

                        // Map 10-bit values (0-1023) to 8-bit values (0-255)
                        dst_row[x] = cv::Vec4b(
                            B >> 2,  // Blue (10 -> 8 bits)
                            G >> 2,  // Green (10 -> 8 bits)
                            R >> 2,  // Red (10 -> 8 bits)
                            A        // Alpha (already 8 bits)
                        );

                        src_row += 5; // Move to next pixel (5 bytes per pixel in WP2_Argb_38)
                    }
                }
            } break;
            }

            if (!img.empty())
                frames.push_back({ img.clone(), (int)duration_ms });
        }

#ifndef NDEBUG
        auto status = decoder.GetStatus();
        if (status != WP2_STATUS_OK) {
            Utils::log("ERROR: {}\n{}", Utils::wstringToUtf8(path), statusExplain(status));
        }
#endif

        return frames;
    }

    // 辅助函数，用于从 PFM 头信息中提取尺寸和比例因子
    bool parsePFMHeader(const vector<uchar>& buf, int& width, int& height, float& scaleFactor, bool& isColor, size_t& dataOffset) {
        string header(reinterpret_cast<const char*>(buf.data()), 2);

        // 判断是否是RGB（PF）或灰度（Pf）
        if (header == "PF") {
            isColor = true;
        }
        else if (header == "Pf") {
            isColor = false;
        }
        else {
            std::cerr << "Invalid PFM format!" << endl;
            return false;
        }

        // 查找下一行的宽度和高度
        size_t maxOffset = buf.size() > 100 ? 100 : buf.size();
        size_t offset = 2;

        while ((buf[offset] == '\n' || buf[offset] == ' ') && offset < maxOffset) offset++;
        string dimLine;
        while (buf[offset] != '\n' && offset < maxOffset) dimLine += buf[offset++];

        switch (sscanf(dimLine.c_str(), "%d %d", &width, &height)) {
        case 1: {
            offset++;
            string dimLine;
            while (buf[offset] != '\n' && offset < maxOffset) dimLine += buf[offset++];
            if (sscanf(dimLine.c_str(), "%d", &height) != 1) {
                Utils::log("parsePFMHeader fail!");
                return false;
            }
        }break;

        case 2:break;

        default: {
            Utils::log("parsePFMHeader fail!");
            return false;
        }
        }

        // 查找比例因子
        string scaleLine;
        offset++;
        while (buf[offset] != '\n' && offset < maxOffset) scaleLine += buf[offset++];

        scaleFactor = std::stof(scaleLine);
        dataOffset = offset+1;

        return true;
    }

    cv::Mat loadPFM(const wstring& path, const vector<uchar>& buf) {
        int width, height;
        float scaleFactor;
        bool isColor;
        size_t dataOffset;

        // 解析 PFM 头信息
        if (!parsePFMHeader(buf, width, height, scaleFactor, isColor, dataOffset)) {
            std::cerr << "Failed to parse PFM header!" << endl;
            return cv::Mat();
        }

        // 创建 OpenCV Mat，格式为 CV_32FC3（或 CV_32FC1 对于灰度图）
        cv::Mat image(height, width, isColor ? CV_32FC3 : CV_32FC1, (void*)(buf.data() + dataOffset));

        // 如果比例因子为负数，图像需要垂直翻转
        if (scaleFactor < 0) {
            cv::flip(image, image, 0);
            scaleFactor = -scaleFactor;
        }

        // 将图像从浮点数格式缩放到范围 [0, 255]
        image *= 255.0f / scaleFactor;

        // 将图像转换为 8 位格式
        image.convertTo(image, CV_8UC3);

        // 转换到 BGR 格式（如果是彩色图像）
        cv::cvtColor(image, image, isColor ? cv::COLOR_RGB2BGR : cv::COLOR_GRAY2BGR);

        return image;
    }


    cv::Mat loadQOI(const wstring& path, const vector<uchar>& buf) {
        cv::Mat mat;
        qoi_desc desc;
        auto pixels = qoi_decode(buf.data(), buf.size(), &desc, 0);
        if (!pixels)
            return mat;

        switch (desc.channels) {
        case 3:  // RGB
            mat = cv::Mat(desc.height, desc.width, CV_8UC3, pixels);
            cvtColor(mat, mat, cv::COLOR_RGB2BGR);  // QOI使用RGB格式，OpenCV默认使用BGR
            break;
        case 4:  // RGBA
            mat = cv::Mat(desc.height, desc.width, CV_8UC4, pixels);
            cvtColor(mat, mat, cv::COLOR_RGBA2BGRA);  // 转换RGBA到BGRA
            break;
        }

        auto ret = mat.clone();
        free(pixels);// 释放QOI解码分配的内存
        return ret;
    }


    std::pair<std::vector<uint8_t>,std::string> unzipLivp(std::vector<uint8_t>& livpFileBuff) {
        // 创建内存文件句柄
        zlib_filefunc_def memory_filefunc;
        memset(&memory_filefunc, 0, sizeof(zlib_filefunc_def));

        // 准备内存访问结构
        struct membuf {
            std::vector<uint8_t>& buffer;
            size_t position;
        } mem = { livpFileBuff, 0 };

        memory_filefunc.opaque = (voidpf)&mem;

        // 实现内存读取函数
        memory_filefunc.zopen_file = [](voidpf opaque, const char* filename, int mode) -> voidpf {
            return opaque;
            };

        memory_filefunc.zread_file = [](voidpf opaque, voidpf stream, void* buf, uLong size) -> uLong {
            membuf* mem = (membuf*)opaque;
            size_t remaining = mem->buffer.size() - mem->position;
            size_t to_read = (size < remaining) ? size : remaining;
            if (to_read > 0) {
                memcpy(buf, mem->buffer.data() + mem->position, to_read);
                mem->position += to_read;
            }
            return to_read;
            };

        memory_filefunc.zwrite_file = [](voidpf, voidpf, const void*, uLong) -> uLong {
            return 0; // 不需要写入
            };

        memory_filefunc.ztell_file = [](voidpf opaque, voidpf stream) -> long {
            return (long)((membuf*)opaque)->position;
            };

        memory_filefunc.zseek_file = [](voidpf opaque, voidpf stream, uLong offset, int origin) -> long {
            membuf* mem = (membuf*)opaque;
            size_t new_position = 0;

            switch (origin) {
            case ZLIB_FILEFUNC_SEEK_CUR:
                new_position = mem->position + offset;
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                new_position = mem->buffer.size() + offset;
                break;
            case ZLIB_FILEFUNC_SEEK_SET:
                new_position = offset;
                break;
            default:
                return -1;
            }

            if (new_position > mem->buffer.size()) {
                return -1;
            }

            mem->position = new_position;
            return 0;
            };

        memory_filefunc.zclose_file = [](voidpf, voidpf) -> int {
            return 0; // 不需要关闭
            };

        memory_filefunc.zerror_file = [](voidpf, voidpf) -> int {
            return 0; // 无错误
            };

        // 打开ZIP文件
        unzFile zipfile = unzOpen2("__memory__", &memory_filefunc);
        if (!zipfile) {
            return {}; // 打开失败返回空vector
        }

        // 遍历ZIP文件中的文件
        if (unzGoToFirstFile(zipfile) != UNZ_OK) {
            unzClose(zipfile);
            return {};
        }

        std::vector<uint8_t> image_data;
        std::string extName;
        do {
            unz_file_info file_info;
            char filename[256];

            if (unzGetCurrentFileInfo(zipfile, &file_info, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK) {
                continue;
            }

            std::string file_name(filename);
            std::transform(file_name.begin(), file_name.end(), file_name.begin(), ::tolower);

            // 检查是否是图像文件
            if (file_name.ends_with(".jpg") || file_name.ends_with(".jpeg") ||
                file_name.ends_with(".heic") || file_name.ends_with(".heif")) {

                // 打开当前文件
                if (unzOpenCurrentFile(zipfile) != UNZ_OK) {
                    continue;
                }

                // 读取文件内容
                image_data.resize(file_info.uncompressed_size);
                int bytes_read = unzReadCurrentFile(zipfile, image_data.data(), file_info.uncompressed_size);

                unzCloseCurrentFile(zipfile);

                if (bytes_read > 0 && static_cast<uLong>(bytes_read) == file_info.uncompressed_size) {
                    extName = file_name.size() >= 4 ? file_name.substr(file_name.length() - 4, 4) : "";
                    for (auto& c : extName)	c = std::tolower(c);
                    break; // 成功读取图像文件
                }
                else {
                    image_data.clear(); // 读取失败，清空数据
                }
            }
        } while (unzGoToNextFile(zipfile) == UNZ_OK);

        unzClose(zipfile);
        return { image_data, extName };
    }


    struct PngSource {
        const uint8_t* data;
        size_t size;
        int    offset;
    };

    std::vector<ImageNode> loadApng(const std::wstring& path, const std::vector<uint8_t>& buf) {
        std::vector<ImageNode> frames;

        if (buf.size() < 8 || png_sig_cmp(buf.data(), 0, 8)) {
            Utils::log("FInvalid PNG signature in file: {}", Utils::wstringToUtf8(path));
            return frames;
        }

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) {
            Utils::log("Failed to create PNG read struct for file: : {}", Utils::wstringToUtf8(path));
            return frames;
        }

        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_read_struct(&png, nullptr, nullptr);
            Utils::log("Failed to create PNG info struct for file: {}", Utils::wstringToUtf8(path));
            return frames;
        }

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_read_struct(&png, &info, nullptr);
            Utils::log("Error during PNG file reading: {}", Utils::wstringToUtf8(path));
            return frames;
        }

        PngSource pngSource{ buf.data(), buf.size(), 0 };
        png_set_read_fn(png, &pngSource, [](png_structp png_ptr, png_bytep data, png_size_t length) {
            PngSource* isource = (PngSource*)png_get_io_ptr(png_ptr);
            if ((isource->offset + length) <= isource->size) {
                memcpy(data, isource->data + isource->offset, length);
                isource->offset += (int)length;
            }
            });

        // 读取PNG信息
        png_read_info(png, info);

        int width = png_get_image_width(png, info);
        int height = png_get_image_height(png, info);
        png_byte color_type = png_get_color_type(png, info);
        png_byte bit_depth = png_get_bit_depth(png, info);

        // 将所有颜色类型转换为RGBA
        if (bit_depth == 16)
            png_set_strip_16(png);

        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);

        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);

        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);

        if (color_type == PNG_COLOR_TYPE_RGB ||
            color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);

        png_read_update_info(png, info);

        // 读取APNG帧信息
        png_uint_32 num_frames = 1;
        png_uint_32 num_plays = 0;

        if (png_get_valid(png, info, PNG_INFO_acTL)) {
            png_get_acTL(png, info, &num_frames, &num_plays);
        }

        // 若是静态图像，不再处理
        if (num_frames <= 1) {
            png_destroy_read_struct(&png, &info, nullptr);
            return frames;
        }

        cv::Mat previous_frame(height, width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
        cv::Mat canvas(height, width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
        cv::Mat current_tile;   // 存储当前帧的局部图像（可能小于画布）
        cv::Mat output_frame;

        for (png_uint_32 frameIdx = 0; frameIdx < num_frames; ++frameIdx) {
            png_read_frame_head(png, info);

            png_uint_32 frame_width = 0, frame_height = 0;
            png_uint_32 frame_x_offset = 0, frame_y_offset = 0;
            png_uint_16 delay_num = 0, delay_den = 0;
            png_byte dispose_op = 0, blend_op = 0;

            png_get_next_frame_fcTL(png, info, &frame_width, &frame_height, &frame_x_offset, &frame_y_offset,
                &delay_num, &delay_den, &dispose_op, &blend_op);

            int delay_ms = delay_num * 1000 / (delay_den ? delay_den : 100);

            // 保存渲染前的画布状态（用于DISPOSE_OP_PREVIOUS情况）
            cv::Mat canvas_before_frame;
            if (dispose_op == 2) { // PNG_DISPOSE_OP_PREVIOUS
                canvas.copyTo(canvas_before_frame);
            }

            // 读取当前帧的数据到current_tile
            current_tile = cv::Mat(frame_height, frame_width, CV_8UC4, cv::Scalar(0, 0, 0, 0));

            // 为当前帧创建行指针数组
            std::vector<png_bytep> row_pointers(frame_height);
            for (png_uint_32 y = 0; y < frame_height; ++y) {
                row_pointers[y] = current_tile.data + y * current_tile.step;
            }

            // 读取当前帧的图像数据
            png_read_image(png, row_pointers.data());

            // 根据blend_op处理当前帧和画布的混合
            cv::Rect frame_rect(frame_x_offset, frame_y_offset, frame_width, frame_height);

            if (blend_op == 0) { // PNG_BLEND_OP_SOURCE - 直接替换
                // 创建ROI并直接复制
                cv::Mat roi = canvas(frame_rect);
                current_tile.copyTo(roi);
            }
            else { // PNG_BLEND_OP_OVER - Alpha混合
                // 对每个像素进行Alpha混合
                for (png_uint_32 y = 0; y < frame_height; ++y) {
                    for (png_uint_32 x = 0; x < frame_width; ++x) {
                        // 跳过画布边界外的像素
                        if (frame_x_offset + x >= width || frame_y_offset + y >= height)
                            continue;

                        cv::Vec4b& canvas_pixel = canvas.at<cv::Vec4b>(frame_y_offset + y, frame_x_offset + x);
                        const cv::Vec4b& tile_pixel = current_tile.at<cv::Vec4b>(y, x);

                        // Alpha混合 RGBA格式：R=0, G=1, B=2, A=3
                        float alpha_src = tile_pixel[3] / 255.0f;
                        float alpha_dst = canvas_pixel[3] / 255.0f;
                        float alpha_out = alpha_src + alpha_dst * (1 - alpha_src);

                        if (alpha_out > 0) {
                            for (int c = 0; c < 3; ++c) { // RGB通道
                                canvas_pixel[c] = static_cast<uchar>(
                                    (tile_pixel[c] * alpha_src + canvas_pixel[c] * alpha_dst * (1 - alpha_src)) / alpha_out
                                    );
                            }
                            // 更新Alpha通道
                            canvas_pixel[3] = static_cast<uchar>(alpha_out * 255);
                        }
                    }
                }
            }

            // 保存当前帧作为输出
            canvas.copyTo(output_frame);

            // 根据dispose_op为下一帧准备画布
            if (dispose_op == 1) { // PNG_DISPOSE_OP_BACKGROUND
                // 清除当前帧区域为透明
                cv::Mat roi = canvas(frame_rect);
                roi = cv::Scalar(0, 0, 0, 0);
            }
            else if (dispose_op == 2) { // PNG_DISPOSE_OP_PREVIOUS
                // 恢复到渲染当前帧之前的状态
                canvas_before_frame.copyTo(canvas);
            }
            // dispose_op == 0 (PNG_DISPOSE_OP_NONE)情况下什么都不做，保留当前画布

            cv::cvtColor(output_frame, output_frame, cv::COLOR_RGBA2BGRA);
            frames.push_back({ output_frame.clone(), delay_ms }); // 注意要clone以保持独立副本
        }

        // 清理
        png_destroy_read_struct(&png, &info, nullptr);

        return frames;
    }

    void handleExifOrientation(int orientation, cv::Mat& img) {
        if (img.empty())
            return;

        switch (orientation) {
        case 2: // 水平翻转
            cv::flip(img, img, 1);
            break;
        case 3: // 旋转180度
            cv::rotate(img, img, cv::ROTATE_180);
            break;
        case 4: // 垂直翻转
            cv::flip(img, img, 0);
            break;
        case 5: // 顺时针旋转90度后垂直翻转
            cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);
            cv::flip(img, img, 0);
            break;
        case 6: // 顺时针旋转90度
            cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);
            break;
        case 7: // 顺时针旋转90度后水平翻转
            cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);
            cv::flip(img, img, 1);
            break;
        case 8: // 逆时针旋转90度
            cv::rotate(img, img, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        }
    }

    Frames loader(const wstring& path) {
        if (path.length() < 4) {
            Utils::log("path.length() < 4: {}", Utils::wstringToUtf8(path));
            return { {{getErrorTipsMat(), 0}}, "" };
        }

        auto f = _wfopen(path.c_str(), L"rb");
        if (f == nullptr) {
            Utils::log("path canot open: {}", Utils::wstringToUtf8(path));
            return { {{getErrorTipsMat(), 0}}, "" };
        }

        fseek(f, 0, SEEK_END);
        auto fileSize = ftell(f);

        if (fileSize == 0) {
            fclose(f);
            Utils::log("path fileSize == 0: {}", Utils::wstringToUtf8(path));
            return { {{getErrorTipsMat(), 0}}, "" };
        }

        fseek(f, 0, SEEK_SET);
        vector<uint8_t> fileBuf(fileSize, 0);
        fread(fileBuf.data(), 1, fileSize, f);
        fclose(f);

        auto dotPos = path.rfind(L'.');
        auto ext = (dotPos != std::wstring::npos && dotPos < path.size() - 1) ?
            path.substr(dotPos) : path;
        for (auto& c : ext)	c = std::tolower(c);

        Frames ret;

        if (ext == L".gif") { //静态或动画
            ret.imgList = loadGif(path, fileBuf);
            if (ret.imgList.empty()) {
                ret.imgList.push_back({ getErrorTipsMat(), 0 });
                ret.exifStr = ExifParse::getSimpleInfo(path, 0, 0, fileBuf.data(), fileBuf.size()) +
                    "\n文件头32字节: " + Utils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
            }
            else {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size());
            }
            return ret;
        }

        if (ext == L".bpg") { //静态或动画
            ret.imgList = loadBPG(path, fileBuf);
            if (ret.imgList.empty()) {
                ret.imgList.push_back({ getErrorTipsMat(), 0 });
                ret.exifStr = ExifParse::getSimpleInfo(path, 0, 0, fileBuf.data(), fileBuf.size()) +
                    "\n文件头32字节: " + Utils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
            }
            else {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size())
                    + ExifParse::getExif(path, fileBuf.data(), fileBuf.size());
            }
            return ret;
        }

        if (ext == L".jxl") { //静态或动画
            ret.imgList = loadJXL(path, fileBuf);
            if (ret.imgList.empty()) {
                ret.imgList.push_back({ getErrorTipsMat(), 0 });
                ret.exifStr = ExifParse::getSimpleInfo(path, 0, 0, fileBuf.data(), fileBuf.size()) +
                    "\n文件头32字节: " + Utils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
            }
            else {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size())
                    + ExifParse::getExif(path, fileBuf.data(), fileBuf.size());
            }
            return ret;
        }

        if (ext == L".wp2") { // webp2 静态或动画
            ret.imgList = loadWP2(path, fileBuf);
            if (ret.imgList.empty()) {
                ret.imgList.push_back({ getErrorTipsMat(), 0 });
                ret.exifStr = ExifParse::getSimpleInfo(path, 0, 0, fileBuf.data(), fileBuf.size()) +
                    "\n文件头32字节: " + Utils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
            }
            else {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size())
                    + ExifParse::getExif(path, fileBuf.data(), fileBuf.size());
            }
            return ret;
        }

        if (ext == L".webp") { //静态或动画
            ret.imgList = loadWebp(path, fileBuf);
            if (!ret.imgList.empty()) {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size())
                    + ExifParse::getExif(path, fileBuf.data(), fileBuf.size());
                return ret;
            }
            // 若空白则往下执行，使用opencv读取
        }

        if (ext == L".png" || ext == L".apng") {
            ret.imgList = loadApng(path, fileBuf); //若是静态图像则不解码，返回空
            if (!ret.imgList.empty()) {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size())
                    + ExifParse::getExif(path, fileBuf.data(), fileBuf.size());
                return ret;
            }
            // 若空白则往下执行，使用opencv读取
        }

        cv::Mat img;

        if (ext == L".livp") {
            auto [picBuff, extName] = unzipLivp(fileBuf);
            if (picBuff.empty()) {
                ret.exifStr = ExifParse::getSimpleInfo(path, 0, 0, fileBuf.data(), fileBuf.size());
                if (img.empty()) {
                    img = getErrorTipsMat();
                }
            }
            else {
                fileBuf = std::move(picBuff);
                if (extName == "heic" || extName == "heif") {
                    img = loadHeic(path, fileBuf);
                    ext = L".heic";
                }
                else if (extName == ".jpg" || extName == "jpeg") {
                    img = loadMat(path, fileBuf);
                    ext = L".jpg";
                }
            }
        }else if (ext == L".heic" || ext == L".heif") {
            img = loadHeic(path, fileBuf);
        }
        else if (ext == L".avif" || ext == L".avifs") {
            img = loadAvif(path, fileBuf);
        }
        else if (ext == L".jxr") {
            img = loadJXR(path, fileBuf);
        }
        else if (ext == L".tga" || ext == L".hdr") {
            img = loadTGA_HDR(path, fileBuf);
        }
        else if (ext == L".svg") {
            img = loadSVG(path, fileBuf);
            ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size());
            if (img.empty()) {
                img = getErrorTipsMat();
            }
        }
        else if (ext == L".qoi") {
            img = loadQOI(path, fileBuf);
            ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size());
            if (img.empty()) {
                img = getErrorTipsMat();
            }
        }
        else if (ext == L".ico" || ext == L".icon") {
            std::tie(img, ret.exifStr) = loadICO(path, fileBuf);
        }
        else if (ext == L".psd") {
            img = loadPSD(path, fileBuf);
            if (img.empty())
                img = getErrorTipsMat();
        }
        else if (ext == L".pfm") {
            img = loadPFM(path, fileBuf);
            if (img.empty()) {
                img = getErrorTipsMat();
                ret.exifStr = ExifParse::getSimpleInfo(path, 0, 0, fileBuf.data(), fileBuf.size());
            }
            else {
                ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size());
            }
        }
        else if (supportRaw.contains(ext)) {
            img = loadRaw(path, fileBuf);
            if (img.empty())
                img = getErrorTipsMat();
        }

        if (img.empty())
            img = loadMat(path, fileBuf);

        if (ret.exifStr.empty()) {
            auto exifTmp = ExifParse::getExif(path, fileBuf.data(), fileBuf.size());
            if (ext != L".heic" && ext != L".heif") { //此格式已经在解码过程应用了裁剪/旋转/镜像等操作
                const size_t idx = exifTmp.find("\n方向: ");
                if (idx != string::npos) {
                    handleExifOrientation(exifTmp[idx + 9] - '0', img);
                }
            }
            ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, fileBuf.data(), fileBuf.size()) + exifTmp;
        }

        if (img.empty())
            img = getErrorTipsMat();
        else if (img.type() == CV_8UC3)
            cv::cvtColor(img, img, cv::COLOR_BGR2BGRA);

        ret.imgList.emplace_back(img, 0);

        return ret;
    }
};