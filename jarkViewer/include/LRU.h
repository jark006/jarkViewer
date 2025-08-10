#pragma once
#include <unordered_map>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <unordered_set>
#include <shared_mutex>

template<typename keyType, typename valueType>
class LRU {
private:
    // 使用shared_ptr包装数据，确保数据不会被意外释放
    using ValuePtr = std::shared_ptr<valueType>;
    using ListIterator = typename std::list<std::pair<keyType, ValuePtr>>::iterator;

    // 原有的缓存结构，但使用shared_ptr
    std::unordered_map<keyType, ListIterator> cache_map;
    std::list<std::pair<keyType, ValuePtr>> cache_list;
    size_t CAPACITY = 5;
    keyType loadingKey;  // 预读取线程正在加载数据中的key

    // 预读取相关的成员
    std::thread preload_thread;
    std::queue<keyType> preload_queue;
    std::unordered_set<keyType> preload_pending;
    mutable std::shared_mutex cache_mutex;  // 使用读写锁提高性能
    std::mutex preload_mutex;
    std::condition_variable preload_cv;
    std::atomic<bool> stop_preload{ false };

    // 预读取工作线程函数
    void preloadWorker() {
        while (!stop_preload) {
            std::unique_lock<std::mutex> lock(preload_mutex);
            preload_cv.wait(lock, [this] { return !preload_queue.empty() || stop_preload; });

            if (stop_preload) break;

            loadingKey = preload_queue.front();
            preload_queue.pop();
            preload_pending.erase(loadingKey);
            lock.unlock();

            // 检查是否已经在缓存中（使用读锁）
            {
                std::shared_lock<std::shared_mutex> cache_lock(cache_mutex);
                if (cache_map.contains(loadingKey)) {
                    loadingKey = {};
                    continue;
                }
            }

            // 执行预读取
            try {
                valueType value = loader(loadingKey);
                auto value_ptr = std::make_shared<valueType>(std::move(value));

                // 将预读取的数据放入缓存（使用写锁）
                std::unique_lock<std::shared_mutex> cache_lock(cache_mutex);
                putInternal(loadingKey, value_ptr);
            }
            catch (...) {
                // 预读取失败，忽略错误继续处理下一个
            }
            loadingKey = {};
        }
    }

    // 内部put函数，不加锁版本
    void putInternal(const keyType& key, ValuePtr value_ptr) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            it->second->second = value_ptr;
            cache_list.splice(cache_list.begin(), cache_list, it->second);
        }
        else {
            if (cache_map.size() >= CAPACITY) {
                cache_map.erase(cache_list.back().first);
                cache_list.pop_back();
            }
            cache_list.emplace_front(key, value_ptr);
            cache_map[key] = cache_list.begin();
        }
    }

public:
    LRU() {
        preload_thread = std::thread(&LRU::preloadWorker, this);
    }

    virtual ~LRU() {
        stop_preload = true;
        preload_cv.notify_all();
        if (preload_thread.joinable()) {
            preload_thread.join();
        }
    }

    // 禁用拷贝构造和赋值
    LRU(const LRU&) = delete;
    LRU& operator=(const LRU&) = delete;

    virtual valueType loader(const keyType&) = 0;

    // 安全的获取函数，返回shared_ptr
    std::shared_ptr<valueType> getSafePtr(const keyType& key) {
        int cnt_16ms = 0;
        while (key == loadingKey) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 因windows系统限制 实际最小 15.625ms
            if (++cnt_16ms > 640) // 最多等10秒
                break;
        }

        std::unique_lock<std::shared_mutex> lock(cache_mutex);
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            cache_list.splice(cache_list.begin(), cache_list, it->second);
            return it->second->second;
        }

        // 需要加载数据
        lock.unlock();
        valueType value = loader(key);
        auto value_ptr = std::make_shared<valueType>(std::move(value));
        lock.lock();

        putInternal(key, value_ptr);
        return cache_list.begin()->second;
    }

    // 带预读取的安全获取函数
    std::shared_ptr<valueType> getSafePtr(const keyType& key, const keyType& nextKey) {
        if(key != nextKey)
            requestPreload(nextKey);
        return getSafePtr(key);
    }

    // 请求预读取指定的key
    void requestPreload(const keyType& key) {
        std::lock_guard<std::mutex> lock(preload_mutex);

        if (preload_pending.find(key) != preload_pending.end()) {
            return;
        }

        // 使用读锁检查缓存
        {
            std::shared_lock<std::shared_mutex> cache_lock(cache_mutex);
            if (cache_map.find(key) != cache_map.end()) {
                return;
            }
        }

        preload_queue.push(key);
        preload_pending.insert(key);
        preload_cv.notify_one();
    }

    // 批量预读取
    void requestPreloadBatch(const std::vector<keyType>& keys) {
        std::lock_guard<std::mutex> lock(preload_mutex);

        for (const auto& key : keys) {
            if (preload_pending.find(key) != preload_pending.end()) {
                continue;
            }

            {
                std::shared_lock<std::shared_mutex> cache_lock(cache_mutex);
                if (cache_map.find(key) != cache_map.end()) {
                    continue;
                }
            }

            preload_queue.push(key);
            preload_pending.insert(key);
        }

        if (!keys.empty()) {
            preload_cv.notify_all();
        }
    }

    void put(const keyType& key, valueType&& value) {
        auto value_ptr = std::make_shared<valueType>(std::move(value));
        std::unique_lock<std::shared_mutex> lock(cache_mutex);
        putInternal(key, value_ptr);
    }

    void clear() {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex);
        std::lock_guard<std::mutex> preload_lock(preload_mutex);

        cache_map.clear();
        cache_list.clear();

        std::queue<keyType> empty_queue;
        preload_queue.swap(empty_queue);
        preload_pending.clear();
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(cache_mutex);
        return cache_map.size();
    }

    void setCapacity(size_t capacity) {
        if (capacity < 3 || capacity > 4096)
            capacity = 3;

        std::unique_lock<std::shared_mutex> lock(cache_mutex);
        CAPACITY = capacity;

        while (cache_map.size() > CAPACITY) {
            cache_map.erase(cache_list.back().first);
            cache_list.pop_back();
        }
    }

    bool contains(const keyType& key) const {
        std::shared_lock<std::shared_mutex> lock(cache_mutex);
        return cache_map.find(key) != cache_map.end();
    }

    // 获取预读取队列的大小
    size_t getPreloadQueueSize() const {
        std::lock_guard<std::mutex> lock(preload_mutex);
        return preload_queue.size();
    }

    // 等待所有预读取任务完成
    void waitForPreloadComplete() {
        std::unique_lock<std::mutex> lock(preload_mutex);
        while (!preload_queue.empty()) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            lock.lock();
        }
    }

    // RAII包装器，用于安全地使用数据
    class SafeDataAccess {
    private:
        std::shared_ptr<valueType> data_ptr;
    public:
        explicit SafeDataAccess(std::shared_ptr<valueType> ptr) : data_ptr(std::move(ptr)) {}

        valueType& operator*() { return *data_ptr; }
        valueType* operator->() { return data_ptr.get(); }
        const valueType& operator*() const { return *data_ptr; }
        const valueType* operator->() const { return data_ptr.get(); }
        valueType* get() { return data_ptr.get(); }
        const valueType* get() const { return data_ptr.get(); }
    };

    // 推荐的安全访问方式
    SafeDataAccess access(const keyType& key) {
        return SafeDataAccess(getSafePtr(key));
    }

    SafeDataAccess access(const keyType& key, const keyType& nextKey) {
        return SafeDataAccess(getSafePtr(key, nextKey));
    }
};