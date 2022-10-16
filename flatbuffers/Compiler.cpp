//
// Created by Markus Pilman on 10/15/22.
//

#include <string_view>
#include <variant>
#include <functional>
#include <cstdio>
#include <cstdint>

#include <fmt/format.h>
#include <fstream>

#include "Compiler.h"
#include "Parser.h"
#include "Error.h"

namespace flatbuffers {

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace expression {

boost::unordered_map<std::string_view, std::pair<std::string_view, PrimitiveTypeClass>> primitiveTypes{
	{ "bool", { "bool", PrimitiveTypeClass::BoolType } },
	{ "byte", { "char", PrimitiveTypeClass::CharType } },
	{ "ubyte", { "unsigned char", PrimitiveTypeClass::CharType } },
	{ "short", { "short", PrimitiveTypeClass::IntType } },
	{ "ushort", { "unsigned short", PrimitiveTypeClass::IntType } },
	{ "int", { "int", PrimitiveTypeClass::IntType } },
	{ "uint", { "unsigned", PrimitiveTypeClass::IntType } },
	{ "float", { "float", PrimitiveTypeClass::FloatType } },
	{ "long", { "long", PrimitiveTypeClass::IntType } },
	{ "ulong", { "unsigned long", PrimitiveTypeClass::IntType } },
	{ "double", { "double", PrimitiveTypeClass::FloatType } },
	{ "int8", { "int8_t", PrimitiveTypeClass::IntType } },
	{ "uint8", { "uint8_t", PrimitiveTypeClass::IntType } },
	{ "int16", { "int16_t", PrimitiveTypeClass::IntType } },
	{ "uint16", { "uint16_t", PrimitiveTypeClass::IntType } },
	{ "int32", { "int32_t", PrimitiveTypeClass::IntType } },
	{ "uint32", { "uint32_t", PrimitiveTypeClass::IntType } },
	{ "int64", { "int64_t", PrimitiveTypeClass::IntType } },
	{ "uint64", { "uint64_t", PrimitiveTypeClass::IntType } },
	{ "float32", { "float", PrimitiveTypeClass::FloatType } },
	{ "float64", { "double", PrimitiveTypeClass::FloatType } },
	{ "string", { "std::string", PrimitiveTypeClass::StringType } },
};
} // namespace expression

namespace {

using namespace expression;

boost::unordered_set<std::string_view> reservedAttributes{
	"id",         "deprecated", "required", "force_align",   "force_align", "bit_flags", "nested_flatbuffer",
	"flexbuffer", "key",        "hash",     "original_order"
};

bool startsWith(std::string_view str, std::string_view prefix) {
	return str.substr(0, prefix.size()) == prefix;
}

struct CompilerVisitor : ast::Visitor {
	expression::ExpressionTree& state;

	explicit CompilerVisitor(expression::ExpressionTree& state) : state(state) {}

