#pragma once
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "span.hpp"

#ifndef ZITH_IS_WASM
#include "mio/mmap.hpp"
#endif

namespace zith::memory {

struct SourceLoc {
#ifndef ZITH_IS_WASM
    using FileVar = std::variant<mio::mmap_source, mio::mmap_sink, std::string>;
#else
    using FileVar = std::string;
#endif
    FileVar file;
    std::string path;
    memory::DynArray<ByteOffset> line_starts;

    SourceLoc(FileVar file, std::string path, memory::Arena &arena)
        : file(std::move(file)), path(std::move(path)), line_starts(arena) {}

    std::string_view getSlice() const;

    const char *get() const;

    // Reconstrói o mapa de linhas. OBRIGATÓRIO chamar após carregar o content.
    void buildLines() noexcept;

    // O(log N) - Traduz um offset absoluto num par Linha/Coluna (1-indexed)
    Loc loc(const ByteOffset offset) const noexcept;

    // Cria um sub-SourceLoc baseado em limites de bytes
    auto slice(const ByteOffset start, const ByteOffset end) const
        -> Result<std::string_view, Error>;

    // Extrai o conteúdo textual exato apontado por um Span
    auto snippet(const Span &span) const noexcept -> Result<std::string_view, Error>;

    // Retorna apenas o nome do ficheiro, ignorando as diretorias
    auto filename() const noexcept -> std::string_view;
};

} // namespace zith::memory
