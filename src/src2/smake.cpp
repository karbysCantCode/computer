#include "smake.hpp"

#include <iostream>
#include <functional>

#include "lexHelper.hpp"
#include "fileHelper.hpp"



SMake::SMakeProject::SMakeProject() {}

std::vector<SMake::Token> SMake::lex(std::filesystem::path path, Debug::FullLogger* logger) {
  LexHelper lexHelper(FileHelper::openFileToString(path, logger));
  if (lexHelper.m_source.empty()) {return {};}

  std::unordered_set<std::string> kwords = {
    "target",
    "include_directory",
    "working_directory",
    "flist",
    "search_set",
    "search_add",
    "add_target",
    "define",
    "entry",
    "output",
    "format",
    "depends",
    "label",
    "ifdef",
    "ifndef"
  };
  lexHelper.m_keywords = &kwords;

  std::vector<SMake::Token> tokens;

  #define notAtEnd() lexHelper.notAtEnd()
  #define skipWhitespace(arg) lexHelper.skipWhitespace(arg)
  #define peek(arg) lexHelper.peek(arg)
  #define line lexHelper.m_line
  #define column lexHelper.m_column
  #define isWordBoundary(arg) lexHelper.isWordBoundary(arg)
  #define consume() lexHelper.consume()
  #define isKeyword(arg) lexHelper.isKeyword(arg)
  #define skipComment(arg) lexHelper.skipComment(arg)
  #define consumeString(arg) lexHelper.consumeString(arg)
  #define getUntilWordBoundary() lexHelper.getUntilWordBoundary()

  while(notAtEnd()) {
    skipWhitespace();
    if(!notAtEnd()) {break;}

    char c = peek();

    std::string val;
    switch (c) {
      case '.':
        consume();
        if(!notAtEnd()) {continue;}
        val = getUntilWordBoundary();
        tokens.emplace_back(val,SMake::Token::Type::KEYWORD,line,column);
      break;
      case '(':
        val = std::string(1,consume());
        tokens.emplace_back(val, SMake::Token::Type::OPENPAREN, line,column);
      break;
      case ')':
        val = std::string(1,consume());
        tokens.emplace_back(val, SMake::Token::Type::CLOSEPAREN, line,column);
      break;
      case '{':
        val = std::string(1,consume());
        tokens.emplace_back(val, SMake::Token::Type::OPENBLOCK, line,column);
      break;
      case '}':
        val = std::string(1,consume());
        tokens.emplace_back(val, SMake::Token::Type::CLOSEBLOCK, line,column);
      break;
      case '"':
      case '\'':
        val = consumeString(c);
        tokens.emplace_back(val, SMake::Token::Type::STRING, line, column);
      break;
      case ',':
        val = std::string(1,consume());
        tokens.emplace_back(val, SMake::Token::Type::COMMA, line,column);
      break;
      default: 
        val = getUntilWordBoundary();
        tokens.emplace_back(val,SMake::Token::Type::IDENTIFIER,line,column);
      break;
    }
  }
  return tokens;

  #undef notAtEnd
  #undef skipWhitespace
  #undef peek
  #undef line 
  #undef column 
  #undef isWordBoundary
  #undef consume
  #undef isKeyword
  #undef skipComment
  #undef consumeString
  #undef getUntilWordBoundary
}

