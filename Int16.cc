
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002,2003 OPeNDAP, Inc.
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

// (c) COPYRIGHT URI/MIT 1996-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Implementation for Int32.
//
// jhrg 9/7/94


#include "config.h"

#include <sstream>

#include "Byte.h"           // synonymous with UInt8 and Char
#include "Int8.h"
#include "Int16.h"
#include "UInt16.h"
#include "Int32.h"
#include "UInt32.h"
#include "Int64.h"
#include "UInt64.h"
#include "Float32.h"
#include "Float64.h"
#include "Str.h"
#include "Url.h"

#include "DDS.h"
#include "Marshaller.h"
#include "UnMarshaller.h"

#include "DMR.h"
#include "D4StreamMarshaller.h"
#include "D4StreamUnMarshaller.h"

#include "util.h"
#include "parser.h"
#include "Operators.h"
#include "dods-limits.h"
#include "debug.h"
#include "InternalErr.h"
#include "DapIndent.h"

using std::cerr;
using std::endl;

namespace libdap {

/** The Int16 constructor accepts the name of the variable to be created.

    @param n A string containing the name of the variable to be created.
*/
Int16::Int16(const string &n) : BaseType(n, dods_int16_c), d_buf(0)
{}

/** The Int16 server-side constructor accepts the name of the variable and
    the dataset name from which this instance is created.

    @param n A string containing the name of the variable to be created.
    @param d A string containing the name of the dataset from which this
    variable is created
*/
Int16::Int16(const string &n, const string &d) : BaseType(n, d, dods_int16_c), d_buf(0)
{}

Int16::Int16(const Int16 &copy_from) : BaseType(copy_from)
{
    d_buf = copy_from.d_buf;
}

BaseType *
Int16::ptr_duplicate()
{
    return new Int16(*this);
}

Int16 &
Int16::operator=(const Int16 &rhs)
{
    if (this == &rhs)
        return *this;

    dynamic_cast<BaseType &>(*this) = rhs;

    d_buf = rhs.d_buf;

    return *this;
}

uint64_t
Int16::width(bool) const
{
    return sizeof(dods_int16);
}

bool
Int16::serialize(ConstraintEvaluator &eval, DDS &dds, Marshaller &m, bool ce_eval)
{
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_on();
#endif
    if (!read_p())
        read();  // read() throws Error and InternalErr

    if (ce_eval && !eval.eval_selection(dds, dataset()))
        return true;
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_off();
#endif
    m.put_int16( d_buf ) ;

    return true;
}

bool
Int16::deserialize(UnMarshaller &um, DDS *, bool)
{
    um.get_int16( d_buf ) ;

    return false;
}

void
Int16::compute_checksum(Crc32 &checksum)
{
	checksum.AddData(reinterpret_cast<uint8_t*>(&d_buf), sizeof(d_buf));
}

/**
 * @brief Serialize an Int8
 * @param m
 * @param dmr Unused
 * @param eval Unused
 * @param filter Unused
 * @exception Error is thrown if the value needs to be read and that operation fails.
 */
void
Int16::serialize(D4StreamMarshaller &m, DMR &, /*ConstraintEvaluator &,*/ bool)
{
    if (!read_p())
        read();          // read() throws Error

    m.put_int16( d_buf ) ;
}

void
Int16::deserialize(D4StreamUnMarshaller &um, DMR &)
{
    um.get_int16( d_buf ) ;
}

uint64_t
Int16::val2buf(void *val, bool)
{
    // Jose Garcia
    // This method is public therefore and I believe it has being designed
    // to be use by read which must be implemented on the surrogated library,
    // thus if the pointer val is NULL, is  an Internal Error.
    if (!val)
        throw InternalErr(__FILE__, __LINE__,
                          "The incoming pointer does not contain any data.");

    d_buf = *(dods_int16 *)val;

    return width();
}

uint64_t
Int16::buf2val(void **val)
{
    // Jose Garcia
    // The same comment justifying throwing an Error in val2buf applies here.
    if (!val)
        throw InternalErr(__FILE__, __LINE__, "NULL pointer.");

    if (!*val)
        *val = new dods_int16;

    *(dods_int16 *)*val = d_buf;

    return width();
}

dods_int16
Int16::value() const
{
    return d_buf;
}

bool
Int16::set_value(dods_int16 i)
{
    d_buf = i;
    set_read_p(true);

    return true;
}

void
Int16::print_val(FILE *out, string space, bool print_decl_p)
{
    ostringstream oss;
    print_val(oss, space, print_decl_p);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

void
Int16::print_val(ostream &out, string space, bool print_decl_p)
{
    if (print_decl_p) {
        print_decl(out, space, false);
	out << " = " << d_buf << ";\n" ;
    }
    else
	out << d_buf ;
}

bool
Int16::ops(BaseType *b, int op)
{

    // Extract the Byte arg's value.
    if (!read_p() && !read()) {
        // Jose Garcia
        // Since the read method is virtual and implemented outside
        // libdap++ if we cannot read the data that is the problem
        // of the user or of whoever wrote the surrogate library
        // implemeting read therefore it is an internal error.
        throw InternalErr(__FILE__, __LINE__, "This value not read!");
    }

    // Extract the second arg's value.
    if (!b || !(b->read_p() || b->read())) {
        // Jose Garcia
        // Since the read method is virtual and implemented outside
        // libdap++ if we cannot read the data that is the problem
        // of the user or of whoever wrote the surrogate library
        // implemeting read therefore it is an internal error.
        throw InternalErr(__FILE__, __LINE__, "This value not read!");
    }

    return d4_ops(b, op);
}

/**
 * @see BaseType::d4_ops(BaseType *, int)
 */
bool Int16::d4_ops(BaseType *b, int op)
{
    switch (b->type()) {
        case dods_int8_c:
            return Cmp<dods_int16, dods_int8>(op, d_buf, static_cast<Int8*>(b)->value());
        case dods_byte_c:
            return Cmp<dods_int16, dods_byte>(op, d_buf, static_cast<Byte*>(b)->value());
        case dods_int16_c:
            return Cmp<dods_int16, dods_int16>(op, d_buf, static_cast<Int16*>(b)->value());
        case dods_uint16_c:
            return Cmp<dods_int16, dods_uint16>(op, d_buf, static_cast<UInt16*>(b)->value());
        case dods_int32_c:
            return Cmp<dods_int16, dods_int32>(op, d_buf, static_cast<Int32*>(b)->value());
        case dods_uint32_c:
            return Cmp<dods_int16, dods_uint32>(op, d_buf, static_cast<UInt32*>(b)->value());
        case dods_int64_c:
            return Cmp<dods_int16, dods_int64>(op, d_buf, static_cast<Int64*>(b)->value());
        case dods_uint64_c:
            return Cmp<dods_int16, dods_uint64>(op, d_buf, static_cast<UInt64*>(b)->value());
        case dods_float32_c:
            return Cmp<dods_int16, dods_float32>(op, d_buf, static_cast<Float32*>(b)->value());
        case dods_float64_c:
            return Cmp<dods_int16, dods_float64>(op, d_buf, static_cast<Float64*>(b)->value());
        case dods_str_c:
        case dods_url_c:
            throw Error(malformed_expr, "Relational operators can only compare compatible types (number, string).");
        default:
            throw Error(malformed_expr, "Relational operators only work with scalar types.");
    }
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and information about this
 * instance.
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void
Int16::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "Int16::dump - ("
    << (void *)this << ")" << endl ;
    DapIndent::Indent() ;
    BaseType::dump(strm) ;
    strm << DapIndent::LMarg << "value: " << d_buf << endl ;
    DapIndent::UnIndent() ;
}

} // namespace libdap

