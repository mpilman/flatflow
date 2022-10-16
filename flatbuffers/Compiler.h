//
// Created by Markus Pilman on 10/15/22.
//

#ifndef FLATBUFFER_COMPILER_H
#define FLATBUFFER_COMPILER_H
#include <ostream>

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "AST.h"
#include "Error.h"

namespace flatbuffers {

namespace expression {

struct Enum {
	std::string name;
	std::string type;
	std::vector<std::pair<std::string, int64_t>> values;
};

struct Union {
	std::string name;
	std::vector<std::string> types;
};

struct Field {
	std::string name;
	std::string type;
	bool isArrayType = false;
	std::optional<std::string> defaultValue;
};

struct StructOrTable {
	std::string name;
	std::vector<Field> fields;
};

struct Struct : StructOrTable {};

struct Table : StructOrTable {};

struct ExpressionTree {
	std::optional<std::vector<std::string>> namespacePath;
	boost::unordered_set<std::string> attributes;
	boost::unordered_set<std::string> rootTypes;
	std::optional<std::string> fileIdentifier;
	std::optional<std::string> fileExtension;
	boost::unordered_map<std::string, Enum> enums;
	boost::unordered_map<std::string, Union> unions;
	boost::unordered_map<std::string, Struct> structs;
	boost::unordered_map<std::string, Table> tables;

	[[nodiscard]] bool typeExists(std::string const& name) const;

	void verifyField(std::string name, bool isStruct, Field const& field) const;

	void verify() const;

	static ExpressionTree fromFile(std::string const& path);
};

enum class PrimitiveTypeClass { BoolType, CharType, IntType, FloatType, StringType };

extern boost::unordered_map<std::string_view, std::pair<std::string_view, PrimitiveTypeClass>> primitiveTypes;

} // namespace expression

void compile(std::ostream& out, ast::SchemaDeclaration const& schema);

void emit(std::ostream& out, expression::ExpressionTree const& tree);

} // namespace flatbuffers
#endif // FLATBUFFER_COMPILER_H
