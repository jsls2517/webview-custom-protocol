/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 * Copyright (c) 2022 Steffen André Langnes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WEBVIEW_DETAIL_UTILITY_ASSETS_HH
#define WEBVIEW_DETAIL_UTILITY_ASSETS_HH

#if defined(__cplusplus) && !defined(WEBVIEW_HEADER)

#include "string.hh"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <climits>
#include <cstdlib>
#endif

namespace webview {
namespace detail {

// Decodes percent-encoded sequences (e.g. "%2e" -> '.', "%2f" -> '/') in the
// given string. Malformed sequences ('%' not followed by two hex digits) are
// copied verbatim. Pure function, safe to unit-test.
inline std::string percent_decode(const std::string &input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c == '%' && i + 2 < input.size()) {
      auto hex_val = [](char h) -> int {
        if (h >= '0' && h <= '9')
          return h - '0';
        if (h >= 'a' && h <= 'f')
          return h - 'a' + 10;
        if (h >= 'A' && h <= 'F')
          return h - 'A' + 10;
        return -1;
      };
      int hi = hex_val(input[i + 1]);
      int lo = hex_val(input[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

// Normalizes a URL path component into a safe relative path.
//
// Splits on '/' and '\', drops empty segments and "." segments, and rejects any
// ".." segment that would escape the root (returns an empty string in that
// case). The result has no leading separator and uses '/' internally, e.g.
//   "/a/./b/../c"  -> "a/c"
//   "/a/../../etc" -> "" (escape attempt)
// Returns an empty string for empty/whitespace-only input.
// Pure function, safe to unit-test.
inline std::string normalize_url_path(const std::string &raw) {
  std::vector<std::string> segments;
  std::size_t i = 0;
  // Split on both '/' and '\'.
  while (i <= raw.size()) {
    std::size_t j = i;
    while (j < raw.size() && raw[j] != '/' && raw[j] != '\\') {
      ++j;
    }
    if (j > i) {
      segments.emplace_back(raw.substr(i, j - i));
    }
    if (j >= raw.size()) {
      break;
    }
    i = j + 1;
  }

  std::vector<std::string> out;
  out.reserve(segments.size());
  for (auto &seg : segments) {
    if (seg.empty() || seg == ".") {
      continue;
    }
    if (seg == "..") {
      if (out.empty()) {
        // Attempt to escape root -> reject.
        return {};
      }
      out.pop_back();
      continue;
    }
    out.push_back(seg);
  }
  std::string joined;
  for (std::size_t k = 0; k < out.size(); ++k) {
    if (k > 0) {
      joined.push_back('/');
    }
    joined += out[k];
  }
  return joined;
}

// Returns the canonical absolute path of the given path, resolving '.', '..'
// and symlinks. Returns an empty string on failure (non-existent file, etc.).
inline std::string canonicalize_path(const std::string &path) {
#if defined(_WIN32)
  // GetFullPathNameW resolves '..' and '.' but not symlinks; for a virtual
  // asset host that is acceptable because we additionally enforce containment.
  std::wstring wpath = widen_string(path);
  if (wpath.empty()) {
    return {};
  }
  DWORD len = GetFullPathNameW(wpath.c_str(), 0, nullptr, nullptr);
  if (len == 0) {
    return {};
  }
  std::wstring resolved(len, L'\0');
  DWORD got = GetFullPathNameW(wpath.c_str(), len, &resolved[0], nullptr);
  if (got == 0 || got >= len) {
    return {};
  }
  resolved.resize(got);
  // Normalize backslashes to forward slashes for consistent prefix checks.
  for (auto &c : resolved) {
    if (c == L'\\') {
      c = L'/';
    }
  }
  return narrow_string(resolved);
#else
  char buf[PATH_MAX];
  if (realpath(path.c_str(), buf) == nullptr) {
    return {};
  }
  return std::string(buf);
#endif
}

// Returns true if `target` is the same as or located inside `base_dir`.
// Both arguments are expected to be canonical absolute paths. The check is
// boundary-aware: "/a/b" is inside "/a", but "/a-b" is NOT inside "/a".
inline bool is_within(const std::string &base_dir, const std::string &target) {
  if (base_dir.empty() || target.empty()) {
    return false;
  }
  if (target.size() < base_dir.size()) {
    return false;
  }
  if (target.compare(0, base_dir.size(), base_dir) != 0) {
    return false;
  }
  if (target.size() == base_dir.size()) {
    return true;
  }
  // target is longer; the next char must be a path separator.
  char sep = target[base_dir.size()];
  return sep == '/' || sep == '\\';
}

// Result of a successful asset resolution.
struct resolved_asset {
  std::vector<char> data;
  std::string mime_type;
  std::string path; // the canonical path that was read
};

// Resolves an asset request safely.
//
// `base_folder` is the registered local folder for the virtual host.
// `url_path` is the raw path component of the request URL (may be
// percent-encoded and may contain '/', '.', '..').
//
// Returns the file contents and MIME type on success, or an empty optional if
// the request escapes `base_folder` (path traversal), the file does not exist,
// or cannot be read. The MIME type is derived from the normalized file name.
inline std::optional<resolved_asset>
resolve_asset(const std::string &base_folder, const std::string &url_path) {
  // Layer 1: percent-decode before any path logic so "%2e%2e" cannot slip past.
  std::string decoded = percent_decode(url_path);
  // Layer 2: structural normalization rejects any ".." that escapes root.
  std::string relative = normalize_url_path(decoded);
  if (relative.empty()) {
    return std::nullopt;
  }

  std::string base_canon = canonicalize_path(base_folder);
  if (base_canon.empty()) {
    return std::nullopt;
  }

  std::string full = base_canon + "/" + relative;
  std::string full_canon = canonicalize_path(full);
  if (full_canon.empty()) {
    // File does not exist yet (realpath fails on missing paths); fall back to
    // a non-resolving join so the subsequent ifstream read can fail naturally,
    // but still enforce containment against the (already-canonical) base.
    full_canon = full;
  }

  // Layer 3: containment check on the canonical paths.
  if (!is_within(base_canon, full_canon)) {
    return std::nullopt;
  }

  std::ifstream file(full_canon, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  resolved_asset result;
  result.data = std::move(buffer);
  result.mime_type = get_mime_type(relative);
  result.path = std::move(full_canon);
  return result;
}

} // namespace detail
} // namespace webview

#endif // defined(__cplusplus) && !defined(WEBVIEW_HEADER)
#endif // WEBVIEW_DETAIL_UTILITY_ASSETS_HH
