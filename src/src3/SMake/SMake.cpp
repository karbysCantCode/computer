#include "SMake/SMake.hpp"

#include <cassert>

#include "Helpers/FileHelper.hpp"

namespace SMake {

void Project::advanceUntilAfterType(SMake::Token::Type type, TokenHolder& tokenHolder) {
  while (!tokenHolder.match(type)) {tokenHolder.skip();}
  tokenHolder.skip(); // for the after part
}

std::pair<bool, int> Project::validateTokenTypes(TokenHolder& tokenHolder, std::initializer_list<Token::Type> allowed)  {
  int i = 0;
  for (const auto& type : allowed) {
    if (tokenHolder.peek(i).type != type) return {true, i};
    i++;
  }
  return {false,0};
};

bool Project::searchHandler(FList& targetList, std::unordered_set<std::string>& fileExtentions , std::filesystem::path pathToSearch, SearchType type, int searchDepth, TokenHolder tokenHolder) {
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

          if (!searchHandler(targetList, fileExtentions,
                    it->path(), type, searchDepth - 1, tokenHolder))
          {
              logError(tokenHolder.peek(), "Error while searching path \"" +
                        it->path().string() +
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
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }
  auto flistIterator = m_flists.find(static_cast<std::string>(tokenHolder.peek(1).value));
  if (flistIterator == m_flists.end()) {
    logError(tokenHolder.peek(1), "Undeclared Flist \"" + static_cast<std::string>(tokenHolder.peek(1).value) + "\" referenced.");
    
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  } 
  std::unordered_set<std::string> fileTypes;
  std::string currentRun;
  size_t index = 0;
  std::string fileTypeString = static_cast<std::string>(tokenHolder.peek(5).value);
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

  std::string searchString = static_cast<std::string>(tokenHolder.peek(3).value);
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
    logError(tokenHolder.peek(3), "Unknown search type.");
    return;
  }
  tokenHolder.skip(6);

  while(tokenHolder.peek().type != SMake::Token::Type::CLOSEPAREN && !tokenHolder.isAtEnd()) {
    if(tokenHolder.peek().type != SMake::Token::Type::COMMA) {
      logError(tokenHolder.peek(),"Expected comma, got \"" + static_cast<std::string>(tokenHolder.peek().typeToString()) + '"');

      tokenHolder.skip();
    }
    if(tokenHolder.peek(1).type != SMake::Token::Type::STRING) {
      logError(tokenHolder.peek(1),"Expected string, got \"" + static_cast<std::string>(tokenHolder.peek(1).typeToString()) + '"');
      if(tokenHolder.peek(1).type == SMake::Token::Type::CLOSEPAREN) {tokenHolder.skip(2); break;}
      tokenHolder.skip(2);
      continue;
    }
    searchHandler(flistIterator->second, fileTypes, m_makefilePath/std::filesystem::path(tokenHolder.peek(1).value), searchType, searchDepth, tokenHolder);
    tokenHolder.skip(2);
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
  const auto identifier = tokenHolder.peek();
  if (m_labels.find(static_cast<std::string>(identifier.value)) != m_labels.end()) {
    logError(identifier, "Redefinition of target (or label/flist by target) \"" + static_cast<std::string>(identifier.value) + '"');
    tokenHolder.skip();
    return;
  }
  m_targets.emplace(identifier.value, identifier.value);
  m_labels.insert(static_cast<std::string>(identifier.value));
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
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }
  const std::string& targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  tokenHolder.skip(3);

  bool isAtEndOfGathering = false;
  while (!tokenHolder.isAtEnd() && !isAtEndOfGathering) {
    if(tokenHolder.peek().type != SMake::Token::Type::STRING) {
      logError(tokenHolder.peek(),"Expected string, got " + static_cast<std::string>(tokenHolder.peek().typeToString()));
      break;
    }
    //validate the string the same way you did in python
    //check is abs, if is then just add, if not then try join makefile dir
    std::filesystem::path path = parsePath(tokenHolder.peek().value,tokenHolder);
    if (!path.empty()) {
      targetIterator->second.m_includeDirectories.insert(path.lexically_normal());
    } else { 
      logError(tokenHolder.peek(),"Got empty path from \"" + static_cast<std::string>(tokenHolder.peek().value) + '"');
    }
    switch(tokenHolder.peek(1).type) {
      case SMake::Token::Type::COMMA:
      tokenHolder.skip(2);
      continue;
      break;
      case SMake::Token::Type::CLOSEPAREN:
      isAtEndOfGathering = true;
      tokenHolder.skip(2);
      break;
      default:
      isAtEndOfGathering = true;
      logError(tokenHolder.peek(1),"Expected token type COMMA or CLOSEPARENTHESESE, got \"" + static_cast<std::string>(tokenHolder.peek(1).typeToString()) + '"');
      advanceUntilAfterType(Token::Type::CLOSEPAREN, tokenHolder);
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
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }
  logError(tokenHolder.peek(), "Unimplemented keyword working_directory WILL NOT PARSE, and will cause issues.");
};
void Project::parseKeywordFlist(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::IDENTIFIER
  });
  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    tokenHolder.skip();
    return;
  }
  const auto& identifier =tokenHolder.peek();
  if (m_labels.find(static_cast<std::string>(identifier.value)) != m_labels.end()) {
    logError(identifier,"Redefinition of flist (or label/target by flist) \"" + static_cast<std::string>(identifier.value) + '"');
    tokenHolder.skip();
    return;
  }

  m_flists.emplace(identifier.value, identifier.value);
  m_labels.insert(static_cast<std::string>(identifier.value));
  tokenHolder.skip();
};
void Project::parseKeywordSearchSet(TokenHolder& tokenHolder) {
  parseSearch(tokenHolder,true);
};
void Project::parseKeywordSearchAdd(TokenHolder& tokenHolder) {
  parseSearch(tokenHolder, false);
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
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  const std::string& targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }
  tokenHolder.skip(3);

  bool isGathering = true;
  while (isGathering && !tokenHolder.isAtEnd() && 
                                     (tokenHolder.peek().type == SMake::Token::Type::IDENTIFIER 
                                    ||tokenHolder.peek().type == SMake::Token::Type::STRING)) {
    if(tokenHolder.peek().type == SMake::Token::Type::IDENTIFIER) {
      auto flistIterator = m_flists.find(static_cast<std::string>(tokenHolder.peek().value));
      if (flistIterator != m_flists.end()) {
        for (const auto& path : flistIterator->second.m_filepaths) {
          targetIterator->second.m_sourceFilepaths.insert(path.lexically_normal());
        }
      } else {
        logError(tokenHolder.peek(), "Undeclared flist \"" +static_cast<std::string>(tokenHolder.peek().value) + '"');
      }
      
    } else if (tokenHolder.peek().type == SMake::Token::Type::STRING) { 
      auto path = parsePath(tokenHolder.peek().value,tokenHolder).lexically_normal();
      if (!path.empty()) {
        targetIterator->second.m_sourceFilepaths.insert(path);
      }
    }

    switch (tokenHolder.peek(1).type) {
      case SMake::Token::Type::CLOSEPAREN:
        isGathering = false;
      case SMake::Token::Type::COMMA:
        tokenHolder.skip(2);
      break;
      default:
        isGathering = false;
        logError(tokenHolder.peek(1), "Expected comma or close parentheses, got \"" +static_cast<std::string>(tokenHolder.peek(1).value) + '"');
        advanceUntilAfterType(Token::Type::CLOSEPAREN, tokenHolder);
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
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  const std::string& targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    tokenHolder.skip(7);
    return;
  }

  assert(false); //define not linked to preprocessor
  //TODO
  // if (targetIterator->second.preprocessorReplacements.find(tokenHolder.peek(4).value) != targetIterator->second.preprocessorReplacements.end()) {
  //   logError("Redeclaration of preprocessor definition (replacment target:) \"" +tokenHolder.peek(4).value + "\" at " +tokenHolder.peek(4).positionToString());
  //   return;
  // }
  // targetIterator->second.preprocessorReplacements.emplace(tokenHolder.peek(4).value, PreprocessorReplacement{peek(4).value,tokenHolder.peek(6).value});
  tokenHolder.skip(7);
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
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  const std::string targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    tokenHolder.skip(5);
    return;
  }

  if (targetIterator->second.m_entrySymbol.length() != 0) {
    logWarning(tokenHolder.peek(3), "Entry symbol \"" + targetIterator->second.m_entrySymbol + "\" is being replaced by \"" + static_cast<std::string>(tokenHolder.peek(3).value) + '"');
  }
  targetIterator->second.m_entrySymbol = static_cast<std::string>(tokenHolder.peek(3).value);

  tokenHolder.skip(5);
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
    tokenHolder.skip();
    return;
  }

  const std::string& targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  if (targetIterator->second.m_outputDirectory.length() != 0) {
    logWarning(tokenHolder.peek(3), "Output directory \"" + targetIterator->second.m_outputDirectory + "\" is being replaced by \"" + static_cast<std::string>(tokenHolder.peek(3).value) + '"');
  }

  auto path = parsePath(tokenHolder.peek(3).value, tokenHolder);
  if (!path.empty()) {
    targetIterator->second.m_outputDirectory = path.string();
  }
  tokenHolder.skip(4);
  if (tokenHolder.isAtEnd()) {return;}
  if (tokenHolder.peek().type == SMake::Token::Type::CLOSEPAREN) {
    tokenHolder.skip();
    return;
  } else if (tokenHolder.peek().type == SMake::Token::Type::COMMA 
          &&tokenHolder.peek(1).type == SMake::Token::Type::STRING 
          &&tokenHolder.peek(2).type == SMake::Token::Type::CLOSEPAREN) 
  {
    if (targetIterator->second.m_outputName.length() != 0) {
      logWarning(tokenHolder.peek(1), "Output directory \"" + targetIterator->second.m_outputName + "\" is being replaced by \"" +static_cast<std::string>(tokenHolder.peek(1).value) + '"');
    }
    targetIterator->second.m_outputName = static_cast<std::string>(tokenHolder.peek(1).value);  
    tokenHolder.skip(3);
  } else {
    logError(tokenHolder.peek(5),"Invalid sequence of tokens (comma, string, close parentheses, expected)");
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
    tokenHolder.skip();
    return;
  }

  const std::string& targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  SMake::Target::Format format;
  if (tokenHolder.peek(3).value == "bin") {
    format = SMake::Target::Format::BIN;
  } else if (tokenHolder.peek(3).value == "hex") {
    format = SMake::Target::Format::HEX;
  } else if (tokenHolder.peek(3).value == "elf") {
    format = SMake::Target::Format::ELF;
  } else {
    logError(tokenHolder.peek(3), "Unknown output format specified");
    return;
  }
  logDebug(tokenHolder.peek(3), "Target \"" + targetName + "\" output format being changed from \"" + targetIterator->second.formatToString() + "\" to \"" + static_cast<std::string>(tokenHolder.peek(3).value) + '"');
  targetIterator->second.m_outputFormat = format;

  tokenHolder.skip(5);
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
    tokenHolder.skip();
    return;
  }

  const std::string& targetName = static_cast<std::string>(tokenHolder.peek(1).value);
  auto targetIterator = m_targets.find(targetName);
  if (targetIterator == m_targets.end()) {
    logError(tokenHolder.peek(1), "Undeclared target \"" + targetName + '"');
    advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN, tokenHolder);
    return;
  }

  tokenHolder.skip(3);
  bool consumingDependencies = true;
  while (!tokenHolder.isAtEnd() && consumingDependencies &&tokenHolder.peek().type == Token::Type::STRING) {
    auto it = m_targets.find(static_cast<std::string>(tokenHolder.peek().value));
    if (it == m_targets.end()) {
      logError(tokenHolder.peek(), "Undeclared target \"" +static_cast<std::string>(tokenHolder.peek().value) + "\" referenced for dependency by \"" + targetName + '"');
      tokenHolder.skip(2);
      break;
    }

    targetIterator->second.m_dependantTargets.insert(&it->second);

    switch (tokenHolder.peek(1).type) {
      case Token::Type::CLOSEPAREN:
        consumingDependencies = false;
      case Token::Type::COMMA:
        tokenHolder.skip(2);
        break;
      default:
        logError(tokenHolder.peek(1),"Expected ',' or ')', got \"" +static_cast<std::string>(tokenHolder.peek(1).value) + '"');
        tokenHolder.skip();
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
    tokenHolder.skip();
    return;
  }

  if (m_labels.find(static_cast<std::string>(tokenHolder.peek().value)) != m_labels.end()) {
    logWarning(tokenHolder.peek(),"Label redefinition (defined by Label keyword) \"" + static_cast<std::string>(tokenHolder.peek().value) + '"');
  }
  m_labels.insert(static_cast<std::string>(tokenHolder.peek().value));
  tokenHolder.skip();
};
void Project::parseKeywordIfdef(TokenHolder& tokenHolder) {
  auto [err, errToken] = validateTokenTypes(
    tokenHolder,{
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::OPENBLOCK
  });

  if (err) {
    logError(tokenHolder.peek(errToken),"Invalid token type.");
    tokenHolder.skip();
    return;
  }
  
  if (m_labels.find(static_cast<std::string>(tokenHolder.peek().value)) == m_labels.end()) {
    logDebug(tokenHolder.peek(), "Label \"" + static_cast<std::string>(tokenHolder.peek().value) + "\" undefined, block skipped");
    advanceUntilAfterType(Token::Type::CLOSEBLOCK, tokenHolder);
    return;
  }
  tokenHolder.skip(2);
  // size_t index = 0;
  advanceUntilAfterType(Token::Type::CLOSEBLOCK, tokenHolder);
  // while (index < tokens.size() &&tokenHolder.peek(index).type != Token::Type::CLOSEBLOCK) {index++;}
  // tokens.erase(tokens.begin() + pos + index); 
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
    tokenHolder.skip();
    return;
  }

  if (m_labels.find(static_cast<std::string>(tokenHolder.peek().value)) != m_labels.end()) {
    logDebug(tokenHolder.peek(), "Label \"" + static_cast<std::string>(tokenHolder.peek().value) + "\" defined, block skipped");
    advanceUntilAfterType(Token::Type::CLOSEBLOCK, tokenHolder);
    return;
  }
  tokenHolder.skip(2);
  // size_t index = 0;
  advanceUntilAfterType(Token::Type::CLOSEBLOCK, tokenHolder);
  // while (index < tokenHolder.m_tokens.size() &&tokenHolder.peek(index).type != Token::Type::CLOSEBLOCK) {index++;}
  // tokens.erase(tokens.begin() + pos); 
};

