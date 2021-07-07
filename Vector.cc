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

// (c) COPYRIGHT URI/MIT 1995-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Implementation for class Vector. This class is the basis for all the
// vector-type classes in libdap's <Array, List>.
//
// 11/21/95 jhrg

#include "config.h"

#include <cstring>
#include <cassert>

//#define DODS_DEBUG 1

#include <sstream>
#include <vector>
#include <algorithm>
#include <typeinfo>

#include <stdint.h>

#include "crc.h"

#include "Vector.h"
#include "Marshaller.h"
#include "UnMarshaller.h"

#include "D4StreamMarshaller.h"
#include "D4StreamUnMarshaller.h"

#include "D4Enum.h"

#include "Type.h"
#include "dods-datatypes.h"
#include "dods-limits.h"
#include "escaping.h"
#include "util.h"
#include "debug.h"
#include "InternalErr.h"
#include "DapIndent.h"

#undef CLEAR_LOCAL_DATA

using std::cerr;
using std::endl;

namespace libdap {

void Vector::m_duplicate(const Vector & v)
{
    d_length = v.d_length;

    // _var holds the type of the elements. That is, it holds a BaseType
    // which acts as a template for the type of each element.
    if (v.d_proto) {
        d_proto = v.d_proto->ptr_duplicate(); // use ptr_duplicate()
        d_proto->set_parent(this); // ptr_duplicate does not set d_parent.
    }
    else {
        d_proto = 0;
    }

    // d_compound_buf and d_buf (further down) hold the values of the Vector. The field
    // d_compound_buf is used when the Vector holds non-numeric data (including strings
    // although it used to be that was not the case jhrg 2/10/05) while d_buf
    // holds numeric values.
    if (v.d_compound_buf.empty()) {
        d_compound_buf = v.d_compound_buf;
    }
    else {
        // Failure to set the size will make the [] operator barf on the LHS
        // of the assignment inside the loop.
        d_compound_buf.resize(d_length);
        for (uint64_t i = 0; i < d_length; ++i) {
            // There's no need to call set_parent() for each element; we
            // maintain the back pointer using the d_proto member. These
            // instances are used to hold _values_ only while the d_proto
            // field holds the type information for the elements.
            d_compound_buf[i] = v.d_compound_buf[i]->ptr_duplicate();
        }
    }

    // copy the strings. This copies the values.
    d_str = v.d_str;

    // copy numeric values if there are any.
    d_buf = 0; // init to null
    if (v.d_buf) // only copy if data present
        val2buf(v.d_buf); // store v's value in this's _BUF.

    d_capacity = v.d_capacity;
}

/**
 * @return whether the type of this Vector is a cardinal type
 * (i.e., stored in d_buf)
 */
bool Vector::m_is_cardinal_type() const
{
    // Not cardinal if no d_proto at all!
    if (!d_proto) {
        return false;
    }

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_float32_c:
        case dods_float64_c:
        	// New cardinal types for DAP4
        case dods_int8_c:
        case dods_uint8_c:
        case dods_int64_c:
        case dods_uint64_c:

        case dods_enum_c:
            return true;

            // These must be handled differently.
        case dods_str_c:
        case dods_url_c:
        case dods_opaque_c:

        case dods_array_c:

        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            return false;

        default:
            assert("Vector::var: Unrecognized type");
            return false;
    }
}

/**
 * Create _buf so that it can store numElts of the
 * (assumed) cardinal type.  This create storage for
 * width() * numElts bytes.
 * If _buf already exists, this DELETES IT and creates a new one.
 * So don't use this if you want to keep the original _buf data around.
 * This also sets the valueCapacity().
 * @param numEltsOfType the number of elements of the cardinal type in var()
 that we want storage for.
 * @return the size of the buffer created.
 * @exception if the Vector's type is not cardinal type.
 */
uint64_t Vector::m_create_cardinal_data_buffer_for_type(uint64_t numEltsOfType)
{
    // Make sure we HAVE a _var, or we cannot continue.
    if (!d_proto) {
        throw InternalErr(__FILE__, __LINE__, "create_cardinal_data_buffer_for_type: Logic error: _var is null!");
    }

    // Make sure we only do this for the correct data types.
    if (!m_is_cardinal_type()) {
        throw InternalErr(__FILE__, __LINE__, "create_cardinal_data_buffer_for_type: incorrectly used on Vector whose type was not a cardinal (simple data types).");
    }

    m_delete_cardinal_data_buffer();

    // Handle this special case where this is an array that holds no values
    if (numEltsOfType == 0)
        return 0;

    // Actually new up the array with enough bytes to hold numEltsOfType of the actual type.
    uint64_t bytesPerElt = d_proto->width();
    uint64_t bytesNeeded = bytesPerElt * numEltsOfType;
    d_buf = new char[bytesNeeded];

    d_capacity = numEltsOfType;
    return bytesNeeded;
}

/** Delete d_buf and zero it and d_capacity out */
void Vector::m_delete_cardinal_data_buffer()
{
	delete[] d_buf;
	d_buf = 0;
	d_capacity = 0;
}

/** Helper to reduce cut and paste in the virtual's.
 *
 */
template<class CardType>
void Vector::m_set_cardinal_values_internal(const CardType* fromArray, uint64_t numElts)
{
    if (numElts < 0) {
        throw InternalErr(__FILE__, __LINE__, "Logic error: Vector::set_cardinal_values_internal() called with negative numElts!");
    }
    if (!fromArray) {
        throw InternalErr(__FILE__, __LINE__, "Logic error: Vector::set_cardinal_values_internal() called with null fromArray!");
    }
    set_length(numElts);
    m_create_cardinal_data_buffer_for_type(numElts);
    memcpy(d_buf, fromArray, numElts * sizeof(CardType));
    set_read_p(true);
}

/** The Vector constructor requires the name of the variable to be
 created, and a pointer to an object of the type the Vector is to
 hold.  The name may be omitted, which will create a nameless
 variable.  The template object may not be omitted.

 @param n A string containing the name of the variable to be
 created.
 @param v A pointer to a prototype for elements.
 @param t The type of the resulting Vector object, from the Type
 enum list.  There is no DAP2 Vector object, so all uses of this
 method will be from the Array class.  This defaults to
 <tt>dods_null_c</tt>.

 @see Type
 @brief The Vector constructor.  */
Vector::Vector(const string & n, BaseType * v, const Type & t, bool is_dap4 /* default:false */) :
    BaseType(n, t, is_dap4), d_length(0), d_length_set(false), d_proto(0), d_buf(0), d_compound_buf(0), d_capacity(0)
{
    if (v)
        add_var(v);

    DBG2(cerr << "Entering Vector ctor for object: " << this << endl);
    if (d_proto)
        d_proto->set_parent(this);
}

/** The Vector server-side constructor requires the name of the variable
 to be created, the dataset name from which this Vector is created, and
 a pointer to an object of the type the Vector is to hold.  The
 name may be omitted, which will create a nameless variable.
 The template object may not be omitted.

 @param n A string containing the name of the variable to be
 created.
 @param d A string containing the dataset name from which the variable is
 being created.
 @param v A pointer to a prototype for elements.
 @param t The type of the resulting Vector object, from the Type
 enum list.  There is no DAP2 Vector object, so all uses of this
 method will be from the Array class.  This defaults to
 <tt>dods_null_c</tt>.

 @see Type
 @brief The Vector constructor.  */