void SMake::parseTokensToProject(std::vector<Token>& tokens, SMakeProject& targetProject, std::filesystem::path makefilePath, Debug::FullLogger* logger) {
  size_t pos = 0;

  #define logError(message)   if (logger != nullptr) logger->Errors.logMessage(message);
  #define logWarning(message) if (logger != nullptr) logger->Warnings.logMessage(message);
  #define logDebug(message)   if (logger != nullptr) logger->Debugs.logMessage(message);


  enum class SearchType {
    SHALLOW,
    CDEPTH,
    ALL
  };

  auto isAtEnd = [&]() -> bool {
    return !(pos < (int)tokens.size());
  };
  auto advance = [&]() -> SMake::Token {
    if (isAtEnd()) return SMake::Token(std::string{}, Token::Type::INVALID, -1, -1);
    const auto& token = tokens[pos];
    pos++;
    return token;
  };
  auto peek = [&](const size_t dist = 0) -> SMake::Token {
    if (!(pos+dist < tokens.size())) return SMake::Token(std::string{}, Token::Type::INVALID, -1, -1);
    return tokens[pos+dist];
  };
  auto advanceUntilAfterType = [&](Token::Type type) -> void {
    while (!isAtEnd() && advance().m_type != type) {continue;}
  };

  auto searchHandler = [&](auto const& self, FList& targetList, std::unordered_set<std::string>& fileExtentions , std::filesystem::path pathToSearch, SearchType type, int searchDepth) -> bool {
    if (searchDepth < 0) {return true;}
    bool success = true;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(pathToSearch, ec); it != std::filesystem::directory_iterator(); it.increment(ec)) {
      if (ec) {success = false; continue;} 
      if (std::filesystem::is_directory(it->path())) {
        if ((type == SearchType::CDEPTH && searchDepth < 1) || type == SearchType::SHALLOW) {continue;}
        if (!self(self, targetList,fileExtentions, it->path(), type, searchDepth - 1)) {logError("Error while searching path \"" + it->path().u8string() + "\" from string at " + peek().positionToString()); success = false;}
      } else {
        targetList.m_filepaths.insert(it->path());
      }
    }
    return success;
  };

  auto validateTokenTypes = [&](std::initializer_list<Token::Type> allowed) -> int {
    int i = 0;
    for (const auto& type : allowed) {
      if (peek(i).m_type != type) return i;
      i++;
    }
    return 0;
  };

  auto parsePath = [&](std::string p) -> std::filesystem::path {
    std::filesystem::path path = p;
      if (path.is_absolute()) {
        return path;
      }
      if (makefilePath.is_absolute()) {
        std::filesystem::path fullpath = makefilePath / path;
        if (!std::filesystem::exists(path)) {
          logWarning("Path \"" + path.u8string() + "\" is not absolute and could not be resolved by concatenation \"" + fullpath.u8string() + "\" at " + peek().positionToString());
          return std::filesystem::path();
        }
        return fullpath;
      }
      return std::filesystem::path();
  };

  auto parseSearch = [&](bool clear)  {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,  //0
      SMake::Token::Type::IDENTIFIER, //1
      SMake::Token::Type::COMMA,      //2
      SMake::Token::Type::IDENTIFIER, //3
      SMake::Token::Type::COMMA,      //4
      SMake::Token::Type::STRING,     //5
      SMake::Token::Type::COMMA       //6
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }
    auto flistIterator = targetProject.m_flists.find(peek(1).m_value);
    if (flistIterator == targetProject.m_flists.end()) {
      logError("Undeclared Flist \"" + peek(1).m_value + "\" referenced at " + peek(1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    } 
    std::cout << "pre filetpye parse" << std::endl;
    std::unordered_set<std::string> fileTypes;
    std::string currentRun;
    size_t index = 0;
    std::string fileTypeString = peek(5).m_value;
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
    std::cout << "post filetpye parse" << std::endl;
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
      logError("Unknown search type at " + peek(3).positionToString());
      return;
    }
    pos += 6;

    std::cout << "pre directory parse" << std::endl;

    while (peek().m_type != SMake::Token::Type::CLOSEPAREN && !isAtEnd()) {
      if (peek().m_type != SMake::Token::Type::COMMA) {
        logError("Expected comma, got \"" + std::string(peek().typeToString()) + "\" at " + peek().positionToString());

        pos++;
      }
      if (peek(1).m_type != SMake::Token::Type::STRING) {
        logError("Expected string, got \"" + std::string(peek(1).typeToString()) + "\" at " + peek(1).positionToString());
        if (peek(1).m_type == SMake::Token::Type::CLOSEPAREN) {break;}
        pos += 2;
        continue;
      }

      searchHandler(searchHandler, flistIterator->second, fileTypes, std::filesystem::path(peek(1).m_value), searchType, searchDepth);
      pos += 2;
    }

    std::cout << "pre directory parse" << std::endl;

  };


  auto parseKeywordTarget = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::IDENTIFIER
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advance();
      return;
    }
    const auto identifier = peek();
    if (targetProject.m_labels.find(identifier.m_value) != targetProject.m_labels.end()) {
      logError("Redefinition of target (or label/flist by target) \"" + identifier.m_value + "\" at " + identifier.positionToString());
      advance();
      return;
    }
    targetProject.m_targets.emplace(identifier.m_value, SMake::Target{identifier.m_value});
    targetProject.m_labels.insert(identifier.m_value);
    advance();
  };
  auto parseKeywordInclude_Directory = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,   
      SMake::Token::Type::IDENTIFIER,  
      SMake::Token::Type::COMMA  
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }
    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(tokenError).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }
    pos += 3;

    bool isAtEndOfGathering = false;
    while (!isAtEnd() || isAtEndOfGathering) {
      if (peek().m_type != SMake::Token::Type::STRING) {
        logError("Expected string at " + peek().positionToString() + ", got " + peek().typeToString());
        break;
      }
      //validate the string the same way you did in python
      //check is abs, if is then just add, if not then try join makefile dir
      std::filesystem::path path = parsePath(peek().m_value);
      if (!path.empty()) {
        targetIterator->second.m_includeDirectories.insert(path);
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
        logError("Expected token type COMMA or CLOSEPARENTHESESE, got \"" + std::string(peek(1).typeToString()) + "\" at " + peek(1).positionToString());
        advanceUntilAfterType(Token::Type::CLOSEPAREN);
        break;
      }
    }
  };
  auto parseKeywordWorking_Directory = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA,
      SMake::Token::Type::STRING,
      SMake::Token::Type::CLOSEPAREN
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }
  };
  auto parseKeywordFlist = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::IDENTIFIER
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }
    const auto& identifier = peek();
    if (targetProject.m_labels.find(identifier.m_value) != targetProject.m_labels.end()) {
      logError("Redefinition of flist (or label/target by flist) \"" + identifier.m_value + "\" at " + identifier.positionToString());
      pos += 1;
      return;
    }

    targetProject.m_flists.emplace(identifier.m_value, FList(identifier.m_value));
    targetProject.m_labels.insert(identifier.m_value);
    pos += 1;
  };
  auto parseKeywordSearchSet = [&]() {
    parseSearch(true);
  };
  auto parseKeywordSearchAdd = [&]() {
    parseSearch(false);
  };
  auto parseKeywordAddTarget = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }

    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(1).positionToString());
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
            targetIterator->second.m_sourceFilepaths.insert(path);
          }
        } else {
          logError("Undeclared flist \"" + peek().m_value + "\" at " + peek().positionToString());
        }
        
      } else if (peek().m_type == SMake::Token::Type::STRING) { 
        auto path = parsePath(peek().m_value);
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
          logError("Expected comma or close parentheses, got \"" + peek(1).m_value + "\" at " + peek(1).positionToString());
          advanceUntilAfterType(Token::Type::CLOSEPAREN);
        break;
      }
    }
  };
  auto parseKeywordDefine = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA,
      SMake::Token::Type::STRING,
      SMake::Token::Type::COMMA,
      SMake::Token::Type::STRING,
      SMake::Token::Type::CLOSEPAREN
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }

    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(1).positionToString());
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
  auto parseKeywordEntry = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA,
      SMake::Token::Type::STRING,
      SMake::Token::Type::CLOSEPAREN
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }

    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(1).positionToString());
      pos += 5;
      return;
    }

    if (targetIterator->second.m_entrySymbol.length() != 0) {
      logWarning("Entry symbol \"" + targetIterator->second.m_entrySymbol + "\" is being replaced by \"" + peek(3).m_value + "\" at " + peek(3).positionToString());
    }
    targetIterator->second.m_entrySymbol = peek(3).m_value;

    pos += 5;
  };
  auto parseKeywordOutput = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA,
      SMake::Token::Type::STRING,
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }

    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }

    if (targetIterator->second.m_outputDirectory.length() != 0) {
      logWarning("Output directory \"" + targetIterator->second.m_outputDirectory + "\" is being replaced by \"" + peek(3).m_value + "\" at " + peek(3).positionToString());
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
        logWarning("Output directory \"" + targetIterator->second.m_outputName + "\" is being replaced by \"" + peek(1).m_value + "\" at " + peek(1).positionToString());
      }
      targetIterator->second.m_outputName = peek(1).m_value;  
      pos += 3;
    } else {
      logError("Invalid sequence of tokens (comma, string, close parentheses, expected) at " +peek(5).positionToString());
      return;
    }

  };
  auto parseKeywordFormat = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA,
      SMake::Token::Type::STRING,
      SMake::Token::Type::CLOSEPAREN
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }

    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(1).positionToString());
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
      logError("Unknown output format specified at " + peek(3).positionToString());
      return;
    }
    logDebug("Target \"" + targetName + "\" output format being changed from \"" + targetIterator->second.formatToString() + "\" to \"" + peek(3).m_value + "\" at " + peek(3).positionToString());
    targetIterator->second.m_outputFormat = format;

    pos += 5;
  };
  auto parseKeywordDepends = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::OPENPAREN,
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::COMMA
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }

    const std::string& targetName = peek(1).m_value;
    auto targetIterator = targetProject.m_targets.find(targetName);
    if (targetIterator == targetProject.m_targets.end()) {
      logError("Undeclared target \"" + targetName + "\" at " + peek(1).positionToString());
      advanceUntilAfterType(SMake::Token::Type::CLOSEPAREN);
      return;
    }

    pos+=3;
    bool consumingDependencies = true;
    while (!isAtEnd() && consumingDependencies && peek().m_type == Token::Type::STRING) {
      auto it = targetProject.m_targets.find(peek().m_value);
      if (it == targetProject.m_targets.end()) {
        logError("Undeclared target \"" + peek().m_value + "\" referenced for dependency by \"" + targetName + "\" at " + peek().positionToString());
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
          logError("Expected ',' or ')', got \"" + peek(1).m_value + "\" at " + peek(1).positionToString());
          pos++;
          consumingDependencies = false;
      }
    }

  };
  auto parseKeywordLabel = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::IDENTIFIER
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }

    if (targetProject.m_labels.find(peek().m_value) != targetProject.m_labels.end()) {
      logWarning("Label redefinition (defined by Label keyword) \"" + peek().m_value + "\" at " + peek().positionToString());
    }
    targetProject.m_labels.insert(peek().m_value);
    pos++;
  };
  auto parseKeywordIfdef = [&]() {
    int tokenError = validateTokenTypes({
      SMake::Token::Type::IDENTIFIER,
      SMake::Token::Type::OPENBLOCK
    });

    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }
    
    if (targetProject.m_labels.find(peek().m_value) == targetProject.m_labels.end()) {
      logDebug("Label \"" + peek().m_value + "\" undefined, block skipped at " + peek().positionToString());
      advanceUntilAfterType(Token::Type::CLOSEBLOCK);
      return;
    }
    pos += 2;
    size_t index = 0;
    while (index < tokens.size() && peek(index).m_type != Token::Type::CLOSEBLOCK) {index++;}
    tokens.erase(tokens.begin() + pos); 
  };
  auto parseKeywordIfndef = [&]() {
    int tokenError = validateTokenTypes({
    SMake::Token::Type::KEYWORD,
    SMake::Token::Type::IDENTIFIER,
    SMake::Token::Type::OPENBLOCK
    });
    if (tokenError) {
      logError("Invalid token type at " + peek(tokenError-1).positionToString());
      pos++;
      return;
    }

    if (targetProject.m_labels.find(peek().m_value) != targetProject.m_labels.end()) {
      logDebug("Label \"" + peek().m_value + "\" defined, block skipped at " + peek().positionToString());
      advanceUntilAfterType(Token::Type::CLOSEBLOCK);
      return;
    }
    pos += 2;
    size_t index = 0;
    while (index < tokens.size() && peek(index).m_type != Token::Type::CLOSEBLOCK) {index++;}
    tokens.erase(tokens.begin() + pos); 
  };

  std::unordered_map<std::string, std::function<void()>> tokenParseHashMap = {
    {"target", parseKeywordTarget},
    {"include_directory", parseKeywordInclude_Directory},
    {"working_directory", parseKeywordWorking_Directory},
    {"flist", parseKeywordFlist},
    {"search_set", parseKeywordSearchSet},
    {"search_add", parseKeywordSearchAdd},
    {"add_target", parseKeywordAddTarget},
    {"define", parseKeywordDefine},
    {"entry", parseKeywordEntry},
    {"output", parseKeywordOutput},
    {"format", parseKeywordFormat},
    {"depends", parseKeywordDepends},
    {"label", parseKeywordLabel},
    {"ifdef", parseKeywordIfdef},
    {"ifndef", parseKeywordIfndef}
  };

  targetProject.m_makefilePath = makefilePath;

  while (!isAtEnd()) {
    if (peek().m_type != Token::Type::KEYWORD) {logError("Unexpected token \"" + peek().m_value + "\", type: " + peek().typeToString() + " at " + peek().positionToString());advance(); continue;}
    const auto& token = advance();
    auto it = tokenParseHashMap.find(token.m_value);
    if (it == tokenParseHashMap.end()) {logError("Unknown keyword \"" + token.m_value + "\" at " + token.positionToString());continue;}
    it->second();
  }

}

