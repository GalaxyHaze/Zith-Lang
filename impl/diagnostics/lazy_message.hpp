#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace zith::diag {

class LazyMessage {
public:
    using Generator = std::function<std::string()>;

    LazyMessage() = default;

    template <std::invocable F>
        requires std::same_as<std::invoke_result_t<F>, std::string>
    explicit LazyMessage(F&& gen)
        : gen_(std::forward<F>(gen)) {}

    explicit LazyMessage(std::string msg)
        : cached_(std::move(msg)) {}

    LazyMessage(const LazyMessage&) = delete;
    LazyMessage& operator=(const LazyMessage&) = delete;

    LazyMessage(LazyMessage&&) noexcept = default;
    LazyMessage& operator=(LazyMessage&&) noexcept = default;

    const std::string& get() const {
        if (!cached_ && gen_) {
            cached_ = gen_();
        }
        if (cached_) return *cached_;
        static const std::string empty;
        return empty;
    }

    bool has_value() const { return cached_.has_value() || !!gen_; }

private:
    Generator gen_;
    mutable std::optional<std::string> cached_;
};

} // namespace zith::diag
