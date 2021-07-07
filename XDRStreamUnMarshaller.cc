// XDRStreamUnMarshaller.cc

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
#include "config.h"
#include "XDRStreamUnMarshaller.h"

#include <cstring> // for memcpy
#include <string>
#include <sstream>

//#define DODS_DEBUG2 1
//#define DODS_DEBUG 1

#include "Str.h"
#include "Array.h"
#include "util.h"
#include "InternalErr.h"
#include "debug.h"
#include "DapIndent.h"

namespace libdap {

char *XDRStreamUnMarshaller::d_buf = 0;

XDRStreamUnMarshaller::XDRStreamUnMarshaller(istream &in) : /*&d_source( 0 ),*/
        d_in(in)
{
    if (!d_buf)
        d_buf = (char *) malloc(XDR_DAP_BUFF_SIZE);
    if (!d_buf)
        throw Error(internal_error, "Failed to allocate memory for data serialization.");

    //&d_source = new XDR;
    xdrmem_create(&d_source, d_buf, XDR_DAP_BUFF_SIZE, XDR_DECODE);
}

XDRStreamUnMarshaller::XDRStreamUnMarshaller() :
        UnMarshaller(), /*&d_source( 0 ),*/d_in(cin)
{
    throw InternalErr(__FILE__, __LINE__, "Default constructor not implemented.");
}

XDRStreamUnMarshaller::XDRStreamUnMarshaller(const XDRStreamUnMarshaller &um) :
        UnMarshaller(um), /*&d_source( 0 ),*/d_in(cin)
{
    throw InternalErr(__FILE__, __LINE__, "Copy constructor not implemented.");
}

XDRStreamUnMarshaller &
XDRStreamUnMarshaller::operator=(const XDRStreamUnMarshaller &)
{
    throw InternalErr(__FILE__, __LINE__, "Copy operator not implemented.");
}

XDRStreamUnMarshaller::~XDRStreamUnMarshaller()
{
    xdr_destroy( &d_source );
    //&d_source = 0;
}

void XDRStreamUnMarshaller::get_byte(dods_byte &val)
{
    if (xdr_setpos( &d_source, 0 ) < 0)
        throw Error("Failed to reposition input stream");
    if (!(d_in.read(d_buf, 4))) {
        if (d_in.eof())
            throw Error("Premature EOF in input stream");
        else {
            ostringstream ss("Error reading from input stream: ");
            ss << d_in.rdstate();
            throw Error(ss.str());
        }
    }

    DBG2( std::cerr << "_in.gcount(): " << d_in.gcount() << std::endl );
    DBG2( std::cerr << "_in.tellg(): " << d_in.tellg() << std::endl );
    DBG2( std::cerr << "_buf[0]: " << hex << d_buf[0] << "; _buf[1]: " << d_buf[1]
            << "; _buf[2]: " << d_buf[2] << "; _buf[3]: " << d_buf[3]
            << dec << std::endl );

    if (!xdr_char(&d_source, (char *) &val))
        throw Error("Network I/O Error. Could not read byte data.");

    DBG2(std::cerr << "get_byte: " << val << std::endl );
}

void XDRStreamUnMarshaller::get_int16(dods_int16 &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 4);

    if (!XDR_INT16(&d_source, &val))
        throw Error("Network I/O Error. Could not read int 16 data.");
}

void XDRStreamUnMarshaller::get_int32(dods_int32 &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 4);

    if (!XDR_INT32(&d_source, &val))
        throw Error("Network I/O Error. Could not read int 32 data.");
}

void XDRStreamUnMarshaller::get_float32(dods_float32 &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 4);

    if (!xdr_float(&d_source, &val))
        throw Error("Network I/O Error. Could not read float 32 data.");
}

void XDRStreamUnMarshaller::get_float64(dods_float64 &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 8);

    if (!xdr_double(&d_source, &val))
        throw Error("Network I/O Error. Could not read float 64 data.");
}

void XDRStreamUnMarshaller::get_uint16(dods_uint16 &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 4);

    if (!XDR_UINT16(&d_source, &val))
        throw Error("Network I/O Error. Could not read uint 16 data.");
}

void XDRStreamUnMarshaller::get_uint32(dods_uint32 &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 4);

    if (!XDR_UINT32(&d_source, &val))
        throw Error("Network I/O Error. Could not read uint 32 data.");
}

void XDRStreamUnMarshaller::get_str(string &val)
{
    int i;
    get_int(i);
    DBG(std::cerr << "i: " << i << std::endl);

    // Must round up string size to next 4
    i = ((i + 3) / 4) * 4;
    DBG(std::cerr << "i: " << i << std::endl);

    char *in_tmp = 0;
    //char *buf = 0;
    //XDR *source = 0;
    // Must address the case where the string is larger than the buffer
    if (i + 4 > XDR_DAP_BUFF_SIZE) {
#if 0
        char *buf = (char *) malloc(i + 4);
        if (!buf)
            throw InternalErr(__FILE__, __LINE__, "Error allocating memory");
#endif
        vector<char> buf(i+4);

        XDR source;// = new XDR;
        xdrmem_create(&source, &buf[0], i + 4, XDR_DECODE);
        memcpy(&buf[0], d_buf, 4);

        d_in.read(&buf[0] + 4, i);

        xdr_setpos( &source, 0);
        if (!xdr_string( &source, &in_tmp, max_str_len)) {
            xdr_destroy( &source );
            throw Error("Network I/O Error. Could not read string data.");
        }

        xdr_destroy( &source );
    }
    else {
        d_in.read(d_buf + 4, i);

        xdr_setpos( &d_source, 0);
        if (!xdr_string(&d_source, &in_tmp, max_str_len))
            throw Error("Network I/O Error. Could not read string data.");
    }

    val = in_tmp;

    free(in_tmp);
}

