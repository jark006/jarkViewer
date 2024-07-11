#pragma once
#include "Utils.h"
#include "exifParse.h"
#include "LRU.h"

class ImageDatabase{
public:

    static inline const unordered_set<wstring> supportExt{
        L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png",
        L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
        L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
        L".heic", L".heif", L".avif", L".avifs", L".gif", L".jxl",
        L".ico", L".icon", L".psd",
    };

    static inline const unordered_set<wstring> supportRaw {
        L".crw", L".cr2", L".cr3", // Canon
        L".nef", // Nikon
        L".arw", L".srf", L".sr2", // Sony
        L".pef", // Pentax
        L".orf", // Olympus
        L".rw2", // Panasonic
        L".raf", // Fujifilm
        L".kdc", // Kodak
        L".raw", L".dng", // Leica
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

    auto& operator()() {
        return database;
    }

    // HEIC ONLY, AVIF not support
    // https://github.com/strukturag/libheif
    // vcpkg install libheif:x64-windows-static
    // vcpkg install libheif[hevc]:x64-windows-static
    static cv::Mat loadHeic(const wstring& path, const vector<uchar>& buf, int fileSize) {

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
    static cv::Mat loadAvif(const wstring& path, const vector<uchar>& buf, int fileSize) {
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

    static cv::Mat loadRaw(const wstring& path, const vector<uchar>& buf, int fileSize) {
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

    static const std::string jxlStatusCode2String(JxlDecoderStatus status) {
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

    static cv::Mat loadJXL(const wstring& path, const vector<uchar>& buf, int fileSize) {

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

    static cv::Mat readDibFromMemory(const uint8_t* data, size_t size) {
        // 确保有足够的数据用于DIB头
        if (size < sizeof(DibHeader)) {
            Utils::log("Insufficient data for DIB header. {}", size);
            return cv::Mat();
        }

        // 读取DIB头
        DibHeader header;
        std::memcpy(&header, data, sizeof(DibHeader));

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
        if (size < header.headerSize + paletteSize + header.imageSize) {
            Utils::log("Insufficient data for image.");
            return cv::Mat();
        }

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
        cv::Mat image(imageHeight, header.width, header.bitCount <= 8 ? CV_8UC3 : CV_8UC4);

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
                    image.at<cv::Vec3b>(y, x) = cv::Vec3b(palette[colorIndex][0], palette[colorIndex][1], palette[colorIndex][2]);
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
                    image.at<cv::Vec3b>(y, x) = cv::Vec3b(palette[colorIndex][0], palette[colorIndex][1], palette[colorIndex][2]);
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
                    image.at<cv::Vec3b>(y, x) = cv::Vec3b(palette[colorIndex][0], palette[colorIndex][1], palette[colorIndex][2]);
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
                    image.at<cv::Vec3b>(y, x) = cv::Vec3b(b << 3, g << 3, r << 3);
                }
            }
            break;
        }
        default:
            Utils::log("Unsupported bit count {}", header.bitCount);
            return cv::Mat();
        }

        // 如果存在透明度掩码
        if (hasAlphaMask) {
            cv::Mat alphaMask(imageHeight, header.width, CV_8UC1);
            const uint8_t* maskData = pixelData + header.imageSize;

            int byteWidth = (header.width + 7) / 8; // 每行字节数
            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    int byteIndex = (imageHeight - y - 1) * byteWidth + x / 8;
                    int bitIndex = x % 8;
                    int alpha = ((maskData[byteIndex] >> (7 - bitIndex)) & 0x01) ? 0 : 255;
                    alphaMask.at<uint8_t>(y, x) = alpha;
                }
            }

            // 将Alpha掩码应用到图像中
            if (image.channels() == 3) {
                cv::cvtColor(image, image, cv::COLOR_BGR2BGRA);
            }

            for (int y = 0; y < imageHeight; ++y) {
                for (int x = 0; x < header.width; ++x) {
                    image.at<cv::Vec4b>(y, x)[3] = alphaMask.at<uint8_t>(y, x);
                }
            }
        }

        return image;
    }

