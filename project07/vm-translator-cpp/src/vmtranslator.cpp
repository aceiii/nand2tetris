#include "vmtranslator.h"

VMTranslator::VMTranslator(const std::string& code) : code(code) {}

tl::expected<std::vector<std::string>, std::string> VMTranslator::translate() {
    return tl::unexpected("Not Implemented!");
}
