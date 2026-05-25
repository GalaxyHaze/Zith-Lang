#pragma once
#include "mio/mmap.hpp"
#include "span.hpp"
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace zith::frontend {

    struct SourceLoc {
        std::variant<mio::mmap_source, mio::mmap_sink, std::string> file;
        std::string path;
        std::vector<ByteOffset> line_starts;

        std::string_view getSlice() const;

        const char* get() const;

        // Reconstrói o mapa de linhas. OBRIGATÓRIO chamar após carregar o content.
        void buildLines() noexcept;

        // O(log N) - Traduz um offset absoluto num par Linha/Coluna (1-indexed)
        Loc loc(const ByteOffset offset) const noexcept;

        // Cria um sub-SourceLoc baseado em limites de bytes
        auto slice(const ByteOffset start, const ByteOffset end) const 
        -> zith::infra::util::Result<std::string_view, zith::infra::util::Error>;

        // Extrai o conteúdo textual exato apontado por um Span
        auto snippet(const Span& span) const noexcept 
        -> zith::infra::util::Result<std::string_view, zith::infra::util::Error>;

        // Retorna apenas o nome do ficheiro, ignorando as diretorias
        auto filename() const noexcept -> std::string_view;
    };

} // namespace zith::frontend
