#pragma once

#include "jarkUtils.h"


class VideoDecoder {
private:
    static inline IMFSourceReader* m_pSourceReader = nullptr;
    static inline IMFMediaType* m_pDecoderOutputType = nullptr;
    static inline GUID m_outputFormat = GUID_NULL;
    static inline bool m_initialized = false;
    static inline UINT32 m_rotation = MFVideoRotationFormat_0;


    static HRESULT Initialize(const uint8_t* videoBuffer, size_t size) {
        HRESULT hr = S_OK;

        // 初始化Media Foundation
        hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            jarkUtils::log("Failed to initialize Media Foundation");
            return hr;
        }

        // 创建内存字节流
        IMFByteStream* pByteStream = nullptr;
        hr = MFCreateTempFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST,
            MF_FILEFLAGS_NONE, &pByteStream);
        if (FAILED(hr)) {
            jarkUtils::log("Failed to create byte stream");
            return hr;
        }

        // 将视频数据写入字节流
        ULONG bytesWritten = 0;
        hr = pByteStream->Write(videoBuffer, size, &bytesWritten);
        if (FAILED(hr)) {
            pByteStream->Release();
            return hr;
        }

        // 重置流位置到开始
        QWORD currentPosition = 0;
        hr = pByteStream->Seek(msoBegin, 0, MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO, &currentPosition);
        if (FAILED(hr)) {
            pByteStream->Release();
            return hr;
        }

        // 创建源读取器
        IMFAttributes* pAttributes = nullptr;
        hr = MFCreateAttributes(&pAttributes, 1);
        if (SUCCEEDED(hr)) {
            // 启用硬件解码（可选）
            hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        }

        if (SUCCEEDED(hr)) {
            hr = MFCreateSourceReaderFromByteStream(pByteStream, pAttributes, &m_pSourceReader);
        }

        if (pAttributes) {
            pAttributes->Release();
        }
        pByteStream->Release();

        if (FAILED(hr)) {
            jarkUtils::log("Failed to create source reader");
            return hr;
        }

        // 配置解码器输出格式为RGB24
        hr = ConfigureDecoder();
        if (FAILED(hr)) {
            return hr;
        }

        m_initialized = true;
        return S_OK;
    }

    static HRESULT ConfigureDecoder() {
        HRESULT hr = S_OK;

        // 首先尝试获取原始媒体类型
        IMFMediaType* pNativeType = nullptr;
        hr = m_pSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType);
        if (FAILED(hr)) {
            jarkUtils::log("Failed to get native media type");
            return hr;
        }

        // 输出原始格式信息用于调试
        GUID subtype;
        if (SUCCEEDED(pNativeType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            char encodeStr[8] = { 0 };
            *((long*)encodeStr) = subtype.Data1;
            jarkUtils::log("Native subtype: [{}]", encodeStr);
        }

        pNativeType->Release();

        // 创建输出媒体类型，使用NV12或YUY2格式（更兼容）
        hr = MFCreateMediaType(&m_pDecoderOutputType);
        if (FAILED(hr)) {
            return hr;
        }

        hr = m_pDecoderOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (FAILED(hr)) {
            return hr;
        }

        // 首先尝试YUY2格式
        hr = m_pDecoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        if (SUCCEEDED(hr)) {
            hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr, m_pDecoderOutputType);
        }

        // 如果YUY2失败，尝试NV12
        if (FAILED(hr)) {
            jarkUtils::log("YUY2 failed, trying NV12...");
            hr = m_pDecoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            if (SUCCEEDED(hr)) {
                hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                    nullptr, m_pDecoderOutputType);
            }
        }

        //如果NV12也失败，尝试RGB32
        if (FAILED(hr)) {
            jarkUtils::log("YUY2 failed, trying RGB32...");
            hr = m_pDecoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            if (SUCCEEDED(hr)) {
                hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                    nullptr, m_pDecoderOutputType);
            }
        }

        if (FAILED(hr)) {
            jarkUtils::log("Failed to set any supported output format, HRESULT: 0x{:x}", hr);
        }
        else {
            // 获取并确认最终设置的媒体类型
            IMFMediaType* pCurrentType = nullptr;
            hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
            if (SUCCEEDED(hr)) {
                if (SUCCEEDED(pCurrentType->GetGUID(MF_MT_SUBTYPE, &m_outputFormat))) {
                    jarkUtils::log("Successfully set output format: {}", []()->const char* {
                        if (m_outputFormat == MFVideoFormat_NV12) {
                            return "NV12";
                        }
                        else if (m_outputFormat == MFVideoFormat_YUY2) {
                            return "YUY2";
                        }
                        else if (m_outputFormat == MFVideoFormat_RGB32) {
                            return "RGB32";
                        }
                        else {
                            return "Unknown format";
                        }
                        });
                }
                pCurrentType->Release();
            }
        }

        return hr;
    }

    static std::vector<cv::Mat> DecodeAllFrames() {
        std::vector<cv::Mat> frames;

        if (!m_initialized) {
            jarkUtils::log("Decoder not initialized");
            return frames;
        }

        HRESULT hr = S_OK;
        DWORD streamFlags = 0;
        LONGLONG timeStamp = 0;
        IMFSample* pSample = nullptr;

        // 获取视频信息
        IMFMediaType* pMediaType = nullptr;
        hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMediaType);
        if (FAILED(hr)) {
            jarkUtils::log("Failed to get media type");
            return frames;
        }

        UINT32 width, height;
        hr = MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);

        // 读取旋转信息
        if (FAILED(pMediaType->GetUINT32(MF_MT_VIDEO_ROTATION, &m_rotation))) {
            m_rotation = MFVideoRotationFormat_0; // 默认不旋转
        }

        pMediaType->Release();

        if (FAILED(hr)) {
            jarkUtils::log("Failed to get frame size");
            return frames;
        }

        jarkUtils::log("Video dimensions: {}x{}", width, height);

        // 解码所有帧
        while (true) {
            hr = m_pSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0, nullptr, &streamFlags, &timeStamp, &pSample);

            if (FAILED(hr)) {
                jarkUtils::log("Failed to read sample");
                break;
            }

            if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                jarkUtils::log("End of stream reached");
                break;
            }

            if (pSample) {
                cv::Mat frame = ConvertSampleToMat(pSample, width, height);
                if (!frame.empty()) {
                    frames.push_back(frame);
                }
                pSample->Release();
                pSample = nullptr;
            }
        }

        return frames;
    }

    static cv::Mat ConvertSampleToMat(IMFSample* pSample, UINT32 width, UINT32 height) {
        HRESULT hr = S_OK;
        IMFMediaBuffer* pBuffer = nullptr;

        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
        if (FAILED(hr)) {
            jarkUtils::log("Failed to convert to contiguous buffer");
            return cv::Mat();
        }

        BYTE* pData = nullptr;
        DWORD maxLength = 0, currentLength = 0;

        hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
        if (FAILED(hr)) {
            pBuffer->Release();
            jarkUtils::log("Failed to lock buffer");
            return cv::Mat();
        }

        cv::Mat result;

        if (m_outputFormat == MFVideoFormat_NV12) {
            cv::Mat yuvFrame(height * 3 / 2, width, CV_8UC1, pData);
            cv::cvtColor(yuvFrame, result, cv::COLOR_YUV2BGR_NV12);
        }
        else if (m_outputFormat == MFVideoFormat_YUY2) {
            cv::Mat yuvFrame(height, width, CV_8UC2, pData);
            cv::cvtColor(yuvFrame, result, cv::COLOR_YUV2BGR_YUY2);
        }
        else if (m_outputFormat == MFVideoFormat_RGB32) {
            cv::Mat bgraFrame(height, width, CV_8UC4, pData);
            cv::cvtColor(bgraFrame, result, cv::COLOR_BGRA2BGR);
            cv::flip(result, result, 0);
        }
        else {
            jarkUtils::log("Unsupported format for conversion");
        }

        // 应用旋转
        switch (m_rotation) {
        case MFVideoRotationFormat_90:
            cv::rotate(result, result, cv::ROTATE_90_CLOCKWISE);
            break;
        case MFVideoRotationFormat_180:
            cv::rotate(result, result, cv::ROTATE_180);
            break;
        case MFVideoRotationFormat_270:
            cv::rotate(result, result, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        default:
            break; // 不旋转
        }

        pBuffer->Unlock();
        pBuffer->Release();

        return result;
    }

    static void Cleanup() {
        if (m_pDecoderOutputType) {
            m_pDecoderOutputType->Release();
            m_pDecoderOutputType = nullptr;
        }

        if (m_pSourceReader) {
            m_pSourceReader->Release();
            m_pSourceReader = nullptr;
        }

        if (m_initialized) {
            MFShutdown();
            m_initialized = false;
        }
    }

public:
    static std::vector<cv::Mat> DecodeVideoFrames(const uint8_t* videoBuffer, size_t size) {
        HRESULT hr = Initialize(videoBuffer, size);
        if (FAILED(hr)) {
            jarkUtils::log("Failed to initialize decoder, HRESULT: 0x{:x}", hr);
            return {};
        }

        auto frames = DecodeAllFrames();

        Cleanup();

        return frames;
    }
};
