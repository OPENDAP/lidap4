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

#include "config.h"

#include <byteswap.h>
#include <cassert>
#include <cstring>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <limits>

//#define DODS_DEBUG 1

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "D4StreamMarshaller.h"
#ifdef USE_POSIX_THREADS
#include "MarshallerThread.h"
#endif

#if USE_XDR_FOR_IEEE754_ENCODING
#include "XDRUtils.h"
#include "util.h"
#endif

#include "DapIndent.h"
#include "debug.h"

using namespace std;

namespace libdap {

#if 0
// We decided to use int64_t to represent sizes of both arrays and strings,
// So this code is not used. jhrg 10/4/13

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
#endif

#if USE_XDR_FOR_IEEE754_ENCODING
void D4StreamMarshaller::m_serialize_reals(char *val, unsigned int num, int width, Type type)
{
    dods_uint64 size = num * width;

    char *buf = new char[size];
    XDR xdr;
    xdrmem_create(&xdr, &buf[0], size, XDR_ENCODE);
    try {
        if(!xdr_array(&xdr, &val, (unsigned int *)&num, size, width, XDRUtils::xdr_coder(type)))
            throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 array");

        if (xdr_getpos(&xdr) != size)
            throw InternalErr(__FILE__, __LINE__, "Error serializing a Float64 array");

        // If this is a little-endian host, twiddle the bytes
        static bool twiddle_bytes = !is_host_big_endian();
        if (twiddle_bytes) {
            if (width == 4) {
                dods_float32 *lbuf = reinterpret_cast<dods_float32*>(&buf[0]);
                while (num--) {
                    dods_int32 *i = reinterpret_cast<dods_int32*>(lbuf++);
                    *i = bswap_32(*i);
                }
            }
            else { // width == 8
                dods_float64 *lbuf = reinterpret_cast<dods_float64*>(&buf[0]);
                while (num--) {
                    dods_int64 *i = reinterpret_cast<dods_int64*>(lbuf++);
                    *i = bswap_64(*i);
                }
            }
        }
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, size);

        // The child thread will delete buf when it's done
        xdr_destroy(&xdr);
#else
        d_out.write(&buf[0], size);
        xdr_destroy(&xdr);
        delete [] buf;
#endif
    }
    catch (...) {
        xdr_destroy(&xdr);
        delete [] buf;

        throw;
    }
}
#endif

/** Build an instance of D4StreamMarshaller. Bind the C++ stream out to this
 * instance. If the write_data parameter is true, write the data in addition
 * to computing and sending the checksum.
 *
 * @param out Write to this stream object.
 * @param write_data If true, write data values. True by default
 */
D4StreamMarshaller::D4StreamMarshaller(ostream &out, bool write_data) :
        d_out(out), d_write_data(write_data), tm(0)
{
	assert(sizeof(std::streamsize) >= sizeof(int64_t));

#if USE_XDR_FOR_IEEE754_ENCODING
    // XDR is used if the call std::numeric_limits<double>::is_iec559()
    // returns false indicating that the compiler is not using IEEE 754.
    // If it is, we just write out the bytes.
    xdrmem_create(&d_scalar_sink, d_ieee754_buf, sizeof(dods_float64), XDR_ENCODE);
#endif

#ifdef USE_POSIX_THREADS
    tm = new MarshallerThread;
#endif

    // This will cause exceptions to be thrown on i/o errors. The exception
    // will be ostream::failure
    out.exceptions(ostream::failbit | ostream::badbit);
}

D4StreamMarshaller::~D4StreamMarshaller()
{
#if USE_XDR_FOR_IEEE754_ENCODING
    xdr_destroy(&d_scalar_sink);
#endif

    delete tm;
}

/** Initialize the checksum buffer. This resets the checksum calculation.
 */
void D4StreamMarshaller::reset_checksum()
{
    d_checksum.Reset();
}

/**
 * Get the current checksum. It is not possible to continue computing the
 * checksum once this has been called.
 *
 * @note This method is not intended to be called often or for inserting the
 * checksum into an I/O stream; see put_checksum(). This is intended for
 * instrumentation code.
 *
 * @return The checksum in a string object that always has eight characters.
 */
