#pragma once
#include <cstddef>
#include <new>
#include <utility>

namespace zith::memory {

    class Arena {
    public:
        static constexpr size_t default_block_size = 4096;

        struct Block {
            Block *next = nullptr;
            Block *prev = nullptr;
            size_t size = 0; // usable data size (not including Block header)
        };

        Arena() : Arena(default_block_size) {}

        explicit Arena(size_t block_size) : block_size_(block_size) {
            addBlock_(block_size_);
        }

        ~Arena() noexcept {
            clear();
        }

        Arena(const Arena &)          = delete;
        auto operator=(const Arena &) = delete;

        Arena(Arena &&other) noexcept :
            head_(other.head_), current_(other.current_), offset_(other.offset_),
            block_size_(other.block_size_) {
            other.head_    = nullptr;
            other.current_ = nullptr;
            other.offset_  = 0;
        }

        auto operator=(Arena &&other) noexcept -> Arena & {
            if (this != &other) {
                clear();
                head_          = other.head_;
                current_       = other.current_;
                offset_        = other.offset_;
                block_size_    = other.block_size_;
                other.head_    = nullptr;
                other.current_ = nullptr;
                other.offset_  = 0;
            }
            return *this;
        }

        [[nodiscard]] auto alloc(size_t size, size_t alignment = alignof(std::max_align_t))
                -> void * {
            size_t aligned_offset = alignUp_(offset_, alignment);

            if (!current_ || aligned_offset + size > current_->size) {
                addBlock_(size);
                aligned_offset = 0;
            }

            offset_ = aligned_offset + size;
            return data_(current_) + aligned_offset;
        }

        template <typename T, typename... Args> [[nodiscard]] auto make(Args &&...args) -> T * {
            auto *ptr = static_cast<T *>(alloc(sizeof(T), alignof(T)));
            if (ptr) {
                ::new (ptr) T(std::forward<Args>(args)...);
            }
            return ptr;
        }

        [[nodiscard]] auto ptr() const noexcept -> const void * {
            return data_(current_) + offset_;
        }

        [[nodiscard]] auto remaining() const noexcept -> size_t {
            return current_ ? (current_->size - offset_) : 0;
        }

        void clear() noexcept {
            auto *block = head_;
            while (block) {
                auto *next = block->next;
                ::operator delete(block, std::align_val_t(alignof(Block)));
                block = next;
            }
            head_    = nullptr;
            current_ = nullptr;
            offset_  = 0;
        }

        friend class MarkPoint;

    private:
        Block *head_       = nullptr;
        Block *current_    = nullptr;
        size_t offset_     = 0;
        size_t block_size_ = default_block_size;

        void addBlock_(size_t min_size) {
            if (min_size < block_size_) {
                min_size = block_size_;
            }
            size_t total = sizeof(Block) + min_size;
            auto *mem    = ::operator new(total, std::align_val_t(alignof(Block)));
            auto *block  = ::new (mem) Block{};
            block->size  = min_size;
            block->prev  = current_;

            if (current_) {
                current_->next = block;
            } else {
                head_ = block;
            }
            current_ = block;
            offset_  = 0;
        }

        static auto data_(Block *block) noexcept -> char * {
            return reinterpret_cast<char *>(block) + sizeof(Block);
        }

        static auto alignUp_(size_t value, size_t alignment) noexcept -> size_t {
            size_t mask = alignment - 1;
            return (value + mask) & ~mask;
        }
    };

    class MarkPoint {
        Arena *arena_        = nullptr;
        Arena::Block *block_ = nullptr;
        size_t offset_       = 0;

    public:
        explicit MarkPoint(Arena &arena) noexcept :
            arena_(&arena), block_(arena.current_), offset_(arena.offset_) {}

        ~MarkPoint() noexcept {
            if (arena_) {
                rollback();
            }
        }

        MarkPoint(const MarkPoint &)      = delete;
        auto operator=(const MarkPoint &) = delete;

        MarkPoint(MarkPoint &&other) noexcept :
            arena_(other.arena_), block_(other.block_), offset_(other.offset_) {
            other.arena_ = nullptr;
        }

        auto operator=(MarkPoint &&other) noexcept -> MarkPoint & {
            if (this != &other) {
                if (arena_) {
                    rollback();
                }
                arena_       = other.arena_;
                block_       = other.block_;
                offset_      = other.offset_;
                other.arena_ = nullptr;
            }
            return *this;
        }

        void rollback() noexcept {
            auto *arena = arena_;
            arena_      = nullptr;
            if (!arena)
                return;

            auto *block = block_;
            auto offset = offset_;

            auto *to_free = block ? block->next : arena->head_;
            while (to_free) {
                auto *next = to_free->next;
                ::operator delete(to_free, std::align_val_t(alignof(Arena::Block)));
                to_free = next;
            }

            if (block) {
                block->next     = nullptr;
                arena->current_ = block;
            } else {
                arena->head_    = nullptr;
                arena->current_ = nullptr;
            }

            arena->offset_ = offset;
        }

        void release() noexcept {
            arena_ = nullptr;
        }
    };

} // namespace zith::memory