void Project::logError(const Token& errToken, const std::string& message) {
  if (p_logger != nullptr) p_logger->Errors.logMessage(errToken.location.toString() + message);
}

void Project::logDebug(const Token& errToken, const std::string& message) {
  if (p_logger != nullptr) p_logger->Debugs.logMessage(errToken.location.toString() + message);
}
void Project::logWarning(const Token& errToken, const std::string& message) {
  if (p_logger != nullptr) p_logger->Warnings.logMessage(errToken.location.toString() + message);
}

std::filesystem::path Project::parsePath(const std::string_view& p, TokenHolder& tokenHolder) {
  std::filesystem::path path = p;
      if (path.is_absolute()) {
        return path;
      }
      if (m_makefilePath.is_absolute()) {
        std::filesystem::path fullpath = (m_makefilePath / path).lexically_normal();
        if (!std::filesystem::exists(fullpath)) {
          logWarning(tokenHolder.peek(), "Path \"" + path.string() + "\" is not absolute and could not be resolved by concatenation \"" + fullpath.string() + '"');
          return std::filesystem::path();
        }
        return fullpath;
      }
      return std::filesystem::path();
}

void Project::parseTokensToProject(TokenHolder& tokenHolder) {
  while (tokenHolder.notAtEnd()) {
    if (!tokenHolder.match(Token::Type::KEYWORD) && !tokenHolder.match(Token::Type::NEWLINE)) {
      logError(tokenHolder.peek(), 
        "Unexpected token \"" + 
        static_cast<std::string>(tokenHolder.peek().value) + 
        "\", type: " + tokenHolder.peek().typeToString()
      ); 
      tokenHolder.skip(); 
      continue;
    }

    const auto& token = tokenHolder.consume();   
    auto it = keywordHandlers.find(static_cast<std::string>(token.value));
    if (it == keywordHandlers.end()) {logError(tokenHolder.peek(),"Unknown keyword \"" + static_cast<std::string>(token.value) + '"');continue;}
    (this->*(it->second))(tokenHolder);
    
  }
}

