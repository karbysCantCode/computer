#include "Spasm/Spasm.hpp"

#include "Spasm/Lexer.hpp"
#include "Spasm/Parser.hpp"
#include "Spasm/Preprocessor.hpp"
#include "Helpers/FileHelper.hpp"

#include <queue>

namespace Spasm {

  void spasmPipeline(SMake::Project& project, Arch::Architecture& arch, CLIOptions& options) {
    for (auto& target : project.m_targets) {

      Debug::FullLogger logger;

      Program program;

      //order built after the address+expression resolvation of dependant order
      std::queue<Program::TranslationUnit*> programTranslationUnitOrder;

      for (const auto& sourcePath : target.second.m_sourceFilepaths) {
        program.m_filePathsToCreateTranslationUnitsOf.push(sourcePath);
      }

      std::unordered_map<std::string_view, Program::IdentifierObject*> programGlobalIdentifiersByFullName;

      std::stack<Program::TranslationUnit*> dependencylessTranslationUnits;
      std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>> dependantTranslationUnitMap;
      //parse trabskaton units
      while (!program.m_filePathsToCreateTranslationUnitsOf.empty()) {
        auto transUnit = std::make_unique<Program::TranslationUnit>();
        auto transUnitPtr = transUnit.get();
        transUnitPtr->m_sourcePath = program.m_filePathsToCreateTranslationUnitsOf.top();
        if (program.m_translationUnits.find(transUnit->m_sourcePath) == program.m_translationUnits.end()) {
          program.m_translationUnits.emplace(transUnit->m_sourcePath, std::move(transUnit));
        } else {
          program.m_translationUnits[transUnit->m_sourcePath] = std::move(transUnit);
        }

        
        program.m_filePathsToCreateTranslationUnitsOf.pop();
        transUnitPtr->m_source = std::make_unique<std::string>(FileHelper::openFileToString(transUnitPtr->m_sourcePath));
        
        SpasmLexer lexer;
        auto preprocessedTokens = lexer.run(*transUnitPtr->m_source, transUnitPtr->m_sourcePath);
        
        
        Preprocessor preproc;
        auto processedTokens = preproc.run(preprocessedTokens, target.second, *transUnitPtr, program, dependantTranslationUnitMap, &logger);
        
        Parser parser;
        parser.ParseTokens(processedTokens, arch, *transUnitPtr, program, &logger);
        
        if (transUnitPtr->m_includedFiles.size() == 0) {
          dependencylessTranslationUnits.push(transUnitPtr);
        }
      }
      
      //resolve addresses and then expressions
      while (!dependencylessTranslationUnits.empty()) {
        auto& translationUnit = *dependencylessTranslationUnits.top();

        resolveAddressesAndExpressionsOfTranslationUnit(translationUnit, programTranslationUnitOrder, );

        dependencylessTranslationUnits.pop();
        for (auto dependantUnit : dependantTranslationUnitMap[translationUnit.m_sourcePath]) {
          dependantUnit->m_dependenciesResolved++;
          if (dependantUnit->m_dependenciesResolved == dependantUnit->m_includedFiles.size()) {
            dependencylessTranslationUnits.push(dependantUnit);
          }
        }
      }

      if (!options.silent) {
        while (!logger.Errors.isEmpty()) {
          std::cout << logger.Errors.consumeMessage() << '\n';
        }
      }

      if (options.warns) {
        while (!logger.Warnings.isEmpty()) {
          std::cout << logger.Warnings.consumeMessage() << '\n';
        }
      }
      if (options.debugs) {
        while (!logger.Debugs.isEmpty()) {
          std::cout << logger.Debugs.consumeMessage() << '\n';
        }
      }
      
      program.debugPrint();
    }
  }

void Spasm::Program::debugPrint() const {
  std::cout << "\n=========== PROGRAM DEBUG DUMP ===========\n";

  for (const auto& [path, tuPtr] : m_translationUnits) {
    std::cout << "\n-- Translation Unit: " << path.string() << " --\n";

    const TranslationUnit& tu = *tuPtr;

    std::cout << "\n  Statements:\n";
    for (const auto& stmt : tu.m_statementVector) {
      debugPrintStatement(stmt.get(), 2);
    }

    std::cout << "\n  Identifiers:\n";
    for (const auto& [name, obj] : tu.m_identifierMap) {
      indent(2);
      std::cout << "Identifier Key: " << name << "\n";
      debugPrintIdentifier(obj.get(), 3);
    }

    std::cout << "\n";
  }

  std::cout << "\n-- Unresolved Expressions Stack --\n";
  auto stackCopy = m_unresolvedExpressions;
  while (!stackCopy.empty()) {
    indent(1);
    std::cout << "Unresolved Expr:\n";
    debugPrintExpr(stackCopy.top(), 2);
    stackCopy.pop();
  }

  std::cout << "\n==========================================\n";
}

void Spasm::Program::debugPrintStatement(const StatementSymbol* stmt, int indentLevel) const {
  indent(indentLevel);

  if (auto lbl = dynamic_cast<const LabelSymbol*>(stmt)) {
    std::cout << "LabelSymbol: " << lbl->name << "\n";
  }
  else if (auto def = dynamic_cast<const DefinitionSymbol*>(stmt)) {
    std::cout << "DefinitionSymbol: " << def->name << "\n";
  }
  else if (auto instr = dynamic_cast<const InstructionSymbol*>(stmt)) {
    std::cout << "InstructionSymbol: opcode=" << instr->opcode
              << " name=" << instr->instruction.m_name << "\n";

    for (const auto& op : instr->operands) {
      debugPrintOperand(op.get(), indentLevel + 1);
    }
  }
  else {
    std::cout << "Unknown StatementSymbol\n";
  }
}

void Spasm::Program::debugPrintIdentifier(const IdentifierObject* obj, int indentLevel) const {
  indent(indentLevel);
  std::cout << "Name: " << obj->name
            << " Address: " << obj->address
            << " Resolved: " << obj->addressResolved
            << "\n";

  if (auto lbl = dynamic_cast<const LabelObject*>(obj)) {
    indent(indentLevel);
    std::cout << "Type: LabelObject\n";

    for (const auto& [childName, child] : lbl->children) {
      indent(indentLevel + 1);
      std::cout << "Child Label: " << childName << "\n";
      debugPrintIdentifier(child.get(), indentLevel + 2);
    }
  }
  else if (auto data = dynamic_cast<const DataObject*>(obj)) {
    indent(indentLevel);
    std::cout << "Type: DataObject\n";

    indent(indentLevel);
    std::cout << "ElementSize: " << data->elementSize
              << " ElementCount: " << data->elementCount << "\n";

    indent(indentLevel);
    std::cout << "Raw Data: ";
    for (auto b : data->data) {
      std::cout << static_cast<int>(b) << " ";
    }
    std::cout << "\n";

    for (const auto& expr : data->exprData) {
      debugPrintExpr(expr.get(), indentLevel + 1);
    }
  }
}

void Spasm::Program::debugPrintExpr(const Expr* expr, int indentLevel) const {
  indent(indentLevel);

  if (!expr) {
    std::cout << "NULL Expr\n";
    return;
  }

  if (auto num = dynamic_cast<const NumberExpr*>(expr)) {
    std::cout << "NumberExpr: " << num->value << "\n";
  }
  else if (auto id = dynamic_cast<const IdentifierExpr*>(expr)) {
    std::cout << "IdentifierExpr: ";
    for (const auto& part : id->identifierPath) {
      std::cout << part;
    }
    std::cout << "\n";
  }
  else if (auto unary = dynamic_cast<const UnaryExpr*>(expr)) {
    std::cout << "UnaryExpr: op=" << static_cast<int>(unary->op) << "\n";
    debugPrintExpr(unary->right.get(), indentLevel + 1);
  }
  else if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
    std::cout << "BinaryExpr: op=" << static_cast<int>(bin->op) << "\n";
    debugPrintExpr(bin->left.get(), indentLevel + 1);
    debugPrintExpr(bin->right.get(), indentLevel + 1);
  }
  else {
    std::cout << "Base Expr (value=" << expr->value
              << ", evaluated=" << expr->evaluated << ")\n";
  }
}

void Spasm::Program::debugPrintOperand(const Operand* op, int indentLevel) const {
  indent(indentLevel);

  if (auto reg = dynamic_cast<const RegisterOperand*>(op)) {
    std::cout << "RegisterOperand: " << reg->reg.m_registerName << "\n";
  }
  else if (auto expr = dynamic_cast<const ExpressionOperand*>(op)) {
    std::cout << "ExpressionOperand:\n";
    debugPrintExpr(expr->expression.get(), indentLevel + 1);
  }
  else if (auto c = dynamic_cast<const ConstantOperand*>(op)) {
    std::cout << "ConstantOperand: " << c->value << "\n";
  }
  else {
    std::cout << "Unknown Operand\n";
  }
}

void Spasm::Program::indent(int count) {
  for (int i = 0; i < count; ++i)
    std::cout << "  ";
}
}