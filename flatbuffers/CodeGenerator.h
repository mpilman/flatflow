//
// Created by Markus Pilman on 10/16/22.
//

#ifndef FLATBUFFER_CODEGENERATOR_H
#define FLATBUFFER_CODEGENERATOR_H

#include "Compiler.h"

namespace flatbuffers {

class CodeGenerator {
	StaticContext* context;
	void emit(struct Streams& out, expression::ExpressionTree const& tree) const;
	void emit(struct Streams& out, expression::Enum const& anEnum) const;
	void emit(struct Streams& out, expression::Union const& anUnion) const;
	void emit(struct Streams& out, expression::Field const& field) const;
	void emit(struct Streams& out, expression::StructOrTable const& st) const;

public:
	explicit CodeGenerator(StaticContext* context);
	void emit(std::string const& stem,
	          boost::filesystem::path const& header,
	          boost::filesystem::path const& source) const;
};

} // namespace flatbuffers

#endif // FLATBUFFER_CODEGENERATOR_H
