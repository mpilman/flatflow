//
// Created by Markus Pilman on 10/16/22.
//

#ifndef FLATBUFFER_STATICCONTEXT_H
#define FLATBUFFER_STATICCONTEXT_H

#include "Compiler.h"

namespace flatbuffers {

class CodeGenerator;

class StaticContext {
public:
	Compiler& compiler;
	std::shared_ptr<expression::ExpressionTree> currentFile;

	explicit StaticContext(Compiler& compiler);

	[[nodiscard]] TypeName qualified(std::string const& name) const;
	[[nodiscard]] expression::Type const& typeByName(std::string const& name) const;
	[[nodiscard]] std::optional<std::pair<TypeName, expression::Type const*>> resolve(const std::string& name) const;
};

} // namespace flatbuffers
#endif // FLATBUFFER_STATICCONTEXT_H
