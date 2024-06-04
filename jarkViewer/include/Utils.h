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

struct rcFileInfo {
	uint8_t* addr = nullptr;
	size_t size = 0;
};

union intUnion {
	uint32_t u32;
	uint8_t u8[4];
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
		int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
		std::wstring ret(len, 0);
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, ret.data(), len);
		return ret;
	}

	std::string wstringToAnsi(const std::wstring& wstr) {
		int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string ret(len, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, ret.data(), len, nullptr, nullptr);
		return ret;
	}

	//UTF8 to UTF16
	std::wstring utf8ToWstring(const std::string& str) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
		if (len == 0) return L"";

		std::wstring ret(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, ret.data(), len);

		return ret;
	}

	//UTF16 to UTF8
	std::string wstringToUtf8(const std::wstring& wstr) {
		int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
		if (len == 0) return "";

		std::string ret(len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, ret.data(), len, NULL, NULL);
		return ret;
	}

	std::string utf8ToAnsi(const std::string& str) {
		return wstringToAnsi(utf8ToWstring(str));
	}

	std::string ansiToUtf8(const std::string& str) {
		return wstringToUtf8(ansiToWstring(str));
	}

	rcFileInfo GetResource(unsigned int idi, const wchar_t* type)
	{
		rcFileInfo rc;

		HMODULE ghmodule = GetModuleHandle(NULL);
		if (ghmodule == NULL) {
			//log("ghmodule null");
			return rc;
		}

		HRSRC hrsrc = FindResource(ghmodule, MAKEINTRESOURCE(idi), type);
		if (hrsrc == NULL) {
			//log("hrstc null");
			return rc;
		}

		HGLOBAL hg = LoadResource(ghmodule, hrsrc);
		if (hg == NULL) {
			//log("hg null");
			return rc;
		}

		rc.addr = (unsigned char*)(LockResource(hg));
		rc.size = SizeofResource(ghmodule, hrsrc);
		return rc;
	}


	// HEIC ONLY, AVIF not support
	// https://github.com/strukturag/libheif
	// vcpkg install libheif
	cv::Mat loadHeic(const wstring& path) {
		cv::Mat ret;

		auto f = _wfopen(path.c_str(), L"rb");
		if (f == nullptr)return ret;

		fseek(f, 0, SEEK_END);
		auto fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		auto fileBuff = std::make_unique<uint8_t[]>(fileSize);
		fread(fileBuff.get(), 1, fileSize, f);
		fclose(f);

		auto filetype_check = heif_check_filetype(fileBuff.get(), 12);
		if (filetype_check == heif_filetype_no) {
			log("Input file is not an HEIF/AVIF file\n");
			return ret;
		}

		if (filetype_check == heif_filetype_yes_unsupported) {
			log("Input file is an unsupported HEIF/AVIF file type\n");
			return ret;
		}

		heif_context* ctx = heif_context_alloc();
		auto err = heif_context_read_from_memory_without_copy(ctx, fileBuff.get(), fileSize, nullptr);
		if (err.code) {
			log(err.message);
			return ret;
		}

		// get a handle to the primary image
		heif_image_handle* handle = nullptr;
		err = heif_context_get_primary_image_handle(ctx, &handle);
		if (err.code) {
			log(err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			return ret;
		}

		// decode the image and convert colorspace to RGB, saved as 24bit interleaved
		heif_image* img = nullptr;
		err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
		if (err.code) {
			log(err.message);
			if (ctx) heif_context_free(ctx);
			if (handle) heif_image_handle_release(handle);
			if (img) heif_image_release(img);
			return ret;
		}

		int stride = 0;
		const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

		auto width = heif_image_handle_get_width(handle);
		auto height = heif_image_handle_get_height(handle);

		cv::Mat matImg(height, width, CV_8UC4);
		memcpy(matImg.ptr(), data, (size_t)height * width * 4);

		//  ARGB -> ABGR
		[](uint32_t* data, size_t pixelCount) {
			for (size_t i = 0; i < pixelCount; ++i) {
				uint32_t pixel = data[i];
				uint32_t r = (pixel >> 16) & 0xFF;
				uint32_t b = pixel & 0xFF;
				data[i] = (b << 16) | (pixel & 0xff00ff00) | r;
			}
			}((uint32_t*)matImg.ptr(), (size_t)width * height);

		// clean up resources
		heif_context_free(ctx);
		heif_image_release(img);
		heif_image_handle_release(handle);

		return matImg;
	}


	cv::Mat loadMat(const wstring& path) {
		if (path.length() < 4)return cv::Mat();

		auto ext = path.substr(path.length() - 4);
		for (auto& c : ext) c = std::tolower(c);
		if (ext == L"heic" || ext == L"heif")
			return loadHeic(path);

		auto f = _wfopen(path.c_str(), L"rb");
		if (f == nullptr)return cv::Mat();
		fseek(f, 0, SEEK_END);
		auto size = ftell(f);
		fseek(f, 0, SEEK_SET);
		vector<uchar> tmp(size, 0);
		fread(tmp.data(), 1, size, f);
		fclose(f);

		auto img = cv::imdecode(tmp, cv::IMREAD_UNCHANGED);

		if (img.empty())
			log("Cannot load: {}", wstringToUtf8(path));

		if (img.channels() != 1 && img.channels() != 3 && img.channels() != 4) {
			log("Unsupport channel: {}", img.channels());
			img.release();
		}
		return img;
	}
}