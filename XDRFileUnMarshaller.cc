// XDRFileUnMarshaller.cc

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

#include "XDRFileUnMarshaller.h"

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

XDRFileUnMarshaller::XDRFileUnMarshaller( FILE *out )
    : _source( 0 )
{
    _source = new_xdrstdio( out, XDR_DECODE ) ;
}

XDRFileUnMarshaller::XDRFileUnMarshaller()
    : UnMarshaller(), _source( 0 )
{
    throw InternalErr( __FILE__, __LINE__, "Default constructor not implemented." ) ;
}

XDRFileUnMarshaller::XDRFileUnMarshaller( const XDRFileUnMarshaller &um )
    : UnMarshaller( um ), _source( 0 )
{
    throw InternalErr( __FILE__, __LINE__, "Copy constructor not implemented." ) ;
}

XDRFileUnMarshaller &
XDRFileUnMarshaller::operator=( const XDRFileUnMarshaller & )
{
    throw InternalErr( __FILE__, __LINE__, "Copy operator not implemented." ) ;
}

XDRFileUnMarshaller::~XDRFileUnMarshaller( )
{
    // Some static code analysis tools complain that delete_xdrstdio
    // does not close the FILE* it holds, but that's not true with
    // modern XDR libraries. Don't try to close that FILE*. jhrg 8/27/13

    delete_xdrstdio( _source ) ;
}

void
XDRFileUnMarshaller::get_byte( dods_byte &val )
{
    if( !xdr_char( _source, (char *)&val ) )
        throw Error("Network I/O Error. Could not read byte data.");
}

void
XDRFileUnMarshaller::get_int16( dods_int16 &val )
{
    if( !XDR_INT16( _source, &val ) )
        throw Error("Network I/O Error. Could not read int 16 data.");
}

void
XDRFileUnMarshaller::get_int32( dods_int32 &val )
{
    if( !XDR_INT32( _source, &val ) )
        throw Error("Network I/O Error. Could not read int 32 data.");
}

void
XDRFileUnMarshaller::get_float32( dods_float32 &val )
{
    if( !xdr_float( _source, &val ) )
        throw Error("Network I/O Error. Could not read float 32 data.");
}

void
XDRFileUnMarshaller::get_float64( dods_float64 &val )
{
    if( !xdr_double( _source, &val ) )
        throw Error("Network I/O Error.Could not read float 64 data.");
}

void
XDRFileUnMarshaller::get_uint16( dods_uint16 &val )
{
    if( !XDR_UINT16( _source, &val ) )
        throw Error("Network I/O Error. Could not read uint 16 data.");
}

void
XDRFileUnMarshaller::get_uint32( dods_uint32 &val )
{
    if( !XDR_UINT32( _source, &val ) )
        throw Error("Network I/O Error. Could not read uint 32 data.");
}

void
XDRFileUnMarshaller::get_str( string &val )
{
    char *in_tmp = NULL ;

    if( !xdr_string( _source, &in_tmp, max_str_len ) )
        throw Error("Network I/O Error. Could not read string data.");

    val = in_tmp ;

    free( in_tmp ) ;
}

void
XDRFileUnMarshaller::get_url( string &val )
{
    get_str( val ) ;
}

void
XDRFileUnMarshaller::get_opaque( char *val, unsigned int len )
{
    xdr_opaque( _source, val, len ) ;
}

void
XDRFileUnMarshaller::get_int( int &val )
{
    if( !xdr_int( _source, &val ) )
	throw Error("Network I/O Error(1). This may be due to a bug in libdap or a\nproblem with the network connection.");
}

void
XDRFileUnMarshaller::get_vector( char **val, uint64_t &num, Vector & )
{
    auto vec_num = static_cast<unsigned int>(num);

    if( !xdr_bytes( _source, val, &vec_num, DODS_MAX_ARRAY) )
	throw Error("Network I/O error (1).");
}

void
XDRFileUnMarshaller::get_vector( char **val, uint64_t &num, int width, Vector &vec )
{
    BaseType *var = vec.var() ;

    auto vec_num = static_cast<unsigned int>(num);

    if( !xdr_array( _source, val, &vec_num, DODS_MAX_ARRAY, width,
		    XDRUtils::xdr_coder( var->type() ) ) )
    {
	throw Error("Network I/O error (2).");
    }
}

void
XDRFileUnMarshaller::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "XDRFileUnMarshaller::dump - ("
         << (void *)this << ")" << endl ;
}

} // namespace libdap