Project smakePipeline(const std::filesystem::path& path, Debug::FullLogger* logger) {
  Project project(logger);

  auto src = FileHelper::openFileToString(path, logger);
  SMakeLexer lexer(src);
  auto tokens = lexer.run(src, path);
  project.m_makefilePath = path;
  std::cout << path << std::endl;
  project.parseTokensToProject(tokens);

  return project;
}

void printProject(const Project& project)
{
    using std::cout;
    using std::endl;

    cout << "===== PROJECT =====" << endl;
    cout << "Makefile Path: " << project.m_makefilePath << endl;

    cout << "\n--- Labels ---" << endl;
    for (const auto& label : project.m_labels)
    {
        cout << "  " << label << endl;
    }

    cout << "\n--- File Lists ---" << endl;
    for (const auto& [name, flist] : project.m_flists)
    {
        cout << "FList: " << flist.m_name << endl;

        for (const auto& path : flist.m_filepaths)
        {
            cout << "    " << path << endl;
        }
    }

    cout << "\n--- Targets ---" << endl;
    for (const auto& [name, target] : project.m_targets)
    {
        cout << "Target: " << target.m_name << endl;
        cout << "  Built: " << (target.m_isBuilt ? "true" : "false") << endl;
        cout << "  Output Format: " << target.formatToString() << endl;
        cout << "  Entry Symbol: " << target.m_entrySymbol << endl;
        cout << "  Working Directory: " << target.m_workingDirectory << endl;
        cout << "  Output Directory: " << target.m_outputDirectory << endl;
        cout << "  Output Name: " << target.m_outputName << endl;

        cout << "  Include Directories:" << endl;
        for (const auto& dir : target.m_includeDirectories)
        {
            cout << "    " << dir << endl;
        }

        cout << "  Source Files:" << endl;
        for (const auto& src : target.m_sourceFilepaths)
        {
            cout << "    " << src << endl;
        }

        cout << "  Dependant Targets:" << endl;
        for (const auto* dep : target.m_dependantTargets)
        {
            if (dep)
                cout << "    " << dep->m_name << endl;
        }

        cout << endl;
    }

    cout << "===== END PROJECT =====" << endl;
}
}