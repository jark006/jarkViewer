#pragma once
#include "Utils.h"
#include "exifParse.h"
#include "LRU.h"

class ImageDatabase{
public:

    static inline const unordered_set<wstring> supportExt {
        L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png",
        L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
        L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
        L".heic", L".heif", L".avif", L".avifs", L".gif", L".jxl",
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
            std::vector<char> imgData(rc.addr, rc.addr + rc.size);
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
            std::vector<char> imgData(rc.addr, rc.addr + rc.size);
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

    // TODO
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
        vector<uchar> tmp(fileSize, 0);
        fread(tmp.data(), 1, fileSize, f);
        fclose(f);

        auto dotPos = path.rfind(L'.');
        auto ext = (dotPos != std::wstring::npos && dotPos < path.size() - 1) ?
            path.substr(dotPos) : path;
        for (auto& c : ext)	c = std::tolower(c);

        Frames ret;

        if (ext == L".gif") {
            ret.imgList = loadGif(path, tmp, fileSize);
            if (!ret.imgList.empty() && ret.imgList.front().delay >= 0)
                ret.exifStr = ExifParse::getSimpleInfo(path, ret.imgList.front().img.cols, 
                    ret.imgList.front().img.rows, tmp.data(), fileSize);
            return ret;
        }

        cv::Mat img, imgBGRA;
        if (ext == L".heic" || ext == L".heif") {
            img = loadHeic(path, tmp, fileSize);
        }
        else if (ext == L".avif" || ext == L".avifs") {
            img = loadAvif(path, tmp, fileSize);
        }
        else if (supportRaw.contains(ext)) {
            img = loadRaw(path, tmp, fileSize);
        }

        if (img.empty())
            img = loadMat(path, tmp, fileSize);

        if (ret.exifStr.empty())
            ret.exifStr = ExifParse::getExif(path, img.cols, img.rows, tmp.data(), fileSize);

        if (img.empty())
            img = getDefaultMat();

        if (img.channels() == 1)
            cv::cvtColor(img, imgBGRA, cv::COLOR_GRAY2BGR);
        else
            imgBGRA = std::move(img);

        ret.imgList.emplace_back(imgBGRA, 0);

        return ret;
    }

private:

    LRU<wstring, Frames> database{ &ImageDatabase::loadImage };
};