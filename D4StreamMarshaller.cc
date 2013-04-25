// D4StreamMarshaller.cc

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
//
// Portions of this code are from Google's great protocol buffers library and
// are copyrighted as follows:

// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/

#include "config.h"

#include <stdint.h>     // for the Google protobuf code
#include <byteswap.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;

//#define DODS_DEBUG 1
#include "D4StreamMarshaller.h"

#include "dods-datatypes.h"
#include "XDRUtils.h"
#include "util.h"
#include "debug.h"

namespace libdap {

// From the Google protobuf library
inline uint8_t* WriteVarint64ToArrayInline(uint64_t value, uint8_t* target) {
  // Splitting into 32-bit pieces gives better performance on 32-bit
  // processors.
  uint32_t part0 = static_cast<uint32_t>(value      );
  uint32_t part1 = static_cast<uint32_t>(value >> 28);
  uint32_t part2 = static_cast<uint32_t>(value >> 56);

  int size;

  // Here we can't really optimize for small numbers, since the value is
  // split into three parts.  Checking for numbers < 128, for instance,
  // would require three comparisons, since you'd have to make sure part1
  // and part2 are zero.  However, if the caller is using 64-bit integers,
  // it is likely that they expect the numbers to often be very large, so
  // we probably don't want to optimize for small numbers anyway.  Thus,
  // we end up with a hard coded binary search tree...
  if (part2 == 0) {
    if (part1 == 0) {
      if (part0 < (1 << 14)) {
        if (part0 < (1 << 7)) {
          size = 1; goto size1;
        } else {
          size = 2; goto size2;
        }
      } else {
        if (part0 < (1 << 21)) {
          size = 3; goto size3;
        } else {
          size = 4; goto size4;
        }
      }
    } else {
      if (part1 < (1 << 14)) {
        if (part1 < (1 << 7)) {
          size = 5; goto size5;
        } else {
          size = 6; goto size6;
        }
      } else {
        if (part1 < (1 << 21)) {
          size = 7; goto size7;
        } else {
          size = 8; goto size8;
        }
      }
    }
  } else {
    if (part2 < (1 << 7)) {
      size = 9; goto size9;
    } else {
      size = 10; goto size10;
    }
  }

  // GOOGLE_LOG(FATAL) << "Can't get here.";

  size10: target[9] = static_cast<uint8_t>((part2 >>  7) | 0x80);
  size9 : target[8] = static_cast<uint8_t>((part2      ) | 0x80);
  size8 : target[7] = static_cast<uint8_t>((part1 >> 21) | 0x80);
  size7 : target[6] = static_cast<uint8_t>((part1 >> 14) | 0x80);
  size6 : target[5] = static_cast<uint8_t>((part1 >>  7) | 0x80);
  size5 : target[4] = static_cast<uint8_t>((part1      ) | 0x80);
  size4 : target[3] = static_cast<uint8_t>((part0 >> 21) | 0x80);
  size3 : target[2] = static_cast<uint8_t>((part0 >> 14) | 0x80);
  size2 : target[1] = static_cast<uint8_t>((part0 >>  7) | 0x80);
  size1 : target[0] = static_cast<uint8_t>((part0      ) | 0x80);

  target[size-1] &= 0x7F;
  return target + size;
}

/** Build an instance of D4StreamMarshaller. Bind the C++ stream out to this
 * instance. If the write_data parameter is true, write the data in addition
 * to computing and sending the checksum.
 *
 * @param out Write to this stream object.
 * @param write_data If true, write data values. True by default
 */
D4StreamMarshaller::D4StreamMarshaller(ostream &out, bool write_data) :
        d_out(out), d_write_data(write_data)
{
    // XDR is used if the call std::numeric_limits<double>::is_iec559()
    // returns false indicating that the compiler is not using IEEE 754.
    // If it is, we just write out the bytes. Why malloc()? Because
    // xdr_destroy is going to call free() for us.
    d_ieee754_buf = (char*)malloc(sizeof(dods_float64));
    if (!d_ieee754_buf)
        throw InternalErr(__FILE__, __LINE__, "Could not create D4StreamMarshaller");
    xdrmem_create(&d_scalar_sink, d_ieee754_buf, sizeof(dods_float64), XDR_ENCODE);

    // This will cause exceptions to be thrown on i/o errors. The exception
    // will be ostream::failure
    out.exceptions(ostream::failbit | ostream::badbit);
}

D4StreamMarshaller::~D4StreamMarshaller()
{
    // Free the buffer this contains. The xdr_destroy() macro does not
    // free the XDR struct (which is fine since we did not dynamically
    // allocate it).
    free(d_ieee754_buf);
    d_ieee754_buf = 0;
    xdr_destroy(&d_scalar_sink);
}

/**
 * Return the is the host big- or little-endian?
 *
 * @return 'big' or ' little'.
 */

string
D4StreamMarshaller::get_endian() const
{
    return (is_host_big_endian()) ? "big": "little";
}

/** Initialize the checksum buffer. This resets the checksum calculation.
 */
void D4StreamMarshaller::reset_checksum()
{
    d_checksum.Reset();
}

/** Get the current checksum. It is not possible to continue computing the
 * checksum once this has been called.
 * @exception InternalErr if called when the object was created without
 * checksum support or if called when the checksum has already been returned.
 */
string D4StreamMarshaller::get_checksum()
{
    ostringstream oss;
    oss.setf(ios::hex, ios::basefield);
    oss << setfill('0') << setw(8) << d_checksum.GetCrc32();

    return oss.str();
}

void D4StreamMarshaller::put_checksum()
{
    Crc32::checksum chk = d_checksum.GetCrc32();
    d_out.write(reinterpret_cast<char*>(&chk), sizeof(Crc32::checksum));
}

void D4StreamMarshaller::checksum_update(const void *data, unsigned long len)
{
    d_checksum.AddData(reinterpret_cast<const uint8_t*>(data), len);
}

void D4StreamMarshaller::put_byte(dods_byte val)
{
    checksum_update(&val, sizeof(dods_byte));

    if (d_write_data) {
        DBG( std::cerr << "put_byte: " << val << std::endl );

        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_byte));
    }
}

