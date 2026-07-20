# u8io

A header-only C++23 library for using `std::u8string` as the one true string
type: formatting, printing, and `std::expected`-based error handling, with
adapters for the `std::string` edges (third-party libraries, iostreams).

## Purpose

C++20 introduced `char8_t`/`std::u8string` to mark UTF-8 text in the type
system, but the standard library has not caught up: `std::format` and
`std::print` accept neither `u8` format strings nor `u8` arguments, ostreams
*deleted* their `char8_t` overloads, and nothing helps carry UTF-8 error
messages through `std::expected` pipelines. In practice that pushes code back
to `std::string` and erases the encoding guarantee the type was created for.

`u8io` fills that gap so a codebase can be u8string-first everywhere, dropping
to `std::string` only at the edges where third-party APIs demand it.

```cpp
#include <u8io/u8io.hpp>

std::expected<std::u8string, u8io::error> load_greeting(const std::filesystem::path& p) {
    auto text = u8io::read_file(p);                       // expected<u8string, error>
    if (!text) return u8io::fail_with(text.error(), u8"greeting unavailable");
    return u8io::format(u8"héllo, {}!", *text);           // compile-time checked
}

int main() {
    auto greeting = load_greeting(u8"greeting.txt");
    u8io::println(u8"{}", greeting);   // value, or "unexpected(<message chain>)"
}
```

## Why a wrapper instead of extending `std::format`?

The standard leaves no legal extension point: `std::formatter<std::u8string,
char>` specializes a standard template purely with standard types (ill-formed,
no diagnostic required), and `char8_t` is not a supported format context
character type. `u8io` therefore transcodes at the boundary — which is free,
see below — and forwards to `std::vformat`. Compile-time format-string
checking is preserved: `u8io::u8format_string`'s `consteval` constructor
re-runs the standard library's own checker against the mapped argument types.

## Requirements and ground rules

- **C++23** with `<format>` and `<expected>` (MSVC 19.37+, GCC 14+,
  Clang/libc++ 18+, Apple Clang 16+). Note: Clang 18 paired with *libstdc++*
  does not work — libstdc++ disables `std::expected` for Clang older than 19
  (`__cpp_concepts` feature gate); use Clang 19+, or libc++ via
  `-stdlib=libc++`.
- **The ordinary literal encoding must be UTF-8.** Enforced by
  `static_assert`; on MSVC compile with `/utf-8` (the CMake target adds it
  automatically). This is what makes `char8_t` ↔ `char` reinterpretation
  free and `std::print`'s Unicode console path work on Windows.
- **Trust by convention:** `u8string` contents are presumed valid UTF-8.
  `as_u8`/`to_u8string` do not validate. Call `u8io::validate()` where bytes
  enter from outside; `read_file` validates by default.

## Tour

| Header | Contents |
| --- | --- |
| `u8io/cast.hpp` | `as_char`, `as_u8` (zero-cost views), `to_string`, `to_u8string` (copies), `validate` |
| `u8io/format.hpp` | `format`, `format_to`, `formatted_size`, `runtime_format`, the `arg_mapper` extension point |
| `u8io/print.hpp` | `print`, `println` to `stdout` or any `FILE*`, via `std::vprint_unicode` when available |
| `u8io/error.hpp` | `error` (message + `source_location` + `error_code` + cause chain), `fail`, `fail_with`, `u8unexpected`, `from_edge`, `from_error_code`, formatting for `error` and `std::expected` |
| `u8io/io.hpp` | `write_to(ostream, …)`, `read_file`, `write_file`, `read_line` (console-aware on Windows), formattable `std::filesystem::path` |
| `u8io/text.hpp` | `code_points(u8string_view)` — a forward range of `char32_t` scalar values (ill-formed input yields U+FFFD) |
| `u8io/transcode.hpp` | UTF-8 ↔ UTF-16/UTF-32: `to_u16string`, `to_u32string`, `to_u8string` — strict (`expected<…, decode_error>`) and `_lossy` (U+FFFD) flavors; `encode(char32_t)`/`append` for single code points; on Windows `to_wstring`, `as_wide`/`as_u16` |
| `u8io/ascii.hpp` | `constexpr`, locale-free ASCII classification and case conversion for `char8_t` (`is_alpha`, `is_digit`, `to_lower`, …) |
| `u8io/charconv.hpp` | `to_chars` / `from_chars` for `char8_t*` buffers and `u8string_view` input; `parse<T>` → `expected<T, error>` for whole-string numbers |
| `u8io/hash.hpp` | `string_hash` (transparent), `u8string_map<T>`, `u8string_set` — heterogeneous lookup without temporary keys |

