#pragma once

#include <algorithm>
#include <concepts>
#include <deque>
#include <mutex>
#include <optional>

namespace dp {
    /**
     * @brief Simple concept for the Lockable and Basic Lockable types as defined by the C++
     * standard.
     * @details See https://en.cppreference.com/w/cpp/named_req/Lockable and
     * https://en.cppreference.com/w/cpp/named_req/BasicLockable for details.
     */
    template <typename Lock>
    concept is_lockable = requires(Lock&& lock) {
        lock.lock();
        lock.unlock();
        { lock.try_lock() } -> std::convertible_to<bool>;
    };

    template <typename T, typename Lock = std::mutex>
        requires is_lockable<Lock>
    class thread_safe_queue {
      public:
        using value_type = T;
        using size_type = typename std::deque<T>::size_type;

        thread_safe_queue() = default;

        void push_back(T&& value) {
            std::scoped_lock lock(mutex_);
            data_.push_back(std::forward<T>(value));
        }

        void push_front(T&& value) {
            std::scoped_lock lock(mutex_);
            data_.push_front(std::forward<T>(value));
        }

        [[nodiscard]] bool empty() const {
            std::scoped_lock lock(mutex_);
            return data_.empty();
        }

        [[nodiscard]] std::optional<T> pop_front() {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) return std::nullopt;

            auto front = std::move(data_.front());
            data_.pop_front();
            return front;
        }

        [[nodiscard]] std::optional<T> pop_back() {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) return std::nullopt;

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        }

        [[nodiscard]] std::optional<T> steal() {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) return std::nullopt;

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        }

        void rotate_to_front(const T& item) {
            std::scoped_lock lock(mutex_);
            auto iter = std::find(data_.begin(), data_.end(), item);

            if (iter != data_.end()) {
                std::ignore = data_.erase(iter);
            }

            data_.push_front(item);
        }

        [[nodiscard]] std::optional<T> copy_front_and_rotate_to_back() {
            std::scoped_lock lock(mutex_);

            if (data_.empty()) return std::nullopt;

            auto front = data_.front();
            data_.pop_front();

            data_.push_back(front);

            return front;
        }

      private:
        std::deque<T> data_{};
        mutable Lock mutex_{};
    };
}  // namespace dp
