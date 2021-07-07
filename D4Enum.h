
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#ifndef _D4Enum_h
#define _D4Enum_h 1

#include <cassert>

#include "BaseType.h"
#include "dods-datatypes.h"

#if 0
#include "InternalErr.h"
#include "dods-datatypes.h"
#include "dods-limits.h"
#include "util.h"
#endif

namespace libdap
{

class D4EnumDef;
class ConstraintEvaluator;
class Marshaller;
class UnMarshaller;

/**
 * @brief Holds a DAP4 enumeration.
 *
 * @note When constructed a type for the Enum must be specified. If
 * it is not an integer type, the Enum will use unsigned int 64. This
 * is not the same as the enumeration type that is defined using the
 * Enumeration XML element in the DMR - that information is stored
 * in additional fields and used for checking values and printing the
 * variable's declaration, but not for the internal storage of values.
 *
 * @todo Note the hack to remove the union...
 */
class D4Enum: public BaseType
{
	friend class D4EnumTest;

protected:
    // Use an unsigned 64-bit int. the value() and set_value()
    // accessors cast to other types as needed, including signed ones.
    uint64_t d_buf;

private:
    Type d_element_type;
    D4EnumDef *d_enum_def;	// This is a weak pointer; don't delete
    bool d_is_signed;

    void m_duplicate(const D4Enum &src);
    void m_check_value(int64_t v) const;

    unsigned int m_type_width() const {
        switch(d_element_type) {
            case dods_byte_c:
            case dods_int8_c:
            case dods_uint8_c:
                return 1;
            case dods_int16_c:
            case dods_uint16_c:
                return 2;
            case dods_int32_c:
            case dods_uint32_c:
                return 4;
            case dods_int64_c:
            case dods_uint64_c:
                return 8;
            case dods_null_c:
            default:
            	assert(!"illegal type for D4Enum");
            	return 0;
        }
    }

    D4Enum();	// No empty constructor

public:
    D4Enum(const string &name, const string &enum_type);

    D4Enum(const string &name, Type type);

    D4Enum(const string &name, const string &dataset, Type type);

    D4Enum(const D4Enum &src) : BaseType(src) { m_duplicate(src); }

    D4Enum &operator=(const D4Enum &rhs) {
        if (this == &rhs)
            return *this;
        static_cast<BaseType &>(*this) = rhs;
        m_duplicate(rhs);
        return *this;
    }

    virtual ~D4Enum() { }

    virtual D4EnumDef *enumeration() const { return d_enum_def; }
    virtual void set_enumeration(D4EnumDef *enum_def);

    virtual BaseType *ptr_duplicate() { return new D4Enum(*this); }

    Type element_type() { return d_element_type; }
    void set_element_type(Type type) { d_element_type = type; }

    bool is_signed() const { return d_is_signed; }
    void set_is_signed(Type t);

	/**
	 * @brief Get the value of an Enum
	 * Get the value of this instance. The caller is responsible
	 * for using a type T than can hold the value.
	 *
	 * @param v Value-result parameter; return the value of the Enum
	 * in this variable.
	 */
	template<typename T> void value(T *v) const {
		*v = static_cast<T>(d_buf);
	}

    /**
     * @brief Set the value of the Enum
     * Template member function to set the value of the Enum. The libdap library
     * contains versions of this member function for dods_byte, ..., dods_uint64
     * types for the parameter \c v.
     *
     * @param v Set the Enum to this value.
     * @param check_value If true test the value 'v' against the type of the
     * Enum. Defaults to true.
     */
    template <typename T> void set_value(T v, bool check_value = true)
    {
    	if (check_value) m_check_value(v);
    	d_buf = static_cast<int64_t>(v);
    }

    /**
     * @brief Return the number of bytes in an instance of an Enum.
     * This returns the number of bytes an instance of Enum will use
     * in memory or on the wire (i.e., in a serialization of
     * the type). On the wire this type uses the minimum number of
     * bytes for the given Enum type - an Enum with type Byte uses
     * one byte, Int16 uses two, and so on. In memory, a single instance
     * uses 64-bits but a vector of these will use the same number of
     * bytes per value as the on-the-wire representation.
     *
     * @note The private method m_type_width() returns the byte width
     * used for the on-the-wire representation of values.
     *
     * @return The number of bytes used by a value.
     */
    virtual uint64_t width(bool /* constrained */ = false) const { return /*sizeof(int64_t);*/ static_cast<uint64_t>(m_type_width());}

    // DAP4
    virtual void compute_checksum(Crc32 &checksum);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    virtual void print_val(ostream &out, string space = "", bool print_decl_p = true);

    virtual void print_xml_writer(XMLWriter &xml, bool constrained);

    virtual bool ops(BaseType *b, int op);

    virtual void dump(ostream &strm) const;

    uint64_t val2buf(void *, bool);
    uint64_t buf2val(void **);

    virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table);

};

} // namespace libdap

#endif // _D4Enum_h