	void visit(const struct ast::IncludeDeclaration& declaration) override { Visitor::visit(declaration); }
	void visit(const struct ast::NamespaceDeclaration& declaration) override {
		if (state.namespacePath.has_value()) {
			fmt::print(stderr, "Error: Unexpected namespace declaration: {}\n", fmt::join(declaration.name, "."));
			fmt::print(stderr, "       {} was declared before\n", fmt::join(state.namespacePath.value(), "."));
			throw Error("Unexpected namespace");
		} else {
			state.namespacePath = declaration.name;
		}
	}
	void visit(const struct ast::AttributeDeclaration& declaration) override {
		if (state.attributes.contains(declaration.attribute)) {
			fmt::print(stderr, "Error: Attribute {} defined multiple times\n", declaration.attribute);
			throw Error("Multiple attributes");
		} else if (reservedAttributes.contains(declaration.attribute) || startsWith(declaration.attribute, "native_")) {
			fmt::print(stderr, "Error: Attribute {} is reserved\n", declaration.attribute);
			throw Error("Reserved attribute");
		} else {
			state.attributes.insert(declaration.attribute);
		}
	}
	void visit(const struct ast::RootDeclaration& declaration) override {
		if (primitiveTypes.contains(declaration.rootType)) {
			fmt::print(stderr, "Error: Primitive type {} can't be declared as root type\n", declaration.rootType);
			throw Error("Primitive root type");
		} else {
			state.rootTypes.insert(declaration.rootType);
		}
	}
	void visit(const struct ast::FileExtensionDeclaration& declaration) override {
		if (state.fileExtension.has_value()) {
			fmt::print(stderr,
			           "Multiple file extensions \"{}\" and \"{}\"\n",
			           state.fileExtension.value(),
			           declaration.extension);
			throw Error("Multiple file extensions");
		} else {
			state.fileExtension = declaration.extension;
		}
	}
	void visit(const struct ast::FileIdentifierDeclaration& declaration) override {
		if (declaration.identifier.size() != 4) {
			fmt::print(stderr,
			           "File identifiers need to be exactly 4 bytes long but \"{}\" is {} bytes long\n",
			           declaration.identifier,
			           declaration.identifier.size());
			throw Error("File identifier too long");
		} else if (state.fileIdentifier.has_value()) {
			fmt::print(stderr,
			           "Multiple file identifiers \"{}\" and \"{}\"\n",
			           state.fileExtension.value(),
			           declaration.identifier);
			throw Error("Multiple file extensions");
		} else {
			state.fileIdentifier = declaration.identifier;
		}
	}
	void visit(const struct ast::EnumDeclaration& declaration) override {
		if (state.typeExists(declaration.identifier)) {
			fmt::print(stderr, "Error: Type {} already exists\n", declaration.identifier);
			throw Error("Duplicate type");
		} else if (!primitiveTypes.contains(declaration.type) ||
		           (primitiveTypes[declaration.type].second != PrimitiveTypeClass::IntType &&
		            primitiveTypes[declaration.type].second != PrimitiveTypeClass::CharType)) {
			fmt::print(stderr, "Error: Type {} can't be used as base for an enum\n", declaration.type);
			throw Error("Incompatible enum type");
		}
		expression::Enum newEnum;
		newEnum.name = declaration.identifier;
		newEnum.type = declaration.type;
		int64_t lastVal = -1;
		boost::unordered_set<std::string_view> usedIdentifiers;
		boost::unordered_set<int64_t> usedValues;
		for (auto const& val : declaration.enumerations) {
			int64_t value;
			if (val.second.has_value()) {
				value = val.second.value();
				lastVal = value;
			} else {
				value = ++lastVal;
			}
			if (usedValues.contains(value)) {
				fmt::print(stderr, "Error: Duplicate value for enum {}: {}\n", declaration.identifier, value);
				throw Error("Duplicate enum value");
			} else if (usedIdentifiers.contains(val.first)) {
				fmt::print(stderr, "Error: Duplicate identifier for enum {}: {}\n", declaration.identifier, val.first);
				throw Error("Duplicate enum identifier");
			}
			usedValues.insert(value);
			newEnum.values.emplace_back(val.first, value);
		}
		state.enums.emplace(newEnum.name, std::move(newEnum));
	}
	void visit(const struct ast::UnionDeclaration& declaration) override {
		if (state.typeExists(declaration.identifier)) {
			fmt::print(stderr, "Error: Type {} already exists\n", declaration.identifier);
			throw Error("Duplicate type");
		}
		expression::Union newUnion;
		newUnion.name = declaration.identifier;
		for (auto const& val : declaration.enumerations) {
			if (val.second.has_value()) {
				fmt::print(stderr,
				           "Can't assign values to union members (union=\"{}\", member=\"{}\", value=\"{}\")\n",
				           newUnion.name,
				           val.first,
				           val.second.value());
				throw Error("Union with default value");
			}
			newUnion.types.push_back(val.first);
		}
		state.unions.emplace(newUnion.name, std::move(newUnion));
	}

	void constructStructOrTable(expression::StructOrTable& res,
	                            std::vector<ast::FieldDeclaration> const& fields) const {
		if (state.typeExists(res.name)) {
			fmt::print(stderr, "Error: Type {} already exists\n", res.name);
			throw Error("Duplicate type");
		}
		boost::unordered_set<std::string_view> prevFields;
		for (auto const& field : fields) {
			expression::Field f;
			if (prevFields.contains(field.identifier)) {
				fmt::print("Error: duplicate field {} in {}\n", field.identifier, res.name);
				throw Error("Duplicate field");
			}
			f.name = field.identifier;
			f.type = field.type.type();
			f.isArrayType = field.type.isArray();
			if (field.value) {
				f.defaultValue = field.value.value().toString();
			}
			res.fields.push_back(f);
		}
	}

