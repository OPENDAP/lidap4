// XDRFileMarshaller.cc

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

#include "XDRFileMarshaller.h"

#include "Byte.h"
#include "Int16.h"
#include "UInt16.h"
#include "Int32.h"
#include "UInt32.h"
#include "Float32.h"
#include "Float64.h"
#include "Str.h"
#include "Url.h"
#include "Array.h"
#include "Structure.h"
#include "Sequence.h"
#include "Grid.h"

#include "util.h"
#include "InternalErr.h"
#include "DapIndent.h"

namespace libdap {

XDRFileMarshaller::XDRFileMarshaller(FILE *out) :
    _sink(0)//, d_out(out)
{
    _sink = new_xdrstdio(out, XDR_ENCODE);
}

XDRFileMarshaller::XDRFileMarshaller() :
    Marshaller(), _sink(0)//, d_out(0)
{
    throw InternalErr( __FILE__, __LINE__, "Default constructor not implemented.");
}

XDRFileMarshaller::XDRFileMarshaller(const XDRFileMarshaller &m) :
    Marshaller(m), _sink(0)//, d_out(0)
{
    throw InternalErr( __FILE__, __LINE__, "Copy constructor not implemented.");
}

XDRFileMarshaller &
XDRFileMarshaller::operator=(const XDRFileMarshaller &)
{
    throw InternalErr( __FILE__, __LINE__, "Copy operator not implemented.");
}

XDRFileMarshaller::~XDRFileMarshaller()
{
    delete_xdrstdio(_sink);
}

void XDRFileMarshaller::put_byte(dods_byte val)
{
    if (!xdr_char(_sink, (char *) &val))
        throw Error(
            "Network I/O Error. Could not send byte data.\nThis may be due to a bug in DODS, on the server or a\nproblem with the network connection.");
}

void XDRFileMarshaller::put_int16(dods_int16 val)
{
    if (!XDR_INT16(_sink, &val))
        throw Error(
            "Network I/O Error. Could not send int 16 data.\nThis may be due to a bug in libdap, on the server or a\nproblem with the network connection.");
}

void XDRFileMarshaller::put_int32(dods_int32 val)
{
    if (!XDR_INT32(_sink, &val))
        throw Error(
            "Network I/O Error. Could not read int 32 data.\nThis may be due to a bug in libdap, on the server or a\nproblem with the network connection.");
}

void XDRFileMarshaller::put_float32(dods_float32 val)
{
    if (!xdr_float(_sink, &val))
        throw Error(
            "Network I/O Error. Could not send float 32 data.\nThis may be due to a bug in libdap, on the server or a\nproblem with the network connection.");
}

void XDRFileMarshaller::put_float64(dods_float64 val)
{
    if (!xdr_double(_sink, &val))
        throw Error(
            "Network I/O Error. Could not send float 64 data.\nThis may be due to a bug in libdap, on the server or a\nproblem with the network connection.");
}

void XDRFileMarshaller::put_uint16(dods_uint16 val)
{
    if (!XDR_UINT16(_sink, &val))
        throw Error("Network I/O Error. Could not send uint 16 data.");
}

void XDRFileMarshaller::put_uint32(dods_uint32 val)
{
    if (!XDR_UINT32(_sink, &val))
        throw Error("Network I/O Error. Could not send uint 32 data.");
}

void XDRFileMarshaller::put_str(const string &val)
{
    const char *out_tmp = val.c_str();

    if (!xdr_string(_sink, (char **) &out_tmp, max_str_len))
        throw Error("Network I/O Error. Could not send string data.");
}

void XDRFileMarshaller::put_url(const string &val)
{
    put_str(val);
}

void XDRFileMarshaller::put_opaque(char *val, unsigned int len)
{
    if (!xdr_opaque(_sink, val, len))
        throw Error("Network I/O Error. Could not send opaque data.");
}

void XDRFileMarshaller::put_int(int val)
{
    if (!xdr_int(_sink, &val))
        throw Error("Network I/O Error(1).");
}

void XDRFileMarshaller::put_vector(char *val, uint64_t num, Vector &)
{
    if (!val) throw InternalErr(__FILE__, __LINE__, "Buffer pointer is not set.");

    // Make sure we only do this for vectors less than DODS_INT_MAX size.
    if (num > DODS_INT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "put_vector: num greater than DODS_INT_MAX.");
    }
    unsigned int vec_num = static_cast<unsigned int>(num);
    put_int(vec_num);

    if (!xdr_bytes(_sink, (char **) &val, (unsigned int *) &vec_num, DODS_MAX_ARRAY)) {
        throw Error("Network I/O Error(2).");
    }
}

void XDRFileMarshaller::put_vector(char *val, uint64_t num, int width, Vector &vec)
{
    if (!val) throw InternalErr(__FILE__, __LINE__, "Buffer pointer is not set.");

    // Make sure we only do this for vectors less than DODS_INT_MAX size.
    if (num > DODS_INT_MAX) {
        throw InternalErr(__FILE__, __LINE__, "put_vector: num greater than DODS_INT_MAX.");
    }
    unsigned int vec_num = static_cast<unsigned int>(num);
    put_int(vec_num);

    BaseType *var = vec.var();
    if (!xdr_array(_sink, (char **) &val, (unsigned int *) &vec_num, DODS_MAX_ARRAY, width,
        XDRUtils::xdr_coder(var->type()))) {
        throw Error("Network I/O Error(2).");
    }
}

void XDRFileMarshaller::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "XDRFileMarshaller::dump - (" << (void *) this << ")" << endl;
}

} // namespace libdap

