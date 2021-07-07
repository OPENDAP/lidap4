// XDRStreamUnMarshaller.h

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

#ifndef I_XDRStreamUnMarshaller_h
#define I_XDRStreamUnMarshaller_h 1

#include <iostream>

using std::istream ;
using std::cin ;

#include "UnMarshaller.h"
#include "XDRUtils.h"

namespace libdap
{

const int XDR_DAP_BUFF_SIZE = 4096; // This will be compared to a signed int

/** @brief unmarshaller that knows how to unmarshall/deserialize dap objects
 * using XDR from a file
 */
class XDRStreamUnMarshaller : public UnMarshaller
{
private:
    XDR 			d_source ;
    istream &		d_in;
    static char *	d_buf;

    				XDRStreamUnMarshaller() ;
    				XDRStreamUnMarshaller( const XDRStreamUnMarshaller &um ) ;
    XDRStreamUnMarshaller &	operator=( const XDRStreamUnMarshaller & ) ;

public:
    				XDRStreamUnMarshaller( istream &in ) ;
    virtual			~XDRStreamUnMarshaller() ;

    virtual void	get_byte( dods_byte &val ) ;

    virtual void	get_int16( dods_int16 &val ) ;
    virtual void	get_int32( dods_int32 &val ) ;

    virtual void	get_float32( dods_float32 &val ) ;
    virtual void	get_float64( dods_float64 &val ) ;

    virtual void	get_uint16( dods_uint16 &val ) ;
    virtual void	get_uint32( dods_uint32 &val ) ;

    virtual void	get_str( string &val ) ;
    virtual void	get_url( string &val ) ;

    virtual void	get_opaque( char *val, unsigned int len ) ;
    virtual void	get_int( int &val ) ;

    virtual void	get_vector( char **val, uint64_t &num, Vector &vec ) ;
    virtual void	get_vector( char **val, uint64_t &num, int width, Vector &vec ) ;

    virtual void get_vector(char **val, uint64_t &num, int width, Type type);

    virtual void	dump(ostream &strm) const ;
} ;

} // namespace libdap

#endif // I_XDRStreamUnMarshaller_h

