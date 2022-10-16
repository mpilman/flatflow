//
// Created by Markus Pilman on 10/15/22.
//

#include "Error.h"

namespace flatbuffers {
const char* Error::what() const noexcept {
	return msg.c_str();
}
Error::Error(std::string msg) : msg(msg) {}
} // namespace flatbuffers