#include <iostream>
#include <fstream>
#include <fmt/format.h>

#include "flatbuffers/Compiler.h"

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		return 1;
	}
	for (int i = 1; i < argc; ++i) {
		fmt::print("Parsing file: {}\n", argv[i]);
		auto expr = flatbuffers::expression::ExpressionTree::fromFile(argv[i]);
		flatbuffers::emit(std::cout, expr);
	}
}
