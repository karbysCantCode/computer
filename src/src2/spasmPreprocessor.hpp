#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <stack>
#include <filesystem>

//#include "arch.hpp"
#include "spasm.hpp"
#include "debugHelpers.hpp"
#include "smake.hpp"
#include "spasmMacro.hpp"

class Preprocessor {
  public:
  Preprocessor(Debug::FullLogger* logger = nullptr) 
    : m_logger(logger) {};

  std::vector<Spasm::Lexer::Token> run(
    std::vector<Spasm::Lexer::Token>& inputTokens,
    Spasm::Program::ProgramForm& program, 
    SMake::Target& target
  );


  Debug::FullLogger* m_logger = nullptr;
  private:
  struct TokenStream {
    std::vector<Spasm::Lexer::Token> m_tokens;
    size_t m_index = 0;

    TokenStream(const std::vector<Spasm::Lexer::Token>& tokens) 
      : m_tokens(tokens) {};

    inline bool isAtEnd() const {return !(m_index < m_tokens.size());}
    inline Spasm::Lexer::Token peek(size_t peekDistance = 0) {
      return m_index+peekDistance < m_tokens.size() ? 
        m_tokens[m_index+peekDistance] : 
        Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED,nullptr, -1,-1);
    };
    inline Spasm::Lexer::Token advance() {
      auto retVal = isAtEnd() ? Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED,nullptr, -1,-1) : m_tokens[m_index];
      m_index++;
      return retVal;
    };
    inline void skip(size_t distance = 1) {
      m_index += distance;
    };
  };
  std::stack<TokenStream> p_stack;
  std::unordered_map<std::string, std::unique_ptr<Macro::Macro>> p_macroMap;


  void handleTokenStream(TokenStream&, std::vector<Spasm::Lexer::Token>&, SMake::Target&, Spasm::Program::ProgramForm&);
  void handleDefine(TokenStream&);
  void handleInclude(TokenStream&, SMake::Target&, Spasm::Program::ProgramForm&);
  bool expandMacroIfExists(TokenStream&, std::unique_ptr<Macro::Macro>&);


  void logError(const Spasm::Lexer::Token& errToken, const std::string& message) const;
  void logWarning(const Spasm::Lexer::Token& errToken, const std::string& message) const;
  void logDebug(const Spasm::Lexer::Token& errToken, const std::string& message) const;


  
};


