//
// Created by Markus Pilman on 10/15/22.
//

#ifndef FLATBUFFER_ERROR_H
#define FLATBUFFER_ERROR_H

#include <string>

namespace flatbuffers {

class Error : std::exception {
	std::string msg;

public:
	explicit Error(std::string msg);

private:
	[[nodiscard]] const char* what() const noexcept override;
};

} // namespace flatbuffers

#endif // FLATBUFFER_ERROR_H
