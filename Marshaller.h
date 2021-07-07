// Marshaller.h

// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002,2003 OPeNDAP, Inc.
// Author: Patrick West <pwest@ucar.edu>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

// (c) COPYRIGHT URI/MIT 1994-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      pwest       Patrick West <pwest@ucar.edu>

#ifndef A_Marshaller_h
#define A_Marshaller_h 1

#include <string>
#include <vector>

#include "DapObj.h"
#include "Type.h"
#include "dods-datatypes.h"
#include "InternalErr.h"

namespace libdap {

class Vector;

/** @brief abstract base class used to marshal/serialize dap data objects
 */
class Marshaller: public DapObj {
public:
    virtual void put_byte(dods_byte val) = 0;

    virtual void put_int16(dods_int16 val) = 0;
    virtual void put_int32(dods_int32 val) = 0;

    virtual void put_float32(dods_float32 val) = 0;
    virtual void put_float64(dods_float64 val) = 0;

    virtual void put_uint16(dods_uint16 val) = 0;
    virtual void put_uint32(dods_uint32 val) = 0;

    virtual void put_str(const std::string &val) = 0;
    virtual void put_url(const std::string &val) = 0;

    virtual void put_opaque(char *val, unsigned int len) = 0;
    virtual void put_int(int val) = 0;

    virtual void put_vector(char *val, uint64_t num, Vector &vec) = 0;
    virtual void put_vector(char *val, uint64_t num, int width, Vector &vec) = 0;

    /**
     * Write the prefix bytes for a vector and reset the state/counter for
     * a vector/array that will be written using put_vector_part() and
     * put_vector_end().
     *
     * @param num The number of elements to write
     */
    virtual void put_vector_start(uint64_t /*num*/) {
        throw InternalErr(__FILE__, __LINE__, "Not Implemented yet");
    }// = 0;

    /**
     * Write one part of a vector's contents.
     *
     * @param val Pointer to the part's values
     * @param num The number of values in this part
     * @param width The number of bytes per value
     * @param type The DAP2 data type for each value
     */
    virtual void put_vector_part(char */*val*/, uint64_t /*num*/, int /*width*/, Type /*type*/) {
        throw InternalErr(__FILE__, __LINE__, "Not Implemented yet");
    }// = 0;

    /**
     * Close a vector written using put_vector_part()
     */
    virtual void put_vector_end() {
        throw InternalErr(__FILE__, __LINE__, "Not Implemented yet");
    }// = 0;

    virtual void dump(std::ostream &strm) const = 0;
};

} // namespace libdap

#endif // A_Marshaller_h

