#pragma once
#include <unordered_map>
#include <list>

template<typename keyType, typename valueType>
class LRU {
private:
    using ListIterator = typename std::list<std::pair<keyType, valueType>>::iterator;
    std::unordered_map<keyType, ListIterator> cache_map;
    std::list<std::pair<keyType, valueType>> cache_list;
    size_t CAPACITY = 20;

public:
    LRU() = default;

    virtual valueType loader(const keyType&) = 0;

    valueType& get(const keyType& key) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            cache_list.splice(cache_list.begin(), cache_list, it->second);
            return it->second->second;
        }

        put(key, loader(key));
        return cache_list.begin()->second;
    }

    valueType* getPtr(const keyType& key) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            cache_list.splice(cache_list.begin(), cache_list, it->second);
            return &(it->second->second);
        }

        put(key, loader(key));
        return &(cache_list.begin()->second);
    }

    void put(const keyType& key, valueType&& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            it->second->second = std::move(value);
            cache_list.splice(cache_list.begin(), cache_list, it->second);
        }
        else {
            if (cache_map.size() >= CAPACITY) {
                cache_map.erase(cache_list.back().first);
                cache_list.pop_back();
            }
            cache_list.emplace_front(key, std::forward<valueType>(value));
            cache_map[key] = cache_list.begin();
        }
    }

    void clear() {
        cache_map.clear();
        cache_list.clear();
    }

    size_t size() const {
        return cache_map.size();
    }

    void setCapacity(size_t capacity) {
        if (capacity < 3 || capacity > 4096)
            capacity = 3;
        CAPACITY = capacity;
    }

    bool contains(const keyType& key) const {
        return cache_map.find(key) != cache_map.end();
    }
};