Vector::Vector(const string & n, const string &d, BaseType * v, const Type & t, bool is_dap4 /* default:false */) :
    BaseType(n, d, t, is_dap4), d_length(0), d_length_set(false), d_proto(0), d_buf(0), d_compound_buf(0), d_capacity(0)
{
    if (v)
        add_var(v);

    DBG2(cerr << "Entering Vector ctor for object: " << this << endl);
    if (d_proto)
        d_proto->set_parent(this);
}

/** The Vector copy constructor. */
Vector::Vector(const Vector & rhs) :
    BaseType(rhs)
{
    DBG2(cerr << "Entering Vector const ctor for object: " << this <<
            endl); DBG2(cerr << "RHS: " << &rhs << endl);

    m_duplicate(rhs);
}

Vector::~Vector()
{
    DBG2(cerr << "Entering ~Vector (" << this << ")" << endl);

    delete d_proto;
    d_proto = 0;

    // Clears all buffers
    clear_local_data();

    DBG2(cerr << "Exiting ~Vector" << endl);
}

Vector & Vector::operator=(const Vector & rhs)
{
    if (this == &rhs)
        return *this;

    dynamic_cast<BaseType &> (*this) = rhs;

    m_duplicate(rhs);

    return *this;
}

void Vector::set_name(const std::string& name)
{
    BaseType::set_name(name);
    // We need to set the prototype name as well since
    // this is what gets output in the dds!  Otherwise, there's a mismatch.
    if (d_proto) {
        d_proto->set_name(name);
    }
}

int Vector::element_count(bool leaves)
{
    if (!leaves)
        return 1;
    else
    	return d_proto->element_count(leaves);
        // var() only works for simple types!
        // jhrg 8/19/13 return var(0)->element_count(leaves);
}

// These mfuncs set the _send_p and _read_p fields of BaseType. They differ
// from BaseType's version in that they set both the Vector object's copy of
// _send_p (_read_p) but also _VAR's copy. This does not matter much when _VAR
// is a scalar, but does matter when it is an aggregate.

/** This function sets the <tt>send_p</tt> flag for both the Vector itself
 and its element template.  This does not matter much when the
 Vector contains simple data types, but does become significant
 when the Vector contains compound types.

 @brief Indicates that the data is ready to send. */
void Vector::set_send_p(bool state)
{
    if (d_proto) {
        d_proto->set_send_p(state);

        // because some code may depend on the BaseType*s held in d_compound_buf
        // behaving as if they are 'ordinary' DAP variables, make sure their send_p
        // flag is set if they exist. Because space in the vector is allocated
        // before values (BaseType*s) are added, check for nulls and limit the
        // iteration to only those elements actually in the object including any
        // constraints that may have been applied - these are values not declarations.
        // jhrg 5/13/16
        switch (d_proto->type()) {
        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            if (d_compound_buf.size() > 0) {
                for (uint64_t i = 0; i < d_length; ++i) {
                    if (d_compound_buf[i]) d_compound_buf[i]->set_send_p(state);
                }
            }
            break;

        default:
            break;
        }
    }

    BaseType::set_send_p(state);
}

/** This function sets the <tt>read_p</tt> flag for both the Vector itself
 and its element template.  This does not matter much when the
 Vector contains simple data types, but does become significant
 when the Vector contains compound types.

 @brief Indicates that the data is ready to send.  */
void Vector::set_read_p(bool state)
{
    if (d_proto) {
        d_proto->set_read_p(state);

        // See comment above.
        switch (d_proto->type()) {
        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            if (d_compound_buf.size() > 0) {
                for (uint64_t i = 0; i < d_length; ++i) {
                    if (d_compound_buf[i]) d_compound_buf[i]->set_read_p(state);
                }
            }
            break;

        default:
            break;
        }
    }

    BaseType::set_read_p(state);
}

/** Returns a copy of the template array element. If the Vector contains
 simple data types, the template will contain the value of the last
 vector element accessed with the <code>Vector::var(int i)</code> function,
 if any. If no such access has been made, or if the Vector contains
 compound data types, the value held by the template instance is
 undefined.

 Note that the parameter <i>exact_match</i> is not used by this mfunc.

 @param n The name of the variable to find.
 @param exact Unused.
 @param s Pointer to a BaseType Pointer Stack. Use this stack to record
 the path to the variable. By default this pointer is null, in which case
 it is not used.

 @return A pointer to the BaseType if found, otherwise null.
 @see Vector::var */
BaseType *Vector::var(const string &n, bool exact, btp_stack *s)
{
    string name = www2id(n);
    DBG2(cerr << "Vector::var: Looking for " << name << endl);

    if (name == "" || d_proto->name() == name) {
        if (s)
            s->push(this);
        return d_proto;
    }

    // If this is a Vector of constructor types, look for 'name' recursively.
    // Make sure to check for the case where name is the default (the empty
    // string). 9/1/98 jhrg
    if (d_proto->is_constructor_type()) {
        BaseType *result = d_proto->var(name, exact, s);
        if (result && s)
            s->push(this);
        return result;
    }

    return NULL;
}

/** This version of var(...) searches for <i>name</i> and returns a
 pointer to the BaseType object if found. It uses the same search
 algorithm as above when <i>exact_match</i> is false. In addition to
 returning a pointer to the variable, it pushes onto <i>s</i> a
 BaseType pointer to each constructor type that ultimately contains
 <i>name</i>.

 @param n Find the variable whose name is <i>name</i>.
 @param s Record the path to <i>name</i>.
 @return A pointer to the named variable. */
BaseType *Vector::var(const string & n, btp_stack & s)
{
    string name = www2id(n);

    if (d_proto->is_constructor_type())
        return d_proto->var(name, s);
    else {
        s.push((BaseType *) this);
        return d_proto;
    }
}

/** Returns a pointer to the specified Vector element.  The return
 pointer will reference the element itself, so multiple calls to this
 method should save each value before making the next call.

 @param i The index of the desired Vector element.  Zero
 indicates the first element of the Vector.
 @return A pointer to a BaseType class instance containing
 the value of the indicated element. The BaseType pointer is locally
 maintained and should not be deleted or referenced. Extract the value
 right after the method returns.
 @see BaseType::var */
BaseType *Vector::var(unsigned int i)
{

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:

        case dods_enum_c:

        case dods_float32_c:
        case dods_float64_c:
            // Transfer the ith value to the BaseType *d_proto
            d_proto->val2buf(d_buf + (i * d_proto->width()));
            return d_proto;

        case dods_str_c:
        case dods_url_c:
            d_proto->val2buf(&d_str[i]);
            return d_proto;

        case dods_opaque_c:
        case dods_array_c:
        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            return d_compound_buf[i];

        default:
            throw Error ("Vector::var: Unrecognized type");
    }
}

/** Returns the number of bytes needed to hold the entire
 array.  This is equal to \c length() (the number of elements in
 in the array) times the width of each
 element.

 @brief Returns the width of the data, in bytes. */
uint64_t Vector::width(bool constrained) const
{
    // Jose Garcia
	assert(d_proto);

    return  static_cast<uint64_t>(length() * d_proto->width(constrained));
}

/** Returns the number of elements in the vector. Note that some
 child classes of Vector use the length of -1 as a flag value.

 @see Vector::append_dim */
uint64_t Vector::length() const
{
    return d_length;
}

/** Sets the length of the vector.  This function does not allocate
 any new space. */
void Vector::set_length(uint64_t l)
{
    d_length = l;
    d_length_set = true;
}

/** Resizes a Vector.  If the input length is greater than the
 current length of the Vector, new memory is allocated (the
 Vector moved if necessary), and the new entries are appended to
 the end of the array and padded with Null values.  If the input
 length is shorter, the tail values are discarded.

 @note This method is applicable to the compound types only.
 */
