#pragma once

#include <memory>
#include <vector>

#include "lexer.hpp"

namespace C {
struct Macro {
  bool hasArgs = false;
  std::vector<const Token*> args;
  TokenHolder contents;
  SourceLocation location;
  std::string_view name;

  Macro(const SourceLocation& loc, const std::string_view& nm) : location(loc), name(nm) {}

  
};
}