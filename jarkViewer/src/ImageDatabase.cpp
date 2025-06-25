#include "ImageDatabase.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#ifndef QOI_IMPLEMENTATION
#define QOI_IMPLEMENTATION
#include "qoi.h"
#endif

const std::string ImageDatabase::jxlStatusCode2String(JxlDecoderStatus status) {
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


std::vector<ImageNode> ImageDatabase::loadJXL(const wstring& path, const vector<uchar>& buf) {
    std::vector<ImageNode> frames;
    cv::Mat mat;

    // Multi-threaded parallel runner.
    auto runner = JxlResizableParallelRunnerMake(nullptr);

    auto dec = JxlDecoderMake(nullptr);
    JxlDecoderStatus status = JxlDecoderSubscribeEvents(dec.get(),
        JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE);
    if (JXL_DEC_SUCCESS != status) {
        jarkUtils::log("JxlDecoderSubscribeEvents failed\n{}\n{}",
            jarkUtils::wstringToUtf8(path),
            jxlStatusCode2String(status));
        return frames;
    }

    status = JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner, runner.get());
    if (JXL_DEC_SUCCESS != status) {
        jarkUtils::log("JxlDecoderSetParallelRunner failed\n{}\n{}",
            jarkUtils::wstringToUtf8(path),
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
            jarkUtils::log("Decoder error\n{}\n{}",
                jarkUtils::wstringToUtf8(path),
                jxlStatusCode2String(status));
            return frames;
        }
        else if (status == JXL_DEC_NEED_MORE_INPUT) {
            jarkUtils::log("Error, already provided all input\n{}\n{}",
                jarkUtils::wstringToUtf8(path),
                jxlStatusCode2String(status));
            return frames;
        }
        else if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                jarkUtils::log("JxlDecoderGetBasicInfo failed\n{}\n{}",
                    jarkUtils::wstringToUtf8(path),
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
            //    jarkUtils::log("JxlDecoderGetICCProfileSize failed\n{}\n{}",
            //        jarkUtils::wstringToUtf8(path),
            //        jxlStatusCode2String(status));
            //    return mat;
            //}
            //icc_profile.resize(icc_size);
            //if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
            //    dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
            //    icc_profile.data(), icc_profile.size())) {
            //    jarkUtils::log("JxlDecoderGetColorAsICCProfile failed\n{}\n{}",
            //        jarkUtils::wstringToUtf8(path),
            //        jxlStatusCode2String(status));
            //    return mat;
            //}
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t buffer_size;

            status = JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size);
            if (JXL_DEC_SUCCESS != status) {
                jarkUtils::log("JxlDecoderImageOutBufferSize failed\n{}\n{}",
                    jarkUtils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return frames;
            }
            auto byteSizeRequire = 4ULL * info.xsize * info.ysize;
            if (buffer_size != byteSizeRequire) {
                jarkUtils::log("Invalid out buffer size {} {}\n{}\n{}",
                    buffer_size, byteSizeRequire,
                    jarkUtils::wstringToUtf8(path),
                    jxlStatusCode2String(status));
                return frames;
            }
            mat = cv::Mat(info.ysize, info.xsize, CV_8UC4);
            status = JxlDecoderSetImageOutBuffer(dec.get(), &format, mat.ptr(), byteSizeRequire);
            if (JXL_DEC_SUCCESS != status) {
                jarkUtils::log("JxlDecoderSetImageOutBuffer failed\n{}\n{}",
                    jarkUtils::wstringToUtf8(path),
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
            jarkUtils::log("Unknown decoder status\n{}\n{}",
                jarkUtils::wstringToUtf8(path),
                jxlStatusCode2String(status));
            return frames;
        }
    }
}


std::vector<ImageNode> ImageDatabase::loadApng(const std::wstring& path, const std::vector<uint8_t>& buf) {
    std::vector<ImageNode> frames;

    if (buf.size() < 8 || png_sig_cmp(buf.data(), 0, 8)) {
        jarkUtils::log("FInvalid PNG signature in file: {}", jarkUtils::wstringToUtf8(path));
        return frames;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        jarkUtils::log("Failed to create PNG read struct for file: : {}", jarkUtils::wstringToUtf8(path));
        return frames;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        jarkUtils::log("Failed to create PNG info struct for file: {}", jarkUtils::wstringToUtf8(path));
        return frames;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        jarkUtils::log("Error during PNG file reading: {}", jarkUtils::wstringToUtf8(path));
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


std::vector<ImageNode> ImageDatabase::loadGif(const wstring& path, const vector<uchar>& buf) {

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
        jarkUtils::log("DGifOpen: Error: {} {}", jarkUtils::wstringToUtf8(path), GifErrorString(error));
        return frames;
    }
    int retCode = DGifSlurp(gif);
    //if (retCode != GIF_OK) { // 部分GIF可能无法完全解码
    //    jarkUtils::log("DGifSlurp Error: {} {}", jarkUtils::wstringToUtf8(path), GifErrorString(gif->Error));
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
                size_t pixelIdx = ((size_t)gifY * gif->SWidth + gifX) * 4ULL;
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
        jarkUtils::log("Gif decode 0 frames: {}", jarkUtils::wstringToUtf8(path));
    }

    return frames;
}


std::vector<ImageNode> ImageDatabase::loadWebp(const wstring& path, const std::vector<uint8_t>& buf) {
    std::vector<ImageNode> frames;

    WebPData webp_data{ buf.data(), buf.size() };
    WebPDemuxer* demux = WebPDemux(&webp_data);
    if (!demux) {
        jarkUtils::log("Failed to create WebP demuxer: {}", jarkUtils::wstringToUtf8(path));
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

                jarkUtils::log("Failed to initialize WebP decoder config: {}", jarkUtils::wstringToUtf8(path));
                frames.push_back({ getErrorTipsMat(), 0 });
                return frames;
            }

            config.output.colorspace = WEBP_CSP_MODE::MODE_bgrA;

            if (WebPDecode(iter.fragment.bytes, iter.fragment.size, &config) != VP8_STATUS_OK) {
                WebPFreeDecBuffer(&config.output);
                WebPDemuxReleaseIterator(&iter);
                WebPDemuxDelete(demux);

                jarkUtils::log("Failed to decode WebP frame: {}", jarkUtils::wstringToUtf8(path));
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


std::string ImageDatabase::statusExplain(WP2Status status) {
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

// https://chromium.googlesource.com/codecs/libwebp2  commit 96720e6410284ebebff2007d4d87d7557361b952  Date:   Mon Sep 9 18:11:04 2024 +0000
// 网络找的不少wp2图像无法解码，使用 libwebp2 的 cwp2.exe 工具编码的 .wp2 图片可以正常解码
std::vector<ImageNode> ImageDatabase::loadWP2(const wstring& path, const std::vector<uint8_t>& buf) {
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
        jarkUtils::log("ERROR: {}\n{}", jarkUtils::wstringToUtf8(path), statusExplain(status));
    }
#endif

    return frames;
}


std::vector<ImageNode> ImageDatabase::loadBPG(const std::wstring& path, const std::vector<uchar>& buf) {
    auto decoderContext = bpg_decoder_open();
    if (bpg_decoder_decode(decoderContext, buf.data(), (int)buf.size()) < 0) {
        jarkUtils::log("cvMat cannot decode: {}", jarkUtils::wstringToUtf8(path));
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


// HEIC ONLY, AVIF not support
// https://github.com/strukturag/libheif
// vcpkg install libheif:x64-windows-static
// vcpkg install libheif[hevc]:x64-windows-static
cv::Mat ImageDatabase::loadHeic(const wstring& path, const vector<uchar>& buf) {
    if (buf.empty())
        return {};

    auto exifStr = std::format("路径: {}\n大小: {}",
        jarkUtils::wstringToUtf8(path), jarkUtils::size2Str(buf.size()));

    auto filetype_check = heif_check_filetype(buf.data(), 12);
    if (filetype_check == heif_filetype_no) {
        jarkUtils::log("Input file is not an HEIF/AVIF file: {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    if (filetype_check == heif_filetype_yes_unsupported) {
        jarkUtils::log("Input file is an unsupported HEIF/AVIF file type: {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    heif_context* ctx = heif_context_alloc();
    auto err = heif_context_read_from_memory_without_copy(ctx, buf.data(), buf.size(), nullptr);
    if (err.code) {
        jarkUtils::log("heif_context_read_from_memory_without_copy error: {} {}", jarkUtils::wstringToUtf8(path), err.message);
        return {};
    }

    // get a handle to the primary image
    heif_image_handle* handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code) {
        jarkUtils::log("heif_context_get_primary_image_handle error: {} {}", jarkUtils::wstringToUtf8(path), err.message);
        if (ctx) heif_context_free(ctx);
        if (handle) heif_image_handle_release(handle);
        return {};
    }

    // decode the image and convert colorspace to RGB, saved as 24bit interleaved
    heif_image* img = nullptr;
    err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
    if (err.code) {
        jarkUtils::log("Error: {}", jarkUtils::wstringToUtf8(path));
        jarkUtils::log("heif_decode_image error: {}", err.message);
        if (ctx) heif_context_free(ctx);
        if (handle) heif_image_handle_release(handle);
        if (img) heif_image_release(img);
        return {};
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
cv::Mat ImageDatabase::loadAvif(const wstring& path, const vector<uchar>& buf) {
    avifImage* image = avifImageCreateEmpty();
    if (image == nullptr) {
        jarkUtils::log("avifImageCreateEmpty failure: {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    avifDecoder* decoder = avifDecoderCreate();
    if (decoder == nullptr) {
        jarkUtils::log("avifDecoderCreate failure: {}", jarkUtils::wstringToUtf8(path));
        avifImageDestroy(image);
        return {};
    }

    decoder->strictFlags = AVIF_STRICT_DISABLED;  // 严格模式下，老旧的不标准格式会解码失败

    avifResult result = avifDecoderReadMemory(decoder, image, buf.data(), buf.size());
    if (result != AVIF_RESULT_OK) {
        jarkUtils::log("avifDecoderReadMemory failure: {} {}", jarkUtils::wstringToUtf8(path), avifResultToString(result));
        avifImageDestroy(image);
        avifDecoderDestroy(decoder);
        return {};
    }

    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);
    result = avifRGBImageAllocatePixels(&rgb);
    if (result != AVIF_RESULT_OK) {
        jarkUtils::log("avifRGBImageAllocatePixels failure: {} {}", jarkUtils::wstringToUtf8(path), avifResultToString(result));
        avifImageDestroy(image);
        avifDecoderDestroy(decoder);
        return {};
    }

    rgb.format = AVIF_RGB_FORMAT_BGRA; // OpenCV is BGRA
    result = avifImageYUVToRGB(image, &rgb);
    if (result != AVIF_RESULT_OK) {
        jarkUtils::log("avifImageYUVToRGB failure: {} {}", jarkUtils::wstringToUtf8(path), avifResultToString(result));
        avifImageDestroy(image);
        avifDecoderDestroy(decoder);
        avifRGBImageFreePixels(&rgb);
        return {};
    }

    avifImageDestroy(image);
    avifDecoderDestroy(decoder);

    auto ret = cv::Mat(rgb.height, rgb.width, CV_8UC4);
    if (rgb.depth == 8) {
        if (rgb.rowBytes == ret.step && rgb.rowBytes == rgb.width * 4) {
            memcpy(ret.ptr(), rgb.pixels, (size_t)rgb.width * rgb.height * 4);
        }
        else {
            size_t minStep = rgb.rowBytes < ret.step ? rgb.rowBytes : ret.step;
            for (uint32_t y = 0; y < rgb.height; y++) {
                memcpy(ret.ptr() + ret.step * y, rgb.pixels + rgb.rowBytes * y, minStep);
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
            uint8_t* dst = (uint8_t*)(ret.ptr() + ret.step * y);
            for (uint32_t x = 0; x < rgb.width * 4; x++) {
                dst[x] = (uint8_t)(src[x] >> bitShift);
            }
        }
    }

    avifRGBImageFreePixels(&rgb);
    return ret;
}


cv::Mat ImageDatabase::loadRaw(const wstring& path, const vector<uchar>& buf) {
    if (buf.empty()) {
        jarkUtils::log("Buf is empty: {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    auto rawProcessor = std::make_unique<LibRaw>();

    int ret = rawProcessor->open_buffer(buf.data(), buf.size());
    if (ret != LIBRAW_SUCCESS) {
        jarkUtils::log("Cannot open RAW file: {} {}", jarkUtils::wstringToUtf8(path), libraw_strerror(ret));
        return {};
    }

    ret = rawProcessor->unpack();
    if (ret != LIBRAW_SUCCESS) {
        jarkUtils::log("Cannot unpack RAW file: {} {}", jarkUtils::wstringToUtf8(path), libraw_strerror(ret));
        return {};
    }

    ret = rawProcessor->dcraw_process();
    if (ret != LIBRAW_SUCCESS) {
        jarkUtils::log("Cannot process RAW file: {} {}", jarkUtils::wstringToUtf8(path), libraw_strerror(ret));
        return {};
    }

    libraw_processed_image_t* image = rawProcessor->dcraw_make_mem_image(&ret);
    if (image == nullptr) {
        jarkUtils::log("Cannot make image from RAW data: {} {}", jarkUtils::wstringToUtf8(path), libraw_strerror(ret));
        return {};
    }

    cv::Mat retImg;
    cv::cvtColor(cv::Mat(image->height, image->width, CV_8UC3, image->data), retImg, cv::COLOR_RGB2BGRA);

    // Clean up
    LibRaw::dcraw_clear_mem(image);
    rawProcessor->recycle();

    return retImg;
}


cv::Mat ImageDatabase::readDibFromMemory(const uint8_t* data, size_t size) {
    // 确保有足够的数据用于DIB头
    if (size < sizeof(DibHeader)) {
        jarkUtils::log("Insufficient data for DIB header. {}", size);
        return {};
    }

    // 读取DIB头
    DibHeader& header = *((DibHeader*)data);

    // 验证头大小是否符合预期
    if (header.headerSize != sizeof(DibHeader)) {
        jarkUtils::log("Unsupported DIB header size {}", header.headerSize);
        return {};
    }

    // 计算调色板大小（如果存在）
    int paletteSize = 0;
    if (header.bitCount <= 8) {
        paletteSize = (header.clrUsed == 0 ? 1 << header.bitCount : header.clrUsed) * 4;
    }

    // 确保有足够的数据用于调色板和像素数据
    //const size_t requireSize = (size_t)header.headerSize + paletteSize + header.imageSize; // header.imageSize偶尔大于size
    //if (size < requireSize) {
    //    jarkUtils::log("Insufficient data for image.");
    //    jarkUtils::log("requireSize {} givenSize {}", requireSize, size);
    //    return {};
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
        jarkUtils::log("Unsupported bit count {}", header.bitCount);
        return {};
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
std::tuple<cv::Mat, string> ImageDatabase::loadICO(const wstring& path, const vector<uchar>& buf) {
    if (buf.size() < 6) {
        jarkUtils::log("Invalid ICO file: {}", jarkUtils::wstringToUtf8(path));
        return { cv::Mat(),"" };
    }

    uint16_t numImages = *reinterpret_cast<const uint16_t*>(&buf[4]);
    std::vector<IconDirEntry> entries;

    if (numImages > 255) {
        jarkUtils::log("numImages Error: {} {}", jarkUtils::wstringToUtf8(path), numImages);
        return { cv::Mat(),"" };
    }

    size_t offset = 6;
    for (int i = 0; i < numImages; ++i) {
        if (offset + sizeof(IconDirEntry) > buf.size()) {
            jarkUtils::log("Invalid ICO file structure: {}", jarkUtils::wstringToUtf8(path));
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
            jarkUtils::log("Invalid image data offset or size: {}", jarkUtils::wstringToUtf8(path));
            jarkUtils::log("endOffset {} fileBuf.size(): {}", endOffset, buf.size());
            jarkUtils::log("entry.dataOffset {}", entry.dataOffset);
            jarkUtils::log("entry.dataSize: {}", entry.dataSize);
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
            jarkUtils::log("Unsupported image format");
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


// https://github.com/MolecularMatters/psd_sdk
cv::Mat ImageDatabase::loadPSD(const wstring& path, const vector<uchar>& buf) {
    const int32_t CHANNEL_NOT_FOUND = UINT_MAX;

    cv::Mat img;

    psd::MallocAllocator allocator;
    psd::NativeFile file(&allocator);

    if (!file.OpenRead(path.c_str())) {
        jarkUtils::log("Cannot open file {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    psd::Document* document = CreateDocument(&file, &allocator);
    if (!document) {
        jarkUtils::log("Cannot create document {}", jarkUtils::wstringToUtf8(path));
        file.Close();
        return {};
    }

    // the sample only supports RGB colormode
    if (document->colorMode != psd::colorMode::RGB)
    {
        jarkUtils::log("Document is not in RGB color mode {}", jarkUtils::wstringToUtf8(path));
        DestroyDocument(document, &allocator);
        file.Close();
        return {};
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
                cv::Mat(document->height, document->width, CV_16UC4, image16).convertTo(img, CV_8UC4, 1.0 / 256);
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


cv::Mat ImageDatabase::loadTGA_HDR(const wstring& path, const vector<uchar>& buf) {
    int width, height, channels;

    // 使用stb_image从内存缓冲区加载图像
    uint8_t* img = stbi_load_from_memory(buf.data(), (int)buf.size(), &width, &height, &channels, 0);

    if (!img) {
        jarkUtils::log("Failed to load image: {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    // 确定OpenCV的色彩空间
    int cv_type;
    switch (channels) {
    case 1: cv_type = CV_8UC1; break;
    case 3: cv_type = CV_8UC3; break;
    case 4: cv_type = CV_8UC4; break;
    default:
        stbi_image_free(img);
        jarkUtils::log("Unsupported number of channels:{} {}", channels, jarkUtils::wstringToUtf8(path));
        return {};
    }

    auto result = cv::Mat(height, width, cv_type, img).clone();

    stbi_image_free(img);

    cv::cvtColor(result, result, channels == 1 ? cv::COLOR_GRAY2BGRA :
        (channels == 3 ? cv::COLOR_RGB2BGRA : cv::COLOR_RGBA2BGRA));

    return result;
}


cv::Mat ImageDatabase::loadSVG(const wstring& path, const vector<uchar>& buf) {
    const int maxEdge = 3840;
    static bool isInitFont = false;

    if (!isInitFont) {
        isInitFont = true;
        auto rc = jarkUtils::GetResource(IDR_MSYHMONO_TTF, L"TTF");
        lunasvg_add_font_face_from_data("", false, false, rc.ptr, rc.size, nullptr, nullptr);
    }

    auto document = lunasvg::Document::loadFromData((const char*)buf.data(), buf.size());
    if (!document) {
        jarkUtils::log("Failed to load SVG data {}", jarkUtils::wstringToUtf8(path));
        return {};
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
        jarkUtils::log("Failed to render SVG to bitmap {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    return cv::Mat(height, width, CV_8UC4, bitmap.data()).clone();
}


cv::Mat ImageDatabase::loadJXR(const wstring& path, const vector<uchar>& buf) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM library." << std::endl;
        return {};
    }

    IWICImagingFactory* pFactory = NULL;
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&pFactory);
    if (FAILED(hr)) {
        std::cerr << "Failed to create WIC Imaging Factory." << std::endl;
        CoUninitialize();
        return {};
    }

    IStream* pStream = NULL;
    hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
    if (FAILED(hr)) {
        std::cerr << "Failed to create stream." << std::endl;
        pFactory->Release();
        CoUninitialize();
        return {};
    }

    ULONG bytesWritten;
    hr = pStream->Write(buf.data(), (ULONG)buf.size(), &bytesWritten);
    if (FAILED(hr) || bytesWritten != buf.size()) {
        std::cerr << "Failed to write to stream." << std::endl;
        pStream->Release();
        pFactory->Release();
        CoUninitialize();
        return {};
    }

    LARGE_INTEGER li = { 0 };
    hr = pStream->Seek(li, STREAM_SEEK_SET, NULL);
    if (FAILED(hr)) {
        std::cerr << "Failed to seek stream." << std::endl;
        pStream->Release();
        pFactory->Release();
        CoUninitialize();
        return {};
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        std::cerr << "Failed to create decoder from stream." << std::endl;
        pStream->Release();
        pFactory->Release();
        CoUninitialize();
        return {};
    }

    IWICBitmapFrameDecode* pFrame = NULL;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) {
        std::cerr << "Failed to get frame from decoder." << std::endl;
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        CoUninitialize();
        return {};
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
        return {};
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
        return {};
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
        return {};
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
        return {};
    }

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pStream->Release();
    pFactory->Release();
    CoUninitialize();

    return mat;
}


vector<cv::Mat> ImageDatabase::loadMats(const wstring& path, const vector<uchar>& buf) {
    vector<cv::Mat> imgs;

    if (!cv::imdecodemulti(buf, cv::IMREAD_UNCHANGED, imgs)) {
        return imgs;
    }
    return imgs;
}


cv::Mat ImageDatabase::loadMat(const wstring& path, const vector<uchar>& buf) {
    cv::Mat img;
    try {
        img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
    }
    catch (cv::Exception e) {
        jarkUtils::log("cvMat cannot decode: {} [{}]", jarkUtils::wstringToUtf8(path), e.what());
        return {};
    }

    if (img.empty()) {
        jarkUtils::log("cvMat cannot decode: {}", jarkUtils::wstringToUtf8(path));
        return {};
    }

    if (img.channels() == 1)
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);

    if (img.channels() != 3 && img.channels() != 4) {
        jarkUtils::log("cvMat unsupport channel: {}", img.channels());
        return {};
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
    jarkUtils::log("Special: {}, img.depth(): {}, img.channels(): {}",
        jarkUtils::wstringToUtf8(path), img.depth(), img.channels());
    return {};
}


// 辅助函数，用于从 PFM 头信息中提取尺寸和比例因子
bool ImageDatabase::parsePFMHeader(const vector<uchar>& buf, int& width, int& height, float& scaleFactor, bool& isColor, size_t& dataOffset) {
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
            jarkUtils::log("parsePFMHeader fail!");
            return false;
        }
    }break;

    case 2:break;

    default: {
        jarkUtils::log("parsePFMHeader fail!");
        return false;
    }
    }

    // 查找比例因子
    string scaleLine;
    offset++;
    while (buf[offset] != '\n' && offset < maxOffset) scaleLine += buf[offset++];

    scaleFactor = std::stof(scaleLine);
    dataOffset = offset + 1;

    return true;
}


cv::Mat ImageDatabase::loadPFM(const wstring& path, const vector<uchar>& buf) {
    int width, height;
    float scaleFactor;
    bool isColor;
    size_t dataOffset;

    // 解析 PFM 头信息
    if (!parsePFMHeader(buf, width, height, scaleFactor, isColor, dataOffset)) {
        std::cerr << "Failed to parse PFM header!" << endl;
        return {};
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


cv::Mat ImageDatabase::loadQOI(const wstring& path, const vector<uchar>& buf) {
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


std::pair<std::vector<uint8_t>, std::string> ImageDatabase::unzipLivp(std::vector<uint8_t>& livpFileBuff) {
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


void ImageDatabase::handleExifOrientation(int orientation, cv::Mat& img) {
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


Frames ImageDatabase::loader(const wstring& path) {
    if (path.length() < 4) {
        jarkUtils::log("path.length() < 4: {}", jarkUtils::wstringToUtf8(path));
        return { {{getErrorTipsMat(), 0}}, "" };
    }

    auto f = _wfopen(path.c_str(), L"rb");
    if (f == nullptr) {
        jarkUtils::log("path canot open: {}", jarkUtils::wstringToUtf8(path));
        return { {{getErrorTipsMat(), 0}}, "" };
    }

    fseek(f, 0, SEEK_END);
    auto fileSize = ftell(f);

    if (fileSize == 0) {
        fclose(f);
        jarkUtils::log("path fileSize == 0: {}", jarkUtils::wstringToUtf8(path));
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
                "\n文件头32字节: " + jarkUtils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
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
                "\n文件头32字节: " + jarkUtils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
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
                "\n文件头32字节: " + jarkUtils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
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
                "\n文件头32字节: " + jarkUtils::bin2Hex(fileBuf.data(), fileBuf.size() > 32 ? 32 : fileBuf.size());
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
    }
    else if (ext == L".heic" || ext == L".heif") {
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