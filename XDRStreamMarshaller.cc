// XDRStreamMarshaller.cc

// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002,2003,2016 OPeNDAP, Inc.
// Author: Patrick West <pwest@ucar.edu>
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

// (c) COPYRIGHT URI/MIT 1994-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      pwest       Patrick West <pwest@ucar.edu>


#include "config.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <cassert>

#include <iostream>
#include <sstream>
#include <iomanip>

// #define DODS_DEBUG

#include "XDRStreamMarshaller.h"
#ifdef USE_POSIX_THREADS
#include "MarshallerThread.h"
#endif
#include "Vector.h"
#include "XDRUtils.h"
#include "util.h"
#include "dods-limits.h"

#include "debug.h"
#include "DapIndent.h"

using namespace std;

// Build this code so it does not use pthreads to write some kinds of
// data (see the put_vector() and put_vector_part() methods) in a child thread.
// #undef USE_POSIX_THREADS

namespace libdap {

char *XDRStreamMarshaller::d_buf = 0;
static const int XDR_DAP_BUFF_SIZE=256;


/** Build an instance of XDRStreamMarshaller. Bind the C++ stream out to this
 * instance. If the checksum parameter is true, initialize a checksum buffer
 * and enable the use of the reset_checksum() and get_checksum() methods.
 *
 * @param out Write to this stream object.
 * @param checksum If true, compute checksums. False by default
 * @param write_data If true, write data values. True by default
 */
XDRStreamMarshaller::XDRStreamMarshaller(ostream &out) :
    d_out(out), d_partial_put_byte_count(0), tm(0)
{
    if (!d_buf) d_buf = (char *) malloc(XDR_DAP_BUFF_SIZE);
    if (!d_buf) throw Error(internal_error, "Failed to allocate memory for data serialization.");

    xdrmem_create(&d_sink, d_buf, XDR_DAP_BUFF_SIZE, XDR_ENCODE);

#ifdef USE_POSIX_THREADS
    tm = new MarshallerThread;
#endif
}

XDRStreamMarshaller::~XDRStreamMarshaller()
{
    // Added this because when USE_POS... is not defined, 'tm' has no
    // type, which the compiler complains about.
#ifdef USE_POSIX_THREADS
    delete tm;
#endif
    xdr_destroy(&d_sink);
}

void XDRStreamMarshaller::put_byte(dods_byte val)
{
     if (!xdr_setpos(&d_sink, 0))
        throw Error("Network I/O Error. Could not send byte data - unable to set stream position.");

    if (!xdr_char(&d_sink, (char *) &val))
        throw Error(
            "Network I/O Error. Could not send byte data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send byte data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_int16(dods_int16 val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send int 16 data - unable to set stream position.");

    if (!XDR_INT16(&d_sink, &val))
        throw Error(
            "Network I/O Error. Could not send int 16 data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send int 16 data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_int32(dods_int32 val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send int 32 data - unable to set stream position.");

    if (!XDR_INT32(&d_sink, &val))
        throw Error(
            "Network I/O Error. Culd not read int 32 data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send int 32 data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_float32(dods_float32 val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send float 32 data - unable to set stream position.");

    if (!xdr_float(&d_sink, &val))
        throw Error(
            "Network I/O Error. Could not send float 32 data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send float 32 data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_float64(dods_float64 val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send float 64 data - unable to set stream position.");

    if (!xdr_double(&d_sink, &val))
        throw Error(
            "Network I/O Error. Could not send float 64 data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send float 64 data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_uint16(dods_uint16 val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send uint 16 data - unable to set stream position.");

    if (!XDR_UINT16(&d_sink, &val))
        throw Error(
            "Network I/O Error. Could not send uint 16 data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send uint 16 data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_uint32(dods_uint32 val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send uint 32 data - unable to set stream position.");

    if (!XDR_UINT32(&d_sink, &val))
        throw Error(
            "Network I/O Error. Could not send uint 32 data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send uint 32 data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_str(const string &val)
{
    int size = val.length() + 8;

    XDR str_sink;
    vector<char> str_buf(size);

    try {
        xdrmem_create(&str_sink, &str_buf[0], size, XDR_ENCODE);

        if (!xdr_setpos(&str_sink, 0))
            throw Error(
                "Network I/O Error. Could not send string data - unable to set stream position.");

        const char *out_tmp = val.c_str();
        if (!xdr_string(&str_sink, (char **) &out_tmp, size))
            throw Error(
                "Network I/O Error. Could not send string data.");

        unsigned int bytes_written = xdr_getpos(&str_sink);
        if (!bytes_written)
            throw Error(
                "Network I/O Error. Could not send string data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

        d_out.write(&str_buf[0], bytes_written);

        xdr_destroy(&str_sink);
    }
    catch (...) {
        xdr_destroy(&str_sink);
        throw;
    }
}

void XDRStreamMarshaller::put_url(const string &val)
{
    put_str(val);
}

void XDRStreamMarshaller::put_opaque(char *val, unsigned int len)
{
    if (len > XDR_DAP_BUFF_SIZE)
        throw Error("Network I/O Error. Could not send opaque data - length of opaque data larger than allowed");

    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send opaque data - unable to set stream position.");

    if (!xdr_opaque(&d_sink, val, len))
        throw Error(
            "Network I/O Error. Could not send opaque data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send opaque data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_int(int val)
{
    if (!xdr_setpos(&d_sink, 0))
        throw Error(
            "Network I/O Error. Could not send int data - unable to set stream position.");

    if (!xdr_int(&d_sink, &val))
        throw Error(
            "Network I/O Error(1). Could not send int data.");

    unsigned int bytes_written = xdr_getpos(&d_sink);
    if (!bytes_written)
        throw Error(
            "Network I/O Error. Could not send int data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    d_out.write(d_buf, bytes_written);
}

void XDRStreamMarshaller::put_vector(char *val, uint64_t num, int width, Vector &vec)
{
    put_vector(val, num, width, vec.var()->type());
}


/**
 * Prepare to send a single array/vector using a series of 'put' calls.
 *
 * @param num The number of elements in the Array/Vector
 * @see put_vector_part()
 * @see put_vector_end()
 */
void XDRStreamMarshaller::put_vector_start(uint64_t num)
{
    // Make sure we only do this for vectors less than DODS_INT_MAX size.
    if (num > DODS_INT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "put_vector: num greater than DODS_INT_MAX.");
    }
    auto vec_num = static_cast<int>(num);

    put_int(vec_num);
    put_int(vec_num);

    d_partial_put_byte_count = 0;
}

/**
 * Close a vector when its values are written using put_vector_part().
 *
 * @see put_vector_start()
 * @see put_vector_part()
 */
void XDRStreamMarshaller::put_vector_end()
{
#ifdef USE_POSIX_THREADS
    Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
#endif

    // Compute the trailing (padding) bytes

    // Note that the XDR standard pads values to 4 byte boundaries.
    //unsigned int pad = (d_partial_put_byte_count % 4) == 0 ? 0: 4 - (d_partial_put_byte_count % 4);
    unsigned int mod_4 = d_partial_put_byte_count & 0x03;
    unsigned int pad = (mod_4 == 0) ? 0: 4 - mod_4;

    if (pad) {
        vector<char> padding(4, 0); // 4 zeros

        d_out.write(&padding[0], pad);
        if (d_out.fail()) throw Error("Network I/O Error. Could not send vector data padding");
    }
}

// Start of parallel I/O support. jhrg 8/19/15
void XDRStreamMarshaller::put_vector(char *val, uint64_t num, Vector &)
{
    if (!val) throw InternalErr(__FILE__, __LINE__, "Could not send byte vector data. Buffer pointer is not set.");

    // Make sure we only do this for vectors less than DODS_INT_MAX size.
    if (num > DODS_INT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "put_vector: num greater than DODS_INT_MAX.");
    }
    unsigned int vec_num = static_cast<unsigned int>(num);
    // write the number of members of the array being written and then set the position to 0
    put_int(vec_num);

    // this is the word boundary for writing xdr bytes in a vector.
    const unsigned int add_to = 8;
    // switch to memory on the heap since the thread will need to access it
    // after this code returns.
    char *byte_buf = new char[vec_num + add_to];
    XDR byte_sink;
    try {
        xdrmem_create(&byte_sink, byte_buf, vec_num + add_to, XDR_ENCODE);
        if (!xdr_setpos(&byte_sink, 0))
            throw Error("Network I/O Error. Could not send byte vector data - unable to set stream position.");

        if (!xdr_bytes(&byte_sink, (char **) &val, (unsigned int *) &vec_num, vec_num + add_to))
            throw Error("Network I/O Error(2). Could not send byte vector data - unable to encode data.");

        unsigned int bytes_written = xdr_getpos(&byte_sink);
        if (!bytes_written)
            throw Error("Network I/O Error. Could not send byte vector data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, byte_buf, bytes_written);
        xdr_destroy(&byte_sink);
#else
        d_out.write(byte_buf, bytes_written);
        xdr_destroy(&byte_sink);
        delete [] byte_buf;
#endif

    }
    catch (...) {
        DBG(cerr << "Caught an exception in put_vector_thread" << endl);
        xdr_destroy(&byte_sink);
        delete [] byte_buf;
        throw;
    }
}

// private
/**
 * Write elements of a Vector (i.e. an Array) to the stream using XDR encoding.
 * Encoding is performed on 'num' values that use 'width' bytes. The parameter
 * 'type' is used to choose the XDR encoding function.
 *
 * @param val Pointer to the values to write
 * @param num The number of elements in the memory referenced by 'val'
 * @param width The number of bytes in each element
 * @param type The DAP type of the elements
 */
void XDRStreamMarshaller::put_vector(char *val, uint64_t num, int width, Type type)
{
    assert(val || num == 0);

    // Make sure we only do this for vectors less than DODS_INT_MAX size.
    if (num > DODS_INT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "put_vector: num greater than DODS_INT_MAX.");
    }
    unsigned int vec_num = static_cast<unsigned int>(num);
    // write the number of array members being written, then set the position back to 0
    put_int(vec_num);

    if (num == 0)
        return;

    int use_width = width;
    if (use_width < 4) use_width = 4;

    // the size is the number of elements num times the width of each
    // element, then add 4 bytes for the number of elements
    int size = (vec_num * use_width) + 4;

    // allocate enough memory for the elements
    //vector<char> vec_buf(size);
    char *vec_buf = new char[size];
    XDR vec_sink;
    try {
        xdrmem_create(&vec_sink, vec_buf, size, XDR_ENCODE);

        // set the position of the sink to 0, we're starting at the beginning
        if (!xdr_setpos(&vec_sink, 0))
            throw Error("Network I/O Error. Could not send vector data - unable to set stream position.");

        // write the array to the buffer
        if (!xdr_array(&vec_sink, (char **) &val, (unsigned int *) &vec_num, size, width, XDRUtils::xdr_coder(type)))
            throw Error("Network I/O Error(2). Could not send vector data - unable to encode.");

        // how much was written to the buffer
        unsigned int bytes_written = xdr_getpos(&vec_sink);
        if (!bytes_written)
            throw Error("Network I/O Error. Could not send vector data - unable to get stream position.");

#ifdef USE_POSIX_THREADS
        Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
        tm->increment_child_thread_count();
        tm->start_thread(MarshallerThread::write_thread, d_out, vec_buf, bytes_written);
        xdr_destroy(&vec_sink);
#else
        d_out.write(vec_buf, bytes_written);
        xdr_destroy(&vec_sink);
        delete [] vec_buf;
#endif
    }
    catch (...) {
        xdr_destroy(&vec_sink);
        delete [] vec_buf;
        throw;
    }
}

/**
 * Write num values for an Array/Vector.
 *
 * @param val The values to write
 * @param num the number of values to write
 * @param width The width of the values
 * @param type The DAP2 type of the values.
 *
 * @see put_vector_start()
 * @see put_vector_end()
 */
void XDRStreamMarshaller::put_vector_part(char *val, uint64_t num, int width, Type type)
{
    // Make sure we only do this for vectors less than DODS_INT_MAX size.
    if (num > DODS_INT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "put_vector: num greater than DODS_INT_MAX.");
    }
    unsigned int vec_num = static_cast<unsigned int>(num);

    if (width == 1) {
        // Add space for the 4 bytes of length info and 4 bytes for padding, even though
        // we will not send either of those.
        const unsigned int add_to = 8;
        unsigned int bufsiz = vec_num + add_to;
        //vector<char> byte_buf(bufsiz);
        char *byte_buf = new char[bufsiz];
        XDR byte_sink;
        try {
            xdrmem_create(&byte_sink, byte_buf, bufsiz, XDR_ENCODE);
            if (!xdr_setpos(&byte_sink, 0))
                throw Error("Network I/O Error. Could not send byte vector data - unable to set stream position.");

            if (!xdr_bytes(&byte_sink, (char **) &val, (unsigned int *) &vec_num, bufsiz))
                throw Error("Network I/O Error(2). Could not send byte vector data - unable to encode data.");

#ifdef USE_POSIX_THREADS
            Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
            tm->increment_child_thread_count();

            // Increment the element count so we can figure out about the padding in put_vector_last()
            d_partial_put_byte_count += vec_num;

            tm->start_thread(MarshallerThread::write_thread_part, d_out, byte_buf, vec_num);
            xdr_destroy(&byte_sink);
#else
            // Only send the num bytes that follow the 4 bytes of length info - we skip the
            // length info because it's already been sent and we don't send any trailing padding
            // bytes in this method (see put_vector_last() for that).
            d_out.write(byte_buf + 4, num);

            if (d_out.fail())
            throw Error ("Network I/O Error. Could not send initial part of byte vector data");

            // Now increment the element count so we can figure out about the padding in put_vector_last()
            d_partial_put_byte_count += num;

            xdr_destroy(&byte_sink);
            delete [] byte_buf;
#endif
        }
        catch (...) {
            xdr_destroy(&byte_sink);
            delete [] byte_buf;
            throw;
        }
    }
    else {
        int use_width = (width < 4) ? 4 : width;

        // the size is the number of elements num times the width of each
        // element, then add 4 bytes for the (int) number of elements
        int size = (vec_num * use_width) + 4;

        // allocate enough memory for the elements
        //vector<char> vec_buf(size);
        char *vec_buf = new char[size];
        XDR vec_sink;
        try {
            xdrmem_create(&vec_sink, vec_buf, size, XDR_ENCODE);

            // set the position of the sink to 0, we're starting at the beginning
            if (!xdr_setpos(&vec_sink, 0))
                throw Error("Network I/O Error. Could not send vector data - unable to set stream position.");

            // write the array to the buffer
            if (!xdr_array(&vec_sink, (char **) &val, (unsigned int *) &vec_num, size, width, XDRUtils::xdr_coder(type)))
                throw Error("Network I/O Error(2). Could not send vector data -unable to encode data.");

#ifdef USE_POSIX_THREADS
            Locker lock(tm->get_mutex(), tm->get_cond(), tm->get_child_thread_count());
            tm->increment_child_thread_count();

            // Increment the element count so we can figure out about the padding in put_vector_last()
            d_partial_put_byte_count += (size - 4);
            tm->start_thread(MarshallerThread::write_thread_part, d_out, vec_buf, size - 4);
            xdr_destroy(&vec_sink);
#else
            // write that much out to the output stream, skipping the length data that
            // XDR writes since we have already written the length info using put_vector_start()
            d_out.write(vec_buf + 4, size - 4);

            if (d_out.fail())
                throw Error ("Network I/O Error. Could not send part of vector data");

            // Now increment the element count so we can figure out about the padding in put_vector_last()
            d_partial_put_byte_count += (size - 4);

            xdr_destroy(&vec_sink);
            delete [] vec_buf;
#endif
        }
        catch (...) {
            xdr_destroy(&vec_sink);
            delete [] vec_buf;
            throw;
        }
    }
}

void XDRStreamMarshaller::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "XDRStreamMarshaller::dump - (" << (void *) this << ")" << endl;
}

} // namespace libdap

