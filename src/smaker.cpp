#include "smaker.hpp"
#include <cassert>
#include <filesystem>

smake::SmakeTokenParser::SmakeTokenParser(const std::vector<SmakeToken::SmakeToken>& tks, SMakeProject& targetProject, std::filesystem::path makefilePath) : tokens(tks), project(targetProject), makeFilePath(makefilePath) {
  while (!isAtEnd()) {
    const SmakeToken::SmakeToken& token = tokens[pos];
    if (token.type == SmakeToken::SmakeTokenTypes::KEYWORD) {
      
    } else {
      assert(false); //baddddd
    }
  }
}


int smake::SmakeTokenParser::validateTokenTypes(std::initializer_list<SmakeToken::SmakeTokenTypes> allowed) const {
  int i = 0;
  for (const auto& type : allowed) {
    if (peek(i).type != type) return i;
    i++;
  }
  return 0;
}

SmakeToken::SmakeToken smake::SmakeTokenParser::advance() {
  if (isAtEnd()) return SmakeToken::SmakeToken(std::string{}, SmakeToken::SmakeTokenTypes::INVALID, -1, -1);
  const auto& token = tokens[pos];
  pos++;
  return token;
}

SmakeToken::SmakeToken smake::SmakeTokenParser::peek(const size_t dist) const {
  if (!(pos+dist < tokens.size())) return SmakeToken::SmakeToken(std::string{}, SmakeToken::SmakeTokenTypes::INVALID, -1, -1);
  return tokens[pos+dist];
}

bool smake::SmakeTokenParser::isAtEnd() const {
  return !(pos < tokens.size());
}

void smake::SmakeTokenParser::advanceUntilAfterType(SmakeToken::SmakeTokenTypes type) {
  while (!isAtEnd() && advance().type != type) {continue;}
}

std::string smake::SmakeTokenParser::consumeError() {
  if (!isErrors()) {return "";}
  const auto val = std::move(errors.back());
  errors.pop_back();
  return val;
}
std::string smake::SmakeTokenParser::consumeWarning() {
  if (!isWarnings()) {return "";}
  const auto val = std::move(warnings.back());
  warnings.pop_back();
  return val;
}
std::string smake::SmakeTokenParser::consumeDebug() {
  if (!isDebugs()) {return "";}
  const auto val = std::move(debugs.back());
  debugs.pop_back();
  return val;
}

bool smake::SmakeTokenParser::searchHandler(FList& targetList, std::unordered_set<std::string>& fileExtentions , std::filesystem::path pathToSearch, SmakeTokenParser::SearchType type, int searchDepth) {
  if (searchDepth < 0) {return true;}
  bool success = true;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(pathToSearch, ec); it != std::filesystem::directory_iterator(); it.increment(ec)) {
    if (ec) {success = false; continue;} 
    if (std::filesystem::is_directory(it->path())) {
      if ((type == SearchType::CDEPTH && searchDepth < 1) || type == SearchType::SHALLOW) {continue;}
      if (!searchHandler(targetList,fileExtentions, it->path(), type, searchDepth - 1)) {logError("Error while searching path \"" + it->path().u8string() + "\" from string at " + peek().positionToString()); success = false;}
    } else {
      targetList.filepaths.insert(it->path());
    }
  }
  return success;
}

std::filesystem::path smake::SmakeTokenParser::parsePath(std::string p) {
  std::filesystem::path path = p;
    if (path.is_absolute()) {
      return path;
    }
    if (makeFilePath.is_absolute()) {
      std::filesystem::path fullpath = makeFilePath / path;
      if (!std::filesystem::exists(path)) {
        logWarning("Path \"" + path.u8string() + "\" is not absolute and could not be resolved by concatenation \"" + fullpath.u8string() + "\" at " + peek().positionToString());
        return std::filesystem::path();
      }
      return fullpath;
    }
    return std::filesystem::path();
    
}