std::string SMake::SMakeProject::toString() const {
  std::ostringstream str;
  str << "SMake file path: \"" << m_makefilePath.generic_string() << "\"\n"
      << "  Targets:\n";
  for (const auto& target : m_targets) {
    str << target.second.toString(2,2);
  }
  str << "  File lists (FLists):\n";
  for (const auto& flist : m_flists) {
    str << flist.second.toString(2,2);
    //static_assert(false); // complete the flist tostring, aswell as this tostring. then return to debug the SMAKE from the main()
  }

  str << "  Labels:\n";
  for (const auto& label : m_labels) {
    str << "    " << label << '\n';
  }


  return str.str();

}

std::string SMake::Target::toString(size_t padding, size_t indnt) const {
  std::ostringstream str;
  const std::string indent(indnt + padding, ' ');
  const std::string indent2(indnt + padding * 2, ' ');
  const std::string indent3(indnt + padding * 3, ' ');
  str << indent << "Target name: \"" << m_name << "\"\n"
      << indent2 << "Is built? " << (m_isBuilt ? "True" : "False") << '\n'
      << indent2 << "Working directory: \"" << m_workingDirectory << "\"\n"
      << indent2 << "Entry symbol: \"" << m_entrySymbol << "\"\n"
      << indent2 << "Output format: \"" << formatToString() << "\"\n"
      << indent2 << "Output directory: \"" << m_outputDirectory << "\"\n"
      << indent2 << "Output name: \"" << m_outputName << "\"\n"
      << indent2 << "Collated Included Directories:\n";
  
  for (const auto& path : m_includeDirectories) {
    str << indent3 << path.generic_string() << '\n';
  }

  str << indent2 << "Source file paths:\n";
  for (const auto& path : m_sourceFilepaths) {
    str << indent3 << path.generic_string() << '\n';
  }

  str << indent2 << "Dependant targets:\n";
  for (const auto& targetPtr : m_dependantTargets) {
    if (targetPtr != nullptr) {
      str << indent3 << '"' << targetPtr->m_name << "\"\n";
    }
  }

    return str.str();
      
}