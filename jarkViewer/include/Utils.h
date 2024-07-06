#pragma once

#include<iostream>
#include<format>
#include<algorithm>

#include<filesystem>
#include<chrono>
#include<mutex>
#include<semaphore>

#include<string>
#include<vector>
#include<unordered_set>
#include<unordered_map>

using std::vector;
using std::string;
using std::wstring;
using std::string_view;
using std::unordered_set;
using std::unordered_map;
using std::cout;
using std::endl;

#include "framework.h"
#include "resource.h"

#include<opencv2/core.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>
#include<opencv2/highgui/highgui_c.h>

#include "libraw/libraw.h"
#include "libheif/heif.h"
#include "avif/avif.h"
#include "gif_lib.h"
#include "exif.h"

struct rcFileInfo {
	uint8_t* addr = nullptr;
	size_t size = 0;
};

union intUnion {
	uint32_t u32;
	uint8_t u8[4];
	intUnion() :u32(0) {}
	intUnion(uint32_t n) :u32(n) {}
	intUnion(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
		u8[0] = b0;
		u8[1] = b1;
		u8[2] = b2;
		u8[3] = b3;
	}
	uint8_t& operator[](const int i) {
		return u8[i];
	}
	void operator=(const int i) {
		u32 = i;
	}
	void operator=(const uint32_t i) {
		u32 = i;
	}
	void operator=(const intUnion i) {
		u32 = i.u32;
	}
};

struct Cood {
	int x = 0;
	int y = 0;

	Cood operator+(const Cood& t) const {
		Cood temp;
		temp.x = this->x + t.x;
		temp.y = this->y + t.y;
		return temp;
	}

	Cood operator-(const Cood& t) const {
		Cood temp;
		temp.x = this->x - t.x;
		temp.y = this->y - t.y;
		return temp;
	}

	Cood operator*(int i) const {
		Cood temp;
		temp.x = this->x * i;
		temp.y = this->y * i;
		return temp;
	}

	Cood operator/(int i) const {
		Cood temp;
		temp.x = this->x / i;
		temp.y = this->y / i;
		return temp;
	}
};

struct ImageNode {
	cv::Mat img;
	int delay;
};
struct Frames {
	std::vector<ImageNode> imgList;
	string exifStr;
};

struct GifData {
	const unsigned char* m_lpBuffer;
	size_t m_nBufferSize;
	size_t m_nPosition;
};

namespace Utils {

	static const unordered_set<wstring> supportExt = {
		L".jpg", L".jp2", L".jpeg", L".jpe", L".bmp", L".dib", L".png",
		L".pbm", L".pgm", L".ppm", L".pxm",L".pnm",L".sr", L".ras",
		L".exr", L".tiff", L".tif", L".webp", L".hdr", L".pic",
		L".heic", L".heif", L".avif", L".avifs", L".gif",
	};

	static const unordered_set<wstring> supportRaw = {
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

	static const bool TO_LOG_FILE = false;
	static FILE* fp = nullptr;

	template<typename... Args>
	static void log(const string_view fmt, Args&&... args) {
		auto ts = time(nullptr) + 8 * 3600ULL;// UTC+8
		int HH = (ts / 3600) % 24;
		int MM = (ts / 60) % 60;
		int SS = ts % 60;

		const string str = std::format("[{:02d}:{:02d}:{:02d}] ", HH, MM, SS) +
			std::vformat(fmt, std::make_format_args(args...)) + "\n";

		if (TO_LOG_FILE) {
			if (!fp) {
				fp = fopen("D:\\log.txt", "a");
				if (!fp)return;
			}
			fwrite(str.c_str(), 1, str.length(), fp);
			fflush(fp);
		}
		else {
			cout << str;
		}
	}

	string bin2Hex(const void* bytes, const size_t len) {
		auto charList = "0123456789ABCDEF";
		if (len == 0) return "";
		string res(len * 3 - 1, ' ');
		for (size_t i = 0; i < len; i++) {
			const uint8_t value = reinterpret_cast<const uint8_t*>(bytes)[i];
			res[i * 3] = charList[value >> 4];
			res[i * 3 + 1] = charList[value & 0x0f];
		}
		return res;
	}

	std::wstring ansiToWstring(const std::string& str) {
		if (str.empty())return L"";

		int wcharLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), nullptr, 0);
		if (wcharLen == 0) return L"";

		std::wstring ret(wcharLen, 0);
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

