#include "SMake/SMake.hpp"


namespace SMake {

std::pair<bool, int> Project::validateTokenTypes(TokenHolder& tokenHolder, std::initializer_list<Token::Type> allowed)  {
  int i = 0;
  for (const auto& type : allowed) {
    if (tokenHolder.peek(i).type != type) return {true, i};
    i++;
  }
  return {false,0};
};

bool Project::searchHandler(FList& targetList, std::unordered_set<std::string>& fileExtentions , std::filesystem::path pathToSearch, SearchType type, int searchDepth) {
  if (searchDepth < 0 && type != SearchType::ALL)
      return true;

  bool success = true;
  std::error_code ec;

  if (std::filesystem::is_regular_file(pathToSearch, ec)) {
      std::string ext = pathToSearch.extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

      if (fileExtentions.find(ext) != fileExtentions.end()) {
          targetList.m_filepaths.insert(pathToSearch);
      }
      return true;
  }

  if (!std::filesystem::is_directory(pathToSearch, ec))
      return false;

  for (std::filesystem::directory_iterator it(pathToSearch, ec);
        it != std::filesystem::directory_iterator();
        it.increment(ec))
  {
      if (ec) {
          success = false;
          continue;
      }

      if (std::filesystem::is_directory(it->path())) {

          if ((type == SearchType::CDEPTH && searchDepth < 1)
              || type == SearchType::SHALLOW)
              continue;

          if (!self(self, targetList, fileExtentions,
                    it->path(), type, searchDepth - 1))
          {
              logError(peek(), "Error while searching path \"" +
                        it->path().u8string() +
                        "\" from string");
              success = false;
          }
      }
      else {
          std::string ext = it->path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

          if (fileExtentions.find(ext) != fileExtentions.end()) {
              targetList.m_filepaths.insert(it->path().lexically_normal());
          }
      }
  }

  return success;
};

void Project::parseSearch(TokenHolder& tokenHolder, bool clear)  {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,  //0
    SMake::Token::Type::IDENTIFIER, //1
    SMake::Token::Type::COMMA,      //2
    SMake::Token::Type::IDENTIFIER, //3
    SMake::Token::Type::COMMA,      //4
    SMake::Token::Type::STRING,     //5
    SMake::Token::Type::COMMA       //6
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }
  auto flistIterator = m_flists.find(tokenHolder.peek(1).value);
  if (flistIterator == m_flists.end()) {
    logError(tokenHolder.peek(1), "Undeclared Flist \"" + peek(1).m_value + "\" referenced.");
    
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  } 
  std::unordered_set<std::string> fileTypes;
  std::string currentRun;
  size_t index = 0;
  std::string fileTypeString = peek(5).m_value;
  while (index < fileTypeString.length()) {
    const char c = fileTypeString[index];
    if (c == ',') {
      fileTypes.emplace(currentRun);
      currentRun.clear();
    } else if (!(c == ' ' || c == '\n' || c == '\t')) {
      currentRun.push_back(c);
    }
    index++;
  }
  if (currentRun.length() > 0) {
    fileTypes.emplace(currentRun);
  }
  if (clear) {
    flistIterator->second.m_filepaths.clear();
  }

  std::string searchString = peek(3).m_value;
  SearchType searchType = SearchType::SHALLOW;
  int searchDepth = -1;
  if (searchString == "all") {
    searchType = SearchType::ALL;
  } else if (searchString == "shallow") {
    searchType = SearchType::SHALLOW;
    searchDepth = 0;
  } else if (searchString.substr(6) == "cdepth" && searchString.length() > 6) {
    searchType = SearchType::CDEPTH;
    assert(false);
    //fix this line VVV
    //searchDepth = std::stoi(std::string(6,searchString.size()));
  } else {
    logError(peek(3), "Unknown search type.");
    return;
  }
  pos += 6;

