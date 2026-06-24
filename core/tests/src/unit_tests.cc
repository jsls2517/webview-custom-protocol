#include "webview/test_driver.hh"
#include "webview/webview.h"

TEST_CASE("Ensure that JSON parsing works") {
  auto J = webview::detail::json_parse;
  // Valid input with expected output
  REQUIRE(J(R"({"foo":"bar"})", "foo", -1) == "bar");
  REQUIRE(J(R"({"foo":""})", "foo", -1).empty());
  REQUIRE(J(R"({"foo":{}")", "foo", -1) == "{}");
  REQUIRE(J(R"({"foo": {"bar": 1}})", "foo", -1) == R"({"bar": 1})");
  REQUIRE(J(R"(["foo", "bar", "baz"])", "", 0) == "foo");
  REQUIRE(J(R"(["foo", "bar", "baz"])", "", 2) == "baz");
  // Valid UTF-8 with expected output
  REQUIRE(J(R"({"フー":"バー"})", "フー", -1) == "バー");
  REQUIRE(J(R"(["フー", "バー", "バズ"])", "", 2) == "バズ");
  // Invalid input with valid output - should probably fail
  REQUIRE(J(R"({"foo":"bar")", "foo", -1) == "bar");
  // Valid input with other invalid parameters - should fail
  REQUIRE(J(R"([])", "", 0).empty());
  REQUIRE(J(R"({})", "foo", -1).empty());
  REQUIRE(J(R"(["foo", "bar", "baz"])", "", -1).empty());
  REQUIRE(J(R"(["foo"])", "", 1234).empty());
  REQUIRE(J(R"(["foo"])", "", -1234).empty());
  // Invalid input - should fail
  REQUIRE(J("", "", 0).empty());
  REQUIRE(J("", "foo", -1).empty());
  REQUIRE(J(R"({"foo":")", "foo", -1).empty());
  REQUIRE(J(R"({"foo":{)", "foo", -1).empty());
  REQUIRE(J(R"({"foo":{")", "foo", -1).empty());
  REQUIRE(J(R"(}")", "foo", -1).empty());
  REQUIRE(J(R"({}}")", "foo", -1).empty());
  REQUIRE(J(R"("foo)", "foo", -1).empty());
  REQUIRE(J(R"(foo)", "foo", -1).empty());
  REQUIRE(J(R"({{[[""foo""]]}})", "", 1234).empty());
  REQUIRE(J("bad", "", 0).empty());
  REQUIRE(J("bad", "foo", -1).empty());
}

TEST_CASE("Ensure that JSON escaping works") {
  using webview::detail::json_escape;

  // Simple case without need for escaping. Quotes added by default.
  REQUIRE(json_escape("hello") == "\"hello\"");
  // Simple case without need for escaping. Quotes explicitly not added.
  REQUIRE(json_escape("hello", false) == "hello");
  // Empty input should return empty output.
  REQUIRE(json_escape("", false).empty());
  // '"' and '\' should be escaped.
  REQUIRE(json_escape("\"", false) == "\\\"");
  REQUIRE(json_escape("\\", false) == "\\\\");
  // Commonly-used characters that should be escaped.
  REQUIRE(json_escape("\b\f\n\r\t", false) == "\\b\\f\\n\\r\\t");
  // ASCII control characters should be escaped.
  REQUIRE(json_escape(std::string{"\0\x1f", 2}, false) == "\\u0000\\u001f");
  // ASCII printable characters (even DEL) shouldn't be escaped.
  REQUIRE(json_escape("\x20\x7e\x7f", false) == "\x20\x7e\x7f");
  // Valid UTF-8.
  REQUIRE(json_escape("\u2328", false) == "\u2328");
  REQUIRE(json_escape("フーバー", false) == "フーバー");
  // Replacement character for invalid characters.
  REQUIRE(json_escape("�", false) == "�");
  // Invalid characters should be replaced with '�' but we just leave them as-is.
  REQUIRE(json_escape("\x80\x9f\xa0\xff", false) == "\x80\x9f\xa0\xff");
  // JS code should not be executed (eval).
  auto expected_gotcha = R"js(alert(\"gotcha\"))js";
  REQUIRE(json_escape(R"(alert("gotcha"))", false) == expected_gotcha);
}

TEST_CASE("optional class") {
  using namespace webview::detail;

  REQUIRE(!optional<int>{}.has_value());
  REQUIRE(optional<int>{1}.has_value());
  REQUIRE(optional<int>{1}.get() == 1);

  REQUIRE_THROW(bad_access, [] { optional<int>{}.get(); });

  REQUIRE(!optional<int>{optional<int>{}}.has_value());
  REQUIRE(optional<int>{optional<int>{1}}.has_value());
  REQUIRE(optional<int>{optional<int>{1}}.get() == 1);
}

TEST_CASE("result class") {
  using namespace webview::detail;
  using namespace webview;

  REQUIRE(result<int>{}.has_value());
  REQUIRE(result<int>{}.value() == 0);
  REQUIRE(result<int>{1}.has_value());
  REQUIRE(result<int>{1}.value() == 1);
  REQUIRE(!result<int>{}.has_error());
  REQUIRE(!result<int>{1}.has_error());
  REQUIRE(result<int>{}.ok());
  REQUIRE(result<int>{1}.ok());
  REQUIRE(!result<int>{error_info{}}.ok());
  REQUIRE(!result<int>{error_info{}}.has_value());
  REQUIRE(result<int>{error_info{}}.has_error());

  auto result_with_error = result<int>{
      error_info{WEBVIEW_ERROR_INVALID_ARGUMENT, "invalid argument"}};
  REQUIRE(result_with_error.error().code() == WEBVIEW_ERROR_INVALID_ARGUMENT);
  REQUIRE(result_with_error.error().message() == "invalid argument");

  REQUIRE_THROW(bad_access, [] { result<int>{}.error(); });
}

TEST_CASE("noresult class") {
  using namespace webview::detail;
  using namespace webview;

  REQUIRE(!noresult{}.has_error());
  REQUIRE(noresult{}.ok());
  REQUIRE(!noresult{error_info{}}.ok());
  REQUIRE(noresult{error_info{}}.has_error());

  auto result_with_error =
      noresult{error_info{WEBVIEW_ERROR_INVALID_ARGUMENT, "invalid argument"}};
  REQUIRE(result_with_error.error().code() == WEBVIEW_ERROR_INVALID_ARGUMENT);
  REQUIRE(result_with_error.error().message() == "invalid argument");

  REQUIRE_THROW(bad_access, [] { noresult{}.error(); });
}

#if _WIN32
TEST_CASE("Ensure that version number parsing works on Windows") {
  using namespace webview::detail;
  auto v = parse_version("");
  REQUIRE(v.size() == 4);
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
  v = parse_version("1");
  REQUIRE(v[0] == 1 && v[1] == 0 && v[2] == 0 && v[3] == 0);
  v = parse_version("0.2");
  REQUIRE(v[0] == 0 && v[1] == 2 && v[2] == 0 && v[3] == 0);
  v = parse_version("0.0.3");
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 3 && v[3] == 0);
  v = parse_version("0.0.0.4");
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 4);
  v = parse_version("1.2.3.4.5");
  REQUIRE(v.size() == 4);
  REQUIRE(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4);
  v = parse_version("1.2.3.4.5.6");
  REQUIRE(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4);
  v = parse_version("11.22.33.44");
  REQUIRE(v[0] == 11 && v[1] == 22 && v[2] == 33 && v[3] == 44);
  v = parse_version("0.0.0.0");
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
  v = parse_version("-1.-2.-3.-4");
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
  v = parse_version("a.b.c.d");
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
  v = parse_version("...");
  REQUIRE(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
}

TEST_CASE("Ensure that narrow/wide string conversion works on Windows") {
  using namespace webview::detail;
  REQUIRE(widen_string("").empty());
  REQUIRE(narrow_string(L"").empty());
  REQUIRE(widen_string("foo") == L"foo");
  REQUIRE(narrow_string(L"foo") == "foo");
  REQUIRE(widen_string("フー") == L"フー");
  REQUIRE(narrow_string(L"フー") == "フー");
  REQUIRE(widen_string("æøå") == L"æøå");
  REQUIRE(narrow_string(L"æøå") == "æøå");
  // Unicode number for the smiley face below: U+1F600
  REQUIRE(widen_string("😀") == L"😀");
  REQUIRE(narrow_string(L"😀") == "😀");
  // Ensure that elements of wide string are correct
  {
    auto s = widen_string("😀");
    REQUIRE(s.size() == 2);
    REQUIRE(static_cast<std::uint16_t>(s[0]) == 0xD83D);
    REQUIRE(static_cast<std::uint16_t>(s[1]) == 0xDE00);
  }
  // Ensure that elements of narrow string are correct
  {
    auto s = narrow_string(L"😀");
    REQUIRE(s.size() == 4);
    REQUIRE(static_cast<std::uint8_t>(s[0]) == 0xf0);
    REQUIRE(static_cast<std::uint8_t>(s[1]) == 0x9f);
    REQUIRE(static_cast<std::uint8_t>(s[2]) == 0x98);
    REQUIRE(static_cast<std::uint8_t>(s[3]) == 0x80);
  }
  // Null-characters must also be converted
  REQUIRE(widen_string(std::string(2, '\0')) == std::wstring(2, L'\0'));
  REQUIRE(narrow_string(std::wstring(2, L'\0')) == std::string(2, '\0'));
}
#endif

TEST_CASE("get_mime_type returns expected MIME types") {
  using webview::detail::get_mime_type;
  REQUIRE(get_mime_type("index.html") == "text/html");
  REQUIRE(get_mime_type("page.htm") == "text/html");
  REQUIRE(get_mime_type("style.css") == "text/css");
  REQUIRE(get_mime_type("app.js") == "application/javascript");
  REQUIRE(get_mime_type("data.json") == "application/json");
  REQUIRE(get_mime_type("photo.png") == "image/png");
  REQUIRE(get_mime_type("photo.jpg") == "image/jpeg");
  REQUIRE(get_mime_type("photo.jpeg") == "image/jpeg");
  REQUIRE(get_mime_type("anim.gif") == "image/gif");
  REQUIRE(get_mime_type("logo.svg") == "image/svg+xml");
  REQUIRE(get_mime_type("favicon.ico") == "image/x-icon");
  // Unknown / missing extension falls back to a binary stream.
  REQUIRE(get_mime_type("archive.unknownext") == "application/octet-stream");
  REQUIRE(get_mime_type("noextension") == "application/octet-stream");
  // Extension matching is case-insensitive.
  REQUIRE(get_mime_type("INDEX.HTML") == "text/html");
  REQUIRE(get_mime_type("App.JS") == "application/javascript");
}

TEST_CASE("normalize_url_path rejects traversal and normalizes segments") {
  using webview::detail::normalize_url_path;
  // Simple paths are preserved (leading separator dropped).
  REQUIRE(normalize_url_path("/index.html") == "index.html");
  REQUIRE(normalize_url_path("/css/style.css") == "css/style.css");
  // '.' segments and empty segments are dropped.
  REQUIRE(normalize_url_path("/a/./b") == "a/b");
  REQUIRE(normalize_url_path("//a///b") == "a/b");
  // Backslashes are treated as separators too.
  REQUIRE(normalize_url_path("\\a\\b\\c") == "a/b/c");
  // A non-escaping '..' collapses the previous segment.
  REQUIRE(normalize_url_path("/a/b/../c") == "a/c");
  // An empty / root input yields an empty result.
  REQUIRE(normalize_url_path("").empty());
  REQUIRE(normalize_url_path("/").empty());
  // Traversal attempts that would escape the root are rejected (empty result).
  REQUIRE(normalize_url_path("/../etc/passwd").empty());
  REQUIRE(normalize_url_path("/a/../../etc/passwd").empty());
  REQUIRE(normalize_url_path("..").empty());
  // The structural check operates BEFORE percent-decoding, so an encoded
  // traversal still appears as literal "%2e%2e" here and is treated as a
  // normal segment (decoding happens one layer up in resolve_asset()).
  REQUIRE(normalize_url_path("/%2e%2e/secret") == "%2e%2e/secret");
}

TEST_CASE("percent_decode decodes percent-escaped sequences") {
  using webview::detail::percent_decode;
  REQUIRE(percent_decode("hello") == "hello");
  REQUIRE(percent_decode("").empty());
  // %2e -> '.', %2f -> '/'
  REQUIRE(percent_decode("%2e%2e%2f") == "../");
  // Lowercase hex works the same as uppercase.
  REQUIRE(percent_decode("%2E%2F") == "./");
  // Non-escaped characters are passed through verbatim.
  REQUIRE(percent_decode("a%20b") == "a b");
  // A malformed sequence ('%' not followed by two hex digits) is copied as-is.
  REQUIRE(percent_decode("%2") == "%2");
  REQUIRE(percent_decode("%zz") == "%zz");
  REQUIRE(percent_decode("100%") == "100%");
}

TEST_CASE("is_within is boundary-aware") {
  using webview::detail::is_within;
  // A path equal to the base is inside.
  REQUIRE(is_within("/a/b", "/a/b"));
  // A direct child is inside.
  REQUIRE(is_within("/a/b", "/a/b/c"));
  REQUIRE(is_within("/a/b", "/a/b/deep/file"));
  // A different directory that merely shares a prefix is NOT inside.
  REQUIRE_FALSE(is_within("/a/b", "/a-b"));
  REQUIRE_FALSE(is_within("/a/b", "/a/bc"));
  // Shorter target and empty inputs are rejected.
  REQUIRE_FALSE(is_within("/a/b", "/a"));
  REQUIRE_FALSE(is_within("", "/a"));
  REQUIRE_FALSE(is_within("/a", ""));
  // Both forward and backslash separators are recognized as boundaries.
  REQUIRE(is_within("C:/app", "C:/app\\dist\\index.html"));
}
