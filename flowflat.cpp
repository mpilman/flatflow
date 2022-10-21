//
// Created by Markus Pilman on 10/16/22.
//
#include "flowflat/flowflat.h"

#include <cassert>

flowflat::Writer::~Writer() = default;

char* flowflat::NewWriter::allocateBuffer(int bytes) {
	buffer.reset(new char[bytes]);
	return buffer.get();
}
