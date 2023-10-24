#pragma once

#include<iostream>
#include<format>

#include<filesystem>
#include<chrono>
#include<mutex>
#include<semaphore>

#include<string>
#include<vector>
#include<set>
#include<map>
#include<unordered_set>
#include<unordered_map>

#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

using std::vector;
using std::string;
using std::wstring;
using std::string_view;
using std::set;
using std::map;
using std::unordered_map;
using std::cout;
using std::endl;

//#include "windows.h"
//#pragma comment(lib,"winmm.lib")

#include "framework.h"
#include "resource.h"

#include<opencv2/core.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>
#include<opencv2/highgui/highgui_c.h>


struct rcFileInfo {
	uint8_t* addr = nullptr;
	size_t size = 0;
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
};

namespace Utils {

	static const bool DEBUG = true;
	template<typename... Args>
	static void logxx(const string_view fmt, Args&&... args) {
		static FILE* fp = nullptr;

		return;

		if (DEBUG) {
			if (!fp) {
				fp = fopen("Z:\\log.txt", "a");
				if (!fp)return;
			}

			auto ts = time(nullptr) + 8 * 3600ULL;// UTC+8
			int HH = (ts / 3600) % 24;
			int MM = (ts / 60) % 60;
			int SS = ts % 60;

			fprintf(fp, "[%02d:%02d:%02d] ", HH, MM, SS);

			const string str = std::vformat(fmt, std::make_format_args(args...));
			fwrite(str.c_str(), 1, str.length(), fp);

			fwrite("\n", 1, 1, fp);
			fflush(fp);
		}
		else {
			cout << std::vformat(fmt, std::make_format_args(args...)) << endl;
		}
	}


	template<typename... Args>
	static void log(const string_view fmt, Args&&... args)
	{
		static SOCKET sockSer = 0;
		static SOCKADDR_IN addr_in{
			.sin_family = AF_INET,
			.sin_port = htons(80),
			//.sin_addr = {.S_un = {.S_addr = inet_addr("127.0.0.1")}}
			.sin_addr = {.S_un = {.S_un_b = {127,0,0,1}}}
		};

		if (sockSer <= 0) {
			WORD wVersionRequested;
			WSADATA wsaData;
			wVersionRequested = MAKEWORD(1, 1);
			int err = WSAStartup(wVersionRequested, &wsaData);
			if (err != 0)
			{
				return;
			}
			if (LOBYTE(wsaData.wVersion) != 1 ||
				HIBYTE(wsaData.wVersion) != 1)
			{
				WSACleanup();
				return;
			}

			sockSer = socket(AF_INET, SOCK_DGRAM, 0);
			//addr_in.sin_family = AF_INET;
			//addr_in.sin_port = htons(60011);
			//addr_in.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
		}

		const string str = std::vformat(fmt, std::make_format_args(args...));
		sendto(sockSer, str.c_str(), (int)str.length(), 0, (const sockaddr*)&addr_in, sizeof(SOCKADDR_IN));
		return;
	}

	string bin2Hex(const void* bytes, const size_t len) {
		auto charList = "0123456789ABCDEF";
		if (len == 0) return "";
		string res(len * 3, ' ');
		for (size_t i = 0; i < len; i++) {
			const uint8_t value = reinterpret_cast<const uint8_t*>(bytes)[i];
			res[i * 3] = charList[value >> 4];
			res[i * 3 + 1] = charList[value & 0x0f];
		}
		return res;
	}

	std::string wstringToUtf8(const std::wstring& wstr) {
		const int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string strRet(len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, strRet.data(), len, nullptr, nullptr);
		return strRet;
	}

	std::wstring utf8ToWstring(const std::string& str) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), len);
		return wstr;
	}

	std::string utf8ToAnsi(const std::string& str) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), len);

		len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string strRet(len, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, strRet.data(), len, nullptr, nullptr);
		return strRet;
	}

	std::string ansiToUtf8(const std::string& str) {
		int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(len, 0);
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, wstr.data(), len);

		len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string strRet(len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, strRet.data(), len, nullptr, nullptr);
		return strRet;
	}


	std::wstring ansiToWstring(const std::string& str) {
		int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(len, 0);
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, wstr.data(), len);
		return wstr;
	}

	std::string wstringToAnsi(const std::wstring& wstr) {
		int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string strRet(len, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, strRet.data(), len, nullptr, nullptr);
		return strRet;
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


	cv::Mat loadMat(const string& path) {
		//log("loading: {}", path);
		cv::Mat img = imread(path, cv::IMREAD_UNCHANGED);

		//if (img.empty())
		//	log("Cannot load: {}", path);

		if (img.channels() != 1 && img.channels() != 3 && img.channels() != 4) {
			//log("Unsupport channel: {}", img.channels());
			img.release();
		}
		return img;
	}
}