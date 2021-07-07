// D4StreamMarshaller.h

// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002,2003,2012 OPeNDAP, Inc.
// Author: Patrick West <pwest@ucar.edu>,
//         James Gallagher <jgallagher@opendap.org>
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#ifndef I_D4StreamMarshaller_h
#define I_D4StreamMarshaller_h 1

#include <iostream>

// By default, only support platforms that use IEEE754 for floating point values.
// Hacked up code leftover from an older version of the class; largely untested.
// jhrg 10/3/13
#define USE_XDR_FOR_IEEE754_ENCODING 0

#if USE_XDR_FOR_IEEE754_ENCODING
#ifdef WIN32
#include <rpc.h>
#include <winsock2.h>
#include <xdr.h>
#else
#include <rpc/types.h>
#include <netinet/in.h>
#include <rpc/xdr.h>
#endif
#endif

#include <stdint.h>
#include "crc.h"

#include "Marshaller.h"
#include "InternalErr.h"

namespace libdap {

class Vector;
class MarshallerThread;

/** @brief Marshaller that knows how to marshal/serialize dap data objects
 * to a C++ iostream using DAP4's receiver-makes-right scheme. This code
 * adds checksums to the stream and uses the xdr library to encode real
 * values if the underlying representation is not IEEE 754. It also supports
 * computing the checksum only.
 *
 * @note This class uses the Marshaller interface; it could be rewritten
 * to use far fewer methods since all of the put_*() methods take different
 * types.
 */
class D4StreamMarshaller: public Marshaller {

private:
#if USE_XDR_FOR_IEEE754_ENCODING
    XDR d_scalar_sink;
    char d_ieee754_buf[sizeof(dods_float64)]; // used to serialize a float or double
#endif

    ostream &d_out;
    bool d_write_data; // jhrg 1/27/12

    Crc32 d_checksum;

    MarshallerThread *tm;

    // These are private so they won't ever get used.
    D4StreamMarshaller();
    D4StreamMarshaller(const D4StreamMarshaller &);
    D4StreamMarshaller & operator=(const D4StreamMarshaller &);

#if USE_XDR_FOR_IEEE754_ENCODING
    void m_serialize_reals(char *val, int64_t num, int width, Type type);
#endif

public:
    D4StreamMarshaller(std::ostream &out, bool write_data = true);
    virtual ~D4StreamMarshaller();

    virtual void reset_checksum();
    virtual string get_checksum();
    virtual void checksum_update(const void *data, unsigned long len);

    virtual void put_checksum();
    virtual void put_count(int64_t count);

    virtual void put_byte(dods_byte val);
    virtual void put_int8(dods_int8 val);

    virtual void put_int16(dods_int16 val);
    virtual void put_int32(dods_int32 val);
    // Added
    virtual void put_int64(dods_int64 val);

    virtual void put_float32(dods_float32 val);
    virtual void put_float64(dods_float64 val);

    virtual void put_uint16(dods_uint16 val);
    virtual void put_uint32(dods_uint32 val);
    // Added
    virtual void put_uint64(dods_uint64 val);

    virtual void put_str(const string &val);
    virtual void put_url(const string &val);

    virtual void put_opaque(char *, unsigned int) {
    	throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4; use put_opaque_dap4() instead.");
    }

    virtual void put_opaque_dap4(const char *val, int64_t len);

    // Never use put_int() to send length information in DAP4.
    virtual void put_int(int) {
        throw InternalErr(__FILE__, __LINE__, "Not Implemented; use put_length_prefix.");
    }

    virtual void put_vector(char *val, uint64_t num_bytes);
    virtual void put_vector(char *val, uint64_t num_elem, int elem_size);
    virtual void put_vector_float32(char *val, uint64_t num_elem);
    virtual void put_vector_float64(char *val, uint64_t num_elem);

    virtual void put_vector(char *, uint64_t , Vector &) {
        throw InternalErr(__FILE__, __LINE__, "Not Implemented; use other put_vector() versions.");
    }
    virtual void put_vector(char *, uint64_t , int , Vector &) {
        throw InternalErr(__FILE__, __LINE__, "Not Implemented; use other put_vector() versions.");
    }

    /**
     * Prepare to send a single array/vector using a series of 'put' calls.
     * In DAP4 this does nothing because arrays are serialized using the server's
     * binary representation (i.e., using 'reader make right').
     *
     * @param num Ignored
     * @see put_vector_part()
     * @see put_vector_end()
     */
    virtual void put_vector_start(uint64_t /*num*/) {
    }

    virtual void put_vector_part(char */*val*/, uint64_t /*num*/, int /*width*/, Type /*type*/);

    /**
     * Close a vector when its values are written using put_vector_part().
     * In DAP4 this does nothing because arrays are serialized using the server's
     * binary representation (i.e., using 'reader make right').
     *
     * @see put_vector_start()
     * @see put_vector_part()
     */
    virtual void put_vector_end() {
    }

    virtual void dump(std::ostream &strm) const;
};

} // namespace libdap

#endif // I_D4StreamMarshaller_h
