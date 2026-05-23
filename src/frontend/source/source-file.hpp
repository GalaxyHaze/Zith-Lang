#pragma once
#include "span.hpp"
#include <string_view>
#include <vector>
#include <algorithm>

namespace zith::frontend {

    struct SourceLoc {
        FileId id;
        std::string_view path;
        std::string_view content;
        std::vector<ByteOffset> line_starts;

        explicit SourceLoc(FileId id, std::string_view p, std::string_view c):
            id(id),
            path(p),
            content(c)
            {buildLines();}

        // Reconstrói o mapa de linhas. OBRIGATÓRIO chamar após carregar o content.
        void buildLines() noexcept {
            line_starts.clear();
            line_starts.push_back(0); // Linha 1 começa no índice 0
            for (ByteOffset i = 0; i < content.size(); i++) {
                if (content[i] == '\n') {
                    line_starts.push_back(i + 1); // Próxima linha começa após o '\n'
                }
            }
        }

        // O(log N) - Traduz um offset absoluto num par Linha/Coluna (1-indexed)
        Loc loc(const ByteOffset offset) const noexcept {
            if (offset >= content.size()) {
                return Loc{0, 0}; // Fallback para fora de limites
            }

            // Procura a primeira linha que começa DEPOIS do offset
            auto it = std::upper_bound(line_starts.begin(), line_starts.end(), offset);
            size_t line_idx = std::distance(line_starts.begin(), it) - 1;

            ByteOffset line = static_cast<ByteOffset>(line_idx + 1);
            ByteOffset col = offset - line_starts[line_idx] + 1;

            return Loc{line, col};
        }

        // Cria um sub-SourceLoc baseado em limites de bytes
        auto slice(const ByteOffset start, const ByteOffset end) const 
            -> zith::infra::util::Result<SourceLoc, zith::infra::util::Error> 
        {
            if (start >= content.size() || end > content.size() || start > end) {
                return zith::infra::util::Result<SourceLoc, zith::infra::util::Error>(
                    zith::infra::util::Error{"Invalid slice boundaries"}
                );
            }

            auto cpy = *this;
            cpy.content = std::string_view{content.data() + start, end - start};
            cpy.buildLines(); // Atualiza os line_starts para o novo contexto local
            
            return cpy;
        }

        // Extrai o conteúdo textual exato apontado por um Span
        auto snippet(const Span& span) const noexcept 
            -> zith::infra::util::Result<std::string_view, zith::infra::util::Error> 
        {
            if (span.file != id) {
                return zith::infra::util::Result<std::string_view, zith::infra::util::Error>(
                    zith::infra::util::Error{"Span file ID mismatch"}
                );
            }
            if (span.start >= content.size() || span.end > content.size() || span.start > span.end) {
                return zith::infra::util::Result<std::string_view, zith::infra::util::Error>(
                    zith::infra::util::Error{"Span out of file bounds"}
                );
            }
            return std::string_view{content.data() + span.start, span.end - span.start};
        }

        // Retorna apenas o nome do ficheiro, ignorando as diretorias
        auto filename() const noexcept -> std::string_view {
            auto last_slash = path.find_last_of('/');
            if (last_slash == std::string_view::npos) {
                return path;
            }
            return path.substr(last_slash + 1);
        }
    };

} // namespace zith::frontend