void D4StreamMarshaller::put_int8(dods_int8 val)
{
    checksum_update(&val, sizeof(dods_int8));

    if (d_write_data) {
        DBG( std::cerr << "put_int8: " << val << std::endl );

        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int8));
    }
}

void D4StreamMarshaller::put_int16(dods_int16 val)
{
    checksum_update(&val, sizeof(dods_int16));

    if (d_write_data)
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int16));
}

void D4StreamMarshaller::put_int32(dods_int32 val)
{
    checksum_update(&val, sizeof(dods_int32));

    if (d_write_data)
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int32));
}

void D4StreamMarshaller::put_int64(dods_int64 val)
{
    checksum_update(&val, sizeof(dods_int64));

    if (d_write_data)
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int64));
}

void D4StreamMarshaller::put_float32(dods_float32 val)
{
    checksum_update(&val, sizeof(dods_float32));

    if (d_write_data) {
        if (std::numeric_limits<float>::is_iec559 ) {
            DBG2(cerr << "Native rep is ieee754." << endl);
            d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_float32));
        }
        else {
            if (!xdr_setpos(&d_scalar_sink, 0))
                throw InternalErr(__FILE__, __LINE__, "Error serializing a Float32 variable");

            if (!xdr_float(&d_scalar_sink, &val))
                throw InternalErr(__FILE__, __LINE__, "Error serializing a Float32 variable");

            if (xdr_getpos(&d_scalar_sink) != sizeof(dods_float32))
                throw InternalErr(__FILE__, __LINE__, "Error serializing a Float32 variable");

            // If this is a little-endian host, twiddle the bytes
            static bool twiddle_bytes = !is_host_big_endian();
            if (twiddle_bytes) {
                dods_int32 *i = reinterpret_cast<dods_int32*>(&d_ieee754_buf);
                *i = bswap_32(*i);
            }

            d_out.write(d_ieee754_buf, sizeof(dods_float32));
        }
    }
}

void D4StreamMarshaller::put_float64(dods_float64 val)
{
    checksum_update(&val, sizeof(dods_float64));

    if (d_write_data) {
        if (std::numeric_limits<double>::is_iec559)
            d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_float64));
        else {
            if (!xdr_setpos(&d_scalar_sink, 0))
                throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 variable");

            if (!xdr_double(&d_scalar_sink, &val))
                throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 variable");

            if (xdr_getpos(&d_scalar_sink) != sizeof(dods_float64))
                throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 variable");

            // If this is a little-endian host, twiddle the bytes
            static bool twiddle_bytes = !is_host_big_endian();
            if (twiddle_bytes) {
                dods_int64 *i = reinterpret_cast<dods_int64*>(&d_ieee754_buf);
                *i = bswap_64(*i);
            }

            d_out.write(d_ieee754_buf, sizeof(dods_float64));
        }
    }
}

void D4StreamMarshaller::put_uint16(dods_uint16 val)
{
    checksum_update(&val, sizeof(dods_uint16));

    if (d_write_data)
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_uint16));
}

void D4StreamMarshaller::put_uint32(dods_uint32 val)
{
    checksum_update(&val, sizeof(dods_uint32));

    if (d_write_data)
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_uint32));
}