void Vector::vec_resize(uint64_t l)
{
    // I added this check, which alters the behavior of the method. jhrg 8/14/13
    if (m_is_cardinal_type())
        throw InternalErr(__FILE__, __LINE__, "Vector::vec_resize() is applicable to compound types only");

    // Use resize() since other parts of the code use operator[]. Note that size() should
    // be used when resize() is used. Using capacity() creates problems as noted in the
    // comment in set_vec_nocopy(). jhrg 5/19/17
    d_compound_buf.resize(l, 0); // Fill with NULLs
    d_capacity = d_compound_buf.size(); // size in terms of number of elements.
}

/** @brief read data into a variable for later use

 Most uses of a variable are to either serialize its data to a stream of
 some sort or to read values from some stream and intern those in the
 variable for later use. These operations are perform by serialize()
 and deserialize() which follow. This function performs essentially both
 of these operations without actually using a stream device. The data are
 read using the read() method(s) and loaded into the variables directly.

 This method is intended to be used by objects which transform DAP objects
 like the DataDDS into an ASCII CSV representation.

 @note A DAP2-only method

 @param eval A reference to a constraint evaluator
 @param dds The complete DDS to which this variable belongs */
void Vector::intern_data(ConstraintEvaluator &eval, DDS &dds)
{
    DBG(cerr << "Vector::intern_data: " << name() << endl);
    if (!read_p())
        read(); // read() throws Error and InternalErr

    // length() is not capacity; it must be set explicitly in read().
    uint64_t num = length();

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_float32_c:
        case dods_float64_c:
            // For these cases, read() puts the data into d_buf,
        	// which is what we need.
            break;

        case dods_str_c:
        case dods_url_c:
            // For these cases, read() will put the data into d_str[],
        	// which is also what we need.
            break;

        case dods_array_c:
            // This is an error since there can never be an Array of Array.
            throw InternalErr(__FILE__, __LINE__, "Array of Array not supported.");

        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            DBG(cerr << "Vector::intern_data: found ctor" << endl);
            // For these cases, we need to call read() for each of the 'num'
            // elements in the 'd_compound_buf[]' array of BaseType object pointers.
            //
            // I changed the test here from '... = 0' to '... < num' to accommodate
            // the case where the array is zero-length.
            if (d_compound_buf.capacity() < num)
                throw InternalErr(__FILE__, __LINE__, "The capacity of this Vector is less than the number of elements.");

            for (uint64_t i = 0; i < num; ++i)
                d_compound_buf[i]->intern_data(eval, dds);

            break;

        default:
            throw InternalErr(__FILE__, __LINE__, "Unknown datatype.");
    }
}

/** @brief Serialize a Vector.

 This uses the Marshaler class to encode each element of a cardinal
 array. For Arrays of Str and Url types, send the element count over
 as a prefix to the data so that deserialize will know how many elements
 to read.

 NB: Arrays of cardinal types must already be in BUF (in the local machine's
 representation) <i>before</i> this call is made.
 */

bool Vector::serialize(ConstraintEvaluator & eval, DDS & dds, Marshaller &m, bool ce_eval)
{
    // Added to streamline zero-length arrays. Not needed for correct function,
    // but explicitly handling this case here makes the code easier to follow.
    // In libdap::Vector::val2buf() there is a test that will catch the zero-length
    // case as well. We still need to call serialize since it will write size
    // information that the client depends on. jhrg 2/17/16
    if (length() == 0)
        set_read_p(true);
    else if (!read_p())
        read(); // read() throws Error and InternalErr

    if (ce_eval && !eval.eval_selection(dds, dataset()))
        return true;

    // length() is not capacity; it must be set explicitly in read().
    uint64_t num = length();

    bool status = false;

    switch (d_proto->type()) {
        case dods_byte_c:
            m.put_vector(d_buf, num, *this);
            status = true;
            break;

        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_float32_c:
        case dods_float64_c:
            m.put_vector(d_buf, num, d_proto->width(), *this);
            status = true;

            break;

        case dods_str_c:
        case dods_url_c:
            if (d_str.capacity() == 0)
                throw InternalErr(__FILE__, __LINE__, "The capacity of the string vector is 0");

            m.put_int(num);

            for (uint64_t i = 0; i < num; ++i)
                m.put_str(d_str[i]);

            status = true;
            break;

        case dods_array_c:
        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            //Jose Garcia
            // Not setting the capacity of d_compound_buf is an internal error.
            if (d_compound_buf.capacity() == 0)
                throw InternalErr(__FILE__, __LINE__, "The capacity of *this* vector is 0.");

            m.put_int(num);
            status = true;
            for (uint64_t i = 0; i < num && status; ++i)
                status = status && d_compound_buf[i]->serialize(eval, dds, m, false);

            break;

        default:
            throw InternalErr(__FILE__, __LINE__, "Unknown datatype.");
    }

#ifdef CLEAR_LOCAL_DATA
    clear_local_data();
#endif

    return status;
}

// Read an object from the network and internalize it. For a Vector this is
// handled differently for a `cardinal' type. Vectors of Cardinals are
// stored using the `C' representations because these objects often are used
// to build huge arrays (e.g., an array of 1024 by 1024 bytes). However,
// arrays of non-cardinal types are stored as Vectors of the C++ objects or
// DAP2 objects (Str and Url are vectors of the string class, Structure, ...,
// Grid are vectors of the libdap Structure, ... classes).
//
// The boolean parameter REUSE determines whether internal storage is reused
// or not. If true, the _buf member is assumed to be large enough to hold the
// incoming cardinal data and is *not* reallocated. If false, new storage is
// allocated. If the internal buffer has not yet been allocated, then this
// parameter has no effect (i.e., storage is allocated). This parameter
// effects storage for cardinal data only.
//
// Returns: True is successful, false otherwise.

bool Vector::deserialize(UnMarshaller &um, DDS * dds, bool reuse)
{
    int vec_num;
    uint64_t num;
    uint64_t i = 0;

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_float32_c:
        case dods_float64_c:
            um.get_int((int &) vec_num);

            DBG(cerr << "Vector::deserialize: num = " << num << endl);
            DBG(cerr << "Vector::deserialize: length = " << length() << endl);

            num = static_cast<uint64_t>(vec_num);
            if (!d_length_set)
                set_length(num);

            if (num != length())
                throw InternalErr(__FILE__, __LINE__, "The server sent declarations and data with mismatched sizes for the variable '" + name() + "'.");

            if (!d_buf || !reuse) {
                // Make d_buf be large enough for length() elements of _var->type()
            	// m_create...() deletes the old buffer.
                m_create_cardinal_data_buffer_for_type(length());
                DBG(cerr << "Vector::deserialize: allocating "
                        << width() << " bytes for an array of "
                        << length() << " " << d_proto->type_name() << endl);
            }

            // Added to accommodate zero-length arrays.
            // Note that the rest of the cases will just send the size without data
            // but that these calls trigger error testing in the UnMarshaller code.
            // jhrg 1/28/16
            if (num == 0)
                return true;

            if (d_proto->type() == dods_byte_c)
                um.get_vector((char **) &d_buf, num, *this);
            else
                um.get_vector((char **) &d_buf, num, d_proto->width(), *this);

            DBG(cerr << "Vector::deserialize: read " << num << " elements\n");

            break;

        case dods_str_c:
        case dods_url_c:
            um.get_int((int &) vec_num);

            num = static_cast<uint64_t>(vec_num);
            if (!d_length_set)
                set_length(num);

            if (num != length())
                throw InternalErr(__FILE__, __LINE__, "The client sent declarations and data with mismatched sizes.");

            d_str.resize((num > 0) ? num : 0); // Fill with NULLs
            d_capacity = num; // capacity is number of strings we can fit.

            for (i = 0; i < num; ++i) {
                string str;
                um.get_str(str);
                d_str[i] = str;

            }

            break;

        case dods_array_c:
            // Added jhrg 5/18/17
            // This replaces a comment that was simply 'TO DO'
            throw InternalErr(__FILE__, __LINE__, "Array of array!");

        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            um.get_int((int &) vec_num);
            num = static_cast<uint64_t>(vec_num);

            if (!d_length_set)
                set_length(num);

            if (num != length())
                throw InternalErr(__FILE__, __LINE__, "The client sent declarations and data with mismatched sizes.");

            vec_resize(num);

            for (i = 0; i < num; ++i) {
                d_compound_buf[i] = d_proto->ptr_duplicate();
                d_compound_buf[i]->deserialize(um, dds);
            }

            break;

        default:
            throw InternalErr(__FILE__, __LINE__, "Unknown type!");
    }

    return false;
}