  while (peek().m_type != SMake::Token::Type::CLOSEPAREN && !isAtEnd()) {
    if (peek().m_type != SMake::Token::Type::COMMA) {
      logError(peek(),"Expected comma, got \"" + std::string(peek().typeToString()) + '"');

      pos++;
    }
    if (peek(1).m_type != SMake::Token::Type::STRING) {
      logError(peek(1),"Expected string, got \"" + std::string(peek(1).typeToString()) + '"');
      if (peek(1).m_type == SMake::Token::Type::CLOSEPAREN) {pos += 2; break;}
      pos += 2;
      continue;
    }
    searchHandler(searchHandler, flistIterator->second, fileTypes, makefilePath/std::filesystem::path(peek(1).m_value), searchType, searchDepth);
    pos += 2;
  }
  tokenHolder.skip();

};


void Project::parseKeywordTarget(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::IDENTIFIER
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    tokenHolder.skip();
    return;
  }
  const auto identifier = peek();
  if (targetProject.m_labels.find(identifier.m_value) != targetProject.m_labels.end()) {
    logError(identifier, "Redefinition of target (or label/flist by target) \"" + identifier.m_value + '"');
    tokenHolder.skip();
    return;
  }
  targetProject.m_targets.emplace(identifier.m_value, SMake::Target{identifier.m_value});
  targetProject.m_labels.insert(identifier.m_value);
  tokenHolder.skip();
};
void Project::parseKeywordInclude_Directory(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,   
    SMake::Token::Type::IDENTIFIER,  
    SMake::Token::Type::COMMA  
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }
  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }
  pos += 3;

  bool isAtEndOfGathering = false;
  while (!isAtEnd() || isAtEndOfGathering) {
    if (peek().m_type != SMake::Token::Type::STRING) {
      logError(peek(),"Expected string, got " + std::string(peek().typeToString()));
      break;
    }
    //validate the string the same way you did in python
    //check is abs, if is then just add, if not then try join makefile dir
    std::filesystem::path path = parsePath(peek().m_value);
    if (!path.empty()) {
      targetIterator->second.m_includeDirectories.insert(path.lexically_normal());
    } else { 
      logError(peek(),"Got empty path from \"" + peek().m_value + '"');
    }
    switch (peek(1).m_type) {
      case SMake::Token::Type::COMMA:
      pos += 2;
      continue;
      break;
      case SMake::Token::Type::CLOSEPAREN:
      isAtEndOfGathering = true;
      pos += 2;
      break;
      default:
      isAtEndOfGathering = true;
      logError(peek(1),"Expected token type COMMA or CLOSEPARENTHESESE, got \"" + std::string(peek(1).typeToString()) + '"');
      advanceUntilAfterType(Token::Type::CLOSEPAREN);
      break;
    }
  }
};
void Project::parseKeywordWorking_Directory(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA,
    SMake::Token::Type::STRING,
    SMake::Token::Type::CLOSEPAREN
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }
  logError(peek(), "Unimplemented keyword working_directory WILL NOT PARSE, and will cause issues.");
};
void Project::parseKeywordFlist(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::IDENTIFIER
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }
  const auto& identifier = peek();
  if (targetProject.m_labels.find(identifier.m_value) != targetProject.m_labels.end()) {
    logError(identifier,"Redefinition of flist (or label/target by flist) \"" + identifier.m_value + '"');
    pos += 1;
    return;
  }

  targetProject.m_flists.emplace(identifier.m_value, FList(identifier.m_value));
  targetProject.m_labels.insert(identifier.m_value);
  pos += 1;
};
void Project::parseKeywordSearchSet(TokenHolder& tokenHolder) {
  parseSearch(true);
};
void Project::parseKeywordSearchAdd(TokenHolder& tokenHolder) {
  parseSearch(false);
};
void Project::parseKeywordAddTarget(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }

  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }
  pos += 3;

  bool isGathering = true;
  while (isGathering && !isAtEnd() && 
                                      (peek().m_type == SMake::Token::Type::IDENTIFIER 
                                    || peek().m_type == SMake::Token::Type::STRING)) {
    if (peek().m_type == SMake::Token::Type::IDENTIFIER) {
      auto flistIterator = targetProject.m_flists.find(peek().m_value);
      if (flistIterator!= targetProject.m_flists.end()) {
        for (const auto& path : flistIterator->second.m_filepaths) {
          targetIterator->second.m_sourceFilepaths.insert(path.lexically_normal());
        }
      } else {
        logError(peek(), "Undeclared flist \"" + peek().m_value + '"');
      }
      
    } else if (peek().m_type == SMake::Token::Type::STRING) { 
      auto path = parsePath(peek().m_value).lexically_normal();
      if (!path.empty()) {
        targetIterator->second.m_sourceFilepaths.insert(path);
      }
    }

    switch (peek(1).m_type) {
      case SMake::Token::Type::CLOSEPAREN:
        isGathering = false;
      case SMake::Token::Type::COMMA:
        pos += 2;
      break;
      default:
        isGathering = false;
        logError(peek(1), "Expected comma or close parentheses, got \"" + peek(1).m_value + '"');
        advanceUntilAfterType(Token::Type::CLOSEPAREN);
      break;
    }
  }
};
void Project::parseKeywordDefine(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA,
    SMake::Token::Type::STRING,
    SMake::Token::Type::COMMA,
    SMake::Token::Type::STRING,
    SMake::Token::Type::CLOSEPAREN
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }

  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    pos += 7;
    return;
  }

  assert(false); //define not linked to preprocessor
  //TODO
  // if (targetIterator->second.preprocessorReplacements.find(peek(4).m_value) != targetIterator->second.preprocessorReplacements.end()) {
  //   logError("Redeclaration of preprocessor definition (replacment target:) \"" + peek(4).m_value + "\" at " + peek(4).positionToString());
  //   return;
  // }
  // targetIterator->second.preprocessorReplacements.emplace(peek(4).m_value, PreprocessorReplacement{peek(4).m_value, peek(6).m_value});
  pos += 7;
};
void Project::parseKeywordEntry(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA,
    SMake::Token::Type::STRING,
    SMake::Token::Type::CLOSEPAREN
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }

  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    pos += 5;
    return;
  }

  if (targetIterator->second.m_entrySymbol.length() != 0) {
    logWarning(peek(3), "Entry symbol \"" + targetIterator->second.m_entrySymbol + "\" is being replaced by \"" + peek(3).m_value + '"');
  }
  targetIterator->second.m_entrySymbol = peek(3).m_value;

  pos += 5;
};
void Project::parseKeywordOutput(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA,
    SMake::Token::Type::STRING,
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }

  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }

  if (targetIterator->second.m_outputDirectory.length() != 0) {
    logWarning(peek(3), "Output directory \"" + targetIterator->second.m_outputDirectory + "\" is being replaced by \"" + peek(3).m_value + '"');
  }

  auto path = parsePath(peek(3).m_value);
  if (!path.empty()) {
    targetIterator->second.m_outputDirectory = path.string();
  }
  pos += 4;
  if (isAtEnd()) {return;}
  if (peek().m_type == SMake::Token::Type::CLOSEPAREN) {
    pos += 1;
    return;
  } else if (peek().m_type == SMake::Token::Type::COMMA 
          && peek(1).m_type == SMake::Token::Type::STRING 
          && peek(2).m_type == SMake::Token::Type::CLOSEPAREN) 
  {
    if (targetIterator->second.m_outputName.length() != 0) {
      logWarning(peek(1), "Output directory \"" + targetIterator->second.m_outputName + "\" is being replaced by \"" + peek(1).m_value + '"');
    }
    targetIterator->second.m_outputName = peek(1).m_value;  
    pos += 3;
  } else {
    logError(peek(5),"Invalid sequence of tokens (comma, string, close parentheses, expected)");
    return;
  }

};
void Project::parseKeywordFormat(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA,
    SMake::Token::Type::STRING,
    SMake::Token::Type::CLOSEPAREN
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }

  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }

  SMake::Target::Format format;
  if (peek(3).m_value == "bin") {
    format = SMake::Target::Format::BIN;
  } else if (peek(3).m_value == "hex") {
    format = SMake::Target::Format::HEX;
  } else if (peek(3).m_value == "elf") {
    format = SMake::Target::Format::ELF;
  } else {
    logError(peek(3), "Unknown output format specified");
    return;
  }
  logDebug(peek(3), "Target \"" + targetName + "\" output format being changed from \"" + targetIterator->second.formatToString() + "\" to \"" + peek(3).m_value + '"');
  targetIterator->second.m_outputFormat = format;

  pos += 5;
};
void Project::parseKeywordDepends(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::OPENPAREN,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::COMMA
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }

  const std::string& targetName = peek(1).m_value;
  auto targetIterator = targetProject.m_targets.find(targetName);
  if (targetIterator == targetProject.m_targets.end()) {
    logError(peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
    return;
  }

  pos+=3;
  bool consumingDependencies = true;
  while (!isAtEnd() && consumingDependencies && peek().m_type == Token::Type::STRING) {
    auto it = targetProject.m_targets.find(peek().m_value);
    if (it == targetProject.m_targets.end()) {
      logError(peek(), "Undeclared target \"" + peek().m_value + "\" referenced for dependency by \"" + targetName + '"');
      pos += 2;
      break;
    }

    targetIterator->second.m_dependantTargets.insert(&it->second);

    switch (peek(1).m_type) {
      case Token::Type::CLOSEPAREN:
        consumingDependencies = false;
      case Token::Type::COMMA:
        pos += 2;
        break;
      default:
        logError(peek(1),"Expected ',' or ')', got \"" + peek(1).m_value + '"');
        pos++;
        consumingDependencies = false;
    }
  }

};
void Project::parseKeywordLabel(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::IDENTIFIER
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }

  if (targetProject.m_labels.find(peek().m_value) != targetProject.m_labels.end()) {
    logWarning(peek(),"Label redefinition (defined by Label keyword) \"" + peek().m_value + '"');
  }
  targetProject.m_labels.insert(peek().m_value);
  pos++;
};
void Project::parseKeywordIfdef(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::OPENBLOCK
  });

  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }
  
  if (targetProject.m_labels.find(peek().m_value) == targetProject.m_labels.end()) {
    logDebug(peek(), "Label \"" + peek().m_value + "\" undefined, block skipped");
    advanceUntilAfterType(Token::Type::CLOSEBLOCK);
    return;
  }
  pos += 2;
  size_t index = 0;
  while (index < tokens.size() && peek(index).m_type != Token::Type::CLOSEBLOCK) {index++;}
  tokens.erase(tokens.begin() + pos + index); 
};
void Project::parseKeywordIfndef(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
  SMake::Token::Type::KEYWORD,
  SMake::Token::Type::IDENTIFIER,
  SMake::Token::Type::OPENBLOCK
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    pos++;
    return;
  }

  if (targetProject.m_labels.find(peek().m_value) != targetProject.m_labels.end()) {
    logDebug(peek(), "Label \"" + peek().m_value + "\" defined, block skipped");
    advanceUntilAfterType(Token::Type::CLOSEBLOCK);
    return;
  }
  pos += 2;
  size_t index = 0;
  while (index < tokens.size() && peek(index).m_type != Token::Type::CLOSEBLOCK) {index++;}
  tokens.erase(tokens.begin() + pos); 
};

void Project::logError(const Token& errToken, const std::string& message) {
  if (p_logger != nullptr) p_logger->Errors.logMessage(errToken.location.toString() + message);
}

}