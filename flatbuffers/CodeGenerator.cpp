//
// Created by Markus Pilman on 10/15/22.
//

#include "Compiler.h"

namespace flatbuffers {

namespace {

struct Defer {
	std::vector<std::function<void()>> functions;
	~Defer() {
		for (auto iter = functions.rbegin(); iter != functions.rend(); ++iter) {
			(*iter)();
		}
	}
	void operator()(std::function<void()> fun) { functions.push_back(std::move(fun)); }
};

std::string_view convertType(std::string const& t) {
	if (auto iter = expression::primitiveTypes.find(t); iter != expression::primitiveTypes.end()) {
		return iter->second.first;
	}
	return t;
}

void emit(std::ostream& out, expression::Enum const& f) {
	Defer defer;
	out << fmt::format("enum class {} : {} {{\n", f.name, convertType(f.type));
	defer([&out]() { out << "};\n"; });
	std::vector<std::string> definitions;
	for (auto const& [k, v] : f.values) {
		definitions.push_back(fmt::format("\t{} = {}", k, v));
	}
	out << fmt::format("{}\n", fmt::join(definitions, ",\n"));
}

void emit(std::ostream& out, expression::Union const& u) {
	std::vector<std::string_view> types;
	types.reserve(u.types.size());
	std::transform(
	    u.types.begin(), u.types.end(), std::back_inserter(types), [](auto const& t) { return convertType(t); });
	out << fmt::format("using {} = std::variant<{}>;\n", u.name, fmt::join(types, ", "));
}

void emit(std::ostream& out, expression::Field const& f) {
	std::string assignment;
	auto type = std::string(convertType(f.type));
	if (f.isArrayType) {
		type = fmt::format("std::vector<{}>", type);
	}
	if (f.defaultValue) {
		if (expression::primitiveTypes.contains(type)) {
			if (expression::primitiveTypes[type].second == expression::PrimitiveTypeClass::StringType) {
				assignment = fmt::format(" = \"{}\"", f.defaultValue.value());
			} else {
				assignment = fmt::format(" = {}", f.defaultValue.value());
			}
		} else {
			// at this point we know this is an enum type
			assignment = fmt::format(" = {}::{}", type, f.defaultValue.value());
		}
	}
	out << fmt::format("\t{} {}{};\n", type, f.name, assignment);
}

void emit(std::ostream& out, expression::StructOrTable const& st) {
	Defer defer;
	out << fmt::format("struct {} {{\n", st.name);
	defer([&out]() { out << "};\n"; });
	for (auto const& f : st.fields) {
		emit(out, f);
	}
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
} // namespace

void emit(std::ostream& out, expression::ExpressionTree const& tree) {
	Defer defer;
	if (tree.namespacePath) {
		out << fmt::format("namespace {} {{\n", fmt::join(tree.namespacePath.value(), "::"));
		defer([&out, &tree]() {
			out << fmt::format("}} // namespace {}\n", fmt::join(tree.namespacePath.value(), "::"));
		});
	}
	// enums have no dependencies, so we will emit them first
	for (auto const& [_, e] : tree.enums) {
		emit(out, e);
		out << "\n";
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
		out << "\n";
	}
}
} // namespace flatbuffers