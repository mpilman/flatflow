//
// Created by Markus Pilman on 10/15/22.
//

#ifndef FLATBUFFER_COMPILER_H
#define FLATBUFFER_COMPILER_H
#include <ostream>
#include <memory>
#include <any>

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "AST.h"
#include "Error.h"

namespace flatbuffers {

class Compiler;

namespace expression {

enum class MetadataType { deprecated };

struct MetadataEntry {
	MetadataType type;
	std::any value;
};

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
	std::vector<MetadataEntry> metadata;

	[[nodiscard]] unsigned size(Compiler const& compiler) const;
};

struct StructOrTable {
	std::string name;
	std::vector<Field> fields;

	[[nodiscard]] unsigned size(Compiler const& compiler) const;
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
};

enum class PrimitiveTypeClass { BoolType, CharType, IntType, FloatType, StringType };

struct TypeDescription {
	std::string_view nativeName;
	PrimitiveTypeClass typeClass;
	unsigned _size;

	[[nodiscard]] unsigned size() const { return _size; }
};

extern boost::unordered_map<std::string_view, TypeDescription> primitiveTypes;

} // namespace expression

class Compiler {
	friend struct expression::Field;
	friend struct expression::StructOrTable;
	// compiled files (used to optimize includes). Filepath -> expression tree
	boost::unordered_map<std::string, std::shared_ptr<expression::ExpressionTree>> files;
	// where to search for included files
	std::vector<std::string> includePaths;
	// Files that got compiled in this run
	boost::unordered_map<std::string, std::shared_ptr<expression::ExpressionTree>> compiledFiles;

public:
	explicit Compiler(std::vector<std::string> includePaths);

	void compile(std::string const& path);

	// defined in CodeGenerator.cpp
	void generateCode(std::string const& headerDir, std::string const& sourceDir);
};

} // namespace flatbuffers
#endif // FLATBUFFER_COMPILER_H