/*
namespace {


struct TokenStream {
  std::vector<Spasm::Lexer::Token> tokens;
  size_t index = 0;
};

struct LineColPair {
  int line = -1;
  int col = -1;

  LineColPair(int ln, int cl) : line(ln), col(cl) {}
};

std::filesystem::path searchForPathInIncluded(const std::string& sPath, 
                                              const SMake::Target& target) {
  namespace fs = std::filesystem; 
  fs::path path{sPath};

  if (fs::is_regular_file(path)) {
    return fs::canonical(path);
  }
  
  for (const auto& directory : target.m_includeDirectories) {
    fs::path candidate = directory / path;

    if (fs::is_regular_file(candidate)) {
      return fs::canonical(candidate);
    }
  }

  return std::filesystem::path();
}



// 
std::vector<Spasm::Lexer::Token> preprocessSpasm(std::vector<Spasm::Lexer::Token>& spasmTokens, Spasm::Program::ProgramForm& program, SMake::Target& target, std::unordered_map<std::filesystem::path, std::unique_ptr<Spasm::Program::ProgramForm>>& filePathProgramMap, Debug::FullLogger* logger = nullptr) {
  std::vector<Spasm::Lexer::Token> newTokens;
  std::stack<TokenStream> tokenStack;


  #define logError(errorMessage) \
    if (logger != nullptr) {logger->Errors.logMessage(errorMessage);}

  #define pushStack(arg)              \
    TokenStream newStream;            \
    newStream.tokens = std::move(arg);\
    tokenStack.push(newStream)  
  #define pushStackCopy(arg)        \
    TokenStream newStream;          \
    newStream.tokens = arg         ;\
    tokenStack.push(newStream)       

  pushStackCopy(spasmTokens);
  std::unordered_map<std::string, std::unique_ptr<Macro>> macroMap;
  
  #define ContinueAndLogIfAtEnd(errorMessage) \
  if (isAtEnd()) {logError(errorMessage); continue;}
  #define EOF_BEFORE_COMPLETE_ERR "Macro \"" + macroName + "\" hit EOF before definition finished."

  size_t its = 0;
  while (!tokenStack.empty()) {
    its++;
    if (its > 20) { assert (false);}
    std::cout << "newstack" << '\n';
    auto& currentStream = tokenStack.top();
    auto& currentTokens = currentStream.tokens;
    #define index currentStream.index

    
    if (currentTokens.size() <= index) {
        std::cout << "stepping off stack" << '\n';
        tokenStack.pop();
        continue;
    }

    auto isAtEnd = [&]() {
      return !(index<currentTokens.size());
    };
    auto peek = [&](size_t peekDistance = 0) {
      return index+peekDistance < currentTokens.size() ? currentTokens[index+peekDistance] : Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED, -1,-1);
    };
    auto advance = [&]() {
      auto retVal = isAtEnd() ? Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED, -1,-1) : currentTokens[index];
      index++;
      return retVal;
    };

    #define skip(arg) \
      index += arg

    bool isProcessingTop = true;
    while (!isAtEnd() && isProcessingTop) {
      #define pushStackAndRestart(arg)    \
        pushStack(arg);                   \
        isProcessingTop = false;          \
        goto __EndOfLoop                          
      

      Spasm::Lexer::Token token = peek();
      std::cout << token.m_value << ':' << token.typeToString() << ':' << token.positionToString() << '\n';

      auto macroIt = macroMap.find(token.m_value);
      if (macroIt != macroMap.end()) 
        { 
          std::cout << "consumeIdenForMacro:" << token.m_value << ':' << token.positionToString() << '\n';
          index++;
          pushStackAndRestart(macroIt->second->getReplacment(index, currentTokens));
        }

      if (token.m_type == Spasm::Lexer::Token::Type::DIRECTIVE && !isAtEnd()) {
        switch (token.m_nicheType) {

          case Spasm::Lexer::Token::NicheType::DIRECTIVE_DEFINE: {
            
          }
          break;

          case Spasm::Lexer::Token::NicheType::DIRECTIVE_INCLUDE: {
            advance();
            if (peek().m_type != Spasm::Lexer::Token::Type::STRING) {logError(std::string("Expected string for include, got ") + peek().typeToString() + " at " + peek().positionToString()); continue;}
            const auto& fileToken = advance();
            auto path = searchForPathInIncluded(fileToken.m_value, target);

            std::unique_ptr<Spasm::Program::ProgramForm>* includeProgram = nullptr;
            const auto it = filePathProgramMap.find(path);
            if (it != filePathProgramMap.end()) {
              includeProgram = &it->second;
            } else {
              auto programForm = std::make_unique<Spasm::Program::ProgramForm>();
              auto [newit, inserted] = filePathProgramMap.emplace(path, std::move(programForm));
              includeProgram = &newit->second;
              //create enrty in fielpathrpogrma map
            }
            program.m_includedPrograms.emplace(path, includeProgram);


            

            // logError("INCLUDE DIRECTIVE NOT IMPLEMENTED FOR PREPROCESSOR");
            // advance();
          }
          break;
          case Spasm::Lexer::Token::NicheType::DIRECTIVE_ENTRY: {
            logError("ENTRY DIRECTIVE NOT IMPLEMENTED FOR PREPROCESSOR");
            advance();
          }
          break;
          default:
            logError("Unrecognise directive \"" + token.m_value + "\" at " + token.positionToString());
          break;
        }
      } else {
        newTokens.push_back(advance());
      }
      __EndOfLoop:
      continue;
    }
  }

  #undef logError
  #undef pushStack
  #undef pushStackCopy
  #undef ContinueAndLogIfAtEnd
  #undef EOF_BEFORE_COMPLETE_ERR
  #undef index
  #undef skip
  
  // std::cout << "Start" << std::endl;
  // for (const auto& entry : macroMap) {
  //   std::cout << entry.first << std::endl;
  // }

  // std::cout << "S3" << std::endl;


  return newTokens;
}

//not consuming first arg? or not replacing?
};

*/