	void visit(const struct ast::StructDeclaration& declaration) override {
		expression::Struct res;
		res.name = declaration.identifier;
		constructStructOrTable(res, declaration.fields);
		state.structs.emplace(res.name, std::move(res));
	}
	void visit(const struct ast::TableDeclaration& declaration) override {
		expression::Table res;
		res.name = declaration.identifier;
		constructStructOrTable(res, declaration.fields);
		state.tables.emplace(res.name, std::move(res));
	}
};

} // namespace

namespace expression {

bool ExpressionTree::typeExists(const std::string& name) const {
	return primitiveTypes.contains(name) || enums.contains(name) || unions.contains(name) || structs.contains(name) ||
	       tables.contains(name);
}
void ExpressionTree::verifyField(std::string name, bool isStruct, const Field& field) const {
	std::string typeLiteral = field.type;
	if (field.isArrayType) {
		typeLiteral = fmt::format("[{}]", field.type);
	}
	if (!typeExists(field.type)) {
		fmt::print(stderr,
		           "{} {} defines field {} of type {}, but {} can't be found\n",
		           isStruct ? "Struct" : "Table",
		           name,
		           field.name,
		           typeLiteral,
		           field.type);
		throw Error("Type not found");
	}
	if (field.defaultValue && field.isArrayType) {
		fmt::print(stderr,
		           "Field {} in {} {}: Can't assign value to array type {}\n",
		           field.name,
		           isStruct ? "struct" : "table",
		           name,
		           typeLiteral);
		throw Error("Assign value to array type");
	}
	if (field.defaultValue) {
		if (field.defaultValue && enums.contains(field.type)) {
			auto const& e = enums.at(field.type);
			bool found = false;
			for (auto const& [n, _] : e.values) {
				found = found || n == field.defaultValue.value();
			}
			if (!found) {
				fmt::print(stderr,
				           "Error: Field {} in {} {}: invalid enum value {}\n",
				           field.name,
				           isStruct ? "struct" : "table",
				           name,
				           field.defaultValue.value());
				std::vector<std::string> values;
				std::transform(e.values.begin(), e.values.end(), std::back_inserter(values), [](auto const& p) {
					return p.first;
				});
				fmt::print("       possible values are: [{}]\n", fmt::join(values, ", "));
				throw Error("Invalid enum value");
			}
		} else if (!primitiveTypes.contains(field.type)) {
			fmt::print(stderr,
			           "Error: Field {} in {} {}: Can't assign value to user defined type {}\n",
			           field.name,
			           isStruct ? "struct" : "table",
			           name,
			           typeLiteral);
			throw Error("Assign value to user defined type");
		}
	}
}

void ExpressionTree::verify() const {
	// verify all root types exist and are tables
	for (auto const& t : rootTypes) {
		if (!tables.contains(t)) {
			fmt::print(
			    stderr, "Error: Type {} was declared to be root but either doesn't exist or is not a table\n", t);
			throw Error("Root type not a table");
		}
	}
	// verify that all unions reference tables
	for (auto const& [name, u] : unions) {
		for (auto const& t : u.types) {
			if (!tables.contains(t)) {
				fmt::print(stderr,
				           "Error: Type {} was used in union {} but either doesn't exist or is not a table\n",
				           t,
				           name);
				throw Error("Union member not a table");
			}
		}
	}
	// verify that structs and tables reference proper types
	for (auto const& s : structs) {
		for (auto const& field : s.second.fields) {
			verifyField(s.first, true, field);
		}
	}
	for (auto const& s : tables) {
		for (auto const& field : s.second.fields) {
			verifyField(s.first, false, field);
		}
	}
}
ExpressionTree ExpressionTree::fromFile(std::string const& path) {
	ExpressionTree res;
	std::ifstream ifs(path.c_str());
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	auto schema = parseSchema(content);
	CompilerVisitor visitor(res);
	schema.accept(visitor);
	res.verify();
	return res;
}
} // namespace expression

void compile(std::ostream& out, const flatbuffers::ast::SchemaDeclaration& schema) {
	expression::ExpressionTree state;
	CompilerVisitor visitor(state);
	schema.accept(visitor);
	std::vector<std::string> tables;
	state.verify();
	// state.emit(out);
}

} // namespace flatbuffers