void smake::SmakeTokenParser::parseSearch(bool clear) {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
    SmakeToken::SmakeTokenTypes::COMMA
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }
  auto flistIterator = project.flists.find(peek(2).value);
  if (flistIterator == project.flists.end()) {
    logError("Undeclared Flist \"" + peek(2).value + "\" referenced at " + peek(2).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  } 
  std::unordered_set<std::string> fileTypes;
  std::string currentRun;
  size_t index = 0;
  std::string fileTypeString = peek(6).value;
  while (index < fileTypeString.length()) {
    const char c = fileTypeString[index];
    if (c == ',') {
      fileTypes.insert(currentRun);
      currentRun.clear();
    } else if (!(c == ' ' || c == '\n' || c == '\t')) {
      currentRun.push_back(c);
    }
    index++;
  }
  if (clear) {
    flistIterator->second.filepaths.clear();
  }

  std::string searchString = peek(4).value;
  SearchType searchType = SearchType::SHALLOW;
  int searchDepth = -1;
  if (searchString == "all") {
    searchType = SearchType::ALL;
  } else if (searchString == "shallow") {
    searchType = SearchType::SHALLOW;
    searchDepth = 0;
  } else if (searchString.substr(6) == "cdepth" && searchString.length() > 6) {
    searchType = SearchType::CDEPTH;
    searchDepth = std::stoi(std::string(6,searchString.size()));
  } else {
    logError("Unknown search type at " + peek(4).positionToString());
    return;
  }
  pos += 7;

  while (peek().type != SmakeToken::SmakeTokenTypes::CLOSEPAREN && !isAtEnd()) {
    if (peek().type != SmakeToken::SmakeTokenTypes::COMMA) {
      logError("Expected comma, got \"" + std::string(SmakeToken::toString(peek().type)) + "\" at " + peek().positionToString());
      pos++;
    }
    if (peek(1).type != SmakeToken::SmakeTokenTypes::STRING) {
      logError("Expected string, got \"" + std::string(SmakeToken::toString(peek().type)) + "\" at " + peek().positionToString());
      if (peek(1).type == SmakeToken::SmakeTokenTypes::CLOSEPAREN) {break;}
      pos += 2;
      continue;
    }

    searchHandler(flistIterator->second, fileTypes, std::filesystem::path(peek(1).value), searchType, searchDepth);
  }

}

//std::unordered_set<std::string> 

void smake::SmakeTokenParser::parseKeywordTarget() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::IDENTIFIER
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos ++;
    return;
  }
  const auto identifier = peek(1);
  if (project.labels.find(identifier.value) != project.labels.end()) {
    logError("Redefinition of target (or label/flist by target) \"" + identifier.value + "\" at " + identifier.positionToString());
    pos += 2;
    return;
  }
  project.targets.emplace(identifier.value, smake::Target{identifier.value});
  project.labels.insert(identifier.value);
  pos += 2;
}

void smake::SmakeTokenParser::parseKeywordInclude_Directory() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }
  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }
  pos += 4;

  bool isAtEndOfGathering = false;
  while (!isAtEnd() || isAtEndOfGathering) {
    if (peek().type != SmakeToken::SmakeTokenTypes::STRING) {
      logError("Expected string at " + peek().positionToString() + ", got " + SmakeToken::toString(peek().type));
      break;
    }
    //validate the string the same way you did in python
    //check is abs, if is then just add, if not then try join makefile dir
    std::filesystem::path path = parsePath(peek().value);
    if (!path.empty()) {
      targetIterator->second.includeDirectories.insert(path);
    }
    switch (peek(1).type) {
      case SmakeToken::SmakeTokenTypes::COMMA:
      continue;
      break;
      case SmakeToken::SmakeTokenTypes::CLOSEPAREN:
      isAtEndOfGathering = true;
      break;
      default:
      isAtEndOfGathering = true;
      logError("Expected token type COMMA or CLOSEPARENTHESESE, got \"" + std::string(SmakeToken::toString(peek(2).type)) + "\" at " + peek(2).positionToString());
      break;
    }
  }
}

void smake::SmakeTokenParser::parseKeywordWorking_Directory() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
    SmakeToken::SmakeTokenTypes::CLOSEPAREN
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }
}

void smake::SmakeTokenParser::parseKeywordFlist() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::IDENTIFIER
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }
  const auto& identifier = peek(1);
  if (project.labels.find(identifier.value) != project.labels.end()) {
    logError("Redefinition of flist (or label/target by flist) \"" + identifier.value + "\" at " + identifier.positionToString());
    pos += 2;
    return;
  }

  project.flists.emplace(identifier.value, FList());
  project.labels.insert(identifier.value);
  pos += 2;
}

void smake::SmakeTokenParser::parseKeywordSearchSet() {
  parseSearch(true);
}

void smake::SmakeTokenParser::parseKeywordSearchAdd() {
  parseSearch(false);
}

