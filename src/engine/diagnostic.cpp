#include "calc/diagnostic.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

#include <fmt/format.h>

namespace calc {

namespace {
// a byte that starts a character: anything that isnt a utf-8 continuation byte
// (10xxxxxx). counting these counts codepoints, so the caret column and width
// track glyphs in a utf-8 terminal instead of drifting one column per extra
// byte.
bool starts_char(unsigned char b) { return (b & 0xC0) != 0x80; }
} // namespace

std::string render_diagnostic(std::string_view input, const CalcError &error) {
  const SourceSpan span = error.span;
  if (span.offset == 0 && span.length == 0) {
    return fmt::format("error: {}", error.message); // no location to point at
  }

  // the span is byte offsets, but the column and the underline width are in
  // characters, so a multibyte glyph before/at the error doesnt push the caret.
  const std::size_t prefix_end = std::min(span.offset, input.size());
  std::size_t column = 1;
  std::string pad;
  for (std::size_t i = 0; i < prefix_end; ++i) {
    if (starts_char(static_cast<unsigned char>(input[i]))) {
      ++column;
      // keep a tab as a tab so the caret holds its column at any tab width.
      pad += input[i] == '\t' ? '\t' : ' ';
    }
  }

  const std::size_t span_end =
      std::min(span.offset + span.length, input.size());
  std::size_t width = 0;
  for (std::size_t i = span.offset; i < span_end; ++i) {
    if (starts_char(static_cast<unsigned char>(input[i]))) {
      ++width;
    }
  }
  if (width == 0) {
    width = 1; // a zero-length span (End) still gets a single caret
  }

  std::string out =
      fmt::format("error: {} (column {})\n", error.message, column);
  out.append(input);
  out += '\n';
  out += pad;
  out += '^';
  for (std::size_t i = 1; i < width; ++i) {
    out += '~';
  }
  return out;
}

} // namespace calc
