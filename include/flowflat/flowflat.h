//
// Created by Markus Pilman on 10/16/22.
//

#ifndef FLATBUFFER_FLOWFLAT_H
#define FLATBUFFER_FLOWFLAT_H
#include <string_view>
#include <string>
#include <memory>

namespace flowflat {

enum class Type { Struct, Table };

using uoffset_t = uint32_t;
using soffset_t = int32_t;
using voffset_t = int16_t;

struct Writer {
	virtual ~Writer();
	// will be called exactly once
	virtual char* allocateBuffer(int bytes) = 0;
};

// a writer using new and delete
class NewWriter : Writer {
	std::unique_ptr<char[]> buffer;

public:
	char* allocateBuffer(int bytes) override;
};

} // namespace flowflat

#endif // FLATBUFFER_FLOWFLAT_H
