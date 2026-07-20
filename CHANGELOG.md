# Changelog

All notable changes to u8io are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
While the version is below 1.0.0, the public API may change between minor
versions.

## [Unreleased]

## [0.1.0] - 2026-07-20

First release. A header-only C++23 library for using `std::u8string` as the
primary string type — formatting, printing, and `std::expected`-based error
handling — with adapters for the `std::string`, `wchar_t`, and iostream edges.

### Added
- `cast.hpp` — `as_char`/`as_u8` zero-cost views, `to_string`/`to_u8string`
  copies, and `validate`.
- `format.hpp` — `format`, `format_to`, `formatted_size`, `runtime_format`,
  and the `arg_mapper` extension point; compile-time format-string checking.
- `print.hpp` — `print`/`println` to `stdout` or any `FILE*`, via
  `std::vprint_unicode` where available.
- `error.hpp` — `error` (message + `source_location` + `error_code` + cause
  chain), `fail`, `fail_with`, `u8unexpected`, `from_edge`, `from_error_code`,
  and formatting for `error` and `std::expected`.
- `io.hpp` — `write_to`, `read_file`, `write_file`, `read_line`
  (console-aware on Windows), and a formattable `std::filesystem::path`.
- `text.hpp` — `code_points(u8string_view)`, a forward range of `char32_t`
  scalar values (ill-formed input yields U+FFFD).
- `transcode.hpp` — UTF-8 ↔ UTF-16/UTF-32 (`to_u16string`, `to_u32string`,
  `to_u8string`) in strict (`expected`) and `_lossy` flavors, single
  code-point `encode`/`append`, and on Windows `to_wstring`/`as_wide`/`as_u16`.
- `ascii.hpp` — `constexpr`, locale-free ASCII classification and case
  conversion for `char8_t`.
- `charconv.hpp` — `to_chars`/`from_chars` for `char8_t` buffers and
  `u8string_view`, plus `parse<T>` → `expected<T, error>`.
- `hash.hpp` — transparent `string_hash`, `u8string_map<T>`, `u8string_set`
  for heterogeneous lookup.

### Requirements
- C++23 with `<format>` and `<expected>`: MSVC 19.37+, GCC 14+,
  Clang/libc++ 18+, Apple Clang 16+.
- The ordinary literal encoding must be UTF-8 (enforced by `static_assert`;
  `/utf-8` added automatically for MSVC consumers).

[Unreleased]: https://github.com/jimpark/u8io/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jimpark/u8io/releases/tag/v0.1.0
