#pragma once

#include "zith/token.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zith::diag {

using FileId = uint32_t;
static constexpr FileId INVALID_FILE_ID = UINT32_MAX;

struct SourceSpan {
    ZithSourceLoc start;
    ZithSourceLoc end;
    FileId file_id;

    bool contains(const SourceSpan& other) const {
        if (file_id != other.file_id) return false;
        if (other.start.line < start.line) return false;
        if (other.start.line == start.line && other.start.index < start.index) return false;
        if (other.end.line > end.line) return false;
        if (other.end.line == end.line && other.end.index > end.index) return false;
        return true;
    }

    static SourceSpan from_token(const ZithToken& tok, FileId fid) {
        SourceSpan s;
        s.start = tok.loc;
        s.end = ZithSourceLoc{tok.loc.index + tok.lexeme.len, tok.loc.line};
        s.file_id = fid;
        return s;
    }

    static SourceSpan from_loc(ZithSourceLoc loc, FileId fid) {
        SourceSpan s;
        s.start = loc;
        s.end = loc;
        s.file_id = fid;
        return s;
    }

    static SourceSpan merge(const SourceSpan& a, const SourceSpan& b) {
        SourceSpan s;
        s.file_id = a.file_id;
        if (a.start.line < b.start.line || (a.start.line == b.start.line && a.start.index < b.start.index))
            s.start = a.start;
        else
            s.start = b.start;
        if (a.end.line > b.end.line || (a.end.line == b.end.line && a.end.index > b.end.index))
            s.end = a.end;
        else
            s.end = b.end;
        return s;
    }
};

struct SourceFile {
    std::string filename;
    std::string source;
    std::vector<size_t> line_starts;

    void build_line_index() {
        line_starts.clear();
        line_starts.push_back(0);
        for (size_t i = 0; i < source.size(); ++i) {
            if (source[i] == '\n') {
                line_starts.push_back(i + 1);
            }
        }
    }

    size_t line_count() const { return line_starts.size(); }

    std::string_view line_text(size_t line_1based) const {
        if (line_1based == 0 || line_1based > line_starts.size()) return {};
        size_t idx = line_1based - 1;
        size_t start = line_starts[idx];
        size_t end = (idx + 1 < line_starts.size()) ? line_starts[idx + 1] : source.size();
        if (end > start && source[end - 1] == '\n') --end;
        return std::string_view(source.data() + start, end - start);
    }
};

class SourceMap {
public:
    FileId add_file(std::string filename, std::string source) {
        FileId id = static_cast<FileId>(files_.size());
        SourceFile sf;
        sf.filename = std::move(filename);
        sf.source = std::move(source);
        sf.build_line_index();
        files_.push_back(std::move(sf));
        return id;
    }

    FileId add_or_get_file(std::string_view filename, std::string_view source) {
        for (size_t i = 0; i < files_.size(); ++i) {
            if (files_[i].filename == filename) return static_cast<FileId>(i);
        }
        return add_file(std::string(filename), std::string(source));
    }

    const SourceFile* lookup(FileId id) const {
        if (id >= files_.size()) return nullptr;
        return &files_[id];
    }

    SourceFile* lookup(FileId id) {
        if (id >= files_.size()) return nullptr;
        return &files_[id];
    }

    size_t file_count() const { return files_.size(); }

private:
    std::vector<SourceFile> files_;
};

} // namespace zith::diag
