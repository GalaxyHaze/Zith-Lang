#pragma once
#include "source_loc.hpp"
#include "span.hpp"
#include "../infra/util/optional.hpp"
#include "../infra/util/result.hpp"
#include <string_view>
#include <unordered_map>
#include <vector>
#include <shared_mutex> // Adicionado para std::shared_mutex e locks

namespace zith::frontend {

    struct Arena;

    class SourceMap {
        inline static Arena* pimp = nullptr;
        inline static std::vector<SourceLoc> files = {};
        inline static std::unordered_map<std::string_view, FileId> cache = {};
        
        // Mutex estático para proteger os dados estáticos acima
        inline static std::shared_mutex rw_mutex = {};

    public:
        SourceMap() = delete;
        SourceMap(SourceMap&&) = delete;
        SourceMap(const SourceMap&) = delete;    

        static auto add_file(const std::string_view path, const std::string_view content) -> FileId {
            // 1. Lock Partilhado (Leitura) para verificar se o ficheiro já existe na cache
            {
                std::shared_lock<std::shared_mutex> lock(rw_mutex);
                auto it = cache.find(path);
                if (it != cache.end()) {
                    return it->second;
                }
            } // O lock de leitura é libertado aqui automaticamente

            // 2. Lock Exclusivo (Escrita) para mutar o vetor e o mapa estáticos
            std::unique_lock<std::shared_mutex> lock(rw_mutex);
            
            // Dupla verificação (Double-checked locking): outra thread pode ter inserido 
            // o mesmo ficheiro no milissegundo em que trocámos de lock.
            auto it = cache.find(path);
            if (it != cache.end()) {
                return it->second;
            }

            FileId id = static_cast<FileId>(files.size());
            cache[path] = id;
            
            SourceLoc src{id, path, content};
            src.buildLines(); // Garante que as linhas são criadas antes da inserção
            
            files.emplace_back(std::move(src));
            return id;
        }

        // Não precisa de const noexcept em funções estáticas
        static bool isValid(FileId id) noexcept {
            std::shared_lock<std::shared_mutex> lock(rw_mutex); // Lock de Leitura
            return id < files.size();
        }

        static auto load_file(const std::string_view path) -> zith::infra::util::Result<FileId> {
            // Nota: Se a tua Arena ('pimp') não for thread-safe, a alocação do ficheiro 
            // no disco para a Arena terá de ser feita dentro de um lock exclusivo aqui.
            std::string_view content = "Just a place-holder for now";
            
            FileId id = add_file(path, content); // O add_file já gere os seus próprios locks
            return zith::infra::util::Result<FileId>(id);
        }

        static auto get(FileId id) noexcept -> zith::infra::util::Optional<SourceLoc> {
            std::shared_lock<std::shared_mutex> lock(rw_mutex); // Lock de Leitura
            if (id < files.size()) {
                return files[id]; 
            }
            return zith::infra::util::Optional<SourceLoc>(nullptr);
        }

        static auto snippet(const Span& a) noexcept -> zith::infra::util::Result<std::string_view> {
            std::shared_lock<std::shared_mutex> lock(rw_mutex); // Lock de Leitura
            if (a.file >= files.size()) {
                return zith::infra::util::Result<std::string_view>(
                    zith::infra::util::Error{"Invalid File ID in Span"}
                );
            }
            return files[a.file].snippet(a);
        }

        static auto loc(const Span& a) noexcept -> Loc {
            std::shared_lock<std::shared_mutex> lock(rw_mutex); // Lock de Leitura
            if (a.file >= files.size()) {
                return Loc{0, 0}; 
            }
            return files[a.file].loc(a.start);
        }
    }; 

} // namespace zith::frontend
