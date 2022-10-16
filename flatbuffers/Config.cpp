//
// Created by Markus Pilman on 10/16/22.
//

#include <vector>
#include <sstream>

#include <fmt/format.h>

#include "Config.h"

namespace flatbuffers::config {

namespace {
std::vector<std::string_view> includes = { "string"sv, "string_view"sv };
}

std::string const& defaultIncludes() {
	static std::string result;
	if (result.empty() && !includes.empty()) {
		std::stringstream ss;
		for (auto s : includes) {
			ss << fmt::format("#include <{}>\n", s);
		}
		result = ss.str();
	}
	return result;
}
} // namespace flatbuffers::config