string D4StreamMarshaller::get_checksum()
{
    ostringstream oss;
    oss.setf(ios::hex, ios::basefield);
    oss << setfill('0') << setw(8) << d_checksum.GetCrc32();

    return oss.str();
}

/**
 * @brief Write the checksum
 * Write the checksum for the data sent since the last call to reset_checksum()
 * to the I/O stream associated with this marshaller. Use this to send the
 * checksum, not get_checksum().
 */
void D4StreamMarshaller::put_checksum()
{
    Crc32::checksum chk = d_checksum.GetCrc32();
#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
    d_out.write(reinterpret_cast<char*>(&chk), sizeof(Crc32::checksum));
}

/**
 * Update the current CRC 32 checksum value. Calling this with len equal to
 * zero has no effect on the checksum value.
 */
void D4StreamMarshaller::checksum_update(const void *data, unsigned long len)
{
    d_checksum.AddData(reinterpret_cast<const uint8_t*>(data), len);
}

void D4StreamMarshaller::put_byte(dods_byte val)
{
    checksum_update(&val, sizeof(dods_byte));

    if (d_write_data) {
        DBG( std::cerr << "put_byte: " << val << std::endl );
#ifdef USE_POSIX_THREADS
        // make sure that a child thread is not writing to d_out.
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_byte));
    }
}

void D4StreamMarshaller::put_int8(dods_int8 val)
{
    checksum_update(&val, sizeof(dods_int8));

    if (d_write_data) {
        DBG( std::cerr << "put_int8: " << val << std::endl );
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int8));
    }
}

void D4StreamMarshaller::put_int16(dods_int16 val)
{
    checksum_update(&val, sizeof(dods_int16));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int16));
    }
}

void D4StreamMarshaller::put_int32(dods_int32 val)
{
    checksum_update(&val, sizeof(dods_int32));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_int32));
    }
}

void D4StreamMarshaller::put_int64(dods_int64 val)
{
    checksum_update(&val, sizeof(dods_int64));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<const char*>(&val), sizeof(dods_int64));
    }
}

void D4StreamMarshaller::put_float32(dods_float32 val)
{
#if !USE_XDR_FOR_IEEE754_ENCODING
	assert(std::numeric_limits<float>::is_iec559);

    checksum_update(&val, sizeof(dods_float32));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
    	d_out.write(reinterpret_cast<const char*>(&val), sizeof(dods_float32));
    }

#else
    // This code uses XDR to convert from a local representation to IEEE754;
    // The extra 'twiddle' operation makes the byte-order correct for this
    // host should it not be big-endian. Also note the assert() at the
    // start of the method.

    if (d_write_data) {
        if (std::numeric_limits<float>::is_iec559 ) {
#ifdef USE_POSIX_THREADS
            Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
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
#ifdef USE_POSIX_THREADS
            Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
            d_out.write(d_ieee754_buf, sizeof(dods_float32));
        }
    }
#endif
}

void D4StreamMarshaller::put_float64(dods_float64 val)
{
#if !USE_XDR_FOR_IEEE754_ENCODING
	assert(std::numeric_limits<double>::is_iec559);

    checksum_update(&val, sizeof(dods_float64));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
    	d_out.write(reinterpret_cast<const char*>(&val), sizeof(dods_float64));
    }

#else
    // See the comment above in put_float32()
    if (d_write_data) {
        if (std::numeric_limits<double>::is_iec559) {
#ifdef USE_POSIX_THREADS
            Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
            d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_float64));}
    }
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

#ifdef USE_POSIX_THREADS
            Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
            d_out.write(d_ieee754_buf, sizeof(dods_float64));
        }
    }
#endif
}

void D4StreamMarshaller::put_uint16(dods_uint16 val)
{
    checksum_update(&val, sizeof(dods_uint16));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_uint16));
    }
}

void D4StreamMarshaller::put_uint32(dods_uint32 val)
{
    checksum_update(&val, sizeof(dods_uint32));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_uint32));
    }
}

void D4StreamMarshaller::put_uint64(dods_uint64 val)
{
    checksum_update(&val, sizeof(dods_uint64));

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
        d_out.write(reinterpret_cast<char*>(&val), sizeof(dods_uint64));
    }
}

