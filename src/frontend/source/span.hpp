#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm> // Necessário para std::min e std::max
#include <infra/util/result.hpp> 
#include <string>

namespace zith::frontend {

    using ByteOffset = uint32_t;
    using FileId = uint32_t;

    struct Loc {
        ByteOffset line = 1, col = 1;
        auto toString(){
            return std::to_string(line)+":"+std::to_string(col);
        }
    };

    struct Span { 
        FileId file; 
        ByteOffset start; 
        ByteOffset end; 

        // Simplificado para usar o template default <T>
        auto len() const noexcept -> zith::infra::util::Result<size_t> {
            if (end < start) {
                return zith::infra::util::Result<size_t>(
                    zith::infra::util::Error{"end lesser than start"}
                );
            }
            return static_cast<size_t>(end - start);
        }

        bool contains(size_t offset) const noexcept {
            return (offset >= start && offset < end); 
        }

        bool isValid() const noexcept {
            return (end >= start);
        }

        // Simplificado para usar o template default <Span>
        auto merge(const Span& other) const noexcept -> zith::infra::util::Result<Span> {
            if (file != other.file) {
                return zith::infra::util::Result<Span>(
                    zith::infra::util::Error{"Cannot merge spans from different files"}
                );
            }

            bool overlaps = (start <= other.end) && (other.start <= end);
            if (!overlaps) {
                return zith::infra::util::Result<Span>(
                    zith::infra::util::Error{"Spans are disjoint and cannot be merged"}
                );
            }

            return Span {
                file,
                std::min(start, other.start),
                std::max(end, other.end)
            };
        }
    };

} // namespace zith::frontend
