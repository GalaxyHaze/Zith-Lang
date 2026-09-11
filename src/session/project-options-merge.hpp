#pragma once

#include "cli/options.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zith::session {

namespace detail {

inline const memory::DynArray<std::string> *projectValues(const ProjectConfig &config,
                                                          const std::string_view field) {
    if (field == "cSourceDirs")
        return &config.cSourceDirs;
    if (field == "defines")
        return &config.defines;
    if (field == "libraryDirs")
        return &config.libraryDirs;
    if (field == "libraries")
        return &config.libraries;
    return &config.includeDirs;
}

inline const memory::DynArray<std::string> *optionsValues(const Options &options,
                                                          const std::string_view field) {
    if (field == "cSourceDirs")
        return &options.cSourceDirs;
    if (field == "defines")
        return &options.defines;
    if (field == "libraryDirs")
        return &options.libraryDirs;
    if (field == "libraries")
        return &options.libraries;
    return &options.includeDirs;
}

} // namespace detail

// Appends project values first and CLI values second. The appender receives
// each value and whether it came from the CLI so call sites that normalize
// paths can keep the existing project/config distinction.
template <typename Appender>
void mergeStrings(const ProjectConfig &config, const Options &options, const std::string_view field,
                  Appender &&append) {
    const auto *configValues  = detail::projectValues(config, field);
    const auto *optionsValues = detail::optionsValues(options, field);
    for (const auto &value : *configValues)
        append(value, false);
    for (const auto &value : *optionsValues)
        append(value, true);
}

inline void mergeStrings(const ProjectConfig &config, const Options &options,
                         const std::string_view field, memory::DynArray<std::string> &output,
                         const bool append = true) {
    if (!append)
        output.clear();
    mergeStrings(config, options, field,
                 [&output](const std::string &value, const bool) { output.push(value); });
}

inline void mergeStrings(const ProjectConfig &config, const Options &options,
                         const std::string_view field, std::vector<std::string> &output,
                         const bool append = true) {
    if (!append)
        output.clear();
    const auto *configValues  = detail::projectValues(config, field);
    const auto *optionsValues = detail::optionsValues(options, field);
    output.reserve(output.size() + configValues->size() + optionsValues->size());
    for (const auto &value : *configValues)
        output.push_back(value);
    for (const auto &value : *optionsValues)
        output.push_back(value);
}

} // namespace zith::session
