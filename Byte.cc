
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

// Implementation for Byte.
//
// jhrg 9/7/94

//#define DODS_DEBUG

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
#include "Operators.h"
#include "Marshaller.h"
#include "UnMarshaller.h"

#include "DMR.h"
#include "D4Attributes.h"
#include "D4StreamMarshaller.h"
#include "D4StreamUnMarshaller.h"

#include "debug.h"
#include "util.h"
#include "parser.h"
#include "dods-limits.h"
#include "InternalErr.h"
#include "DapIndent.h"

using std::cerr;
using std::endl;

namespace libdap {

/** The Byte constructor requires only the name of the variable
    to be created.  The name may be omitted, which will create a
    nameless variable.  This may be adequate for some applications.

    @brief The Byte constructor.
    @param n A string containing the name of the variable to be
    created.

*/
Byte::Byte(const string & n): BaseType(n, dods_byte_c), d_buf(0)
{}

/** This Byte constructor requires the name of the variable to be created
    and the name of the dataset from which this variable is being created.
    This constructor is used in server-side processing, loading structure in
    from a dataset.

    @brief The Byte server-side constructor.
    @param n A string containing the name of the variable to be created.
    @param d A string containing the name of the dataset from which the
    variable is being created.
*/
Byte::Byte(const string &n, const string &d): BaseType(n, d, dods_byte_c), d_buf(0)
{}

Byte::Byte(const Byte & copy_from): BaseType(copy_from)
{
    d_buf = copy_from.d_buf;
}

BaseType *Byte::ptr_duplicate()
{
    return new Byte(*this);
}

Byte & Byte::operator=(const Byte & rhs)
{
    if (this == &rhs)
        return *this;

    dynamic_cast < BaseType & >(*this) = rhs;

    d_buf = rhs.d_buf;

    return *this;
}

uint64_t Byte::width(bool) const
{
    return sizeof(dods_byte);
}

/** Serialize the contents of member _BUF (the object's internal
    buffer, used to hold data) and write the result to stdout. If
    FLUSH is true, write the contents of the output buffer to the
    kernel. FLUSH is false by default. If CE_EVAL is true, evaluate
    the current constraint expression; only send data if the CE
    evaluates to true.

    @return False if a failure to read, send or flush is detected, true
    otherwise.
*/
bool Byte::serialize(ConstraintEvaluator & eval, DDS & dds, Marshaller &m, bool ce_eval)
{
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_on();
#endif
    if (!read_p())
        read();          // read() throws Error and InternalErr

    if (ce_eval && !eval.eval_selection(dds, dataset()))
        return true;
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_off();
#endif
    m.put_byte( d_buf ) ;

    return true;
}

/** @brief Deserialize the char on stdin and put the result in
    <tt>_BUF</tt>.
*/
bool Byte::deserialize(UnMarshaller &um, DDS *, bool)
{
    um.get_byte( d_buf ) ;

    return false;
}

void
Byte::compute_checksum(Crc32 &checksum)
{
	checksum.AddData(reinterpret_cast<uint8_t*>(&d_buf), sizeof(d_buf));
}

/**
 * @brief Serialize a Byte
 * @param m
 * @param dmr Unused
 * @param eval Unused
 * @param filter Unused
 * @exception Error is thrown if the value needs to be read and that operation fails.
 */
void
Byte::serialize(D4StreamMarshaller &m, DMR &, /*ConstraintEvaluator &,*/ bool)
{
    if (!read_p())
        read();          // read() throws Error

    m.put_byte( d_buf ) ;
}

void
Byte::deserialize(D4StreamUnMarshaller &um, DMR &)
{
    um.get_byte( d_buf ) ;
}

/** Store the value referenced by <i>val</i> in the object's internal
    buffer. <i>reuse</i> has no effect because this class does not
    dynamically allocate storage for the internal buffer.

    @return The size (in bytes) of the value's representation.  */
uint64_t Byte::val2buf(void *val, bool)
{
    // Jose Garcia
    // This method is public therefore and I believe it has being designed
    // to be use by read which must be implemented on the surrogate library,
    // thus if the pointer val is NULL, is an Internal Error.
    if (!val)
        throw InternalErr("the incoming pointer does not contain any data.");

    d_buf = *(dods_byte *) val;

    return width();
}

uint64_t Byte::buf2val(void **val)
{
    // Jose Garcia
    // The same comment justifying throwing an Error in val2buf applies here.
    if (!val)
        throw InternalErr("NULL pointer");

    if (!*val)
        *val = new dods_byte;

    *(dods_byte *) * val = d_buf;

    return width();
}

/** Set the value of this instance.
    @param value The value
    @return Always returns true; the return type of bool is for compatibility
    with the Passive* subclasses written by HAO. */
bool Byte::set_value(dods_byte value)
{
    d_buf = value;
    set_read_p(true);

    return true;
}

/** Get the value of this instance.
    @return The value. */
dods_byte Byte::value() const
{
    return d_buf;
}

void Byte::print_val(FILE * out, string space, bool print_decl_p)
{
    ostringstream oss;
    print_val(oss, space, print_decl_p);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

void Byte::print_val(ostream &out, string space, bool print_decl_p)
{
    if (print_decl_p) {
        print_decl(out, space, false);
        out << " = " << (int) d_buf << ";\n";
    }
    else
        out << (int) d_buf;
}

bool Byte::ops(BaseType * b, int op)
{

    // Extract the Byte arg's value.
    if (!read_p() && !read()) {
        // Jose Garcia
        // Since the read method is virtual and implemented outside
        // libdap++ if we cannot read the data that is the problem
        // of the user or of whoever wrote the surrogate library
        // implementing read therefore it is an internal error.
        throw InternalErr("This value not read!");
    }
    // Extract the second arg's value.
    if (!b || !(b->read_p() || b->read())) {
        // Jose Garcia
        // Since the read method is virtual and implemented outside
        // libdap++ if we cannot read the data that is the problem
        // of the user or of whoever wrote the surrogate library
        // implementing read therefore it is an internal error.
        throw InternalErr("This value not read!");
    }

    // By using the same operator code numbers for both the DAP2 and DAP4
    // parser/evaluator we can use the same evaluation code.
    return d4_ops(b, op);
}

/**
 * @see BaseType::d4_ops(BaseType *, int)
 */
bool Byte::d4_ops(BaseType *b, int op)
{
    switch (b->type()) {
        case dods_int8_c:
            return Cmp<dods_byte, dods_int8>(op, d_buf, static_cast<Int8*>(b)->value());
        case dods_byte_c:
            return Cmp<dods_byte, dods_byte>(op, d_buf, static_cast<Byte*>(b)->value());
        case dods_int16_c:
            return Cmp<dods_byte, dods_int16>(op, d_buf, static_cast<Int16*>(b)->value());
        case dods_uint16_c:
            return Cmp<dods_byte, dods_uint16>(op, d_buf, static_cast<UInt16*>(b)->value());
        case dods_int32_c:
            return Cmp<dods_byte, dods_int32>(op, d_buf, static_cast<Int32*>(b)->value());
        case dods_uint32_c:
            return Cmp<dods_byte, dods_uint32>(op, d_buf, static_cast<UInt32*>(b)->value());
        case dods_int64_c:
            return Cmp<dods_byte, dods_int64>(op, d_buf, static_cast<Int64*>(b)->value());
        case dods_uint64_c:
            return Cmp<dods_byte, dods_uint64>(op, d_buf, static_cast<UInt64*>(b)->value());
        case dods_float32_c:
            return Cmp<dods_byte, dods_float32>(op, d_buf, static_cast<Float32*>(b)->value());
        case dods_float64_c:
            return Cmp<dods_byte, dods_float64>(op, d_buf, static_cast<Float64*>(b)->value());
        case dods_str_c:
        case dods_url_c:
            throw Error(malformed_expr, "Relational operators can only compare compatible types (number, string).");
        default:
            throw Error(malformed_expr, "Relational operators only work with scalar types.");
    }
}




/** @brief DAP4 to DAP2 transform
 *
 * Return a DAP2 'copy' of the variable.
 *
 * NOTE: This little bit of magic ensures that the DAP4 shenanigans
 * in which UInt8, Char , and Byte are synonymous is reduced
 * to the DAP2 simplicity of Byte.
 *
 *
 * @param root The root group that should hold this new variable. Add Group-level
 * stuff here (e.g., D4Dimensions).
 * @param container Add the new variable to this container.
 *
 * @return A pointer to the transformed variable
 */
std::vector<BaseType *> *
Byte::transform_to_dap2(AttrTable *parent_attr_table)
{
    DBG(cerr << __func__ << "() - BEGIN" << endl;);
    vector<BaseType *> *vec = BaseType::transform_to_dap2(parent_attr_table);
    if(vec->size()!=1){
        ostringstream oss;
        oss << __func__ << "() -  Something Bad Happened. This transform should produce only ";
        oss << " a single BaseType yet it produced " << vec->size();
        throw Error(internal_error,oss.str());
    }

    BaseType *dest = (*vec)[0];
    DBG(cerr << __func__ << "() - type():       " << type() << endl;);
    DBG(cerr << __func__ << "() - dest->type(): " << dest->type() << endl;);

    // This little bit of magic ensures that the DAP4 shenanigans
    // in which UInt8, Char , and Byte are synonymous is reduced
    // to the DAP2 simplicity of Byte.
    if(type()!=dods_byte_c){
        dest->set_type(dods_byte_c);
    }
    DBG (dest->get_attr_table().print(cerr););

    DBG(cerr << __func__ << "() - END" << endl;);
    return vec;
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and information about this
 * instance.
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void Byte::dump(ostream & strm) const
{
    strm << DapIndent::LMarg << "Byte::dump - ("
    << (void *) this << ")" << endl;
    DapIndent::Indent();
    BaseType::dump(strm);
    strm << DapIndent::LMarg << "value: " << d_buf << endl;
    DapIndent::UnIndent();
}

} // namespace libdap