/**
 * Used only for Sequences, where the count must be added to the stream
 * and then the fields sent using separate calls to methods here. The
 * methods put_opaque_dap4(), ..., that need counts sent as prefixes to
 * their data handle it themselves.
 *
 * @param count How many elements follow.
 */
void D4StreamMarshaller::put_count(int64_t count)
{
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
	d_out.write(reinterpret_cast<const char*>(&count), sizeof(int64_t));
}

void D4StreamMarshaller::put_str(const string &val)
{
    checksum_update(val.c_str(), val.length());

    if (d_write_data) {
    	int64_t len = val.length();
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif
    	d_out.write(reinterpret_cast<const char*>(&len), sizeof(int64_t));
        d_out.write(val.data(), val.length());
    }
}

void D4StreamMarshaller::put_url(const string &val)
{
    put_str(val);
}

void D4StreamMarshaller::put_opaque_dap4(const char *val, int64_t len)
{
    assert(val);
    assert(len >= 0);

    checksum_update(val, len);

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        d_out.write(reinterpret_cast<const char*>(&len), sizeof(int64_t));

        char *byte_buf = new char[len];
        memcpy(byte_buf, val, len);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, byte_buf, len);
#else
        d_out.write(reinterpret_cast<const char*>(&len), sizeof(int64_t));
        d_out.write(val, len);
#endif
    }
}

/**
 * @brief Write a fixed size vector
 * @param val Pointer to the data
 * @param num Number of bytes to write
 */
void D4StreamMarshaller::put_vector(char *val, uint64_t num_bytes)
{
    assert(val);
    //assert(num_bytes >= 0);

    checksum_update(val, num_bytes);

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        char *buf = new char[num_bytes];
        memcpy(buf, val, num_bytes);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, num_bytes);
#else
        d_out.write(val, num_bytes);
#endif
    }
}

void D4StreamMarshaller::put_vector(char *val, uint64_t num_elem, int elem_size)
{
	assert(val);
	//assert(num_elem >= 0);
	assert(elem_size > 0);

	int64_t bytes;

	switch (elem_size) {
	case 1:
		assert(!"Don't call this method for bytes, use put_vector(val, bytes) instead");
		bytes = num_elem;
		break;
	case 2:
		// Don't bother testing the sign bit
		assert(!(num_elem & 0x4000000000000000)); // 0x 40 00 --> 0100 0000
		bytes = num_elem << 1;
		break;
	case 4:
		assert(!(num_elem & 0x6000000000000000)); // 0x 60 00 --> 0110 0000
		bytes = num_elem << 2;
		break;
	case 8:
		assert(!(num_elem & 0x7000000000000000)); // 0111 0000
		bytes = num_elem << 3;
		break;
	default:
		bytes = num_elem * elem_size;
		break;
	}

    checksum_update(val, bytes);

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        char *buf = new char[bytes];
        memcpy(buf, val, bytes);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, bytes);
#else
        d_out.write(val, bytes);
#endif
    }
}

/**
 * @brief Write a fixed size vector
 * @note This method and its companion for float64 exists in case we need to
 * support machine that do not use IEEE754 for their floating point representation.
 * @param val Pointer to the data
 * @param num Number of elements
 * @param width Size of a single element
 * @param type DAP variable type; used to handle float32 and float64 types correctly
 */
void D4StreamMarshaller::put_vector_float32(char *val, uint64_t num_elem)
{
#if !USE_XDR_FOR_IEEE754_ENCODING

	assert(std::numeric_limits<float>::is_iec559);
	assert(val);
	//assert(num_elem >= 0);
	// sizeof() a 32-bit float is 4, so we're going to send 4 * num_elem bytes, so
	// make sure that doesn't overflow a 63-bit integer (the max positive value in
	// a signed int64; use 1110 0000 0.. (0xe000 ...) to mask for non-zero bits
	// to test that num can be multiplied by 4. A
	assert(!(num_elem & 0xe000000000000000));

	num_elem = num_elem << 2;	// num_elem is now the number of bytes

    checksum_update(val, num_elem);

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        char *buf = new char[num_elem];
        memcpy(buf, val, num_elem);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, num_elem);
#else
    	d_out.write(val, num_elem);
#endif
    }

