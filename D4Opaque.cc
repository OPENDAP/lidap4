// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2013 OPeNDAP, Inc.
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
// Foundation, Inc., 51 Franklin D4Opaqueeet, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

//#define DODS_DEBUG

#include "config.h"

#include <sstream>
#include <iterator>

#include "D4Opaque.h"

#include "DMR.h"
#include "D4StreamMarshaller.h"
#include "D4StreamUnMarshaller.h"

#include "util.h"
#include "crc.h"

#include "debug.h"
#include "DapIndent.h"

#undef CLEAR_LOCAL_DATA

using namespace std;

namespace libdap {

D4Opaque &
D4Opaque::operator=(const D4Opaque &rhs)
{
    if (this == &rhs)
        return *this;

    // Call BaseType::operator=
    dynamic_cast<BaseType &>(*this) = rhs;

    d_buf = rhs.d_buf;

    return *this;
}

void
D4Opaque::clear_local_data()
{
    if (!d_buf.empty()) {
        d_buf.erase(d_buf.begin(), d_buf.end());
        d_buf.resize(0);
    }

    set_read_p(false);
}

void
D4Opaque::compute_checksum(Crc32 &checksum)
{
	checksum.AddData(&d_buf[0], d_buf.size());
}

void
D4Opaque::serialize(D4StreamMarshaller &m, DMR &, bool)
{
    if (!read_p())
        read();          // read() throws Error

    m.put_opaque_dap4( reinterpret_cast<char*>(&d_buf[0]), d_buf.size() ) ;

#ifdef CLEAR_LOCAL_DATA
    clear_local_data();
#endif

}

void
D4Opaque::deserialize(D4StreamUnMarshaller &um, DMR &)
{
    um.get_opaque_dap4( d_buf ) ;
}

uint64_t
D4Opaque::buf2val(void **val)
{
	assert(val);

    // If *val is null, then the caller has not allocated storage for the
    // value; we must. If there is storage there, assume it is a vector<uint8_t>
	// (i.e., dods_opaque) and assign d_buf's value to that storage.
    if (!*val)
        *val = new vector<uint8_t>;
    else
        *static_cast<vector<uint8_t>*>(*val) = d_buf;

    return sizeof(vector<uint8_t>*);
}

uint64_t
D4Opaque::val2buf(void *val, bool)
{
    assert(val);

    d_buf = *static_cast<dods_opaque*>(val);

    return sizeof(dods_opaque*);
}

/** Set the value of this instance.
    @param value The value
    @return Always returns true; the return type of bool is for compatibility
    with the Passive* subclasses written by HAO. */
bool
D4Opaque::set_value(const dods_opaque &value)
{
    d_buf = value;
    set_read_p(true);

    return true;
}

/** Get the value of this instance.
    @return The value. */
D4Opaque::dods_opaque
D4Opaque::value() const
{
    return d_buf;
}

std::vector<BaseType *> *
D4Opaque::transform_to_dap2(AttrTable *){
    DBG(cerr << __func__ << "() - Transform not implemented DAP4 Opaque type." << endl;);
    return NULL;
}


void
D4Opaque::print_val(ostream &out, string space, bool print_decl_p)
{
	if (print_decl_p) print_decl(out, space, false);

	if (d_buf.size()) {
		// end() - 1 is only OK if size() is > 0
		std::ostream_iterator<unsigned int> out_it(out, ",");
		std::copy(d_buf.begin(), d_buf.end() - 1, out_it);
		out << (unsigned int) d_buf.back(); // can also use: *(d_buf.end()-1);
	}

	if (print_decl_p) out << ";" << endl;
}

void
D4Opaque::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "D4Opaque::dump - ("
    << (void *)this << ")" << endl ;
    DapIndent::Indent() ;
    BaseType::dump(strm) ;
    //strm << DapIndent::LMarg << "value: " << d_buf << endl ;
    ostream_iterator<uint8_t> out_it (strm," ");
    std::copy ( d_buf.begin(), d_buf.end(), out_it );

    DapIndent::UnIndent() ;
}

} // namespace libdap

