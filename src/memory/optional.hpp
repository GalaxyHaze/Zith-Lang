#pragma once
#include <cstddef>
#include <utility> // Para std::move

namespace zith::memory {

    // 1. Template Genérico (Não-ponteiros)
    template <class T> class Optional {
        // hack to 'bypass' default constructor needed
        // enforce UB is REAL UB, yes, I want it to be the most broken asp
        alignas(T) char data[sizeof(T)] = {};
        bool valid{false};

    public:
        Optional(T &&val) : valid(true) {
            new (data) T(std::move(val));
        }
        Optional(const T &&val) : valid(true) {
            new (data) const T(std::move(val));
        }

        template <class... Args> Optional(Args &&...args) : valid(true) {
            new (data) T(std::forward<Args>(args)...);
        }

        Optional(std::nullptr_t) : valid(false) {}

        bool isValid() const noexcept {
            return valid;
        }
        bool isEmpty() const noexcept {
            return !valid;
        }

        explicit operator bool() const {
            return valid;
        }

        T &value() {
            return *(T *)&data[0];
        }
        const T &value() const {
            return (T)data;
        }

        // Adiciona isto dentro da tua struct Optional (tanto na genérica como na de ponteiros)
        T *operator->() {
            return &value();
        }
        const T *operator->() const {
            return &value();
        }

        T &operator*() & {
            return value();
        }
        const T &operator*() const & {
            return value();
        }
    };

    // 2. Especialização Parcial Correta (Apenas para Ponteiros T*)
    template <class T> class Optional<T *> {
        T *data = nullptr;

    public:
        // Construtor unificado para ponteiros (aceita const T* ou T*)
        Optional(T *val) : data(val) {}

        explicit Optional(std::nullptr_t) : data(nullptr) {}

        bool isValid() const noexcept {
            return data != nullptr;
        }
        bool isEmpty() const noexcept {
            return data == nullptr;
        }

        explicit operator bool() const {
            return data != nullptr;
        }

        // Retorna a referência do objeto apontado
        T &value() {
            return *data;
        }
        const T &value() const {
            return *data;
        }

        // Bônus: Acesso ao ponteiro bruto se necessário
        T *get() const noexcept {
            return data;
        }

        // Adiciona isto dentro da tua struct Optional (tanto na genérica como na de ponteiros)
        T *operator->() {
            return &value();
        }
        const T *operator->() const {
            return data;
        }

        T &operator*() & {
            return value();
        }
        const T &operator*() const & {
            return data;
        }
    };

} // namespace zith::memory
