#pragma once

#include <vector>
#include <string>
#include <tinyxml2.h>

// 因lunaSVG不支持<switch>标签，需预处理SVG
class SVGPreprocessor {
public:
    std::string preprocessSVG(const char* svgContentPtr, size_t nBytes, const std::string& language = "en") {
        cv::tinyxml2::XMLDocument doc;
        if (doc.Parse(svgContentPtr, nBytes) != cv::tinyxml2::XML_SUCCESS) {
            return {};
        }

        processSwitchElements(doc.RootElement(), language);

        cv::tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        return printer.CStr();
    }

private:
    void processSwitchElements(cv::tinyxml2::XMLElement* element, const std::string& language) {
        if (!element) return;

        // 先处理所有子元素中的switch（深度优先）
        std::vector<cv::tinyxml2::XMLElement*> children;
        for (auto child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
            children.push_back(child);
        }

        for (auto child : children) {
            processSwitchElements(child, language);
        }

        // 处理当前元素如果是 switch
        if (std::string(element->Name()) == "switch") {
            processSwitchElement(element, language);
        }
    }

    void processSwitchElement(cv::tinyxml2::XMLElement* switchElement, const std::string& language) {
        cv::tinyxml2::XMLElement* selectedChild = nullptr;

        // 按顺序检查子元素
        for (auto child = switchElement->FirstChildElement(); child; child = child->NextSiblingElement()) {
            if (shouldSelectElement(child, language)) {
                selectedChild = child;
                break;
            }
        }

        if (selectedChild) {
            auto parent = switchElement->Parent();
            auto doc = switchElement->GetDocument();

            // 手动克隆选中的元素
            auto clonedElement = cloneElement(selectedChild, doc);

            // 替换 switch 元素
            parent->InsertAfterChild(switchElement, clonedElement);
            parent->DeleteChild(switchElement);
        }
        else {
            // 如果没有匹配的元素，删除整个switch
            auto parent = switchElement->Parent();
            parent->DeleteChild(switchElement);
        }
    }

    // 手动实现元素克隆
    cv::tinyxml2::XMLElement* cloneElement(cv::tinyxml2::XMLElement* source, cv::tinyxml2::XMLDocument* doc) {
        auto cloned = doc->NewElement(source->Name());

        // 复制属性
        for (auto attr = source->FirstAttribute(); attr; attr = attr->Next()) {
            cloned->SetAttribute(attr->Name(), attr->Value());
        }

        // 复制文本内容
        if (source->GetText()) {
            cloned->SetText(source->GetText());
        }

        // 递归复制子元素
        for (auto child = source->FirstChildElement(); child; child = child->NextSiblingElement()) {
            auto clonedChild = cloneElement(child, doc);
            cloned->InsertEndChild(clonedChild);
        }

        return cloned;
    }

    bool shouldSelectElement(cv::tinyxml2::XMLElement* element, const std::string& language) {
        // 检查 systemLanguage 属性
        const char* systemLang = element->Attribute("systemLanguage");
        if (systemLang) {
            std::string lang(systemLang);
            // 支持语言列表（空格分隔）
            return lang.find(language) != std::string::npos ||
                lang.find(language.substr(0, 2)) != std::string::npos;
        }

        // 检查 requiredFeatures 属性
        const char* requiredFeatures = element->Attribute("requiredFeatures");
        if (requiredFeatures) {
            // 这里可以根据lunaSVG支持的特性进行判断
            // 暂时返回true，表示支持所有特性
            return true;
        }

        // 检查 requiredExtensions 属性
        const char* requiredExtensions = element->Attribute("requiredExtensions");
        if (requiredExtensions) {
            // lunaSVG通常不支持扩展，返回false
            return false;
        }

        // 没有条件属性的元素总是被选中
        return true;
    }
};