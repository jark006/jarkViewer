#pragma once

#include <unordered_map>

template<typename keyType, typename valueType, int MAX_SIZE = 10>
class LRU {  // 假假的 LRU
private:
    int size = 0;
    keyType link[MAX_SIZE];
    std::unordered_map<keyType, valueType> mapData;

public:
    valueType& get(const keyType& key, valueType(load)(const keyType&), valueType& defaultMat) {
        auto it = mapData.find(key);
        if (it != mapData.end()) {
            for (int i = 0; i < size; i++) {  // 顺序查找
                if (link[i] != key) continue;

                auto backup = std::move(link[i]);
                for (int j = i; j > 0; j--)
                    link[j] = std::move(link[j - 1]);
                link[0] = std::move(backup);;

                break;
            }
            return it->second;
        }

        valueType newData = load(key);
        if (newData.empty())
            newData = defaultMat;

        for (int j = size - 1; j > 0; j--)
            link[j] = std::move(link[j - 1]);

        link[0] = key;
        mapData[key] = std::move(newData);

        if (size < MAX_SIZE)
            size++;

        return mapData[key];
    }
};