void Vector::compute_checksum(Crc32 &checksum)
{
    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:

        case dods_int16_c:
        case dods_uint16_c:

        case dods_int32_c:
        case dods_uint32_c:
        case dods_float32_c:

        case dods_int64_c:
        case dods_uint64_c:
        case dods_float64_c:

        case dods_enum_c:
        	checksum.AddData(reinterpret_cast<uint8_t*>(d_buf), length() * d_proto->width());
        	break;

        case dods_str_c:
        case dods_url_c:
        	for (uint64_t i = 0, e = length(); i < e; ++i)
        		checksum.AddData(reinterpret_cast<const uint8_t*>(d_str[i].data()), d_str[i].length());
            break;

        case dods_opaque_c:
        case dods_structure_c:
        case dods_sequence_c:
        	d_proto->compute_checksum(checksum);
        	break;

        case dods_array_c:	// No array of array
        case dods_grid_c:	// No grids in DAP4
        default:
            throw InternalErr(__FILE__, __LINE__, "Unknown or unsupported datatype (" + d_proto->type_name() + ").");
    }
}

void Vector::intern_data(/*Crc32 &checksum, DMR &dmr, ConstraintEvaluator &eval*/)
{
    if (!read_p())
        read(); // read() throws Error and InternalErr

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:

        case dods_enum_c:

        case dods_float32_c:
        case dods_float64_c:

        case dods_str_c:
        case dods_url_c:
#if 0
        	compute_checksum(checksum);
#endif
            break;

        case dods_opaque_c:
        case dods_structure_c:
        case dods_sequence_c:
            // Modified the assert here from '... != 0' to '... >= length())
            // to accommodate the case of a zero-length array. jhrg 1/28/16
            assert(d_compound_buf.capacity() >= length());

            for (uint64_t i = 0, e = length(); i < e; ++i)
                d_compound_buf[i]->intern_data(/*checksum, dmr, eval*/);
            break;

        case dods_array_c:      // No Array of Array in DAP4 either...
        case dods_grid_c:
        default:
        	throw InternalErr(__FILE__, __LINE__, "Unknown or unsupported datatype (" + d_proto->type_name() + ").");
    }
}

void
Vector::serialize(D4StreamMarshaller &m, DMR &dmr, bool filter /*= false*/)
{
    if (!read_p())
        read(); // read() throws Error and InternalErr
#if 0
    if (filter && !eval.eval_selection(dmr, dataset()))
        return true;
#endif
    uint64_t num = length();	// The constrained length in elements

    DBG(cerr << __func__ << ", num: " << num << endl);

    // Added in case we're trying to serialize a zero-length array. jhrg 1/27/16
    if (num == 0)
        return;

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
            m.put_vector(d_buf, num);
            break;

        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:
        	m.put_vector(d_buf, num, d_proto->width());
        	break;

        case dods_enum_c:
        	if (d_proto->width() == 1)
        		m.put_vector(d_buf, num);
        	else
        		m.put_vector(d_buf, num, d_proto->width());
        	break;

        case dods_float32_c:
            m.put_vector_float32(d_buf, num);
            break;

        case dods_float64_c:
            m.put_vector_float64(d_buf, num);
            break;

        case dods_str_c:
        case dods_url_c:
            assert((uint64_t)d_str.capacity() >= num);

            for (uint64_t i = 0; i < num; ++i)
                m.put_str(d_str[i]);

            break;

        case dods_array_c:
        	throw InternalErr(__FILE__, __LINE__, "Array of Array not allowed.");

        case dods_opaque_c:
        case dods_structure_c:
        case dods_sequence_c:
            assert(d_compound_buf.capacity() >= 0);

            for (uint64_t i = 0; i < num; ++i) {
                DBG(cerr << __func__ << "d_compound_buf[" << i << "] " << d_compound_buf[i] << endl);
                d_compound_buf[i]->serialize(m, dmr, filter);
            }

            break;

        case dods_grid_c:
        	throw InternalErr(__FILE__, __LINE__, "Grid is not part of DAP4.");

        default:
            throw InternalErr(__FILE__, __LINE__, "Unknown datatype.");
    }

#ifdef CLEAR_LOCAL_DATA
    clear_local_data();
#endif
}

void
Vector::deserialize(D4StreamUnMarshaller &um, DMR &dmr)
{
    if (m_is_cardinal_type()) {
        if (d_buf)
            m_delete_cardinal_data_buffer();
        if (!d_buf)
            m_create_cardinal_data_buffer_for_type(length());
    }

    DBG(cerr << __FUNCTION__ << name() << ", length(): " << length() << endl);

    // Added in case we're trying to deserialize a zero-length array. jhrg 1/27/16
    if (length() == 0)
        return;

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
        	um.get_vector((char *)d_buf, length());
        	break;

        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:
        	um.get_vector((char *)d_buf, length(), d_proto->width());
        	break;

        case dods_enum_c:
        	if (d_proto->width() == 1)
        		um.get_vector((char *)d_buf, length());
        	else
        		um.get_vector((char *)d_buf, length(), d_proto->width());
        	break;

        case dods_float32_c:
            um.get_vector_float32((char *)d_buf, length());
            break;

        case dods_float64_c:
        	um.get_vector_float64((char *)d_buf, length());
            break;

        case dods_str_c:
        case dods_url_c: {
        	uint64_t len = length();
            d_str.resize((len > 0) ? len : 0); // Fill with NULLs
            d_capacity = len; // capacity is number of strings we can fit.

            for (uint64_t i = 0; i < len; ++i) {
                um.get_str(d_str[i]);
            }

            break;
        }

        case dods_array_c:
        	throw InternalErr(__FILE__, __LINE__, "Array of Array not allowed.");

        case dods_opaque_c:
        case dods_structure_c:
        case dods_sequence_c: {
            vec_resize(length());

            for (uint64_t i = 0, end = length(); i < end; ++i) {
                d_compound_buf[i] = d_proto->ptr_duplicate();
                d_compound_buf[i]->deserialize(um, dmr);
            }

            break;
        }

        case dods_grid_c:
        	throw InternalErr(__FILE__, __LINE__, "Grid is not part of DAP4.");

        default:
            throw InternalErr(__FILE__, __LINE__, "Unknown type.");
    }
}

