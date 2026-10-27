// src/tools/workspace.cpp — YOU IMPLEMENT (spec §4.4).
//
// Workspace::resolve: a purely lexical check, steps 1–6 of the spec.
// Do not touch the filesystem here (no exists(), canonical(), ...).
// The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    try {
        reason->clear();
        if (user_path.empty()) {
            *reason = "empty path";
            return false;
        }
        if (user_path.find('\0') != std::string_view::npos) {
            *reason = "path contains a NUL byte";
            return false;
        }

        const fs::path path{std::string(user_path)};
        if (path.is_absolute() || path.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }

        // Simplify the path first. a/../../b becomes ../b.
        const fs::path normalized = path.lexically_normal();
        // A leading .. means the path leaves the workspace.
        if (normalized.begin() != normalized.end() && *normalized.begin() == "..") {
            *reason = "path escapes the workspace";
            return false;
        }

        // a/.. refers to the workspace root after normalization.
        *out = normalized == "." ? root() : root() / normalized;
        return true;
    } catch (...) {
        *reason = "invalid path";
        return false;
    }
}
