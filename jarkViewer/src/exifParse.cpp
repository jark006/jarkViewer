#include "exifParse.h"


std::string ExifParse::getSimpleInfo(const std::wstring& path, int width, int height, const uint8_t* buf, size_t fileSize) {
    return (path.ends_with(L".ico") || (width == 0 && height == 0)) ?
        std::format("路径: {}\n大小: {}\n",
            jarkUtils::wstringToUtf8(path), jarkUtils::size2Str(fileSize)) :
        std::format("路径: {}\n大小: {}\n分辨率: {}x{}",
            jarkUtils::wstringToUtf8(path), jarkUtils::size2Str(fileSize), width, height);
}

std::string ExifParse::handleMathDiv(const std::string& str) {
    if (str.empty())return "";

    int divIdx = -1;
    bool isNegative = (str[0] == '-');
    for (int i = isNegative ? 1 : 0; i < str.length(); i++) {
        int c = str[i];
        if ('0' <= c && c <= '9') {
            continue;
        }
        else if (c == '/') {
            if (divIdx == -1) {
                divIdx = i;
                continue;
            }
            else {
                divIdx = -1;
                break;
            }
        }
        else {
            divIdx = -1;
            break;
        }
    }

    if (divIdx > 0) {
        auto a = std::stoll(str.substr(0, divIdx));
        auto b = std::stoll(str.substr((size_t)divIdx + 1));

        if (isNegative)
            a = 0 - a;

        auto resStr = std::format("{:.2f}", (double)a / b);

        if (resStr.ends_with(".00"))
            resStr = resStr.substr(0, resStr.size() - 3);

        return resStr;
    }
    return "";
}