    static cv::Mat loadICO(const wstring& path, const vector<uchar>& buf, int fileSize) {
        if (buf.size() < 6) {
            Utils::log("Invalid ICO file: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        uint16_t numImages = *reinterpret_cast<const uint16_t*>(&buf[4]);
        std::vector<IconDirEntry> entries;

        if (numImages > 255) {
            Utils::log("numImages Error: {} {}", Utils::wstringToUtf8(path), numImages);
            return cv::Mat();
        }

        size_t offset = 6;
        for (int i = 0; i < numImages; ++i) {
            if (offset + sizeof(IconDirEntry) > buf.size()) {
                Utils::log("Invalid ICO file structure: {}", Utils::wstringToUtf8(path));
                return cv::Mat();
            }

            entries.push_back(*reinterpret_cast<const IconDirEntry*>(&buf[offset]));
            offset += sizeof(IconDirEntry);
        }

        auto maxResEntry = std::max_element(entries.begin(), entries.end(),
            [](const IconDirEntry& a, const IconDirEntry& b) {
                if (a.width == 0)return false;
                if (b.width == 0)return true;
                return a.width < b.width;
            });

        if (maxResEntry == entries.end()) {
            Utils::log("No valid image entries found: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        int width = maxResEntry->width == 0 ? 256 : maxResEntry->width;
        int height = maxResEntry->height == 0 ? 256 : maxResEntry->height;

        if (maxResEntry->dataOffset + maxResEntry->dataSize > buf.size()) {
            Utils::log("Invalid image data offset or size: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        cv::Mat rawData(1, maxResEntry->dataSize, CV_8UC1, (uint8_t*)(buf.data() + maxResEntry->dataOffset));

        // PNG BMP
        if (memcmp(rawData.ptr(), "\x89PNG", 4) == 0 || memcmp(rawData.ptr(), "BM", 2) == 0)
            return cv::imdecode(rawData, cv::IMREAD_UNCHANGED);

        DibHeader* dibHeader = (DibHeader*)(buf.data() + maxResEntry->dataOffset);
        if (dibHeader->headerSize != 0x28)
            return cv::Mat();

        size_t byteSize = (size_t)width * height * maxResEntry->bitsPerPixel / 8;
        if (maxResEntry->bitsPerPixel >= 24 && maxResEntry->dataSize >= byteSize + dibHeader->headerSize) {
            auto imgOrg = cv::Mat(height, width, maxResEntry->bitsPerPixel == 24 ? CV_8UC3 : CV_8UC4,
                (uint8_t*)(buf.data() + maxResEntry->dataOffset + dibHeader->headerSize));
            cv::Mat img;

            if (maxResEntry->bitsPerPixel == 24)
                cv::cvtColor(imgOrg, img, CV_BGR2BGRA);
            else img = imgOrg.clone();

            uint32_t* ptr = (uint32_t*)img.ptr();
            // 上下翻转Y轴， 即上下翻转
            for (int y = 0; y < height / 2; y++)
                for (int x = 0; x < width; x++) {
                    uint32_t tmp = ptr[y * width + x];
                    ptr[y * width + x] = ptr[(height - 1 - y) * width + x];
                    ptr[(height - 1 - y) * width + x] = tmp;
                }
            return img;
        }

        return readDibFromMemory((uint8_t*)(buf.data() + maxResEntry->dataOffset), maxResEntry->dataSize);
    }


    static cv::Mat loadPSD(const wstring& path, const vector<uchar>& buf, int fileSize) {
        psd_context* context = nullptr;
        psd_status status = psd_image_load_from_memory(&context, (psd_char*)buf.data(), buf.size());

        if (status != psd_status_done) {
            Utils::log("Failed to load PSD file: {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        if (context->width == 0 || context->height == 0) {
            Utils::log("PSD error {} width:{} height:{}", Utils::wstringToUtf8(path), context->width, context->height);
            return cv::Mat();
        }

        const auto* imageData = context->merged_image_data;
        if (!imageData) {
            psd_image_free(context);
            Utils::log("Failed to get image data from PSD file {}", Utils::wstringToUtf8(path));
            return cv::Mat();
        }

        auto img = cv::Mat(context->height, context->width, CV_8UC4, (void*)imageData).clone();
        psd_image_free(context);

        return img;
    }

    static vector<cv::Mat> loadMats(const wstring& path, const vector<uchar>& buf, int fileSize) {
        vector<cv::Mat> imgs;

        if (!cv::imdecodemulti(buf, cv::IMREAD_UNCHANGED, imgs)) {
            return imgs;
        }
        return imgs;
    }

    static cv::Mat loadMat(const wstring& path, const vector<uchar>& buf, int fileSize) {
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


    static std::vector<ImageNode> loadGif(const wstring& path, const vector<uchar>& buf, size_t size) {

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

    static Frames loadImage(const wstring& path) {
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
        else if (ext == L".ico" || ext == L".icon") {
            img = loadICO(path, buf, fileSize);
            if (img.empty())
                img = getDefaultMat();
            ret.exifStr = ExifParse::getSimpleInfo(path, img.cols, img.rows, buf.data(), fileSize);
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

        ret.imgList.emplace_back(img, 0);

        return ret;
    }

private:

    LRU<wstring, Frames> database{ &ImageDatabase::loadImage };
};