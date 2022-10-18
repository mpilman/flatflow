//
// Created by Markus Pilman on 10/16/22.
//

#ifndef FLATBUFFER_STATICCONTEXT_H
#define FLATBUFFER_STATICCONTEXT_H

#include <flowflat/flowflat.h>

#include "Compiler.h"

namespace flatbuffers {

class CodeGenerator;

struct SerializationInfo {
	unsigned alignment = 4;
	unsigned staticSize = 0u;
	std::optional<std::vector<flowflat::voffset_t>> vtable;
};

class StaticContext {
	using TypeDescr = std::pair<TypeName, expression::Type const*>;
	void serializationInformation(boost::unordered_map<TypeName, SerializationInfo>& state, TypeDescr const& t) const;

public:
	Compiler& compiler;
	std::shared_ptr<expression::ExpressionTree> currentFile;

	explicit StaticContext(Compiler& compiler);

	[[nodiscard]] boost::unordered_map<TypeName, SerializationInfo> serializationInformation(
	    std::string const& name) const;
	[[nodiscard]] std::optional<std::pair<TypeName, expression::Type const*>> resolve(TypeName const& name) const;
	[[nodiscard]] std::optional<std::pair<TypeName, expression::Type const*>> resolve(
	    const std::string& name,
	    bool excludeCurrent = false) const;
	void describeTable(std::string const& name) const;
};

} // namespace flatbuffers
#endif // FLATBUFFER_STATICCONTEXT_H
