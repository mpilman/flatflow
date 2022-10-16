//
// Created by Markus Pilman on 10/16/22.
//

#include <iostream>
#include <fmt/format.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "Compiler.h"
#include "StaticContext.h"

namespace flatbuffers {

expression::Type::Type(const std::string& name) : name(name) {}

std::optional<std::pair<TypeName, const expression::Type*>> StaticContext::resolve(const std::string& name) const {
	using Res = std::pair<TypeName, const expression::Type*>;
	if (auto iter = expression::primitiveTypes.find(name); iter != expression::primitiveTypes.end()) {
		return Res(TypeName{ .name = name,
		                     .path = currentFile->namespacePath ? currentFile->namespacePath.value()
		                                                        : std::vector<std::string>() },
		           &iter->second);
	}
	if (name.find('.') == std::string::npos) {
		// unqualified name -- check whether this is a "local" type (defined in current file
		auto res = currentFile->findType(name);
		if (res) {
			return Res(TypeName{ .name = name,
			                     .path = currentFile->namespacePath ? currentFile->namespacePath.value()
			                                                        : std::vector<std::string>() },
			           *res);
		}
		// this type is not defined in the current file -- so either it was defined in another file which defines
		// the same namespace, or it is in the global namespace.

		if (currentFile->namespacePath && !currentFile->namespacePath->empty()) {
			// if a type of this name exists in the global namespace AND in the current namespace, we will return the
			// one from the current namespace. So we have to check there first.
			auto result = resolve(fmt::format("{}.{}", fmt::join(*currentFile->namespacePath, "."), name));
			if (result) {
				return result;
			}
		} else {
			// We checked the current namespace by calling ourselves recursively. So now we check whether we can find
			// this type in the global namespace
			for (auto const& [_, tree] : compiler.files) {
				if (!tree->currentFile->namespacePath) {
					res = tree->currentFile->findType(name);
					if (res) {
						return Res(TypeName{ .name = name }, *res);
					}
				}
			}
		}
	}
	return {};
}

TypeName StaticContext::qualified(const std::string& name) const {
	auto res = resolve(name);
	if (!res) {
		std::cerr << fmt::format("Error: Type not found: {}", name);
		throw Error("Type not found");
	}
	return res->first;
}

const expression::Type& StaticContext::typeByName(const std::string& name) const {
	auto res = resolve(name);
	if (!res) {
		std::cerr << fmt::format("Error: Type not found: {}", name);
		throw Error("Type not found");
	}
	return *res->second;
}

} // namespace flatbuffers