#else
	assert(val);
	assert(num_elem >= 0);
	// sizeof() a 32-bit float is 4, so we're going to send 4 * num_elem bytes, so
	// make sure that doesn't overflow a 63-bit integer (the max positive value in
	// a signed int64; use 1110 0000 0.. (0xe000 ...) to mask for non-zero bits
	// to test that num can be multiplied by 4. A
	assert(!(num_elem & 0xe000000000000000));

	int64_t bytes = num_elem << 2;	// num_elem is now the number of bytes

    checksum_update(val, bytes);

    if (d_write_data) {
        if (!std::numeric_limits<float>::is_iec559) {
            // If not using IEEE 754, use XDR to get it that way.
            m_serialize_reals(val, num_elem, 4, type);
        }
        else {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        char *buf = new char[bytes];
        memcpy(buf, val, bytes);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, bytes);
#else
        d_out.write(val, bytes);
#endif
        }
    }
#endif
}

/**
 * @brief Write a fixed size vector of float64s
 *
 * @param val Pointer to the data
 * @param num Number of elements
 * @param width Size of a single element
 * @param type DAP variable type; used to handle float32 and float64 types correctly
 */
void D4StreamMarshaller::put_vector_float64(char *val, uint64_t num_elem)
{
#if !USE_XDR_FOR_IEEE754_ENCODING

	assert(std::numeric_limits<double>::is_iec559);
	assert(val);
	//assert(num_elem >= 0);
	// See comment above
	assert(!(num_elem & 0xf000000000000000));

	num_elem = num_elem << 3;	// num_elem is now the number of bytes

    checksum_update(val, num_elem);

    if (d_write_data) {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        char *buf = new char[num_elem];
        memcpy(buf, val, num_elem);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, num_elem);
#else
        d_out.write(val, num_elem);
#endif
    }
#else
	assert(val);
	assert(num_elem >= 0);
	// sizeof() a 32-bit float is 4, so we're going to send 4 * num_elem bytes, so
	// make sure that doesn't overflow a 63-bit integer (the max positive value in
	// a signed int64; use 1110 0000 0.. (0xe000 ...) to mask for non-zero bits
	// to test that num can be multiplied by 4. A
	assert(!(num_elem & 0xe000000000000000));

	int64_t bytes = num_elem << 3;	// num_elem is now the number of bytes

    checksum_update(val, bytes);

    if (d_write_data) {
        if (!std::numeric_limits<double>::is_iec559) {
            // If not using IEEE 754, use XDR to get it that way.
            m_serialize_reals(val, num_elem, 8, type);
        }
        else {
#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());

        char *buf = new char[bytes];
        memcpy(buf, val, bytes);

        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, buf, bytes);
#else
        d_out.write(val, bytes);
#endif
        }
    }
#endif

}

void D4StreamMarshaller::put_vector_part(char *val, uint64_t num, int width, Type type)
{
    assert(val);
    //assert(num >= 0);
    assert(width > 0);

    switch(type) {
    case dods_byte_c:
    case dods_char_c:
    case dods_int8_c:
    case dods_uint8_c:
        put_vector(val, num);
        break;

    case dods_int16_c:
    case dods_uint16_c:
    case dods_int32_c:
    case dods_uint32_c:
    case dods_int64_c:
    case dods_uint64_c:
        put_vector(val, num, width);
        break;

    case dods_enum_c:
        if (width == 1)
            put_vector(val, num);
        else
            put_vector(val, num, width);
        break;

    case dods_float32_c:
        put_vector_float32(val, num);
        break;

    case dods_float64_c:
        put_vector_float32(val, num);
        break;

    case dods_str_c:
    case dods_url_c:
        throw InternalErr(__FILE__, __LINE__, "Array of String should not be passed to put_vector.");

    case dods_array_c:
        throw InternalErr(__FILE__, __LINE__, "Array of Array not allowed.");

    case dods_opaque_c:
    case dods_structure_c:
    case dods_sequence_c:
        throw InternalErr(__FILE__, __LINE__, "Array of String should not be passed to put_vector.");

    case dods_grid_c:
        throw InternalErr(__FILE__, __LINE__, "Grid is not part of DAP4.");

    default:
        throw InternalErr(__FILE__, __LINE__, "Unknown datatype.");
    }
}

void D4StreamMarshaller::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "D4StreamMarshaller::dump - (" << (void *) this << ")" << endl;
}

} // namespace libdap

