#include "Spasm/Linker.hpp"

namespace Spasm {

std::queue<Program::TranslationUnit*> Linker::run(

) {
  std::queue<Program::TranslationUnit*> linkedTUQue;
  while (areUnlinkedIndependentTranslationUnits()) {
    auto& translationUnit = *consumeIndependentTranslationUnitFromStack();

    linkTU(translationUnit, linkedTUQue);

    for (auto dependantUnit : m_dependantTranslationUnitMap[translationUnit.m_sourcePath]) {
      dependantUnit->m_dependenciesResolved++;
      if (dependantUnit->m_dependenciesResolved == dependantUnit->m_includedFiles.size()) {
        addIndependentTranslationUnits(dependantUnit);
      }
    }
  }

  return linkedTUQue;
}

void Linker::linkTU(
  Program::TranslationUnit& translationUnit, 
  std::queue<Program::TranslationUnit*>& linkedTUQue
) {
  //resolve addresses
  size_t currentAddress = 0;
  for (auto& statementSymbol : translationUnit.m_statementVector) {
    switch (statementSymbol.get()->getKind())
    {
    using KIND = Program::StatementSymbol::Kind;
    case KIND::LABEL:
    {
      auto& labelSymbol = *static_cast<Program::LabelSymbol*>(statementSymbol.get());
      auto& labelObject = *labelSymbol.labelObject;
      auto& labelObjectPtr = labelSymbol.labelObject;
      
      const auto it = m_fullNameCollatedIdentifierMap.find(labelObject.fullName);

      if (iteratorAlreadyInFullNameMap(it)) {
        if (auto ptr = dynamic_cast<Spasm::Program::LabelObject*>(it->second)) {
          logError(labelSymbol.location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
        } else if (auto ptr = dynamic_cast<Spasm::Program::DataObject*>(it->second)) {
          logError(labelSymbol.location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
        } else {
          logError(labelSymbol.location, "Identifier is already defined with unknown type so no location is provided.");
        }

        continue;
      }
      //if it doesnt already exist ie, no fullname found
      if (!labelObject.parent ) {
        if (!nameAlreadyInGlobalMap(labelObject.name)) {
          m_globalIdentifierMap.emplace(labelObject.name, &labelObject); 
        } else {
          logError(labelSymbol.location, "somehow there is a parent-less identifier object in the linkers map, without an entry in the fullname, in the globalmap, this just shouldnt happen...");
          continue;
        }
      } else {
        const auto parentIt = m_fullNameCollatedIdentifierMap.find(labelObject.parent->fullName);
        if (iteratorAlreadyInFullNameMap(parentIt)) {
          const auto& lblSharedPtr = parentIt->second->children.find(labelObject.name)->second;
          parentIt->second->children.emplace(labelObject.name, lblSharedPtr);
        }
      }

      m_fullNameCollatedIdentifierMap.emplace(labelObject.fullName, labelObjectPtr);
      
      labelObjectPtr->address = currentAddress;
      labelObjectPtr->addressResolved = true;
      break;
    }
    case KIND::DEFINITION:
      {
        auto& definitionSymbol = *static_cast<Program::DefinitionSymbol*>(statementSymbol.get());
        auto& definitionObject = *definitionSymbol.dataObject;
        auto& definitionObjectPtr = definitionSymbol.dataObject;
        const auto it = m_fullNameCollatedIdentifierMap.find(definitionObject.fullName);

        if (iteratorAlreadyInFullNameMap(it)) {
          if (auto ptr = dynamic_cast<Spasm::Program::LabelObject*>(it->second)) {
            logError(definitionSymbol.location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
          } else if (auto ptr = dynamic_cast<Spasm::Program::DataObject*>(it->second)) {
            logError(definitionSymbol.location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
          } else {
            logError(definitionSymbol.location, "Identifier is already defined with unknown type so no location is provided.");
          }

          continue;
        }
        //if it doesnt already exist ie, no fullname found
        if (!definitionObject.parent ) {
          if (!nameAlreadyInGlobalMap(definitionObject.name)) {
            m_globalIdentifierMap.emplace(definitionObject.name, &definitionObject); 
          } else {
            logError(definitionSymbol.location, "somehow there is a parent-less identifier object in the linkers map, without an entry in the fullname, in the globalmap, this just shouldnt happen...");
            continue;
          }
        } else {
          const auto parentIt = m_fullNameCollatedIdentifierMap.find(definitionObject.parent->fullName);
          if (iteratorAlreadyInFullNameMap(parentIt)) {
            if (parentIt->second->isIdentifier()) {
              //illegal bc self is definiton and parent is def
              logError(definitionSymbol.location, std::format("Parent of data object \"{}\" is defined as a data object at \"{}\" (data objects cannot be parents.)", definitionObject.fullName, static_cast<Program::DataObject*>(it->second)->symbolObject->location.toString()));
              continue;
            } else {
              const auto& defSharedPtr = parentIt->second->children.find(definitionObject.name)->second;
              parentIt->second->children.emplace(definitionObject.name, defSharedPtr);
            }
          }
        }

        m_fullNameCollatedIdentifierMap.emplace(definitionObject.fullName, definitionObjectPtr);
        
        definitionObjectPtr->address = currentAddress;
        definitionObjectPtr->addressResolved = true;
        
        currentAddress += 
          definitionObjectPtr->elementCount * 
          definitionObjectPtr->elementSize;
        currentAddress += currentAddress % 2 == 1; //pad for 2byte alignment of instructions
        break;  
      }
    case KIND::INSTRUCTION:
    {
      auto& instructionSymbol = *static_cast<Program::InstructionSymbol*>(statementSymbol.get());
      currentAddress += instructionSymbol.instruction.m_byteLength;
      currentAddress += currentAddress % 2 == 1; //pad for 2byte alignment of instructions
      break;
    }
    default:
      assert(false);
      //just shouldnt happen and i raely cba to implement this error
      break;
    }
  }

  //resolve expressions
  while (!translationUnit.m_unresolvedExpressions.empty()) {
    auto& expr = translationUnit.m_unresolvedExpressions.front();
    translationUnit.m_unresolvedExpressions.pop();

    auto eval = expr->evaluate(m_globalIdentifierMap);

    if (!eval.second.empty()) {
      logError(expr->location, eval.second);
      continue;
    }
  }

  //all linked!
}

}