/** Copies data into the class instance buffer.  This function
 assumes that the input \e val points to memory which
 contains, in row major order, enough elements of the correct
 type to fill the array. For an array of a cardinal type the
 memory is simply copied in whole into the Vector buffer.

 If the variable has already been constrained, this method will load only
 number of values/bytes specified by that constraint and will load them
 into the 'front' of the object's internal buffer. This is where serialize()
 expects to find the data.

 For a Vector of Str (OPeNDAP Strings), this assumes \e val points to an
 array of C++ strings.

 This method should not be used for Structure, Sequence or Grid.

 @brief Reads data into the Vector buffer.
 @exception InternalErr Thrown if called for Structure, Sequence or
 Grid.
 @return The number of bytes used by the array.
 @param val A pointer to the input data.
 @param reuse A boolean value, indicating whether the class
 internal data storage can be reused or not.  If this argument is
 TRUE, the class buffer is assumed to be large enough to hold the
 incoming data, and it is <i>not</i> reallocated.  If FALSE, new
 storage is allocated.  If the internal buffer has not been
 allocated at all, this argument has no effect. */
uint64_t Vector::val2buf(void *val, bool reuse)
{
    // Jose Garcia

    // Added for zero-length arrays - support in the handlers. jhrg 1/29/16
    if (!val && length() == 0)
        return 0;

    // I *think* this method has been mainly designed to be use by read which
    // is implemented in the surrogate library. Passing NULL as a pointer to
    // this method will be an error of the creator of the surrogate library.
    // Even though I recognize the fact that some methods inside libdap++ can
    // call val2buf, I think by now no coding bugs such as misusing val2buf
    // will be in libdap++, so it will be an internal error from the
    // surrogate library.
    if (!val)
        throw InternalErr(__FILE__, __LINE__, "The incoming pointer does not contain any data.");

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:

        case dods_enum_c:

        case dods_float32_c:
        case dods_float64_c:
#if 0
        	if (d_buf && !reuse)
                m_delete_cardinal_data_buffer();
#endif
            // First time or no reuse (free'd above)
            if (!d_buf || !reuse)
                m_create_cardinal_data_buffer_for_type(length());

            // width(true) returns the size in bytes given the constraint
            memcpy(d_buf, val, width(true));
            break;

        case dods_str_c:
        case dods_url_c:
            // Assume val points to an array of C++ string objects. Copy
            // them into the vector<string> field of this object.
            // Note: d_length is the number of elements in the Vector
            d_str.resize(d_length);
            d_capacity = d_length;
            for (uint64_t i = 0; i < d_length; ++i)
                d_str[i] = *(static_cast<string *> (val) + i);

            break;

        default:
            throw InternalErr(__FILE__, __LINE__, "Vector::val2buf: bad type");

    }

    return width(true);
}

/**
 @brief Copies data from the Vector buffer.

 Copy data from a numeric or string arry to a buffer. This method will
 allocate memory if the handle @p val references NULL, otherwise it
 assumes the handle references enough storage for the data to be copied.

 Never call this method for constructor types Structure, Sequence or Grid.

 When reading data out of a variable that has been constrained, this method
 assumes the N values/bytes of constrained data start at the beginning
 of the object's internal buffer. For example, do not load an entire
 Vector's data using val2buf(), constrain and then use this method to
 get the data. Unless your constraint starts with the [0]th element, the
 result will not be the correct values.

 In the case of a Vector of Str objects, this method will return a
 pointer to an array of C++ std::string objects.

 @note It's best to define the pointer to reference the data as
 'char *data' or some other non-void type and then call this method
 using '..->buf2val((void**)&data)'. You must free the storage once
 you're done using 'delete[] data'.

 @note It's also important to initialize the handle to NULL. That is
 your code should declare the handle like this: 'char *data = 0' if
 it expects buf2val() to allocate memory. With most compilers, the
 pointer may be null the first time the code is run, but often not on
 subsequent calls.

 @return The number of bytes used to store the array.
 @param val A pointer to a pointer to the memory into which the
 class data will be copied.  If the value pointed to is NULL,
 memory will be allocated to hold the data, and the pointer value
 modified accordingly.  The calling program is responsible for
 deallocating the memory indicated by this pointer.
 @exception InternalErr Thrown if \e val is null.
 @see Vector::set_vec */
uint64_t Vector::buf2val(void **val)
{
    // Jose Garcia
    // The same comment in Vector::val2buf applies here!
    if (!val)
        throw InternalErr(__FILE__, __LINE__, "NULL pointer.");

    uint64_t wid = static_cast<uint64_t> (width(true /* constrained */));

    // This is the width computed using length(). The
    // length() property is changed when a projection
    // constraint is applied. Thus this is the number of
    // bytes in the buffer given the current constraint.

    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:

        case dods_enum_c:

        case dods_float32_c:
        case dods_float64_c:
            if (!d_buf)
                throw InternalErr(__FILE__, __LINE__, "Vector::buf2val: Logic error: called when cardinal type data buffer was empty!");
            if (!*val)
                *val = new char[wid];

            memcpy(*val, d_buf, wid);
            return wid;

        case dods_str_c:
        case dods_url_c: {
        	if (d_str.empty())
        		throw InternalErr(__FILE__, __LINE__, "Vector::buf2val: Logic error: called when string data buffer was empty!");
            if (!*val)
                *val = new string[d_length];

            for (uint64_t i = 0; i < d_length; ++i)
                *(static_cast<string *> (*val) + i) = d_str[i];

            return width();
        }

        default:
            throw InternalErr(__FILE__, __LINE__, "Vector::buf2val: bad type");
    }

    //return wid;
}

/** Sets an element of the vector to a given value.  If the type of
 the input and the type of the Vector do not match, an error
 condition is returned.

 Use this function only with Vectors containing compound
 types.  See \c buf2val() or the \c set_value() methods to access
 members of Vector containing simple types.

 @note This method copies \e val; the caller is responsible for deleting
 instance passed as the actual parameter.

 @brief Sets element <i>i</i> to value <i>val</i>.
 @return void
 @exception InternalErr Thrown if \e i is out of range, \e val is null or
 there was a type mismatch between the BaseType referenced by \e val and
 the \e ith element of this Vector.
 @param i The index of the element to be changed.
 @param val A pointer to the value to be inserted into the
 array.
 @see Vector::buf2val */
void Vector::set_vec(uint64_t i, BaseType * val)
{
	Vector::set_vec_nocopy(i, val->ptr_duplicate());
}

/**
 * @brief Sets element <i>i</i> to value <i>val</i>.
 * Set the ith element to val. Extend the vector if needed.
 *
 * @note It is best to call vec_resize() first and allocate enough elements
 * before calling this method.
 *
 * @note This method does not copy \e val; this class will free the instance
 * when the variable is deleted or when clear_local_data() is called.
 * @see Vector::set_vec()
 * */
void Vector::set_vec_nocopy(uint64_t i, BaseType * val)
{
    // Jose Garcia
    // This is a public method which allows users to set the elements
    // of *this* vector. Passing an invalid index, a NULL pointer or
    // mismatching the vector type are internal errors.
    if (i >= static_cast<unsigned int> (d_length))
        throw InternalErr(__FILE__, __LINE__, "Invalid data: index too large.");
    if (!val)
        throw InternalErr(__FILE__, __LINE__, "Invalid data: null pointer to BaseType object.");
    if (val->type() != d_proto->type())
        throw InternalErr(__FILE__, __LINE__, "invalid data: type of incoming object does not match *this* vector type.");

    // This code originally used capacity() instead of size(), but that was an error.
    // Use capacity() when using reserve() and size() when using resize(). Mixing
    // capacity() with resize() leaves holes in the data, where (pointer) values are
    // filled with nulls during successive calls to resize(). The resize() heuristic
    // remembers previous calls on a given vector<> and allocates larger than requested
    // blocks of memory on successive calls, which has the strange affect of erasing
    // values already in the vector in the parts just added.
    // jhrg 5/18/17
    if (i >= d_compound_buf.size()) {
        vec_resize(d_compound_buf.size() + 100);
    }

    d_compound_buf[i] = val;
}