void XDRStreamUnMarshaller::get_url(string &val)
{
    get_str(val);
}

void XDRStreamUnMarshaller::get_opaque(char *val, unsigned int len)
{
    xdr_setpos( &d_source, 0);

    // Round len up to the next multiple of 4. There is also the RNDUP()
    // macro in xdr.h, at least on OS/X.
    len += len & 3;
    if (static_cast<int>(len) > XDR_DAP_BUFF_SIZE)
        throw Error("Network I/O Error. Length of opaque data larger than allowed");

    d_in.read(d_buf, len);

    xdr_opaque(&d_source, val, len);
}

void XDRStreamUnMarshaller::get_int(int &val)
{
    xdr_setpos( &d_source, 0);
    d_in.read(d_buf, 4);

    if (!xdr_int(&d_source, &val))
        throw Error("Network I/O Error(1).");

    DBG(std::cerr << "get_int: " << val << std::endl);
}

void XDRStreamUnMarshaller::get_vector(char **val, uint64_t &num, Vector &)
{
    int i;
    get_int(i); // This leaves the XDR encoded value in d_buf; used later
    DBG(std::cerr << "i: " << i << std::endl);

    // Must round up string size to next 4
    i += i & 3;
    DBG(std::cerr << "i: " << i << std::endl);

    // Make sure we only do this uint32 size vectors.
    if (num > DODS_UINT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "get_vector: number of elements exceeds DODS_UINT_MAX.");
    }
    auto vec_num = static_cast<unsigned int>(num);
    //char *buf = 0;
    //XDR *source = 0;
    // Must address the case where the string is larger than the buffer
    if (i + 4 > XDR_DAP_BUFF_SIZE) {
    	vector<char> buf(i+4);
        XDR source;
        xdrmem_create(&source, &buf[0], i + 4, XDR_DECODE);
        memcpy(&buf[0], d_buf, 4);

        d_in.read(&buf[0] + 4, i);
        DBG2(cerr << "bytes read: " << d_in.gcount() << endl);

        xdr_setpos(&source, 0);
        if (!xdr_bytes(&d_source, val, &vec_num, DODS_MAX_ARRAY)) {
            xdr_destroy(&source);
            throw Error("Network I/O Error. Could not read byte array data.");
        }

        xdr_destroy( &source );
    }
    else {
        d_in.read(d_buf + 4, i);
        DBG2(cerr << "bytes read: " << d_in.gcount() << endl);

        xdr_setpos(&d_source, 0);
        if (!xdr_bytes(&d_source, val, &vec_num, DODS_MAX_ARRAY))
            throw Error("Network I/O Error. Could not read byte array data.");
    }
}

void XDRStreamUnMarshaller::get_vector(char **val, uint64_t &num, int width, Vector &vec)
{
    get_vector(val, num, width, vec.var()->type());
}

void XDRStreamUnMarshaller::get_vector(char **val, uint64_t &num, int width, Type type)
{
    int i;
    get_int(i); // This leaves the XDR encoded value in d_buf; used later
    DBG(std::cerr << "i: " << i << std::endl);

    width += width & 3;
    DBG(std::cerr << "width: " << width << std::endl);

    int size = i * width; // + 4; // '+ 4' to hold the int already read

    // Make sure we only do this uint32 size vectors.
    if (num > DODS_UINT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "get_vector: number of elements exceeds DODS_UINT_MAX.");
    }
    auto vec_num = static_cast<unsigned int>(num);

    // Must address the case where the string is larger than the buffer
    if (size > XDR_DAP_BUFF_SIZE) {
    	vector<char> buf(size+4);
        XDR source;
        xdrmem_create(&source, &buf[0], size + 4, XDR_DECODE);
        DBG(cerr << "size: " << size << endl);
        memcpy(&buf[0], d_buf, 4);

        d_in.read(&buf[0] + 4, size); // +4 for the int already read
        DBG(cerr << "bytes read: " << d_in.gcount() << endl);

        xdr_setpos(&source, 0);
        if (!xdr_array(&source, val, &vec_num, DODS_MAX_ARRAY, width, XDRUtils::xdr_coder(type))) {
            xdr_destroy( &source );
            throw Error("Network I/O Error. Could not read array data.");
        }

        xdr_destroy( &source );
    }
    else {
        d_in.read(d_buf + 4, size);
        DBG(cerr << "bytes read (2): " << d_in.gcount() << endl);

        xdr_setpos( &d_source, 0);
        if (!xdr_array(&d_source, val, &vec_num, DODS_MAX_ARRAY, width, XDRUtils::xdr_coder(type)))
            throw Error("Network I/O Error. Could not read array data.");
    }
}

void XDRStreamUnMarshaller::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "XDRStreamUnMarshaller::dump - (" << (void *) this << ")" << endl;
}

} // namespace libdap

