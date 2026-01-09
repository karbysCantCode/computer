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

void smake::SmakeTokenParser::parseKeywordTarget() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::IDENTIFIER
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos += 2;
    return;
  }
  const auto identifier = peek(2);
  if (project.targets.find(identifier.value) != project.targets.end() && project.labels.find(identifier.value) != project.labels.end()) {
    logError("Redefinition of target (or label by target) \"" + identifier.value + "\" at " + identifier.positionToString());
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
  const std::string& targetName = peek(3).value;
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
    std::filesystem::path path = {peek().value};
    if (path.is_absolute()) {
      targetIterator->second.includeDirectories.insert(path);
    } else {
      if (makeFilePath.is_absolute()) {
        std::filesystem::path fullpath = makeFilePath / path;
        if (!std::filesystem::exists(path)) {
          logWarning("Path \"" + path.u8string() + "\" is not absolute and could not be resolved by concatenation \"" + fullpath.u8string() + "\" at " + peek().positionToString());
        } else {
          targetIterator->second.includeDirectories.insert(fullpath);
        }
        
      } 
    }
    switch (peek(2).type) {
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
    pos++;
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
    pos += 2;
    return;
  }
  const auto& identifier = peek(2)
  if (project.flists.find())
}

void smake::SmakeTokenParser::parseKeywordSearchSet() {
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
  auto flistIterator = project.flists.find(peek(3).value);
  if (flistIterator == project.flists.end()) {
    logError("Undeclared Flist \"" + peek(3).value + "\" referenced at " + peek(3).positionToString());
    advanceUntilAfterType(SmakeToken::SmakeTokenTypes::CLOSEPAREN);
    return;
  } 
  std::unordered_set<std::string> fileTypes;
  std::string currentRun;
  size_t index = 0;
  std::string fileTypeString = peek(7).value;
  while (index < fileTypeString.length()) {
    const char c = fileTypeString[index];
    if (c == ',') {
      fileTypes.insert(currentRun);
      currentRun.clear();
    } else {
      currentRun.push_back(c);
    }
    index++;
  }
  flistIterator->second.filepaths.clear();

  while (peek().type != SmakeToken::SmakeTokenTypes::CLOSEPAREN && !isAtEnd()) {
    if (peek().type == SmakeToken::SmakeTokenTypes::COMMA) 
    if (peek().type != SmakeToken::SmakeTokenTypes::STRING) {
      logError("Expected string, got \"" + std::string(SmakeToken::toString(peek().type)) + "\" at " + peek().positionToString());
      if (peek(2).type == SmakeToken::SmakeTokenTypes::CLOSEPAREN) {break;}
      pos += 2;
      continue;
    }


  }
  searchHandler(flistIterator->second.filepaths, fileTypes, )

}

void smake::SmakeTokenParser::parseKeywordSearchAdd() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }
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
    pos++;
    return;
  }
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
    pos++;
    return;
  }
}

void smake::SmakeTokenParser::parseKeywordEntry() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::IDENTIFIER,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
    return;
  }
}

void smake::SmakeTokenParser::parseKeywordOutput() {
  int tokenError = validateTokenTypes({
    SmakeToken::SmakeTokenTypes::KEYWORD,
    SmakeToken::SmakeTokenTypes::OPENPAREN,
    SmakeToken::SmakeTokenTypes::COMMA,
    SmakeToken::SmakeTokenTypes::STRING,
  });
  if (tokenError) {
    logError("Invalid token type in use of keyword: \"" + peek().value + "\"\n  With token at " + peek(tokenError).positionToString());
    pos++;
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