/**
 * Remove any read or set data in the private data of this Vector,
 * setting read_p() to false.
 * Essentially clears the _buf, d_str, and d_compound_buf of any data.
 * Useful for tightening up memory when the data is no longer needed,
 * but the object cannot yet be destroyed.
 *
 * On exit: get_value_capacity() == 0 && !read_p()
 */
void Vector::clear_local_data()
{
    if (d_buf) {
        delete[] d_buf;
        d_buf = 0;
    }

    for (uint64_t i = 0; i < d_compound_buf.size(); ++i) {
        delete d_compound_buf[i];
        d_compound_buf[i] = 0;
    }

    // Force memory to be reclaimed.
    d_compound_buf.resize(0);
    d_str.resize(0);

    d_capacity = 0;
    set_read_p(false);
}

/**
 * Return the capacity of the Vector in terms of number of
 * elements of its data type that it can currently hold (i.e. not bytes).
 * For example, this could be
 * the size of the _buf array in bytes / sizeof(T) for the cardinal
 * types T, or the capacity of the d_str vector if T is string or url type.
 */
uint64_t Vector::get_value_capacity() const
{
    return d_capacity;
}

/**
 * Allocate enough memory for the Vector to contain
 * numElements data elements of the Vector's type.
 * Must be used before set_value_slice_from_row_major_vector
 * to ensure memory exists.
 * @param numElements  the number of elements of the Vector's type
 *                     to preallocate storage for.
 * @exception if the memory cannot be allocated
 */
void Vector::reserve_value_capacity(uint64_t numElements)
{
    if (!d_proto) {
        throw InternalErr(__FILE__, __LINE__, "reserve_value_capacity: Logic error: _var is null!");
    }
    switch (d_proto->type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_int8_c:
        case dods_uint8_c:
        case dods_int16_c:
        case dods_uint16_c:
        case dods_int32_c:
        case dods_uint32_c:
        case dods_int64_c:
        case dods_uint64_c:

        case dods_enum_c:

        case dods_float32_c:
        case dods_float64_c:
            // Make _buf be the right size and set _capacity
            m_create_cardinal_data_buffer_for_type(numElements);
            break;

        case dods_str_c:
        case dods_url_c:
            // Make sure the d_str has enough room for all the strings.
            // Technically not needed, but it will speed things up for large arrays.
            d_str.reserve(numElements);
            d_capacity = numElements;
            break;

        case dods_array_c:
            throw InternalErr(__FILE__, __LINE__, "reserve_value_capacity: Arrays not supported!");

        case dods_opaque_c:
        case dods_structure_c:
        case dods_sequence_c:
        case dods_grid_c:
            // not clear anyone will go this path, but best to be complete.
            d_compound_buf.reserve(numElements);
            d_capacity = numElements;
            break;

        default:
            throw InternalErr(__FILE__, __LINE__, "reserve_value_capacity: Unknown type!");
    } // switch

}

/**
 * Make sure there's storage allocated for the current length()
 * of the Vector.
 * Same as reserveValueCapacity(length())
 */
void Vector::reserve_value_capacity()
{
    // Use the current length of the vector as the reserve amount.
    reserve_value_capacity(length());
}

/**
 * Copy rowMajorData.length() elements currently in a rowMajorData buffer
 * into this value buffer starting at element index startElement and
 * continuing up to startElement+rowMajorData.length()-1
 *
 * This is used for aggregating together smaller rowMajor vectors
 * into a larger one.
 *
 * Note: unlike the other set_value calls, this does NOT set read_p()
 *       since it is assumed to be used as a partial read and the caller
 *       is expected to set_read_p() when the data is complete.
 *
 * ASSUMES: rowMajorData.read_p() so that the data is valid!
 * ASSUMES: this Vector has enough value_capacity() to contain
 *          all the elements such that:
 *          startElement + rowMajorData.length()
 *          <= this->value_capacity().
 * ASSUMES: the data type of this->var() and rowMajorData.var()
 *          MUST be non-NULL and be the same!
 *
 * @param rowMajorDataC the vector from which to copy data,
 *                     assumed already read in or set.
 * @param startElement the element index
 *                     (NOT byte, but rather data type element)
 *                     to place the first data value.
 * @return the number of elements added, such that:
 *         startElement + the return value is the next "free" element.
 */
uint64_t
Vector::set_value_slice_from_row_major_vector(const Vector& rowMajorDataC, uint64_t startElement)
{
	static const string funcName = "set_value_slice_from_row_major_vector:";

	// semantically const from the caller's viewpoint, but some calls are not syntactic const.
	Vector& rowMajorData = const_cast<Vector&>(rowMajorDataC);

	bool typesMatch = rowMajorData.var() && d_proto && (rowMajorData.var()->type() == d_proto->type());
	if (!typesMatch) {
		throw InternalErr(__FILE__, __LINE__, funcName + "Logic error: types do not match so cannot be copied!");
	}

	// Make sure the data exists
	if (!rowMajorData.read_p()) {
		throw InternalErr(__FILE__, __LINE__,
				funcName + "Logic error: the Vector to copy data from has !read_p() and should have been read in!");
	}

	// Check this otherwise the static_cast<unsigned int> below will do the wrong thing.
	if (rowMajorData.length() < 0) {
		throw InternalErr(__FILE__, __LINE__,
				funcName
						+ "Logic error: the Vector to copy data from has length() < 0 and was probably not initialized!");
	}

	// The read-in capacity had better be at least the length (the amount we will copy) or we'll memcpy into bad memory
	// I imagine we could copy just the capacity rather than throw, but I really think this implies a problem to be addressed.
	if (rowMajorData.get_value_capacity() < static_cast<unsigned int>(rowMajorData.length())) {
		throw InternalErr(__FILE__, __LINE__,
				funcName
						+ "Logic error: the Vector to copy from has a data capacity less than its length, can't copy!");
	}

	// Make sure there's enough room in this Vector to store all the elements requested.  Again,
	// better to throw than just copy what we can since it implies a logic error that needs to be solved.
	if (d_capacity < (startElement + rowMajorData.length())) {
		throw InternalErr(__FILE__, __LINE__,
				funcName + "Logic error: the capacity of this Vector cannot hold all the data in the from Vector!");
	}

	// OK, at this point we're pretty sure we can copy the data, but we have to do it differently depending on type.
	switch (d_proto->type()) {
		case dods_int8_c:
		case dods_uint8_c:
		case dods_byte_c:
        case dods_char_c:
		case dods_int16_c:
		case dods_uint16_c:
		case dods_int32_c:
		case dods_uint32_c:
		case dods_int64_c:
		case dods_uint64_c:

		case dods_enum_c:

		case dods_float32_c:
		case dods_float64_c: {
			if (!d_buf) {
				throw InternalErr(__FILE__, __LINE__, funcName + "Logic error: this->_buf was unexpectedly null!");
			}
			if (!rowMajorData.d_buf) {
				throw InternalErr(__FILE__, __LINE__, funcName + "Logic error: rowMajorData._buf was unexpectedly null!");
			}
			// memcpy the data into this, taking care to do ptr arithmetic on bytes and not sizeof(element)
			int varWidth = d_proto->width();
			char* pFromBuf = rowMajorData.d_buf;
			uint64_t numBytesToCopy = rowMajorData.width(true);
			char* pIntoBuf = d_buf + (startElement * varWidth);
			memcpy(pIntoBuf, pFromBuf, numBytesToCopy);
			break;
		}

		case dods_str_c:
		case dods_url_c:
			// Strings need to be copied directly
			for (uint64_t i = 0; i < static_cast<unsigned int>(rowMajorData.length()); ++i) {
				d_str[startElement + i] = rowMajorData.d_str[i];
			}
			break;

		case dods_array_c:
        case dods_opaque_c:
		case dods_structure_c:
		case dods_sequence_c:
		case dods_grid_c:
			// Not sure that this function will be used for these type of nested objects, so I will throw here.
			throw InternalErr(__FILE__, __LINE__,
					funcName + "Unimplemented method for Vectors of type: array, opaque, structure, sequence or grid.");

		default:
			throw InternalErr(__FILE__, __LINE__, funcName + ": Unknown type!");
	} // switch (_var->type())

	// This is how many elements we copied.
	return rowMajorData.length();
}