Formatting accepts `u8string`, `u8string_view`, `u8` literals, and lone
`char8_t` alongside all ordinary `std::format` argument types; string format
specs (width, precision, alignment) apply to the mapped UTF-8 code units.
`std::expected<T, E>` formats as the bare value (nothing, for `void`) or
`unexpected(<error>)`; `u8io::error` formats as its full cause chain
(`top: mid: root`).

To make your own type formattable through `u8io::format`, specialize
`u8io::arg_mapper<T>` to map it to a `char`-formattable type.

### The edges

- **iostreams:** C++20 deleted `operator<<` for `char8_t` on purpose. Write
  `os << u8io::as_char(text)` or `u8io::write_to(os, text)`.
- **`std::u8fstream`:** `u8io` deliberately provides no stream drop-ins.
  The standard left `basic_fstream<char8_t>` unusable on purpose — no
  standard locale supplies a `codecvt<char8_t, char, mbstate_t>` facet, so
  a real replacement means reimplementing `filebuf` and the stream layer,
  and inheriting the failbit/badbit error model that `u8io`'s
  `std::expected`-based errors exist to avoid. The committee's own direction
  for new code is `std::print`/`std::println` (C++23) and `std::scan`
  (P1729), not streams. Use `read_file`/`write_file` for whole files,
  `read_line` for line input, `u8io::print` to a `FILE*` for incremental
  output, and `write_to` when an API hands you an `ostream`.
- **The Windows API boundary (`wchar_t`):** Win32 W-functions speak UTF-16.
  `to_wstring(text)` outbound, `to_u8string(wide)` inbound — strict variants
  return `expected` and report the offset of the first ill-formed sequence,
  `_lossy` variants substitute U+FFFD and always succeed. `as_wide`/`as_u16`
  reinterpret between `wchar_t` and `char16_t` views for free. All of it is
  locale-free, deterministic, and needs no `<windows.h>`.
- **`stdin`:** `u8io::read_line()` reads one line as UTF-8. On Windows, real
  console input is read natively via `ReadConsoleW` and transcoded — the
  input-side counterpart of `std::print`'s Unicode console path — so typed
  text arrives as UTF-8 regardless of the console code page. Redirected
  input is read as raw bytes and validated by default, like `read_file`.
- **Third-party APIs taking/returning `std::string`:** `as_char`/`to_string`
  outbound, `to_u8string`/`validate` inbound, `from_edge` for
  `std::expected<T, std::string>` / `std::error_code` returns.
- **`std::filesystem::path`:** construct from `u8string` and read back with
  `.u8string()` — already lossless in the standard; `u8io` adds formatting.
- **Regex:** `u8io` deliberately does not wrap `<regex>`. Byte-level
  `std::regex` gives wrong answers on non-ASCII text for anything beyond
  literal patterns (`.`, character classes, and case-insensitivity operate
  on code units), and its performance is poor. For ASCII-literal patterns,
  `u8io::as_char()` the input; for real Unicode matching, use RE2, PCRE2
  (`PCRE2_UTF`), or ICU.

## Building the tests

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

With [just](https://github.com/casey/just) installed, the common chores are
one word each:

```sh
just test               # configure, build, and run the tests
just format             # clang-format all headers and sources in-place
just install [prefix]   # install headers + CMake package (find_package(u8io))
```

`just` alone lists the recipes; `builddir` and `config` can be overridden,
e.g. `just config=Release test`.

## Known limitations

- A lone `char8_t` argument formats as a single code unit (byte), which is
  only a whole character for ASCII.
- `error_code::message()` text is used as-is; with non-English system locales
  on Windows it may not be UTF-8 (standard categories emit ASCII).
- No grapheme/normalization/width awareness — that is ICU's job. Format
  width/precision count code units, exactly as `std::format` does for `char`;
  `code_points()` yields scalar values, which are still not user-perceived
  characters.
- `ascii::` classification and case conversion are ASCII-only by design;
  non-ASCII bytes are in no class and case-convert to themselves.
- `read_line`'s Windows console path transcodes lossily (unpaired UTF-16
  surrogates become U+FFFD) and treats Ctrl-Z at the start of a line as end
  of input, matching console conventions.

## Versioning

Release history is in [CHANGELOG.md](CHANGELOG.md). u8io follows
[Semantic Versioning](https://semver.org); while the version is below 1.0.0
the public API may change between minor versions.

## License

[MIT](LICENSE).
