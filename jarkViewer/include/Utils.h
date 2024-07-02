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

#include "libheif/heif.h"
#include "avif/avif.h"
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

struct ImageNode{
	cv::Mat img;
	string exifStr;
};

namespace Utils {

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

	// HEIC ONLY, AVIF not support
	// https://github.com/strukturag/libheif
	// vcpkg install libheif:x64-windows-static
	// vcpkg install libheif[hevc]:x64-windows-static
	cv::Mat loadHeic(const wstring& path, const vector<uchar>& buf, int fileSize) {

		auto filetype_check = heif_check_filetype(buf.data(), 12);
		if (filetype_check == heif_filetype_no) {
			log("Input file is not an HEIF/AVIF file: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		if (filetype_check == heif_filetype_yes_unsupported) {
			log("Input file is an unsupported HEIF/AVIF file type: {}", wstringToUtf8(path));
			return cv::Mat();
		}

		heif_context* ctx = heif_context_alloc();
		auto err = heif_context_read_from_memory_without_copy(ctx, buf.data(), fileSize, nullptr);
		if (err.code) {
			log("Error: {}", wstringToUtf8(path));
			log("heif_context_read_from_memory_without_copy error: {}", err.message);
			return cv::Mat();
		}

		// get a handle to the primary image
		heif_image_handle* handle = nullptr;
		err = heif_context_get_primary_image_handle(ctx, &handle);
		if (err.code) {
			log("Error: {}", wstringToUtf8(path));
			log("heif_context_get_primary_image_handle error: {}", err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			return cv::Mat();
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


	static std::string getExif(const wstring& path, int width, int height, const uint8_t* buf, int fileSize) {
		static easyexif::EXIFInfo exifInfo;

		auto res = std::format("    路径: {}\n  分辨率: {}x{}", Utils::wstringToUtf8(path), width, height);

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


	static ImageNode loadImage(const wstring& path) {
		if (path.length() < 4) {
			log("path.length() < 4: {}", wstringToUtf8(path));
			return { getDefaultMat(), "" };
		}

		auto f = _wfopen(path.c_str(), L"rb");
		if (f == nullptr) {
			log("path canot open: {}", wstringToUtf8(path));
			return { getDefaultMat(), "" };
		}

		fseek(f, 0, SEEK_END);
		auto fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		vector<uchar> tmp(fileSize, 0);
		fread(tmp.data(), 1, fileSize, f);
		fclose(f);

		auto ext = path.substr(path.length() - 4);
		for (auto& c : ext)	c = std::tolower(c);

		ImageNode ret;
		if (ext == L"heic" || ext == L"heif")
			ret.img = loadHeic(path, tmp, fileSize);
		else if(ext == L"avif" || ext == L"vifs") // .avifs
			ret.img = loadAvif(path, tmp, fileSize);
		else
			ret.img = loadMat(path, tmp, fileSize);

		if (ret.img.empty()) {
			ret.img = getDefaultMat();
			ret.exifStr = "";
		}
		else {
			ret.exifStr = getExif(path, ret.img.cols, ret.img.rows, tmp.data(), fileSize);
		}

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