std::string ExifParse::exifDataToString(const std::wstring& path, const Exiv2::ExifData& exifData) {
    if (exifData.empty()) {
        jarkUtils::log("No EXIF data {}", jarkUtils::wstringToUtf8(path));
        return "";
    }

    std::ostringstream oss;
    std::ostringstream ossEnd;

    for (const auto& tag : exifData) {
        const std::string& tagName = tag.key();
        std::string translatedTagName;
        bool toEnd;
        if (tagName.starts_with("Exif.SubImage")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(14);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("子图" + tagName.substr(13, 2) + exifTagsMap.at(tag)) :
                ("子图" + tagName.substr(13));
        }
        else if (tagName.starts_with("Exif.Thumbnail")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(14);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("缩略图." + exifTagsMap.at(tag)) :
                ("缩略图" + tagName.substr(14));
        }
        else if (tagName.starts_with("Exif.Nikon")) {
            toEnd = true;
            translatedTagName = "尼康" + tagName.substr(10);
        }
        else if (tagName.starts_with("Exif.CanonCs")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(12);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(12);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("佳能Cs." + exifTagsMap.at(tag)) :
                ("佳能Cs." + tagName.substr(13));
        }
        else if (tagName.starts_with("Exif.CanonSi")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(12);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(12);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("佳能Si." + exifTagsMap.at(tag)) :
                ("佳能Si." + tagName.substr(13));
        }
        else if (tagName.starts_with("Exif.CanonPi")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(12);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(12);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("佳能Pi." + exifTagsMap.at(tag)) :
                ("佳能Pi." + tagName.substr(13));
        }
        else if (tagName.starts_with("Exif.CanonPa")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(12);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(12);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("佳能Pa." + exifTagsMap.at(tag)) :
                ("佳能Pa." + tagName.substr(13));
        }
        else if (tagName.starts_with("Exif.Canon")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(10);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(10);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("佳能." + exifTagsMap.at(tag)) :
                ("佳能." + tagName.substr(11));
        }
        else if (tagName.starts_with("Exif.Pentax")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(11);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(11);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("宾得." + exifTagsMap.at(tag)) :
                ("宾得." + tagName.substr(12));
        }
        else if (tagName.starts_with("Exif.Fujifilm")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(13);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(13);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("富士." + exifTagsMap.at(tag)) :
                ("富士." + tagName.substr(14));
        }
        else if (tagName.starts_with("Exif.Olympus")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(12);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(12);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("奥林巴斯." + exifTagsMap.at(tag)) :
                ("奥林巴斯." + tagName.substr(13));
        }
        else if (tagName.starts_with("Exif.Panasonic")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(14);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(14);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("松下." + exifTagsMap.at(tag)) :
                ("松下." + tagName.substr(15));
        }
        else if (tagName.starts_with("Exif.Sony1")) {
            toEnd = true;
            string tag = "Exif.Image" + tagName.substr(10);
            if (!exifTagsMap.contains(tag))tag = "Exif.Photo" + tagName.substr(10);
            translatedTagName = exifTagsMap.contains(tag) ?
                ("索尼." + exifTagsMap.at(tag)) :
                ("索尼." + tagName.substr(11));
        }
        else {
            toEnd = false;
            translatedTagName = exifTagsMap.contains(tagName) ? exifTagsMap.at(tagName) : tagName;
        }

        std::string tagValue;
        if (tag.typeId() == Exiv2::TypeId::undefined || tagName == "Exif.Image.XMLPacket") {
            auto tmp = tag.toString();
            std::istringstream iss(tmp);
            std::string result;
            int number;
            while (iss >> number) {
                if (number < ' ' || number > 127)
                    break;
                result += static_cast<char>(number);
            }
            tagValue = result.empty() ? tmp : result + " [" + tmp + "]";
        }
        else {
            tagValue = tag.toString();
        }

        if (tagName == "Exif.GPSInfo.GPSLatitudeRef" || tagName == "Exif.GPSInfo.GPSLongitudeRef") {
            if (tagValue.length() > 0) {
                switch (tagValue[0])
                {
                case 'N':tagValue = "北纬 " + tagValue; break;
                case 'S':tagValue = "南纬 " + tagValue; break;
                case 'E':tagValue = "东经 " + tagValue; break;
                case 'W':tagValue = "西经 " + tagValue; break;
                }
            }
        }

        if (tagName == "Exif.Photo.MakerNote") {
            if (tagValue.length() > 0 && tagValue.starts_with("Apple iOS")) {
                tagValue = "Apple IOS";
            }
        }

        if (tagName == "Exif.GPSInfo.GPSLatitude" || tagName == "Exif.GPSInfo.GPSLongitude") {
            auto firstSpaceIdx = tagValue.find_first_of(' ');
            auto secondSpaceIdx = tagValue.find_last_of(' ');
            if (firstSpaceIdx != string::npos && secondSpaceIdx != string::npos && firstSpaceIdx < secondSpaceIdx) {
                auto n1 = handleMathDiv(tagValue.substr(0, firstSpaceIdx));
                auto n2 = handleMathDiv(tagValue.substr(firstSpaceIdx + 1, secondSpaceIdx - firstSpaceIdx - 1));
                auto n3 = handleMathDiv(tagValue.substr(secondSpaceIdx + 1));
                tagValue = std::format("{}°{}' {}'' ({})", n1, n2, n3, tagValue);
            }
        }
        else if (tagName == "Exif.GPSInfo.GPSTimeStamp") {
            auto firstSpaceIdx = tagValue.find_first_of(' ');
            auto secondSpaceIdx = tagValue.find_last_of(' ');
            if (firstSpaceIdx != string::npos && secondSpaceIdx != string::npos && firstSpaceIdx < secondSpaceIdx) {
                auto n1 = handleMathDiv(tagValue.substr(0, firstSpaceIdx));
                auto n2 = handleMathDiv(tagValue.substr(firstSpaceIdx + 1, secondSpaceIdx - firstSpaceIdx - 1));
                auto n3 = handleMathDiv(tagValue.substr(secondSpaceIdx + 1));
                tagValue = std::format("{}:{}:{} ({})", n1, n2, n3, tagValue);
            }
        }
        else if (tagName == "Exif.Photo.UserComment") { // 可能包含AI生图prompt信息
            auto a = tag.value().clone();
            vector<uint8_t> buf(a->size());
            a->copy(buf.data(), Exiv2::ByteOrder::bigEndian);
            if (!memcmp(buf.data(), "UNICODE\0", 8)) {
                wstring str((wchar_t*)(buf.data() + 8), (buf.size() - 8) / 2);
                tagValue = jarkUtils::wstringToUtf8(str);
                auto idx = tagValue.find("\nNegative prompt");
                if (idx != string::npos) {
                    tagValue.replace(idx, 16, "\n\n反提示词");
                    tagValue = "\n\n正提示词: " + tagValue;
                }

                idx = tagValue.find("\nSteps:");
                if (idx != string::npos) {
                    tagValue.replace(idx, 7, "\n\n参数: Steps:");
                }
            }
        }
        else if (2 < tagValue.length() && tagValue.length() < 100) {
            auto res = handleMathDiv(tagValue);
            if (!res.empty())
                tagValue = std::format("{} ({})", res, tagValue);
        }

        string tmp;
        if (tagName == "Exif.Photo.UserComment")
            tmp = "\n" + translatedTagName + ": " + tagValue;
        else
            tmp = "\n" + translatedTagName + ": " + (tagValue.length() < 100 ? tagValue :
                tagValue.substr(0, 100) + std::format(" ...] length:{}", tagValue.length()));

        if (toEnd)ossEnd << tmp;
        else oss << tmp;
    }

    return oss.str() + ossEnd.str();
}

