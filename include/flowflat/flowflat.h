//
// Created by Markus Pilman on 10/16/22.
//

#ifndef FLATBUFFER_FLOWFLAT_H
#define FLATBUFFER_FLOWFLAT_H
#include <string_view>

namespace flowflat {

enum class Type { Struct, Table };

using uoffset_t = uint32_t;
using soffset_t = int32_t;
using voffset_t = int16_t;

} // namespace flowflat

#endif // FLATBUFFER_FLOWFLAT_H
