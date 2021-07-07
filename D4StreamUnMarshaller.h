// D4StreamUnMarshaller.h

// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2012 OPeNDAP, Inc.
// Author: James Gallagher <jgallagher@opendap.org>
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

#ifndef I_D4StreamUnMarshaller_h
#define I_D4StreamUnMarshaller_h 1

#include <iostream>

// See comment in D4StreamMarshaller
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

#include <crc.h>

// #include "Type.h"
#include "dods-datatypes.h"
#include "UnMarshaller.h"
#include "InternalErr.h"

#include "util.h"
#include "debug.h"

using std::istream;

namespace libdap {

class Vector;

/** @brief Read data from the stream made by D4StreamMarshaller.
 */
class D4StreamUnMarshaller: public UnMarshaller {
public:
    const static unsigned int c_checksum_length = 4;

private:
    istream &d_in;
    bool d_twiddle_bytes;

#if USE_XDR_FOR_IEEE754_ENCODING
    // These are used for reals that need to be converted from IEEE 754
    XDR d_source;
    char d_buf[sizeof(dods_float64)];
#endif

    D4StreamUnMarshaller();
    D4StreamUnMarshaller(const D4StreamUnMarshaller &);
    D4StreamUnMarshaller & operator=(const D4StreamUnMarshaller &);
#if USE_XDR_FOR_IEEE754_ENCODING
    void m_deserialize_reals(char *val, int64_t num, int width, Type type);
#endif
    void m_twidle_vector_elements(char *vals, uint64_t num, int width);

public:
    D4StreamUnMarshaller(istream &in, bool twiddle_bytes);
    D4StreamUnMarshaller(istream &in);
    virtual ~D4StreamUnMarshaller();

    void set_twiddle_bytes(bool twiddle) { d_twiddle_bytes = twiddle; }

    /**
     * @brief Is the data source we are reading from a big-endian machine?
     * We need this because the value of the CRC32 checksum is dependent on
     * byte order.
     *
     * @note This is somewhat tortured logic, but if this host is big-endian and
     * twiddle_bytes is not true, then the remote host must be big-endian. Similarly,
     * if this host is not big-endian and twiddle_bytes is true, then the remote
     * host must be big-endian
     */
    bool is_source_big_endian() const { return (is_host_big_endian() && !d_twiddle_bytes)
                                               || (!is_host_big_endian() && d_twiddle_bytes); }

    Crc32::checksum get_checksum();
    string get_checksum_str();
    int64_t get_count();

    virtual void get_byte(dods_byte &val);
    virtual void get_int8(dods_int8 &val);

    virtual void get_int16(dods_int16 &val);
    virtual void get_int32(dods_int32 &val);

    virtual void get_int64(dods_int64 &val);

    virtual void get_float32(dods_float32 &val);
    virtual void get_float64(dods_float64 &val);

    virtual void get_uint16(dods_uint16 &val);
    virtual void get_uint32(dods_uint32 &val);

    virtual void get_uint64(dods_uint64 &val);

    virtual void get_str(string &val);
    virtual void get_url(string &val);

    virtual void get_opaque(char *, unsigned int) {
    	throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4, use get_opaque_dap4() instead.");
    }

    virtual void get_opaque_dap4(char **val, int64_t &len);
    virtual void get_opaque_dap4( vector<uint8_t> &val );

    virtual void get_int(int &) {
        throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4");
    }

    // Note that DAP4 assumes clients know the size of arrays when they
    // read the data; it's the 'varying' get methods that read & return the
    // number of elements. These methods are here to appease the UnMarshaller
    // 'interface' code
    virtual void get_vector(char **, uint64_t &, Vector &) {
        throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4");
    }

    virtual void get_vector(char **, uint64_t &, int, Vector & ) {
        throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4");
    }

    virtual void get_vector(char *val, uint64_t num_bytes);
    virtual void get_vector(char *val, uint64_t num_elem, int elem_size);
    virtual void get_vector_float32(char *val, uint64_t num_elem);
    virtual void get_vector_float64(char *val, uint64_t num_elem);

    virtual void dump(ostream &strm) const;
};

} // namespace libdap

#endif // I_D4StreamUnMarshaller_h

