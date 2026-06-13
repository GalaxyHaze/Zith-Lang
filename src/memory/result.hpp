#pragma once
#include <concepts>
#include <string>
#include <type_traits>
#include <variant> // Substitui a união manual propensa a bugs de ciclo de vida

namespace zith::memory {

struct Error {
    std::string msg;
};

template <class T>
concept Failable = std::derived_from<T, Error>;

template <class T, Failable E = Error> class Result {
    static_assert(!std::is_same_v<T, E>, "Result<T, E> requires T and E to be different types");
    std::variant<T, E> data;

public:
    Result(T &&val) : data(std::move(val)) {}
    Result(const T &val) : data(val) {}

    Result(E &&err) : data(std::move(err)){}
    Result(const E &err) : data(err){}

    bool isOk() const noexcept {
        return data.index() == 0;
    }
    bool isError() const noexcept {
        return !isOk();
    }

    explicit operator bool() const {
        return isOk();
    }

    T &value() {
        return std::get<0>(data);
    }
    const T &value() const {
        return std::get<0>(data);
    }

    E &error() {
        return std::get<1>(data);
    }
    const E &error() const {
        return std::get<1>(data);
    }

    template <typename F> auto map(F &&func) -> Result<std::invoke_result_t<F, T>, E> {
        using NewValueType = std::invoke_result_t<F, T>;
        if (isOk()) {
            return Result<NewValueType, E>(func(std::get<0>(data)));
        }
        return Result<NewValueType, E>(std::get<1>(data));
    }

    template <typename F> auto and_then(F &&func) -> std::invoke_result_t<F, T> {
        using NewResultType = std::invoke_result_t<F, T>;
        if (isOk()) {
            return func(std::get<0>(data));
        }
        return NewResultType(std::get<1>(data));
    }
};

} // namespace zith::memory
