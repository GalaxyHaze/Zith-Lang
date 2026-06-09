#include "source-file.hpp"
#include <string_view>

namespace zith::memory {

    std::string_view SourceLoc::getSlice() const {
        return std::visit(
                [](const auto &arg) -> std::string_view { return {arg.data(), arg.size()}; }, file);
    }

    const char *SourceLoc::get() const {
        return std::visit([](const auto &arg) { return arg.data(); }, file);
    }

    // Reconstrói o mapa de linhas. OBRIGATÓRIO chamar após carregar o content.
    void SourceLoc::buildLines() noexcept {
        line_starts.clear();
        line_starts.push(0); // Linha 1 começa no índice 0
        auto content = getSlice();

        for (ByteOffset i = 0; i < content.size(); i++) {
            if (content[i] == '\n') {
                line_starts.push(i + 1); // Próxima linha começa após o '\n'
            }
        }
    }

    // O(log N) - Traduz um offset absoluto num par Linha/Coluna (1-indexed)
    Loc SourceLoc::loc(const ByteOffset offset) const noexcept {
        if (offset >= getSlice().size()) {
            return Loc{0, 0}; // Fallback para fora de limites
        }

        // Procura a primeira linha que começa DEPOIS do offset
        auto it         = std::upper_bound(line_starts.begin(), line_starts.end(), offset);
        size_t line_idx = std::distance(line_starts.begin(), it) - 1;

        ByteOffset line = static_cast<ByteOffset>(line_idx + 1);
        ByteOffset col  = offset - line_starts[line_idx] + 1;

        return Loc{line, col};
    }

    // Cria um sub-SourceLoc baseado em limites de bytes
    auto SourceLoc::slice(const ByteOffset start, const ByteOffset end) const
            -> Result<std::string_view, Error> {
        auto content = getSlice();

        if (start >= content.size() || end > content.size() || start > end) {
            return Result<std::string_view, Error>(Error{"Invalid slice boundaries"});
        }
        return content.substr(start, end - start);
    }

    // Extrai o conteúdo textual exato apontado por um Span
    auto SourceLoc::snippet(const Span &span) const noexcept
            -> Result<std::string_view, Error> {
        auto content = getSlice();
        if (span.start >= content.size() || span.end > content.size() || span.start > span.end) {
            return Result<std::string_view, Error>(Error{"Span out of file bounds"});
        }
        return std::string_view{content.data() + span.start, span.end - span.start};
    }

    // Retorna apenas o nome do ficheiro, ignorando as diretorias
    auto SourceLoc::filename() const noexcept -> std::string_view {
        auto last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos) {
            return path;
        }
        return path.substr(last_slash + 1);
    }

} // namespace zith::memory