		return ret;
	}

	std::string wstringToAnsi(const std::wstring& wstr) {
		if (wstr.empty())return "";

		int byteLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
		if (byteLen == 0) return "";

		std::string ret(byteLen, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
		return ret;
	}

	//UTF8 to UTF16
	std::wstring utf8ToWstring(const std::string& str) {
		if (str.empty())return L"";

		int wcharLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
		if (wcharLen == 0) return L"";

		std::wstring ret(wcharLen, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), ret.data(), wcharLen);

		return ret;
	}

	//UTF16 to UTF8
	std::string wstringToUtf8(const std::wstring& wstr) {
		if (wstr.empty())return "";

		int byteLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
		if (byteLen == 0) return "";

		std::string ret(byteLen, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), ret.data(), byteLen, nullptr, nullptr);
		return ret;
	}

	std::string utf8ToAnsi(const std::string& str) {
		return wstringToAnsi(utf8ToWstring(str));
	}

	std::string ansiToUtf8(const std::string& str) {
		return wstringToUtf8(ansiToWstring(str));
	}

	rcFileInfo GetResource(unsigned int idi, const wchar_t* type) {
		rcFileInfo rc;

		HMODULE ghmodule = GetModuleHandle(NULL);
		if (ghmodule == NULL) {
			log("ghmodule null");
			return rc;
		}

		HRSRC hrsrc = FindResource(ghmodule, MAKEINTRESOURCE(idi), type);
		if (hrsrc == NULL) {
			log("hrstc null");
			return rc;
		}

		HGLOBAL hg = LoadResource(ghmodule, hrsrc);
		if (hg == NULL) {
			log("hg null");
			return rc;
		}

		rc.addr = (unsigned char*)(LockResource(hg));
		rc.size = SizeofResource(ghmodule, hrsrc);
		return rc;
	}

	string size2Str(const int fileSize) {
		if (fileSize < 1024) return std::format("{} Bytes", fileSize);
		if (fileSize < 1024 * 1024) return std::format("{:.1f} KiB", fileSize / 1024.0);
		if (fileSize < 1024 * 1024 * 1024) return std::format("{:.1f} MiB", fileSize / (1024.0 * 1024));
		return std::format("{:.1f} GiB", fileSize / (1024.0 * 1024 * 1024));
	}

	string timeStamp2Str(time_t timeStamp) {
		timeStamp += 8ULL * 3600; // UTC+8
		std::tm* ptm = std::gmtime(&timeStamp);
		std::stringstream ss;
		ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S UTC+8");
		return ss.str();
	}

	std::string getExif(const wstring& path, int width, int height, const uint8_t* buf, int fileSize) {
		static easyexif::EXIFInfo exifInfo;

		auto res = std::format("    路径: {}\n    大小：{}\n  分辨率: {}x{}",
			wstringToUtf8(path),
			size2Str(fileSize),
			width, height);

		auto ext = path.substr(path.length() - 4);
		for (auto& c : ext) c = std::tolower(c);
		if (ext != L".jpg" && ext != L"jpeg" && ext != L".jpe") {
			return res;
		}

		exifInfo.hasInfo = (exifInfo.parseFrom(buf, fileSize) == 0);

		if (!exifInfo.hasInfo) {
			log("getExif no exif info: {}", wstringToUtf8(path));
			return res;
		}

		if (exifInfo.ISOSpeedRatings)
			res += std::format("\n     ISO: {}", exifInfo.ISOSpeedRatings);

		if (exifInfo.FNumber)
			res += std::format("\n   F光圈: f/{}", exifInfo.FNumber);

		if (exifInfo.FocalLength)
			res += std::format("\n    焦距: {} mm", exifInfo.FocalLength);

		if (exifInfo.FocalLengthIn35mm)
			res += std::format("\n35mm焦距: {} mm", exifInfo.FocalLengthIn35mm);

		if (exifInfo.ExposureBiasValue)
			res += std::format("\n曝光误差: {} Ev", exifInfo.ExposureBiasValue);

		if (exifInfo.ExposureTime)
			res += std::format("\n曝光时长: {:.2f} s", exifInfo.ExposureTime);

		if (exifInfo.SubjectDistance)
			res += std::format("\n目标距离: {:.2f} m", exifInfo.SubjectDistance);

		if (exifInfo.GeoLocation.Longitude)
			res += std::format("\nGPS 经度: {:.6f}°", exifInfo.GeoLocation.Longitude);

		if (exifInfo.GeoLocation.Latitude)
			res += std::format("\nGPS 纬度: {:.6f}°", exifInfo.GeoLocation.Latitude);

		if (exifInfo.GeoLocation.Altitude)
			res += std::format("\n海拔高度: {:.2f} m", exifInfo.GeoLocation.Altitude);

		if (exifInfo.Copyright.length())
			res += std::format("\n    版权: {}", exifInfo.Copyright);

		if (exifInfo.Software.length())
			res += std::format("\n    软件: {}", exifInfo.Software);

		if (exifInfo.ImageDescription.length())
			res += std::format("\n    描述: {}", exifInfo.ImageDescription);

		if (exifInfo.Flash)
			res += "\n    闪光: 是";

		if (exifInfo.DateTime.length())
			res += std::format("\n创建时间: {}", exifInfo.DateTime);

		if (exifInfo.DateTimeOriginal.length())
			res += std::format("\n原始时间: {}", exifInfo.DateTimeOriginal);

		if (exifInfo.DateTimeDigitized.length())
			res += std::format("\n数字时间: {}", exifInfo.DateTimeDigitized);

		if (exifInfo.Make.length() + exifInfo.Model.length())
			res += std::format("\n相机型号: {}", exifInfo.Make + " " + exifInfo.Model);

		if (exifInfo.LensInfo.Make.length() + exifInfo.LensInfo.Model.length())
			res += std::format("\n镜头型号: {}", exifInfo.LensInfo.Make + " " + exifInfo.LensInfo.Model);

		return res;
	}

	std::string getExifFromEXIFSegment(const wstring& path, int width, int height, const uint8_t* buf, int size) {
		static easyexif::EXIFInfo exifInfo;

		auto res = std::format("    路径: {}\n    大小：{}\n  分辨率: {}x{}",
			wstringToUtf8(path),
			size2Str(size),
			width, height);

		log("{}", bin2Hex(buf, 6));

		exifInfo.clear();
		exifInfo.hasInfo = (exifInfo.parseFromEXIFSegment(buf, size) == 0);

		//if (!exifInfo.hasInfo) {
		//	log("getExif no exif info: {}", wstringToUtf8(path));
		//	return res;
		//}

		if (exifInfo.ISOSpeedRatings)
			res += std::format("\n     ISO: {}", exifInfo.ISOSpeedRatings);

		if (exifInfo.FNumber)
			res += std::format("\n   F光圈: f/{}", exifInfo.FNumber);

		if (exifInfo.FocalLength)
			res += std::format("\n    焦距: {} mm", exifInfo.FocalLength);

		if (exifInfo.FocalLengthIn35mm)
			res += std::format("\n35mm焦距: {} mm", exifInfo.FocalLengthIn35mm);

		if (exifInfo.ExposureBiasValue)
			res += std::format("\n曝光误差: {} Ev", exifInfo.ExposureBiasValue);

		if (exifInfo.ExposureTime)
			res += std::format("\n曝光时长: {:.2f} s", exifInfo.ExposureTime);

		if (exifInfo.SubjectDistance)
			res += std::format("\n目标距离: {:.2f} m", exifInfo.SubjectDistance);

		if (exifInfo.GeoLocation.Longitude)
			res += std::format("\nGPS 经度: {:.6f}°", exifInfo.GeoLocation.Longitude);

		if (exifInfo.GeoLocation.Latitude)
			res += std::format("\nGPS 纬度: {:.6f}°", exifInfo.GeoLocation.Latitude);

		if (exifInfo.GeoLocation.Altitude)
			res += std::format("\n海拔高度: {:.2f} m", exifInfo.GeoLocation.Altitude);

		if (exifInfo.Copyright.length())
			res += std::format("\n    版权: {}", exifInfo.Copyright);

		if (exifInfo.Software.length())
			res += std::format("\n    软件: {}", exifInfo.Software);

		if (exifInfo.ImageDescription.length())
			res += std::format("\n    描述: {}", exifInfo.ImageDescription);

		if (exifInfo.Flash)
			res += "\n    闪光: 是";

		if (exifInfo.DateTime.length())
			res += std::format("\n创建时间: {}", exifInfo.DateTime);

		if (exifInfo.DateTimeOriginal.length())
			res += std::format("\n原始时间: {}", exifInfo.DateTimeOriginal);

		if (exifInfo.DateTimeDigitized.length())
			res += std::format("\n数字时间: {}", exifInfo.DateTimeDigitized);

		if (exifInfo.Make.length() + exifInfo.Model.length())
			res += std::format("\n相机型号: {}", exifInfo.Make + " " + exifInfo.Model);

		if (exifInfo.LensInfo.Make.length() + exifInfo.LensInfo.Model.length())
			res += std::format("\n镜头型号: {}", exifInfo.LensInfo.Make + " " + exifInfo.LensInfo.Model);

		return res;
	}

	static cv::Mat& getDefaultMat() {
		static cv::Mat defaultMat;
		static bool isInit = false;

		if (!isInit) {
			isInit = true;
			auto rc = GetResource(IDB_PNG, L"PNG");
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
			auto rc = GetResource(IDB_PNG1, L"PNG");
			std::vector<char> imgData(rc.addr, rc.addr + rc.size);
			homeMat = cv::imdecode(imgData, cv::IMREAD_UNCHANGED);
		}
		return homeMat;
	}

	// HEIC ONLY, AVIF not support
	// https://github.com/strukturag/libheif
	// vcpkg install libheif:x64-windows-static
	// vcpkg install libheif[hevc]:x64-windows-static
	std::pair<cv::Mat, string> loadHeic(const wstring& path, const vector<uchar>& buf, int fileSize) {

		auto exifStr = std::format("    路径: {}\n    大小：{}",
			wstringToUtf8(path),
			size2Str(fileSize));

		auto filetype_check = heif_check_filetype(buf.data(), 12);
		if (filetype_check == heif_filetype_no) {
			log("Input file is not an HEIF/AVIF file: {}", wstringToUtf8(path));
			return { cv::Mat(), exifStr };
		}

		if (filetype_check == heif_filetype_yes_unsupported) {
			log("Input file is an unsupported HEIF/AVIF file type: {}", wstringToUtf8(path));
			return { cv::Mat(), exifStr };
		}

		heif_context* ctx = heif_context_alloc();
		auto err = heif_context_read_from_memory_without_copy(ctx, buf.data(), fileSize, nullptr);
		if (err.code) {
			log("Error: {}", wstringToUtf8(path));
			log("heif_context_read_from_memory_without_copy error: {}", err.message);
			return { cv::Mat(), exifStr };
		}

		// get a handle to the primary image
		heif_image_handle* handle = nullptr;
		err = heif_context_get_primary_image_handle(ctx, &handle);
		if (err.code) {
			log("Error: {}", wstringToUtf8(path));
			log("heif_context_get_primary_image_handle error: {}", err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			return { cv::Mat(), exifStr };
		}

		// decode the image and convert colorspace to RGB, saved as 24bit interleaved
		heif_image* img = nullptr;
		err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
		if (err.code) {
			log("Error: {}", wstringToUtf8(path));
			log("heif_decode_image error: {}", err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			if (img) heif_image_release(img);
			return { cv::Mat(), exifStr };
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

		heif_item_id metadata_id;
		int metadata_count = heif_image_handle_get_list_of_metadata_block_IDs(handle, "Exif", &metadata_id, 1);
		log("metadata_count {}", metadata_count);
		if (metadata_count > 0) {
			size_t exif_size = heif_image_handle_get_metadata_size(handle, metadata_id);
			std::vector<uint8_t> exif_data(exif_size);

			err = heif_image_handle_get_metadata(handle, metadata_id, exif_data.data());
			if (err.code == heif_error_Ok) {
				// 现在exif_data包含了EXIF数据
				// 你可以使用其他库(如libexif)来解析这些数据
				// 或者直接处理原始字节

				// 示例:打印EXIF数据的大小
				exifStr = getExifFromEXIFSegment(path, width, height, exif_data.data(), exif_data.size());
				log("EXIF bytes {} ", exif_size);
				log("EXIF {}", exifStr);
			}
		}

		// clean up resources
		heif_context_free(ctx);
		heif_image_release(img);
		heif_image_handle_release(handle);

		return { matImg, exifStr };
	}

	// vcpkg install libavif[aom]:x64-windows-static libavif[dav1d]:x64-windows-static
	// https://github.com/AOMediaCodec/libavif/issues/1451#issuecomment-1606903425
	cv::Mat loadAvif(const wstring& path, const vector<uchar>& buf, int fileSize) {
		avifImage* image = avifImageCreateEmpty();
		if (image == nullptr) {
			log("avifImageCreateEmpty failure: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		avifDecoder* decoder = avifDecoderCreate();
		if (decoder == nullptr) {
			log("avifDecoderCreate failure: {}", wstringToUtf8(path));
			avifImageDestroy(image);
			return cv::Mat();
		}

		//decoder->strictFlags = 0; // 

		avifResult result = avifDecoderReadMemory(decoder, image, buf.data(), fileSize);
		if (result != AVIF_RESULT_OK) {
			log("avifDecoderReadMemory failure: {} {}", wstringToUtf8(path), avifResultToString(result));
			avifImageDestroy(image);
			avifDecoderDestroy(decoder);
			return cv::Mat();
		}

		avifRGBImage rgb;
		avifRGBImageSetDefaults(&rgb, image);
		result = avifRGBImageAllocatePixels(&rgb);
		if (result != AVIF_RESULT_OK) {
			log("avifRGBImageAllocatePixels failure: {} {}", wstringToUtf8(path), avifResultToString(result));
			avifImageDestroy(image);
			avifDecoderDestroy(decoder);
			return cv::Mat();
		}

		rgb.format = AVIF_RGB_FORMAT_BGRA; // OpenCV is BGRA
		result = avifImageYUVToRGB(image, &rgb);
		if (result != AVIF_RESULT_OK) {
			log("avifImageYUVToRGB failure: {} {}", wstringToUtf8(path), avifResultToString(result));
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
			log("rgb.depth: {} {}", rgb.depth, wstringToUtf8(path));
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

	std::pair<cv::Mat, string> loadRaw(const wstring& path, const vector<uchar>& buf, int fileSize) {

		auto exifStr = std::format("    路径: {}\n    大小：{}",
			wstringToUtf8(path),
			size2Str(fileSize));

		if (buf.empty() || fileSize <= 0) {
			log("Buf is empty: {} {}", wstringToUtf8(path), fileSize);
			return { cv::Mat(), exifStr };
		}

		auto rawProcessor = std::make_unique<LibRaw>();

		int ret = rawProcessor->open_buffer(buf.data(), fileSize);
		if (ret != LIBRAW_SUCCESS) {
			log("Cannot open RAW file: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return { cv::Mat(), exifStr };
		}

		ret = rawProcessor->unpack();
		if (ret != LIBRAW_SUCCESS) {
			log("Cannot unpack RAW file: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return { cv::Mat(), exifStr };
		}

		ret = rawProcessor->dcraw_process();
		if (ret != LIBRAW_SUCCESS) {
			log("Cannot process RAW file: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return { cv::Mat(), exifStr };
		}

		libraw_processed_image_t* image = rawProcessor->dcraw_make_mem_image(&ret);
		if (image == nullptr) {
			log("Cannot make image from RAW data: {} {}", wstringToUtf8(path), libraw_strerror(ret));
			return { cv::Mat(), exifStr };
		}

		cv::Mat img(image->height, image->width, CV_8UC3, image->data);

		cv::Mat bgrImage;
		cv::cvtColor(img, bgrImage, cv::COLOR_RGB2BGR);

		exifStr += std::format("\n  分辨率: {}x{}", image->width, image->height);
		exifStr += std::format("\n     ISO: {}", rawProcessor->imgdata.other.iso_speed);
		exifStr += std::format("\n    相机: {} {}", rawProcessor->imgdata.idata.make, rawProcessor->imgdata.idata.model);
		if (rawProcessor->imgdata.other.shutter != 0)
			exifStr += std::format("\n    快门: {:.4f} s", rawProcessor->imgdata.other.shutter);
		if (rawProcessor->imgdata.other.aperture != 0)
			exifStr += std::format("\n    光圈: f/{}", rawProcessor->imgdata.other.aperture);
		if (rawProcessor->imgdata.other.focal_len != 0)
			exifStr += std::format("\n    焦距: {:.4f} mm", 1.0 / rawProcessor->imgdata.other.focal_len);
		if (rawProcessor->imgdata.other.timestamp)
			exifStr += std::format("\n    时间: {}\n  时间戳: {}",
				timeStamp2Str(rawProcessor->imgdata.other.timestamp),
				rawProcessor->imgdata.other.timestamp);

		auto& longitude = rawProcessor->imgdata.other.parsed_gps.longitude;
		if (longitude[0] != 0 && longitude[1] != 0 && longitude[2] != 0)
			exifStr += std::format("\n GPS经度: {:.6f}°{}",
				longitude[0] + longitude[1] / 60 + longitude[2] / 3600,
				rawProcessor->imgdata.other.parsed_gps.longref);

		auto& latitude = rawProcessor->imgdata.other.parsed_gps.latitude;
		if (latitude[0] != 0 && latitude[1] != 0 && latitude[2] != 0)
			exifStr += std::format("\n GPS纬度: {:.6f}°{}",
				latitude[0] + latitude[1] / 60 + latitude[2] / 3600,
				rawProcessor->imgdata.other.parsed_gps.latref);

		if (rawProcessor->imgdata.other.parsed_gps.altitude != 0)
			exifStr += std::format("\n GPS海拔: {:.2f} m",
				rawProcessor->imgdata.other.parsed_gps.altitude);

		exifStr += std::format("\n RAW尺寸: {}x{}", rawProcessor->imgdata.sizes.raw_width, rawProcessor->imgdata.sizes.raw_height);
		exifStr += std::format("\n颜色模式: {}", rawProcessor->imgdata.idata.cdesc);
		exifStr += std::format("\n  Foveon: {}", rawProcessor->imgdata.idata.is_foveon ? "是" : "否");

		// 获取EXIF信息
		if (rawProcessor->imgdata.idata.dng_version) {
			exifStr += std::format("\n DNG版本: {}", rawProcessor->imgdata.idata.dng_version);
		}
		if (rawProcessor->imgdata.other.artist[0]) {
			exifStr += std::format("\n  艺术家: {}", rawProcessor->imgdata.other.artist);
		}
		if (rawProcessor->imgdata.idata.software) {
			exifStr += std::format("\n    软件: {}", rawProcessor->imgdata.idata.software);
		}
		if (rawProcessor->imgdata.other.desc[0]) {
			exifStr += std::format("\n    描述: {}", rawProcessor->imgdata.other.desc);
		}

		// Clean up
		LibRaw::dcraw_clear_mem(image);
		rawProcessor->recycle();

		return { bgrImage, exifStr };
	}

	// TODO
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
			log("cvMat cannot decode: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		if (img.channels() != 1 && img.channels() != 3 && img.channels() != 4) {
			log("cvMat unsupport channel: {}", img.channels());
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
		log("Special: {}, img.depth(): {}, img.channels(): {}",
			wstringToUtf8(path), img.depth(), img.channels());
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
			log("DGifOpen: Error: {} {}", wstringToUtf8(path), GifErrorString(error));
			frames.push_back({ getDefaultMat(), 0 });
			return frames;
		}

		if (DGifSlurp(gif) != GIF_OK) {
			log("DGifSlurp Error: {} {}", wstringToUtf8(path), GifErrorString(gif->Error));
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

	Frames loadImage(const wstring& path) {
		if (path.length() < 4) {
			log("path.length() < 4: {}", wstringToUtf8(path));
			return { {{getDefaultMat(), 0}}, "" };
		}

		auto f = _wfopen(path.c_str(), L"rb");
		if (f == nullptr) {
			log("path canot open: {}", wstringToUtf8(path));
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
				ret.exifStr = getExif(path, ret.imgList.front().img.cols, ret.imgList.front().img.rows, tmp.data(), fileSize);
			return ret;
		}

		cv::Mat img;
		if (ext == L".heic" || ext == L".heif") {
			auto [_img, exifStr] = loadHeic(path, tmp, fileSize);
			img = std::move(_img);
			ret.exifStr = std::move(exifStr);
		}
		else if (ext == L".avif" || ext == L".avifs") {
			img = loadAvif(path, tmp, fileSize);
		}
		else if (supportRaw.contains(ext)) {
			auto [_img, exifStr] = loadRaw(path, tmp, fileSize);
			img = std::move(_img);
			ret.exifStr = std::move(exifStr);
		}

		if (img.empty())
			img = loadMat(path, tmp, fileSize);

		if (ret.exifStr.empty())
			ret.exifStr = getExif(path, img.cols, img.rows, tmp.data(), fileSize);

		if (img.empty())
			img = getDefaultMat();

		ret.imgList.emplace_back(img, 0);

		return ret;
	}

	static HWND getCvWindow(LPCSTR lpWndName) {
		HWND hWnd = (HWND)cvGetWindowHandle(lpWndName);
		if (IsWindow(hWnd)) {
			HWND hParent = GetParent(hWnd);
			DWORD dwPid = 0;
			GetWindowThreadProcessId(hWnd, &dwPid);
			if (dwPid == GetCurrentProcessId()) {
				return hParent;
			}
		}

		return hWnd;
	}

	static void setCvWindowIcon(HINSTANCE hInstance, HWND hWnd, WORD wIconId) {
		if (IsWindow(hWnd)) {
			HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(wIconId));
			SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			SendMessageW(hWnd, (WPARAM)WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
	}
}