std::string ExifParse::xmpDataToString(const std::wstring& path, const Exiv2::XmpData& xmpData) {
    if (xmpData.empty()) {
        jarkUtils::log("No XMP data {}", jarkUtils::wstringToUtf8(path));
        return "";
    }

    string xmpStr = "\n\nXmp Data:";
    for (const auto& tag : xmpData) {
        xmpStr += "\n" + tag.key() + ": " + tag.value().toString();
    }
    return xmpStr;
}

std::string ExifParse::iptcDataToString(const std::wstring& path, const Exiv2::IptcData& IptcData) {
    if (IptcData.empty()) {
        jarkUtils::log("No Iptc data {}", jarkUtils::wstringToUtf8(path));
        return "";
    }

    string itpcStr = "\n\nIptc Data:";
    for (const auto& tag : IptcData) {
        itpcStr += "\n" + tag.key() + ": " + tag.value().toString();
    }
    return itpcStr;
}

std::string ExifParse::AI_Prompt(const std::wstring& path, const uint8_t* buf) {
    if (!strncmp((const char*)buf + 0x25, "tEXtparameters", 14)) {
        int length = (buf[0x21] << 24) + (buf[0x22] << 16) + (buf[0x23] << 8) + buf[0x24];
        if (length > 16 * 1024) {
            return "";
        }

        string prompt((const char*)buf + 0x29, length); // format: Windows-1252  ISO/IEC 8859-1（Latin-1）

        prompt = jarkUtils::wstringToUtf8(jarkUtils::latin1ToWstring(prompt));

        auto idx = prompt.find("parameters");
        if (idx != string::npos) {
            prompt.replace(idx, 11, "\n正提示词: ");
        }
        else {
            prompt = "\n正提示词: " + prompt;
        }

        idx = prompt.find("Negative prompt");
        if (idx != string::npos) {
            prompt.replace(idx, 16, "\n反提示词: ");
        }

        idx = prompt.find("\nSteps:");
        if (idx != string::npos) {
            prompt.replace(idx, 7, "\n\n参数: Steps:");
        }

        return "\nAI生图提示词 Prompt:\n" + prompt;
    }
    else if (!strncmp((const char*)buf + 0x25, "iTXtparameters", 14)) {
        int length = (buf[0x21] << 24) + (buf[0x22] << 16) + (buf[0x23] << 8) + buf[0x24];
        if (length > 16 * 1024) {
            return "";
        }

        string prompt((const char*)buf + 0x38, length - 15); // format: Windows-1252  ISO/IEC 8859-1（Latin-1）

        prompt = jarkUtils::wstringToUtf8(jarkUtils::latin1ToWstring(prompt));

        auto idx = prompt.find("parameters");
        if (idx != string::npos) {
            prompt.replace(idx, 11, "\n正提示词: ");
        }
        else {
            prompt = "\n正提示词: " + prompt;
        }

        idx = prompt.find("\nNegative prompt");
        if (idx != string::npos) {
            prompt.replace(idx, 16, "\n\n反提示词: ");
        }

        idx = prompt.find("\nSteps:");
        if (idx != string::npos) {
            prompt.replace(idx, 7, "\n\n参数: Steps:");
        }

        return "\nAI生图提示词 Prompt:\n" + prompt;
    }
    else if (!strncmp((const char*)buf + 0x25, "tEXtprompt", 10)) {

        int length = (buf[0x21] << 24) + (buf[0x22] << 16) + (buf[0x23] << 8) + buf[0x24];
        if (length > 16 * 1024) {
            return "";
        }

        string prompt((const char*)buf + 0x29 + 7, length - 7); // format: Windows-1252  ISO/IEC 8859-1（Latin-1）

        prompt = jarkUtils::wstringToUtf8(jarkUtils::latin1ToWstring(prompt));

        return "\nAI生图提示词 Prompt:\n" + prompt;
    }

    return "";
}

std::string ExifParse::getExif(const std::wstring& path, const uint8_t* buf, int fileSize) {
    try {
        auto image = Exiv2::ImageFactory::open(buf, fileSize);
        image->readMetadata();

        auto exifStr = exifDataToString(path, image->exifData());
        auto xmpStr = xmpDataToString(path, image->xmpData());
        auto iptcStr = iptcDataToString(path, image->iptcData());
        string prompt = AI_Prompt(path, buf);

        if ((exifStr.length() + xmpStr.length() + iptcStr.length() + prompt.length()) > 0)
            return  "\n\n【按 C 键复制图像全部信息】\n" + exifStr + xmpStr + iptcStr + prompt;
        else
            return "";
    }
    catch (Exiv2::Error& e) {
        jarkUtils::log("Caught Exiv2 exception {}\n{}", jarkUtils::wstringToUtf8(path), e.what());
        return "";
    }
    return "";
}