/**
 * Does the C++ type correspond to the DAP Type enum value? This works only for
 * numeric cardinal types. For Enums, pass the value of element_type(); for all
 * others use type().
 * @param t
 * @param dt
 * @return True if the types match, false otherwise
 */
template <typename T>
static bool types_match(Type t, T *cpp_var)
{
    switch (t) {
    case dods_byte_c:
    case dods_char_c:
    case dods_uint8_c:
        return typeid(cpp_var) == typeid(dods_byte*);

    case dods_int8_c:
        return typeid(cpp_var) == typeid(dods_int8*);
    case dods_int16_c:
        return typeid(cpp_var) == typeid(dods_int16*);
    case dods_uint16_c:
        return typeid(cpp_var) == typeid(dods_uint16*);
    case dods_int32_c:
        return typeid(cpp_var) == typeid(dods_int32*);
    case dods_uint32_c:
        return typeid(cpp_var) == typeid(dods_uint32*);
    case dods_int64_c:
        return typeid(cpp_var) == typeid(dods_int64*);
    case dods_uint64_c:
        return typeid(cpp_var) == typeid(dods_uint64*);

    case dods_float32_c:
        return typeid(cpp_var) == typeid(dods_float32*);
    case dods_float64_c:
        return typeid(cpp_var) == typeid(dods_float64*);

    case dods_null_c:
    case dods_enum_c:
    case dods_str_c:
    case dods_url_c:
    case dods_opaque_c:
    case dods_array_c:
    case dods_structure_c:
    case dods_sequence_c:
    case dods_group_c:
    default:
        return false;
    }
}

//@{
/** @brief set the value of a byte array */

template <typename T>
bool Vector::set_value_worker(T *v, int sz)
{
    if (!v || !types_match(d_proto->type() == dods_enum_c ? static_cast<D4Enum*>(d_proto)->element_type() : d_proto->type(), v))
        return false;

    m_set_cardinal_values_internal(v, sz);
    return true;
}