void D4StreamMarshaller::put_uint64(dods_uint64 val)
{
    checksum_update(&val, sizeof(dods_uint64));

    if (d_write_data)
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_uint64));
}


void D4StreamMarshaller::put_str(const string &val)
{
    checksum_update(val.c_str(), val.length());

    if (d_write_data) {
        put_length_prefix(val.length());
        d_out.write(val.data(), val.length());
    }
}

void D4StreamMarshaller::put_url(const string &val)
{
    put_str(val);
}

void D4StreamMarshaller::put_opaque(char *val, unsigned int len)
{
    checksum_update(val, len);

    if (d_write_data) {
        put_length_prefix(len);
        d_out.write(val, len);
    }
}

void D4StreamMarshaller::put_length_prefix(dods_uint64 val)
{
    if (d_write_data) {
        DBG2(cerr << "val: " << val << endl);

        vector<uint8_t> target(sizeof(dods_uint64) + 1, 0);
        uint8_t* to_send = WriteVarint64ToArrayInline(val, &target[0]);
        d_out.write(reinterpret_cast<char*>(&target[0]), to_send - &target[0]);

        DBG2(cerr << "varint: " << hex << *(uint64_t*)&target[0] << dec << endl);
    }
}

void D4StreamMarshaller::put_vector(char *val, unsigned int num)
{
    checksum_update(val, num);

    if (d_write_data)
        d_out.write(val, num);
}

/**
 * Write a vector of values prefixed by the number of elements. This is a
 * special version for vectors of bytes and it calls put_opaque()
 *
 * @note This function writes the number of elements in the vector which,
 * in this case, is equal to the number of bytes
 *
 * @param val Pointer to the data to write
 * @param num The number of elements to write
 */
void D4StreamMarshaller::put_varying_vector(char *val, unsigned int num)
{
    put_opaque(val, num);
}

/**
 * @todo recode this so that it does not copy data to a new buffer but
 * serializes directly to the stream (element by element) and compare the
 * run times.
 */
void D4StreamMarshaller::m_serialize_reals(char *val, unsigned int num, int width, Type type)
{
    dods_uint64 size = num * width;
    char *buf = (char*)malloc(size);
    XDR xdr;
    xdrmem_create(&xdr, buf, size, XDR_ENCODE);
    try {
        if(!xdr_array(&xdr, &val, (unsigned int *)&num, size, width, XDRUtils::xdr_coder(type)))
            throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 array");

        if (xdr_getpos(&xdr) != size)
            throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 array");

        // If this is a little-endian host, twiddle the bytes
        static bool twiddle_bytes = !is_host_big_endian();
        if (twiddle_bytes) {
            if (width == 4) {
                dods_float32 *lbuf = reinterpret_cast<dods_float32*>(buf);
                while (num--) {
                    dods_int32 *i = reinterpret_cast<dods_int32*>(lbuf++);
                    *i = bswap_32(*i);
                }
            }
            else { // width == 8
                dods_float64 *lbuf = reinterpret_cast<dods_float64*>(buf);
                while (num--) {
                    dods_int64 *i = reinterpret_cast<dods_int64*>(lbuf++);
                    *i = bswap_64(*i);
                }
            }
        }

        d_out.write(buf, size);
    }
    catch (...) {
        xdr_destroy(&xdr);
        throw;
    }
    xdr_destroy(&xdr);
}

void D4StreamMarshaller::put_vector(char *val, unsigned int num, int width, Type type)
{
    checksum_update(val, num * width);

    if (d_write_data) {
        if (type == dods_float32_c && !std::numeric_limits<float>::is_iec559) {
            // If not using IEEE 754, use XDR to get it that way.
            m_serialize_reals(val, num, 4, type);
        }
        else if (type == dods_float64_c && !std::numeric_limits<double>::is_iec559) {
            m_serialize_reals(val, num, 8, type);
        }
        else {
            d_out.write(val, num * width);
        }
    }
}

/**
 * Write a vector of values prefixed by the number of elements.
 *
 * @note This function writes the number of elements in the vector, not the
 * number of bytes.
 *
 * @param val Pointer to the data to write
 * @param num The number of elements to write
 * @param width The number of bytes in each element
 * @param type The DAP type code (used only for float32 and float64 values).
 */
void D4StreamMarshaller::put_varying_vector(char *val, unsigned int num, int width, Type type)
{
    put_length_prefix(num);
    put_vector(val, num, width, type);
}

void D4StreamMarshaller::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "D4StreamMarshaller::dump - (" << (void *) this << ")" << endl;
}

} // namespace libdap