void smake::SmakeTokenParser::parseKeywordAddTarget() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }

  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }
  pos += 4;

  bool isGathering = true;
  while (isGathering && !isAtEnd() && (peek().type == SmakeToken::SmakeTokenTypes::IDENTIFIER || peek().type == SmakeToken::SmakeTokenTypes::STRING)) {
    if (peek().type == SmakeToken::SmakeTokenTypes::IDENTIFIER) {
      auto flistIterator = project.flists.find(peek().value);
      if (flistIterator!= project.flists.end()) {
        for (const auto& path : flistIterator->second.filepaths) {
          targetIterator->second.sourceFilepaths.insert(path);
        }
      } else {
        logError("Undeclared flist \"" + peek().value + "\" at " + peek().positionToString());
      }
      
    } else if (peek().type == SmakeToken::SmakeTokenTypes::STRING) { 
      auto path = parsePath(peek().value);
      if (!path.empty()) {
        targetIterator->second.sourceFilepaths.insert(path);
      }
    }

    switch (peek(1).type) {
      case SmakeToken::SmakeTokenTypes::CLOSEPAREN:
        isGathering = false;
      case SmakeToken::SmakeTokenTypes::COMMA:
        pos += 2;
      break;
      default:
        isGathering = false;
        logError("Expected comma or close parentheses, got \"" + peek(1).value + "\" at " + peek(1).positionToString());
      break;
    }
  }
  advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);

}

void smake::SmakeTokenParser::parseKeywordDefine() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
    SmakeToken::SmakeTokenTypes::CLOSEPAREN
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }

  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    pos += 8;
    return;
  }

  if (targetIterator->second.preprocessorReplacements.find(peek(4).value) != targetIterator->second.preprocessorReplacements.end()) {
    logError("Redeclaration of preprocessor definition (replacment target:) \"" + peek(4).value + "\" at " + peek(4).positionToString());
    return;
  }
  targetIterator->second.preprocessorReplacements.emplace(peek(4).value, PreprocessorReplacement{peek(4).value, peek(6).value});
}

void smake::SmakeTokenParser::parseKeywordEntry() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
    SmakeToken::SmakeTokenTypes::CLOSEPAREN
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }

  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    pos += 6;
    return;
  }

  if (targetIterator->second.entrySymbol.length() != 0) {
    logWarning("Entry symbol \"" + targetIterator->second.entrySymbol + "\" is being replaced by \"" + peek(4).value + "\" at " + peek(4).positionToString());
  }
  targetIterator->second.entrySymbol = peek(4).value;
}

void smake::SmakeTokenParser::parseKeywordOutput() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }

  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }

  if (targetIterator->second.outputDirectory.length() != 0) {
    logWarning("Output directory \"" + targetIterator->second.outputDirectory + "\" is being replaced by \"" + peek(4).value + "\" at " + peek(4).positionToString());
  }

  const auto path = parsePath(peek(4).value);
  if (!path.empty()) {
    targetIterator->second.outputDirectory = path;
  }

  if (peek(5).type == SmakeToken::SmakeTokenTypes::CLOSEPAREN) {
    pos += 6;
    return;
  } else if (peek(5).type == SmakeToken::SmakeTokenTypes::COMMA 
          && peek(6).type == SmakeToken::SmakeTokenTypes::STRING 
          && peek(7).type == SmakeToken::SmakeTokenTypes::CLOSEPAREN) 
  {
    if (targetIterator->second.outputName.length() != 0) {
      logWarning("Output directory \"" + targetIterator->second.outputName + "\" is being replaced by \"" + peek(6).value + "\" at " + peek(6).positionToString());
    }
    targetIterator->second.outputName = peek(6).value;  
    pos += 8;
  } else {
    logError("Invalid sequence of tokens (comma, string, close parentheses, expected) at " +peek(5).positionToString());
    return;
  }

}

void smake::SmakeTokenParser::parseKeywordFormat() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
    SmakeToken::SmakeTokenTypes::CLOSEPAREN
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }

  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }

  smake::TargetFormats format;
  if (peek(4).value == "bin") {
    format = smake::TargetFormats::BIN;
  } else if (peek(4).value == "hex") {
    format = smake::TargetFormats::HEX;
  } else if (peek(4).value == "elf") {
    format = smake::TargetFormats::ELF;
  } else {
    logError("Unknown output format specified at " + peek(4).positionToString());
    return;
  }
  logDebug("Target \"" + targetName + "\" output format being changed from \"" + smake::targetFormatToString(targetIterator->second.outputFormat) + "\" to \"" + peek(4).value + "\" at " + peek(4).positionToString());
  targetIterator->second.outputFormat = format;

  pos += 6;
}

void smake::SmakeTokenParser::parseKeywordDepends() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }

  const std::string& targetName = peek(2).value;
  auto targetIterator = project.targets.find(targetName);
  if (targetIterator == project.targets.end()) {
    logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  }

  pos+=4;

  //while (!isAtEnd() && )

}

void smake::SmakeTokenParser::parseKeywordLabel() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::IDENTIFIER
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }
}

void smake::SmakeTokenParser::parseKeywordIfdef() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::OPENBLOCK
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }
}

void smake::SmakeTokenParser::parseKeywordIfndef() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::OPENBLOCK
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }
}
