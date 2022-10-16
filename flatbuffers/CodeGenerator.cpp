//
// Created by Markus Pilman on 10/15/22.
//

#include <fstream>
#include <string>
#include <any>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <flowflat/flowflat.h>

#include "Config.h"
#include "Compiler.h"

using namespace std::string_literals;

namespace flatbuffers {

namespace {

struct Streams {
	std::ostream* header;
	std::ostream* source;
};

struct Defer {
	std::vector<std::function<void()>> functions;
	~Defer() {
		for (auto iter = functions.rbegin(); iter != functions.rend(); ++iter) {
			(*iter)();
		}
	}
	void operator()(std::function<void()> fun) { functions.push_back(std::move(fun)); }
};

std::string convertType(std::string const& t) {
	if (auto iter = expression::primitiveTypes.find(t); iter != expression::primitiveTypes.end()) {
		return std::string(iter->second.nativeName);
	}
	std::vector<std::string> path;
	boost::split(path, t, boost::is_any_of("."));
	return fmt::format("{}", fmt::join(path, "::"));
}

void emit(Streams out, expression::Enum const& f) {
	// 0. generate the enum
	{
		Defer defer;
		*out.header << fmt::format("enum class {} : {} {{\n", f.name, convertType(f.type));
		defer([&out]() { *out.header << "};\n\n"; });
		std::vector<std::string> definitions;
		for (auto const& [k, v] : f.values) {
			definitions.push_back(fmt::format("\t{} = {}", k, v));
		}
		*out.header << fmt::format("{}\n", fmt::join(definitions, ",\n"));
	}
	// 1. Generate the helper functions
	{
		*out.header << fmt::format("// {} helper functions\n", f.name);
		*out.source << fmt::format("// {} helper functions\n", f.name);
		// toString
		*out.header << fmt::format("{0} toString({1});\n", config::stringType, f.name);
		*out.source << fmt::format("{0} toString({1} e) {{\n", config::stringType, f.name);
		*out.source << "\tswitch (e) {\n";
		for (auto const& [k, _] : f.values) {
			*out.source << fmt::format("\tcase {0}::{1}:\n", f.name, k);
			*out.source << fmt::format("\t\treturn \"{0}\"{1};\n", k, config::stringLiteral);
		}
		*out.source << "\t}\n";
		*out.source << "}\n\n";
		// fromString and fromStringView

		auto fromString = [out, &f](auto stringType, auto stringLiteral) {
			*out.header << fmt::format("void fromString({0}& out, {1} const& str);\n", f.name, stringType);
			*out.source << fmt::format("void fromString({0}& out, {1} const& str) {{\n", f.name, stringType);
			bool first = true;
			for (auto const& [k, _] : f.values) {
				*out.source << fmt::format(
				    "\t{0} (str == \"{1}\"{2}) {{\n", first ? "if" : "} else if", k, stringLiteral);
				*out.source << fmt::format("\t\treturn {}::{};\n", f.name, k);
				first = false;
			}
			*out.source << "\t} else {\n";
			*out.source << fmt::format("\t\t{};\n", config::parseException);
			*out.source << "\t}\n";
			*out.source << "}\n";
		};
		fromString(config::stringType, config::stringLiteral);
		fromString(config::stringViewType, config::stringViewLiteral);
		*out.header << '\n';
		*out.source << '\n';
	}
}

void emit(Streams out, expression::Union const& u) {
	std::vector<std::string_view> types;
	types.reserve(u.types.size());
	std::transform(
	    u.types.begin(), u.types.end(), std::back_inserter(types), [](auto const& t) { return convertType(t); });
	*out.header << fmt::format("using {} = std::variant<{}>;\n", u.name, fmt::join(types, ", "));
}

void emit(Streams out, expression::Field const& f) {
	std::string assignment;
	auto type = std::string(convertType(f.type));
	if (f.isArrayType) {
		type = fmt::format("std::vector<{}>", type);
	}
	if (f.defaultValue) {
		if (expression::primitiveTypes.contains(type)) {
			if (expression::primitiveTypes[type].typeClass == expression::PrimitiveTypeClass::StringType) {
				assignment = fmt::format(" = \"{}\"", f.defaultValue.value());
			} else {
				assignment = fmt::format(" = {}", f.defaultValue.value());
			}
		} else {
			// at this point we know this is an enum type
			assignment = fmt::format(" = {}::{}", type, f.defaultValue.value());
		}
	}
	*out.header << fmt::format("\t{} {}{};\n", type, f.name, assignment);
}

void emit(Streams out, expression::StructOrTable const& st, flowflat::Type type) {
	std::string typeStr = type == flowflat::Type::Struct ? "flowflat::Type::Struct" : "flowflat::Type::Table";
	Defer defer;
	*out.header << fmt::format("struct {} {{\n", st.name);
	*out.header << fmt::format("\t[[nodiscard]] flowflat::Type flowFlatType() const {{ return {}; }};\n\n", typeStr);
	defer([&out]() { *out.header << "};\n"; });
	for (auto const& f : st.fields) {
		emit(out, f);
	}
}

void emit(Streams out, expression::Struct const& st) {
	emit(out, st, flowflat::Type::Struct);
}

void emit(Streams out, expression::Table const& st) {
	emit(out, st, flowflat::Type::Table);
}

using DependencyMap = boost::unordered_map<std::string, boost::unordered_set<std::string>>;

bool isInternalDependency(std::string const& type, expression::ExpressionTree const& tree) {
	return tree.unions.contains(type) || tree.tables.contains(type) || tree.structs.contains(type);
}

DependencyMap buildDependencyMap(expression::ExpressionTree const& tree) {
	DependencyMap res;
	// enums don't have dependencies. Therefore, we can ignore them
	for (auto const& [n, u] : tree.unions) {
		// make sure types with only primitive types still get inserted
		auto [iter, _] = res.emplace(n, DependencyMap::mapped_type());
		for (auto const& t : u.types) {
			if (isInternalDependency(t, tree)) {
				iter->second.insert(t);
			}
		}
	}
	auto processStructOrTable = [&res, &tree](expression::StructOrTable const& s) {
		auto [iter, _] = res.emplace(s.name, DependencyMap::mapped_type());
		for (auto const& f : s.fields) {
			if (isInternalDependency(f.type, tree)) {
				iter->second.insert(f.type);
			}
		}
	};
	for (auto const& [_, s] : tree.structs) {
		processStructOrTable(s);
	}
	for (auto const& [_, t] : tree.tables) {
		processStructOrTable(t);
	}
	return res;
}

/**
 * C++ doesn't allow us to use a type before it is declared. This function goes through all available types
 * and orders them by their dependencies. It will throw an error if this is not possible.
 *
 * @param tree
 * @return A list of types in a conflict-free order
 */
std::vector<std::string> establishEmitOrder(expression::ExpressionTree const& tree) {
	std::vector<std::string> res;
	boost::unordered_set<std::string> resolvedTypes;
	DependencyMap dependencies = buildDependencyMap(tree);
	while (!dependencies.empty()) {
		auto before = res.size();
		// 0. remove all resolved types from all dependencies
		for (auto& [name, deps] : dependencies) {
			for (auto const& t : resolvedTypes) {
				deps.erase(t);
			}
		}
		// 1. collect all types with no dependencies
		for (auto const& [name, deps] : dependencies) {
			if (deps.empty()) {
				res.push_back(name);
				resolvedTypes.insert(name);
			}
		}
		if (res.size() - before == 0) {
			throw Error("Cyclic dependencies");
		}
		// 2, Delete types with no dependencies from the map
		for (auto i = before; i < res.size(); ++i) {
			dependencies.erase(res[i]);
		}
	}
	return res;
}

void emit(Streams out, expression::ExpressionTree const& tree) {
	Defer defer;
	if (tree.namespacePath) {
		*out.header << fmt::format("namespace {} {{\n", fmt::join(tree.namespacePath.value(), "::"));
		defer([&out, &tree]() {
			*out.header << fmt::format("}} // namespace {}\n", fmt::join(tree.namespacePath.value(), "::"));
		});
	}
	// enums have no dependencies, so we will emit them first
	for (auto const& [_, e] : tree.enums) {
		emit(out, e);
		*out.header << "\n";
	}
	auto types = establishEmitOrder(tree);
	for (auto const& t : types) {
		if (auto uIter = tree.unions.find(t); uIter != tree.unions.end()) {
			emit(out, uIter->second);
		} else if (auto sIter = tree.structs.find(t); sIter != tree.structs.end()) {
			emit(out, sIter->second);
		} else if (auto tIter = tree.tables.find(t); tIter != tree.tables.end()) {
			emit(out, tIter->second);
		} else {
			throw Error("BUG");
		}
		*out.header << "\n";
	}
}

std::string headerGuard(std::string const& stem) {
	auto res = "FLOWFLAT_"s;
	res.reserve(res.size() + stem.size() + 2);
	std::transform(stem.begin(), stem.end(), std::back_inserter(res), [](auto c) { return std::toupper(c); });
	res.push_back('_');
	res.push_back('H');
	return res;
}

} // namespace

void Compiler::generateCode(const std::string& headerDir, const std::string& sourceDir) {
	namespace fs = boost::filesystem;
	for (auto const& [name, expr] : compiledFiles) {
		auto stem = fs::path(name).stem().string();
		auto headerFileName = stem + ".h";
		auto sourceFileName = stem + ".cpp";
		auto header = fs::path(headerDir) / headerFileName;
		auto source = fs::path(sourceDir) / sourceFileName;
		std::ofstream headerStream(header.c_str(), std::ios_base::out | std::ios_base::trunc);
		std::ofstream sourceStream(source.c_str(), std::ios_base::out | std::ios_base::trunc);
		Defer defer;
		auto guard = headerGuard(stem);
		headerStream << "// THIS FILE WAS GENERATED BY FLOWFLATC, DO NOT EDIT!\n";
		headerStream << fmt::format("#ifndef {0}\n#define {0}\n#include <flowflat/flowflat.h>\n\n", guard);
		headerStream << config::defaultIncludes() << "\n";
		defer([&headerStream, &guard]() { headerStream << fmt::format("\n#endif // #ifndef {}\n", guard); });
		Streams streams{ &headerStream, &sourceStream };
		emit(streams, *expr);
	}
}

} // namespace flatbuffers