#pragma once
#include "Utils.h"
#include "exifParse.h"
#include "LRU.h"

using namespace psd;

class ImageDatabase:public LRU<wstring, Frames> {
public:

    static inline const unordered_set<wstring> supportExt{
        L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png", L".apng",
        L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
        L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
        L".heic", L".heif", L".avif", L".avifs", L".gif", L".jxl",
        L".ico", L".icon", L".psd", L".tga", L".svg", L".jfif"
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
        L".3fr"  // Hasselblad
    };

    static cv::Mat& getDefaultMat() {
        static cv::Mat defaultMat;
        static bool isInit = false;

        if (!isInit) {
            isInit = true;
            auto rc = Utils::GetResource(IDB_PNG, L"PNG");
            cv::Mat imgData(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr);
            defaultMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
        }
        return defaultMat;
    }

    static cv::Mat& getHomeMat() {
        static cv::Mat homeMat;
        static bool isInit = false;

        if (!isInit) {
            isInit = true;
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
    cv::Mat loadHeic(const wstring& path, const vector<uchar>& buf, int fileSize) {

        auto exifStr = std::format("路径: {}\n大小: {}",
            Utils::wstringToUtf8(path), Utils::size2Str(fileSize));

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
        auto err = heif_context_read_from_memory_without_copy(ctx, buf.data(), fileSize, nullptr);
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

        cv::Mat matImg(height, width, CV_8UC4);

        auto srcPtr = (uint32_t*)data;
        auto dstPtr = (uint32_t*)matImg.ptr();
        auto pixelCount = (size_t)width * height;
        for (size_t i = 0; i < pixelCount; ++i) { // ARGB -> ABGR
            uint32_t pixel = srcPtr[i];
            uint32_t r = (pixel >> 16) & 0xFF;
            uint32_t b = pixel & 0xFF;
            dstPtr[i] = (b << 16) | (pixel & 0xff00ff00) | r;
        }

        // clean up resources
        heif_context_free(ctx);
        heif_image_release(img);
        heif_image_handle_release(handle);

        return matImg;
    }

    // vcpkg install libavif[aom]:x64-windows-static libavif[dav1d]:x64-windows-static
    // https://github.com/AOMediaCodec/libavif/issues/1451#issuecomment-1606903425
    cv::Mat loadAvif(const wstring& path, const vector<uchar>& buf, int fileSize) {
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

        avifResult result = avifDecoderReadMemory(decoder, image, buf.data(), fileSize);
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
            memcpy(ret.ptr(), rgb.pixels, (size_t)rgb.width * rgb.height * 4);
        }
        else {
            Utils::log("rgb.depth: {} {}", rgb.depth, Utils::wstringToUtf8(path));
            const uint16_t* src = (uint16_t*)rgb.pixels;
            uint8_t* dst = ret.ptr();
            int bitShift = 2;
            switch (rgb.depth) {
            case 10: bitShift = 2; break;
            case 12: bitShift = 4; break;
            case 16: bitShift = 8; break;
            }
            const size_t length = (size_t)rgb.width * rgb.height * 4;
            for (size_t i = 0; i < length; ++i) {
                dst[i] = (uint8_t)(src[i] >> bitShift);
            }
        }

        avifRGBImageFreePixels(&rgb);
        return ret;
    }

    cv::Mat loadRaw(const wstring& path, const vector<uchar>& buf, int fileSize) {
        if (buf.empty() || fileSize <= 0) {
            Utils::log("Buf is empty: {} {}", Utils::wstringToUtf8(path), fileSize);
            return cv::Mat();
        }

        auto rawProcessor = std::make_unique<LibRaw>();

        int ret = rawProcessor->open_buffer(buf.data(), fileSize);
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

        cv::Mat img(image->height, image->width, CV_8UC3, image->data);

        cv::Mat bgrImage;
        cv::cvtColor(img, bgrImage, cv::COLOR_RGB2BGR);

        // Clean up
        LibRaw::dcraw_clear_mem(image);
        rawProcessor->recycle();

        return bgrImage;
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

    cv::Mat loadJXL(const wstring& path, const vector<uchar>& buf, int fileSize) {

        uint32_t xsize = 0, ysize = 0;
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
            return mat;
        }

        status = JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner, runner.get());
        if (JXL_DEC_SUCCESS != status) {
            Utils::log("JxlDecoderSetParallelRunner failed\n{}\n{}",
                Utils::wstringToUtf8(path),
                jxlStatusCode2String(status));
            return mat;
        }

        JxlBasicInfo info;
        JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

        JxlDecoderSetInput(dec.get(), buf.data(), fileSize);
        JxlDecoderCloseInput(dec.get());

        for (;;) {
            status = JxlDecoderProcessInput(dec.get());

            if (status == JXL_DEC_ERROR) {
                Utils::log("Decoder error\n{}\n{}",
                    Utils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return mat;
            }
            else if (status == JXL_DEC_NEED_MORE_INPUT) {
                Utils::log("Error, already provided all input\n{}\n{}",
                    Utils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return mat;
            }
            else if (status == JXL_DEC_BASIC_INFO) {
                if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                    Utils::log("JxlDecoderGetBasicInfo failed\n{}\n{}",
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return mat;
                }
                xsize = info.xsize;
                ysize = info.ysize;
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
                    return mat;
                }
                auto byteSizeRequire = 4ULL * xsize * ysize;
                if (buffer_size != byteSizeRequire) {
                    Utils::log("Invalid out buffer size {} {}\n{}\n{}",
                        buffer_size, byteSizeRequire,
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return mat;
                }
                mat = cv::Mat(ysize, xsize, CV_8UC4);
                status = JxlDecoderSetImageOutBuffer(dec.get(), &format, mat.ptr(), byteSizeRequire);
                if (JXL_DEC_SUCCESS != status) {
                    Utils::log("JxlDecoderSetImageOutBuffer failed\n{}\n{}",
                        Utils::wstringToUtf8(path),
                        jxlStatusCode2String(status));
                    return mat;
                }
            }
            else if (status == JXL_DEC_FULL_IMAGE) {
                // Nothing to do. Do not yet return. If the image is an animation, more
                // full frames may be decoded. This example only keeps the last one.
            }
            else if (status == JXL_DEC_SUCCESS) {
                // All decoding successfully finished.
                // It's not required to call JxlDecoderReleaseInput(dec.get()) here since
                // the decoder will be destroyed.

                auto srcPtr = (uint32_t* const)mat.ptr();
                auto pixelCount = (size_t)xsize * ysize;
                for (size_t i = 0; i < pixelCount; ++i) { // ARGB -> ABGR
                    uint32_t& pixel = srcPtr[i];
                    uint32_t r = (pixel >> 16) & 0xFF;
                    uint32_t b = pixel & 0xFF;
                    pixel = (b << 16) | (pixel & 0xff00ff00) | r;
                }
                return mat;
            }
            else {
                Utils::log("Unknown decoder status\n{}\n{}",
                    Utils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return mat;
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
    std::tuple<cv::Mat, string> loadICO(const wstring& path, const vector<uchar>& buf, int fileSize) {
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
                Utils::log("endOffset {} buf.size(): {}", endOffset, buf.size());
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
                img = getDefaultMat();
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
    void* ExpandChannelToCanvas(Allocator* allocator, const DataHolder* layer, const void* data, unsigned int canvasWidth, unsigned int canvasHeight)
    {
        T* canvasData = static_cast<T*>(allocator->Allocate(sizeof(T) * canvasWidth * canvasHeight, 16u));
        memset(canvasData, 0u, sizeof(T) * canvasWidth * canvasHeight);

        imageUtil::CopyLayerData(static_cast<const T*>(data), canvasData, layer->left, layer->top, layer->right, layer->bottom, canvasWidth, canvasHeight);

        return canvasData;
    }


    void* ExpandChannelToCanvas(const Document* document, Allocator* allocator, Layer* layer, Channel* channel)
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
    void* ExpandMaskToCanvas(const Document* document, Allocator* allocator, T* mask)
    {
        if (document->bitsPerChannel == 8)
            return ExpandChannelToCanvas<uint8_t>(allocator, mask, mask->data, document->width, document->height);
        else if (document->bitsPerChannel == 16)
            return ExpandChannelToCanvas<uint16_t>(allocator, mask, mask->data, document->width, document->height);
        else if (document->bitsPerChannel == 32)
            return ExpandChannelToCanvas<float32_t>(allocator, mask, mask->data, document->width, document->height);

        return nullptr;
    }


    unsigned int FindChannel(Layer* layer, int16_t channelType)
    {
        for (unsigned int i = 0; i < layer->channelCount; ++i)
        {
            Channel* channel = &layer->channels[i];
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
    T* CreateInterleavedImage(Allocator* allocator, const void* srcR, const void* srcG, const void* srcB, unsigned int width, unsigned int height)
    {
        T* image = static_cast<T*>(allocator->Allocate(4ULL * width * height * sizeof(T), 16u));

        const T* r = static_cast<const T*>(srcR);
        const T* g = static_cast<const T*>(srcG);
        const T* b = static_cast<const T*>(srcB);
        imageUtil::InterleaveRGB(b, g, r, TmpValue<T>::alphaMax, image, width, height); // RGB -> BGR

        return image;
    }


    template <typename T>
    T* CreateInterleavedImage(Allocator* allocator, const void* srcR, const void* srcG, const void* srcB, const void* srcA, unsigned int width, unsigned int height)
    {
        T* image = static_cast<T*>(allocator->Allocate(4ULL * width * height * sizeof(T), 16u));

        const T* r = static_cast<const T*>(srcR);
        const T* g = static_cast<const T*>(srcG);
        const T* b = static_cast<const T*>(srcB);
        const T* a = static_cast<const T*>(srcA);
        imageUtil::InterleaveRGBA(b, g, r, a, image, width, height); // RGB -> BGR

        return image;
    }


    // https://github.com/MolecularMatters/psd_sdk
    cv::Mat loadPSD(const wstring& path, const vector<uchar>& buf, int fileSize) {
        cv::Mat img;

        MallocAllocator allocator;
        NativeFile file(&allocator);

        if (!file.OpenRead(path.c_str())) {
            Utils::log("Cannot open file {}", Utils::wstringToUtf8(path));
            return img;
        }

        Document* document = CreateDocument(&file, &allocator);
        if (!document) {
            Utils::log("Cannot create document {}", Utils::wstringToUtf8(path));
            file.Close();
            return img;
        }

        // the sample only supports RGB colormode
        if (document->colorMode != colorMode::RGB)
        {
            Utils::log("Document is not in RGB color mode {}", Utils::wstringToUtf8(path));
            DestroyDocument(document, &allocator);
            file.Close();
            return img;
        }

        // extract all layers and masks.
        bool hasTransparencyMask = false;
        LayerMaskSection* layerMaskSection = ParseLayerMaskSection(document, &file, &allocator);
        if (layerMaskSection)
        {
            hasTransparencyMask = layerMaskSection->hasTransparencyMask;

            // extract all layers one by one. this should be done in parallel for maximum efficiency.
            for (unsigned int i = 0; i < layerMaskSection->layerCount; ++i)
            {
                Layer* layer = &layerMaskSection->layers[i];
                ExtractLayer(document, &file, &allocator, layer);

                // check availability of R, G, B, and A channels.
                // we need to determine the indices of channels individually, because there is no guarantee that R is the first channel,
                // G is the second, B is the third, and so on.
                const unsigned int indexR = FindChannel(layer, channelType::R);
                const unsigned int indexG = FindChannel(layer, channelType::G);
                const unsigned int indexB = FindChannel(layer, channelType::B);
                const unsigned int indexA = FindChannel(layer, channelType::TRANSPARENCY_MASK);

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
            ImageDataSection* imageData = ParseImageDataSection(document, &file, &allocator);
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

    cv::Mat loadTGA(const wstring& path, const vector<uchar>& buf, int fileSize) {
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

        // 如果是RGB或RGBA,需要转换颜色顺序
        if (channels == 3 || channels == 4) {
            cv::cvtColor(result, result, cv::COLOR_RGB2BGR);
        }

        return result;
    }

    cv::Mat loadSVG(const wstring& path, const vector<uchar>& buf, int fileSize){
        const int maxEdge = 1024;

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
        if (!bitmap.valid()) {
            Utils::log("Failed to render SVG to bitmap {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        return cv::Mat(height, width, CV_8UC4, bitmap.data()).clone();
    }

    vector<cv::Mat> loadMats(const wstring& path, const vector<uchar>& buf, int fileSize) {
        vector<cv::Mat> imgs;

        if (!cv::imdecodemulti(buf, cv::IMREAD_UNCHANGED, imgs)) {
            return imgs;
        }
        return imgs;
    }

    cv::Mat loadMat(const wstring& path, const vector<uchar>& buf, int fileSize) {
        auto img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);

        if (img.empty()) {
            Utils::log("cvMat cannot decode: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        if (img.channels() != 1 && img.channels() != 3 && img.channels() != 4) {
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


    std::vector<ImageNode> loadGif(const wstring& path, const vector<uchar>& buf, size_t size) {

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

        GifData data;
        memset(&data, 0, sizeof(data));
        data.m_lpBuffer = buf.data();
        data.m_nBufferSize = size;

        int error;
        GifFileType* gif = DGifOpen((void*)&data, InternalRead_Mem, &error);

        if (gif == nullptr) {
            Utils::log("DGifOpen: Error: {} {}", Utils::wstringToUtf8(path), GifErrorString(error));
            frames.push_back({ getDefaultMat(), 0 });
            return frames;
        }

        if (DGifSlurp(gif) != GIF_OK) {
            Utils::log("DGifSlurp Error: {} {}", Utils::wstringToUtf8(path), GifErrorString(gif->Error));
            DGifCloseFile(gif, &error);
            frames.push_back({ getDefaultMat(), 0 });
            return frames;
        }

        const auto byteSize = 4ULL * gif->SWidth * gif->SHeight;
        auto gifRaster = std::make_unique<GifByteType[]>(byteSize);

        memset(gifRaster.get(), 0, byteSize);

        for (int i = 0; i < gif->ImageCount; ++i) {
            SavedImage& image = gif->SavedImages[i];
            GraphicsControlBlock gcb;
            if (DGifSavedExtensionToGCB(gif, i, &gcb) != GIF_OK) {
                continue;
            }
            auto colorMap = image.ImageDesc.ColorMap != nullptr ? image.ImageDesc.ColorMap : gif->SColorMap;
            auto ptr = gifRaster.get();

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
                    ptr[pixelIdx] = color.Blue;
                    ptr[pixelIdx + 1] = color.Green;
                    ptr[pixelIdx + 2] = color.Red;
                    ptr[pixelIdx + 3] = 255;
                }
            }

            cv::Mat frame(gif->SHeight, gif->SWidth, CV_8UC4, gifRaster.get());
            cv::Mat cloneFrame;
            frame.copyTo(cloneFrame);
            frames.emplace_back(cloneFrame, gcb.DelayTime * 10);
        }

        DGifCloseFile(gif, nullptr);
        return frames;
    }


    std::vector<ImageNode> loadWebp(const wstring& path, const std::vector<uint8_t>& buf, size_t size) {
        std::vector<ImageNode> frames;

        WebPData webp_data{};
        webp_data.bytes = buf.data();
        webp_data.size = size;

        WebPDemuxer* demux = WebPDemux(&webp_data);
        if (!demux) {
            Utils::log("Failed to create WebP demuxer: {}", Utils::wstringToUtf8(path));
            frames.push_back({ getDefaultMat(), 0 });
            return frames;
        }

        uint32_t canvas_width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
        uint32_t canvas_height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);

        WebPIterator iter;
        cv::Mat lastFrame(canvas_height, canvas_width, CV_8UC4, cv::Scalar(0, 0, 0, 0)); // Initialize the global canvas with transparency
        cv::Mat canvas(canvas_height, canvas_width, CV_8UC4, cv::Scalar(0, 0, 0, 0)); // Initialize the global canvas with transparency

        if (WebPDemuxGetFrame(demux, 1, &iter)) {
            do {
                WebPDecoderConfig config;
                if (!WebPInitDecoderConfig(&config)) {
                    WebPDemuxReleaseIterator(&iter);
                    WebPDemuxDelete(demux);

                    Utils::log("Failed to initialize WebP decoder config: {}", Utils::wstringToUtf8(path));
                    frames.push_back({ getDefaultMat(), 0 });
                    return frames;
                }

                config.output.colorspace = WEBP_CSP_MODE::MODE_bgrA;

                if (WebPDecode(iter.fragment.bytes, iter.fragment.size, &config) != VP8_STATUS_OK) {
                    WebPFreeDecBuffer(&config.output);
                    WebPDemuxReleaseIterator(&iter);
                    WebPDemuxDelete(demux);

                    Utils::log("Failed to decode WebP frame: {}", Utils::wstringToUtf8(path));
                    frames.push_back({ getDefaultMat(), 0 });
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


    struct PngSource {
        const uint8_t* data;
        size_t size;
        int    offset;
    };

    std::vector<ImageNode> loadApng(const std::wstring& path, const std::vector<uint8_t>& buf, size_t size) {
        std::vector<ImageNode> frames;

        if (size < 8 || png_sig_cmp(buf.data(), 0, 8)) {
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

        PngSource pngSource{ buf.data(), size, 0 };
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

        // 处理每一帧
        for (png_uint_32 frame = 0; frame < num_frames; ++frame) {
            png_read_frame_head(png, info); // 若是静态图像(num_frames==1)，这里有会问题

            png_uint_32 frame_width = 0, frame_height = 0;
            png_uint_32 frame_x_offset = 0, frame_y_offset = 0;
            png_uint_16 delay_num = 0, delay_den = 0;
            png_byte dispose_op = 0, blend_op = 0;

            png_get_next_frame_fcTL(png, info, &frame_width, &frame_height, &frame_x_offset, &frame_y_offset, &delay_num, &delay_den, &dispose_op, &blend_op);

            // 计算延迟时间（毫秒）
            int delay = delay_num * 1000 / (delay_den ? delay_den : 100);

            // 创建完整尺寸的Mat
            cv::Mat full_frame(height, width, CV_8UC4, cv::Scalar(0, 0, 0, 0));

            // 读取当前帧数据
            std::vector<png_bytep> row_pointers(frame_height);
            for (png_uint_32 y = 0; y < frame_height; ++y) {
                row_pointers[y] = new png_byte[png_get_rowbytes(png, info)];
            }

            png_read_image(png, row_pointers.data());

            // 将帧数据复制到完整尺寸的Mat中
            for (png_uint_32 y = 0; y < frame_height; ++y) {
                std::memcpy(full_frame.ptr(frame_y_offset + y) + frame_x_offset * 4, row_pointers[y], 4ULL * frame_width);
            }

            // 释放行指针
            for (png_uint_32 y = 0; y < frame_height; ++y) {
                delete[] row_pointers[y];
            }

            // 将RGBA转换为BGRA
            cv::cvtColor(full_frame, full_frame, cv::COLOR_RGBA2BGRA);

            // 添加到结果vector
            frames.push_back({ full_frame, delay });
        }

        // 清理
        png_destroy_read_struct(&png, &info, nullptr);

        return frames;
    }

    Frames loader(const wstring& path) {
        if (path.length() < 4) {
            Utils::log("path.length() < 4: {}", Utils::wstringToUtf8(path));
            return { {{getDefaultMat(), 0}}, "" };
        }

        auto f = _wfopen(path.c_str(), L"rb");
        if (f == nullptr) {
            Utils::log("path canot open: {}", Utils::wstringToUtf8(path));
            return { {{getDefaultMat(), 0}}, "" };
        }

        fseek(f, 0, SEEK_END);
        auto fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        vector<uchar> buf(fileSize, 0);
        fread(buf.data(), 1, fileSize, f);
        fclose(f);

        auto dotPos = path.rfind(L'.');
        auto ext = (dotPos != std::wstring::npos && dotPos < path.size() - 1) ?
            path.substr(dotPos) : path;
        for (auto& c : ext)	c = std::tolower(c);

        Frames ret;

        if (ext == L".gif") {
            ret.imgList = loadGif(path, buf, fileSize);
            if (!ret.imgList.empty() && ret.imgList.front().delay >= 0)
                ret.exifStr = ExifParse::getSimpleInfo(path, ret.imgList.front().img.cols,
                    ret.imgList.front().img.rows, buf.data(), fileSize);
            return ret;
        }

        if (ext == L".webp") { //静态或动画
            ret.imgList = loadWebp(path, buf, fileSize);
            if (!ret.imgList.empty()) {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getExif(path, img.cols, img.rows, buf.data(), fileSize);
                return ret;
            }
            // 若空白则往下执行，使用opencv读取
        }

        if (ext == L".png" || ext == L".apng") {
            ret.imgList = loadApng(path, buf, fileSize); //若是静态图像则不解码，返回空
            if (!ret.imgList.empty()) {
                auto& img = ret.imgList.front().img;
                ret.exifStr = ExifParse::getExif(path, img.cols, img.rows, buf.data(), fileSize);
                return ret;
            }
            // 若空白则往下执行，使用opencv读取
        }

        cv::Mat img;
        if (ext == L".heic" || ext == L".heif") {
            img = loadHeic(path, buf, fileSize);
        }
        else if (ext == L".avif" || ext == L".avifs") {
            img = loadAvif(path, buf, fileSize);
        }
        else if (ext == L".jxl") {
            img = loadJXL(path, buf, fileSize);
        }
        else if (ext == L".tga") {
            img = loadTGA(path, buf, fileSize);
        }
        else if (ext == L".svg") {
            img = loadSVG(path, buf, fileSize);
            ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, buf.data(), fileSize);
            if (img.empty()) {
                img = getDefaultMat();
            }
        }
        else if (ext == L".ico" || ext == L".icon") {
            std::tie(img, ret.exifStr) = loadICO(path, buf, fileSize);
        }
        else if (ext == L".psd") {
            img = loadPSD(path, buf, fileSize);
            if (img.empty())
                img = getDefaultMat();
        }
        else if (supportRaw.contains(ext)) {
            img = loadRaw(path, buf, fileSize);
            if (img.empty())
                img = getDefaultMat();
        }

        if (img.empty())
            img = loadMat(path, buf, fileSize);

        if (ret.exifStr.empty())
            ret.exifStr = ExifParse::getExif(path, img.cols, img.rows, buf.data(), fileSize);

        if (img.empty())
            img = getDefaultMat();

        if (img.channels() == 1)
            cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);

        const size_t idx = ret.exifStr.find("\n方向: ");
        if (idx != string::npos) {
            int exifOrientation = ret.exifStr[idx + 9] - '0';

            switch (exifOrientation) {
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

        ret.imgList.emplace_back(img, 0);

        return ret;
    }
};