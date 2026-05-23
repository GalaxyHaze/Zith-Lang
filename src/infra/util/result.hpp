#pragma once
#include <string_view>
#include <type_traits>
#include <concepts>
#include <variant> // Substitui a união manual propensa a bugs de ciclo de vida

namespace zith::infra::util {

    struct Error {
        std::string_view msg;
    };

    template<class T>
    concept Failable = std::derived_from<T, Error>;

    template<class T, Failable E = Error>
    class Result {
        // std::variant gere automaticamente os construtores, destrutores e movimentos de T e E!
        std::variant<T, E> data;
        bool valid;

    public:
        Result(T&& val) : data(std::move(val)), valid(true) {}
        Result(const T& val) : data(val), valid(true) {}
        
        explicit Result(E&& err) : data(std::move(err)), valid(false) {}
        explicit Result(const E& err) : data(err), valid(false) {}

        bool isOk() const noexcept { return valid; }
        bool isError() const noexcept { return !valid; }

        explicit operator bool() const { return valid; }

        T& value() { return std::get<0>(data); }
        const T& value() const { return std::get<0>(data); }

        E& error() { return std::get<1>(data); }
        const E& error() const { return std::get<1>(data); }

        template<typename F>
        auto map(F&& func) -> Result<std::invoke_result_t<F, T>, E> {
            using NewValueType = std::invoke_result_t<F, T>;
            if (valid) {
                return Result<NewValueType, E>(func(std::get<0>(data)));
            }
            return Result<NewValueType, E>(std::get<1>(data));
        }

        template<typename F>
        auto and_then(F&& func) -> std::invoke_result_t<F, T> {
            using NewResultType = std::invoke_result_t<F, T>;
            if (valid) {
                return func(std::get<0>(data));
            }
            return NewResultType(std::get<1>(data));
        }
    };

} // namespace zith::infra::util