bool Vector::set_value(dods_byte *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_int8 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_int16 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_uint16 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_int32 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_uint32 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_int64 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_uint64 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_float32 *val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(dods_float64 *val, int sz)
{
    return set_value_worker(val, sz);
}

/**
 * @brief set the value of a string or url array
 * @param val An array of string objects
 * @param sz The number of elements in the string array
 * @return false if the type of the array is neither Str nor Url
 * or val is null, otherwise returns true.
 */
bool Vector::set_value(string *val, int sz)
{
    if ((var()->type() == dods_str_c || var()->type() == dods_url_c) && val) {
        d_str.resize(sz);
        d_capacity = sz;
        for (uint64_t t = 0; t < sz; t++) {
            d_str[t] = val[t];
        }
        set_length(sz);
        set_read_p(true);
        return true;
    }
    else {
        return false;
    }
}

template<typename T>
bool Vector::set_value_worker(vector<T> &v, int sz)
{
    return set_value(&v[0], sz);
}

bool Vector::set_value(vector<dods_byte> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_int8> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_int16> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_uint16> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_int32> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_uint32> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_int64> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_uint64> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_float32> &val, int sz)
{
    return set_value_worker(val, sz);
}
bool Vector::set_value(vector<dods_float64> &val, int sz)
{
    return set_value_worker(val, sz);
}


/** @brief set the value of a string or url array */
bool Vector::set_value(vector<string> &val, int sz)
{
    if (var()->type() == dods_str_c || var()->type() == dods_url_c) {
        d_str.resize(sz);
        d_capacity = sz;
        for (uint64_t t = 0; t < sz; t++) {
            d_str[t] = val[t];
        }
        set_length(sz);
        set_read_p(true);
        return true;
    }
    else {
        return false;
    }
}
//@}

//@{

/** @brief Get a copy of the data held by this variable using the passed subsetIndex
 * vector to identify which values to return.
 *
 * Read data from this variable's internal storage using the passed std::vector
 * as an sub-setting index to the values to be returned. For example, if \c subsetIndex
 * contains 1,3,5,7 and 9, then 'b' will contain the five values found at indexes
 * 1,3, ..., 9.
 *
 * @note The memory referenced by \c b must point to enough memory to hold index.size()
 * bytes; no test for this is performed.
 * @note This can only be called for cardinal types.
 *
 * @param index A std::vector<long> where each value in the vector is the
 * location in the Vector's internal storage from which to read the returned value.
 * @param b A pointer to the memory to hold the data; must be at least
 * length() * sizeof(dods_byte) in size.*/
template <typename T>
void Vector::value_worker(vector<unsigned int> *indices, T *b) const
{
   // unsigned long currentIndex;
#if 0
    // Iterator version. Not tested, jhrg 8/14/13
    for (vector<unsigned int>::iterator i = indices->begin(), e = indices->end(); i != e; ++i) {
        unsigned long currentIndex = *i;
        if(currentIndex > (unsigned int)length()){
            stringstream s;
            s << "Vector::value() - Subset index[" << i - subsetIndex->begin() <<  "] = " << currentIndex << " references a value that is " <<
                    "outside the bounds of the internal storage [ length()= " << length() << " ] name: '" << name() << "'. ";
            throw Error(s.str());
        }
        b[i - indices->begin()] = reinterpret_cast<T*>(d_buf )[currentIndex];
    }
#endif
    for (uint64_t i = 0, e = indices->size(); i < e; ++i) {
        uint64_t currentIndex = (*indices)[i];
        if (currentIndex > length()) {
            stringstream s;
            s << "Vector::value() - Subset index[" << i <<  "] = " << currentIndex << " references a value that is " <<
                    "outside the bounds of the internal storage [ length()= " << length() << " ] name: '" << name() << "'. ";
            throw Error(s.str());
        }
        b[i] = reinterpret_cast<T*>(d_buf )[currentIndex]; // I like this version - and it works!
    }
}
void Vector::value(vector<unsigned int> *indices, dods_byte *b) const    { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_int8 *b) const    { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_int16 *b) const   { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_uint16 *b) const  { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_int32 *b) const   { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_uint32 *b) const  { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_int64 *b) const   { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_uint64 *b) const  { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_float32 *b) const { value_worker(indices, b); }
void Vector::value(vector<unsigned int> *indices, dods_float64 *b) const { value_worker(indices, b); }

#if 0
template void Vector::value(vector<unsigned int> *indices, dods_byte *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_int8 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_int16 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_uint16 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_int32 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_uint32 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_int64 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_uint64 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_float32 *b) const;
template void Vector::value(vector<unsigned int> *indices, dods_float64 *b) const;
#endif

/** @brief Get a copy of the data held by this variable using the passed subsetIndex vector to identify which values to return. **/
void Vector::value(vector<unsigned int> *subsetIndex, vector<string> &b) const
{
    uint64_t currentIndex;

    if (d_proto->type() == dods_str_c || d_proto->type() == dods_url_c){
        for(uint64_t i=0; i<subsetIndex->size() ;++i){
            currentIndex = (*subsetIndex)[i] ;
            if(currentIndex > length()){
                stringstream s;
                s << "Vector::value() - Subset index[" << i <<  "] = " << currentIndex << " references a value that is " <<
                        "outside the bounds of the internal storage [ length()= " << length() << " ] name: '" << name() << "'. ";
                throw Error(s.str());
            }
            b[i] = d_str[currentIndex];
        }
    }
}

template <typename T>
void Vector::value_worker(T *v) const
{
    // Only copy if v is not null and the proto's  type matches.
    // For Enums, use the element type since type == dods_enum_c.
    if (v && types_match(d_proto->type() == dods_enum_c ? static_cast<D4Enum*>(d_proto)->element_type() : d_proto->type(), v))
        memcpy(v, d_buf, length() * sizeof(T));
}
void Vector::value(dods_byte *b) const    { value_worker(b); }
void Vector::value(dods_int8 *b) const    { value_worker(b); }
void Vector::value(dods_int16 *b) const   { value_worker(b); }
void Vector::value(dods_uint16 *b) const  { value_worker(b); }
void Vector::value(dods_int32 *b) const   { value_worker(b); }
void Vector::value(dods_uint32 *b) const  { value_worker(b); }
void Vector::value(dods_int64 *b) const   { value_worker(b); }
void Vector::value(dods_uint64 *b) const  { value_worker(b); }
void Vector::value(dods_float32 *b) const { value_worker(b); }
void Vector::value(dods_float64 *b) const { value_worker(b); }

#if 0
template void Vector::value(dods_byte *v) const;
template void Vector::value(dods_int8 *v) const;
template void Vector::value(dods_int16 *v) const;
template void Vector::value(dods_uint16 *v) const;
template void Vector::value(dods_int32 *v) const;
template void Vector::value(dods_uint32 *v) const;
template void Vector::value(dods_int64 *v) const;
template void Vector::value(dods_uint64 *v) const;
template void Vector::value(dods_float32 *v) const;
template void Vector::value(dods_float64 *v) const;
#endif


/** @brief Get a copy of the data held by this variable. */
void Vector::value(vector<string> &b) const
{
    if (d_proto->type() == dods_str_c || d_proto->type() == dods_url_c)
        b = d_str;
}

/** Allocate memory and copy data into the new buffer. Return the new
 buffer's pointer. The caller must delete the storage. */
void *Vector::value()
{
    void *buffer = new char[width(true)];

    memcpy(buffer, d_buf, width(true));

    return buffer;
}
//@}

/** @brief Add the BaseType pointer to this constructor type
 instance.

 Propagate the name of the BaseType instance to this instance. This
 ensures that variables at any given level of the DDS table have
 unique names (i.e., that Arrays do not have their default name ""). If
 <tt>v</tt>'s name is null, then assume that the array \e is named and
 don't overwrite it with <tt>v</tt>'s null name.

 @note As is the case with Array, this method can be called with a null
 BaseType pointer.

 @param v The template variable for the array
 @param p The Part parameter defaults to nil and is ignored by this method.
 */
void Vector::add_var(BaseType * v, Part /*p*/)
{
#if 0
	// Why doesn't this work?  tried all 3 variants. jhrg 8/14/13
	Vector::add_var_nocopy(v->ptr_duplicate(), p);
	add_var_nocopy(v->ptr_duplicate(), p);
	add_var_nocopy(v->ptr_duplicate());
#else
	// Delete the current template variable
    if (d_proto) {
        delete d_proto;
        d_proto = 0;
    }

    // if 'v' is null, just set _var to null and exit.
    if (!v) {
        d_proto = 0;
    }
    else {
        // Jose Garcia
        // By getting a copy of this object to be assigned to _var
        // we let the owner of 'v' to deallocate it as necessary.
        d_proto = v->ptr_duplicate();

        // If 'v' has a name, use it as the name of the array. If v doesn't have
        // a name, then make sure to copy the array's name to it
        // so that software which uses the template's name will still work.
        if (!v->name().empty())
            set_name(v->name());
        else
            d_proto->set_name(name());

        d_proto->set_parent(this); // Vector --> child

        DBG(cerr << "Vector::add_var: Added variable " << v << " ("
                << v->name() << " " << v->type_name() << ")" << endl);
    }
#endif
}

void Vector::add_var_nocopy(BaseType * v, Part)
{
	// Delete the current template variable
    if (d_proto) {
        delete d_proto;
        d_proto = 0;
    }

    // if 'v' is null, just set _var to null and exit.
    if (!v) {
        d_proto = 0;
    }
    else {
        d_proto = v;

        // If 'v' has a name, use it as the name of the array. If it *is*
        // empty, then make sure to copy the array's name to the template
        // so that software which uses the template's name will still work.
        if (!v->name().empty())
            set_name(v->name());
        else
            d_proto->set_name(name());

        d_proto->set_parent(this); // Vector is the parent; proto is the child

        DBG(cerr << "Vector::add_var_no_copy: Added variable " << v << " ("
                << v->name() << " " << v->type_name() << ")" << endl);
    }
}

bool Vector::check_semantics(string & msg, bool)
{
    return BaseType::check_semantics(msg);
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and information about this
 * instance.
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void Vector::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "Vector::dump - (" << (void *) this << ")" << endl;
    DapIndent::Indent();
    BaseType::dump(strm);
    strm << DapIndent::LMarg << "# elements in vector: " << d_length << endl;
    if (d_proto) {
        strm << DapIndent::LMarg << "base type:" << endl;
        DapIndent::Indent();
        d_proto->dump(strm);
        DapIndent::UnIndent();
    }
    else {
        strm << DapIndent::LMarg << "base type: not set" << endl;
    }
    strm << DapIndent::LMarg << "vector contents:" << endl;
    DapIndent::Indent();
    for (uint64_t i = 0; i < d_compound_buf.size(); ++i) {
        if (d_compound_buf[i])
            d_compound_buf[i]->dump(strm);
        else
            strm << DapIndent::LMarg << "vec[" << i << "] is null" << endl;
    }
    DapIndent::UnIndent();
    strm << DapIndent::LMarg << "strings:" << endl;
    DapIndent::Indent();
    for (uint64_t i = 0; i < d_str.size(); i++) {
        strm << DapIndent::LMarg << d_str[i] << endl;
    }
    DapIndent::UnIndent();
    if (d_buf) {
        switch (d_proto != 0 ? d_proto->type() : 0) {
            case dods_byte_c:
            case dods_char_c:
                strm << DapIndent::LMarg << "_buf: ";
                strm.write(d_buf, d_length);
                strm << endl;
                break;

            case 0:
            default:
                strm << DapIndent::LMarg << "_buf: " << (void *) d_buf << endl;
                break;
        }
    }
    else {
        strm << DapIndent::LMarg << "_buf: EMPTY" << endl;
    }

    DapIndent::UnIndent();
}

} // namespace libdap

