
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

// (c) COPYRIGHT URI/MIT 1994-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Implementation for Str.
//
// jhrg 9/7/94


#include "config.h"

#include <sstream>

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

#include "DDS.h"
#include "Marshaller.h"
#include "UnMarshaller.h"

#include "DMR.h"
#include "D4StreamMarshaller.h"
#include "D4StreamUnMarshaller.h"

#include "util.h"
#include "parser.h"
#include "Operators.h"
#include "InternalErr.h"
#include "escaping.h"
#include "debug.h"
#include "DapIndent.h"

using std::cerr;
using std::endl;

namespace libdap {

/** The Str constructor requires only the name of the variable
    to be created.  The name may be omitted, which will create a
    nameless variable.  This may be adequate for some applications.

    @param n A string containing the name of the variable to be
    created.

*/
Str::Str(const string &n) : BaseType(n, dods_str_c), d_buf("")
{}

/** The Str server-side constructor accepts the name of the variable and the
    dataset name from which this instance is created.

    @param n A string containing the name of the variable to be created.
    @param d A string containing the name of the dataset from which this
    variable is created
*/
Str::Str(const string &n, const string &d) : BaseType(n, d, dods_str_c), d_buf("")
{}

Str::Str(const Str &copy_from) : BaseType(copy_from)
{
    d_buf = copy_from.d_buf;
}

BaseType *
Str::ptr_duplicate()
{
    return new Str(*this);
}

Str &
Str::operator=(const Str &rhs)
{
    if (this == &rhs)
        return *this;

    // Call BaseType::operator=.
    dynamic_cast<BaseType &>(*this) = rhs;

    d_buf = rhs.d_buf;

    return *this;
}

uint64_t
Str::length() const
{
    return (uint64_t)d_buf.length();
}

uint64_t
Str::width(bool) const
{
    return (uint64_t)sizeof(string);
}

bool
Str::serialize(ConstraintEvaluator &eval, DDS &dds, Marshaller &m, bool ce_eval)
{

    DBG(cerr << "Entering (" << this->name() << " [" << this << "])" << endl);
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_on();
#endif
    if (!read_p())
        read();

    if (ce_eval && !eval.eval_selection(dds, dataset()))
        return true;
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_off();
#endif
    m.put_str( d_buf ) ;

    DBG(cerr << "Exiting: buf = " << d_buf << endl);

    return true;
}

// deserialize the string on stdin and put the result in BUF.

bool
Str::deserialize(UnMarshaller &um, DDS *, bool)
{
    um.get_str( d_buf ) ;

    return false;
}

void
Str::compute_checksum(Crc32 &checksum)
{
	checksum.AddData(reinterpret_cast<const uint8_t*>(d_buf.data()), d_buf.length());
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
Str::serialize(D4StreamMarshaller &m, DMR &, /*ConstraintEvaluator &,*/ bool)
{
    if (!read_p())
        read();          // read() throws Error

    m.put_str( d_buf ) ;
}

void
Str::deserialize(D4StreamUnMarshaller &um, DMR &)
{
    um.get_str( d_buf ) ;
}

/** Read the object's value and put a copy in the C++ string object
    referenced by \e **val. If \e *val is null, this method will allocate
    a string object using new and store the result there. If \e *val
    is not null, it will assume that \e *val references a string object
    and put the value there.

    @param val A pointer to null or to a string object.
    @return The sizeof(string*)
    @exception InternalErr Thrown if \e val is null. */
uint64_t
Str::buf2val(void **val)
{
    // Jose Garcia
    // The same comment justifying throwing an Error in val2buf applies here.
    if (!val)
        throw InternalErr(__FILE__, __LINE__,
                          "No place to store a reference to the data.");
    // If *val is null, then the caller has not allocated storage for the
    // value; we must. If there is storage there, assume it is a string and
    // assign d_buf's value to that storage.
    if (!*val)
        *val = new string(d_buf);
    else
        *static_cast<string*>(*val) = d_buf;

    return sizeof(string*);
}

/** Store the value referenced by \e val in this object. Even though the
    type of \e val is \c void*, this method assumes the type is \c string*.
    Note that the value is copied so the caller if free to throw away/reuse
    the actual parameter once this call has returned.

    @param val A pointer to a C++ string object.
    @param reuse Not used by this version of the method.
    @exception IntenalErr if \e val is null.
    @return The width of the pointer. */
uint64_t
Str::val2buf(void *val, bool)
{
    // Jose Garcia
    // This method is public therefore and I believe it has being designed
    // to be use by read which must be implemented on the surrogated library,
    // thus if the pointer val is NULL, is an Internal Error.
    if (!val)
        throw InternalErr(__FILE__, __LINE__, "NULL pointer.");

    d_buf = *static_cast<string*>(val);

    return sizeof(string*);
}

/** Set the value of this instance.
    @param value The value
    @return Always returns true; the return type of bool is for compatibility
    with the Passive* subclasses written by HAO. */
bool
Str::set_value(const string &value)
{
    d_buf = value;
    set_read_p(true);

    return true;
}

/** Get the value of this instance.
    @return The value. */
string
Str::value() const
{
    return d_buf;
}

void
Str::print_val(FILE *out, string space, bool print_decl_p)
{
    ostringstream oss;
    print_val(oss, space, print_decl_p);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

void
Str::print_val(ostream &out, string space, bool print_decl_p)
{
    if (print_decl_p) {
        print_decl(out, space, false);
	out << " = \"" << escattr(d_buf) << "\";\n" ;
    }
    else
	out << "\"" << escattr(d_buf) << "\"" ;
}

bool
Str::ops(BaseType *b, int op)
{
    // Extract the Byte arg's value.
    if (!read_p() && !read()) {
        // Jose Garcia
        // Since the read method is virtual and implemented outside
        // libdap++ if we cannot read the data that is the problem
        // of the user or of whoever wrote the surrogate library
        // implemeting read therefore it is an internal error.
        throw InternalErr(__FILE__, __LINE__, "This value was not read!");
    }

    // Extract the second arg's value.
    if (!b || !(b->read_p() || b->read())) {
        // Jose Garcia
        // Since the read method is virtual and implemented outside
        // libdap++ if we cannot read the data that is the problem
        // of the user or of whoever wrote the surrogate library
        // implemeting read therefore it is an internal error.
        throw InternalErr(__FILE__, __LINE__, "Argument value was not read!");
    }

    return d4_ops(b, op);
}

/**
 * @see BaseType::d4_ops(BaseType *, int)
 */
bool Str::d4_ops(BaseType *b, int op)
{
    switch (b->type()) {
    case dods_byte_c:
    case dods_int8_c:
    case dods_int16_c:
    case dods_uint16_c:
    case dods_int32_c:
    case dods_uint32_c:
    case dods_int64_c:
    case dods_uint64_c:
    case dods_float32_c:
    case dods_float64_c:
        throw Error(malformed_expr, "Relational operators can only compare compatible types (string, number).");
    case dods_str_c:
        return StrCmp<string, string>(op, d_buf, static_cast<Str*>(b)->value());
    case dods_url_c:
        return StrCmp<string, string>(op, d_buf, static_cast<Url*>(b)->value());
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
Str::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "Str::dump - ("
    << (void *)this << ")" << endl ;
    DapIndent::Indent() ;
    BaseType::dump(strm) ;
    strm << DapIndent::LMarg << "value: " << d_buf << endl ;
    DapIndent::UnIndent() ;
}

} // namespace libdap

