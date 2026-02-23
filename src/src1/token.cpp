#include "token.hpp"

#include <iostream>
#include <iomanip>

void Token::Token::print(size_t padding) const {
  std::cout << std::setw(padding) << "Token Type: " << toString(this->type) << '\n'
            << std::setw(padding) << "    " << "Token Value: \"" << this->value << "\"" << '\n'
            << std::setw(padding) << "    " << "Line: " << this->line << '\n'
            << std::setw(padding) << "    " << "Col:  " << this->column << std::endl;
}

void ArchToken::ArchToken::print(size_t padding) const {
  std::cout << std::setw(padding) << "Token Type: " << toString(this->type) << '\n'
            << std::setw(padding) << "    " << "Token Value: \"" << this->value << "\"" << '\n'
            << std::setw(padding) << "    " << "Line: " << this->line << '\n'
            << std::setw(padding) << "    " << "Col:  " << this->column << std::endl;
}