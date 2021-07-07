
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

#include "config.h"

// #define DODS_DEBUG

#include <cassert>
#include <sstream>

#include <libxml/encoding.h>

#include "Byte.h"           // synonymous with UInt8 and Char
#include "Int8.h"
#include "Int16.h"
#include "UInt16.h"
#include "Int32.h"
#include "UInt32.h"
#include "Int64.h"
#include "UInt64.h"

#include "D4Group.h"
#include "D4Enum.h"
#include "D4EnumDefs.h"
#include "D4Attributes.h"

#include "Float32.h"
#include "Float64.h"

#include "D4StreamMarshaller.h"
#include "D4StreamUnMarshaller.h"

#include "Operators.h"
#include "InternalErr.h"
#include "dods-datatypes.h"
#include "dods-limits.h"
#include "util.h"
#include "debug.h"
#include "DapIndent.h"

using std::cerr;
using std::endl;

namespace libdap {

// Private
void D4Enum::m_duplicate(const D4Enum &src)
{
    d_buf = src.d_buf;
    d_element_type = src.d_element_type;
    d_enum_def = src.d_enum_def;
#if 0
    // The enum_def is a weak pointer managed by D4Group. We just copy it
    // and do not delete it. jhrg 10/19/15
    d_enum_def = src.d_enum_def == 0 ? 0 : new D4EnumDef(*(src.d_enum_def));
#endif
    d_is_signed = src.d_is_signed;
}

/**
 * @brief Convert an Enum to a DAP2 int type
 *
 * Build a DAP2 integer type that matches the D4Enum's internal type and then
 * add an attribute table to that new variable that holds the D4Enum's symbolic
 * name and value mapping.
 *
 * @param attr AttrTable pointer, ignored by this method.
 */
vector<BaseType *> *
D4Enum::transform_to_dap2(AttrTable *)
{
    BaseType *btp;

    DBG(cerr << __func__ << "() - BEGIN" << endl;);

    switch (d_element_type) {
    case dods_byte_c:
    case dods_int8_c:
    case dods_uint8_c: {
        Byte *var = new Byte(name());
        dods_byte val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    case dods_uint16_c: {
        UInt16 *var = new UInt16(name());
        dods_uint16 val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    case dods_uint32_c: {
        UInt32 *var = new UInt32(name());
        dods_uint32 val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    case dods_uint64_c: {
        UInt64 *var = new UInt64(name());
        dods_uint64 val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    case dods_int16_c: {
        Int16 *var = new Int16(name());
        dods_int16 val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    case dods_int32_c: {
        Int32 *var = new Int32(name());
        dods_int32 val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    case dods_int64_c: {
        Int64 *var = new Int64(name());
        dods_int64 val;
        this->value(&val);
        var->set_value(val);
        btp = var;
        break;
    }
    default: {
        ostringstream oss;
        oss << "Unknown D4Enum type:" << d_element_type << ", name: " << name() << endl;
        throw InternalErr(__FILE__, __LINE__, oss.str());
    }
    }

    DBG( cerr << __func__ << "() - Processing Enum  type:"<<
        btp->type_name() << " name: " << btp->name() << endl;);

    // Grab the attributes!
#if 0
    AttrTable d2_attrs = *(this->attributes()->get_AttrTable(name()));
    btp->set_attr_table(d2_attrs);
#else
    if (btp->get_attr_table().get_size() == 0) {
        attributes()->transform_attrs_to_dap2(&btp->get_attr_table());
        btp->get_attr_table().set_name(name());
    }
#endif

    // make the Enum label, and the enum's definition into DAP2 attributes for our returned item.
    long long my_value;
    this->value(&my_value);

    DBG(cerr << __func__ << "() - value: "<< my_value << endl;);

    string my_label = "";
    AttrTable *enum_def = new AttrTable();
    enum_def->set_name("d4:enum_def");

    D4EnumDef::D4EnumValueIter dIter = d_enum_def->value_begin();
    D4EnumDef::D4EnumValueIter dEnd = d_enum_def->value_end();
    while (dIter != dEnd) {
        long long a_value = (*dIter).value;
        string a_label = (*dIter).label;
        ostringstream oss;
        oss << a_value;
        DBG(cerr << __func__ << "() - a_value: "<< a_value << endl;);
        enum_def->append_attr(a_label, btp->type_name(), oss.str());
        if (a_value == my_value) {
            my_label = (*dIter).label;
        }
        dIter++;
    }

    if (!my_label.empty()) btp->get_attr_table().append_attr("d4:enum_label", "String", my_label);

    btp->get_attr_table().append_container(enum_def, enum_def->get_name());

    vector<BaseType *> *result = new vector<BaseType *>();
    result->push_back(btp);
    DBG(cerr << __func__ << "() - END" << endl;);
    return result;
}

void D4Enum::m_check_value(int64_t v) const
{
	switch (d_element_type) {
	case dods_byte_c:
	case dods_uint8_c:
		if ((uint64_t)v > DODS_UCHAR_MAX || v < 0) {
			ostringstream oss;
			oss << "The value " << v << " will not fit in an unsigned byte. (" << __func__ << ")";
			throw Error(oss.str());
		}
		break;
	case dods_uint16_c:
		if ((uint64_t)v > DODS_USHRT_MAX || v < 0) {
			ostringstream oss;
			oss << "The value " << v << " will not fit in an unsigned 16-bit integer. (" << __func__ << ")";
			throw Error(oss.str());
		}
		break;
	case dods_uint32_c:
		if ((uint64_t)v > DODS_UINT_MAX || v < 0) {
			ostringstream oss;
			oss << "The value " << v << " will not fit in an unsigned 32-bit integer. (" << __func__ << ")";
			throw Error(oss.str());
		}
		break;
	case dods_uint64_c:
		// If 'v' can never be bigger than ULLONG_MAX
		break;

        case dods_int8_c:
		if (v > DODS_SCHAR_MAX || v < DODS_SCHAR_MIN) {
			ostringstream oss;
			oss << "The value " << v << " will not fit in an unsigned byte. (" << __func__ << ")";
			throw Error(oss.str());
		}

		break;
	case dods_int16_c:
		if (v > DODS_SHRT_MAX || v < DODS_SHRT_MIN) {
			ostringstream oss;
			oss << "The value " << v << " will not fit in an unsigned byte. (" << __func__ << ")";
			throw Error(oss.str());
		}
		break;
	case dods_int32_c:
		if (v > DODS_INT_MAX || v < DODS_INT_MIN) {
			ostringstream oss;
			oss << "The value " << v << " will not fit in an unsigned byte. (" << __func__ << ")";
			throw Error(oss.str());
		}
		break;
	case dods_int64_c:
		// There's no value 'v' can have that won't fit into a 64-bit int.
		break;
	default:
		assert(!"illegal type for D4Enum");
	}
}

D4Enum::D4Enum(const string &name, const string &enum_type) :
    BaseType(name, dods_enum_c, true /*is_dap4*/), d_buf(0), d_element_type(dods_null_c), d_enum_def(0)
{
    d_element_type = get_type(enum_type.c_str());

    if (!is_integer_type(d_element_type)) d_element_type = dods_uint64_c;
    set_is_signed(d_element_type);
}

D4Enum::D4Enum(const string &name, Type type) :
    BaseType(name, dods_enum_c, true /*is_dap4*/), d_buf(0), d_element_type(type), d_enum_def(0)
{
    if (!is_integer_type(d_element_type)) d_element_type = dods_uint64_c;
    set_is_signed(d_element_type);
}

D4Enum::D4Enum(const string &name, const string &dataset, Type type) :
    BaseType(name, dataset, dods_enum_c, true /*is_dap4*/), d_buf(0), d_element_type(type), d_enum_def(0)
{
    if (!is_integer_type(d_element_type)) d_element_type = dods_uint64_c;
    set_is_signed(d_element_type);
}

// Explicit instantiation of the template member function 'value(T *)'.
// This is required in order to have the library contain these member
// functions when its own code does not use them. Normally, C++ instantiates
// templates when they are used, and this forces that process so the
// library file contains the various versions of the member function.
template void D4Enum::value<dods_byte>(dods_byte *v) const;
template void D4Enum::value<dods_int16>(dods_int16 *v) const;
template void D4Enum::value<dods_uint16>(dods_uint16 *v) const;
template void D4Enum::value<dods_int32>(dods_int32 *v) const;
template void D4Enum::value<dods_uint32>(dods_uint32 *v) const;
template void D4Enum::value<dods_int64>(dods_int64 *v) const;
template void D4Enum::value<dods_uint64>(dods_uint64 *v) const;

template void D4Enum::set_value<dods_byte>(dods_byte v, bool check_value);
template void D4Enum::set_value<dods_int16>(dods_int16 v, bool check_value);
template void D4Enum::set_value<dods_uint16>(dods_uint16 v, bool check_value);
template void D4Enum::set_value<dods_int32>(dods_int32 v, bool check_value);
template void D4Enum::set_value<dods_uint32>(dods_uint32 v, bool check_value);
template void D4Enum::set_value<dods_int64>(dods_int64 v, bool check_value);
template void D4Enum::set_value<dods_uint64>(dods_uint64 v, bool check_value);

void
D4Enum::set_enumeration(D4EnumDef *enum_def) {
    d_enum_def = enum_def;
    d_element_type = enum_def->type();
}

void
D4Enum::compute_checksum(Crc32 &checksum)
{
	DBG(cerr << __func__ << ": element type: " << ::libdap::type_name(d_element_type) << endl);

    switch (d_element_type) {
    case dods_byte_c:
    case dods_uint8_c:
    case dods_int8_c: {
    	dods_byte v = static_cast<dods_byte>(d_buf);
        checksum.AddData(reinterpret_cast<uint8_t*>(&v), sizeof(uint8_t));
        break;
    }
    case dods_uint16_c:
    case dods_int16_c: {
    	dods_int16 v = static_cast<dods_int16>(d_buf);
        checksum.AddData(reinterpret_cast<uint8_t*>(&v), sizeof(uint16_t));
        break;
    }
    case dods_uint32_c:
    case dods_int32_c: {
    	dods_int32 v = static_cast<dods_int32>(d_buf);
        checksum.AddData(reinterpret_cast<uint8_t*>(&v), sizeof(uint32_t));
        break;
    }
    case dods_uint64_c:
    case dods_int64_c:
        checksum.AddData(reinterpret_cast<uint8_t*>(&d_buf), sizeof(uint64_t));
        break;

    default:
        assert(!"illegal type for D4Enum");
    }
}

void
D4Enum::set_is_signed(Type t)
{
	switch (t) {
	case dods_byte_c:
	case dods_uint8_c:
	case dods_uint16_c:
	case dods_uint32_c:
	case dods_uint64_c:
		d_is_signed = false;
		break;

	case dods_int8_c:
	case dods_int16_c:
	case dods_int32_c:
	case dods_int64_c:
		d_is_signed =  true;
		break;

	default:
#if 0
	    // I removed this because it hinders testing and is no better
	    // than throwing an exception. jhrg 3/29/18
	    assert(!"illegal type for D4Enum");
#endif
		throw InternalErr(__FILE__, __LINE__, "Illegal type");
	}
}

/**
 * @brief Serialize a D4Enum
 * Use the (integer) data type associated with an Enumeration definition to
 * serialize the value of a D4Enum variable. This send just the bits that
 * correspond to the declared type, not all 64-bits of storage used by a
 * scalar D4Enum.
 * @param m
 * @param dmr Unused
 * @param eval Unused
 * @param filter Unused
 * @exception Error is thrown if the value needs to be read and that operation fails.
 */
void
D4Enum::serialize(D4StreamMarshaller &m, DMR &, /*ConstraintEvaluator &,*/ bool)
{
    if (!read_p())
        read();          // read() throws Error

	switch (d_element_type) {
	case dods_byte_c:
	case dods_uint8_c:
		m.put_byte(d_buf);
		break;
	case dods_uint16_c:
		m.put_uint16(d_buf);
		break;
	case dods_uint32_c:
		m.put_uint32(d_buf);
		break;
	case dods_uint64_c:
		m.put_uint64(d_buf);
		break;

	case dods_int8_c:
		m.put_int8(d_buf);
		break;
	case dods_int16_c:
		m.put_int16(d_buf);
		break;
	case dods_int32_c:
		m.put_int32(d_buf);
		break;
	case dods_int64_c:
		m.put_int64(d_buf);
		break;
	default:
		assert(!"illegal type for D4Enum");
	}
}

void
D4Enum::deserialize(D4StreamUnMarshaller &um, DMR &)
{
	switch (d_element_type) {
	case dods_byte_c:
	case dods_uint8_c: {
		dods_byte v;
		um.get_byte(v);
		d_buf = v;
		break;
	}
	case dods_uint16_c: {
		dods_uint16 v;
		um.get_uint16(v);
		d_buf = v;
		break;
	}
	case dods_uint32_c: {
		dods_uint32 v;
		um.get_uint32(v);
		d_buf = v;
		break;
	}
	case dods_uint64_c: {
		dods_uint64 v;
		um.get_uint64(v);
		d_buf = v;
		break;
	}

	case dods_int8_c: {
		dods_int8 v;
		um.get_int8(v);
		d_buf = v;
		break;
	}
	case dods_int16_c: {
		dods_int16 v;
		um.get_int16(v);
		d_buf = v;
		break;
	}
	case dods_int32_c: {
		dods_int32 v;
		um.get_int32(v);
		d_buf = v;
		break;
	}
	case dods_int64_c: {
		dods_int64 v;
		um.get_int64(v);
		d_buf = v;
		break;
	}
	default:
		assert(!"illegal type for D4Enum");
	}
}

uint64_t D4Enum::val2buf(void *val, bool)
{
    if (!val)
        throw InternalErr("The incoming pointer does not contain any data.");

    switch (d_element_type) {
     case dods_byte_c:
     case dods_uint8_c:
         d_buf = *(dods_byte*)val;
         break;
     case dods_uint16_c:
         d_buf = *(dods_uint16*)val;
         break;
     case dods_uint32_c:
         d_buf = *(dods_uint32*)val;
         break;
     case dods_uint64_c:
         d_buf = *(dods_uint64*)val;
         break;

     case dods_int8_c:
         d_buf = *(dods_int8*)val;
         break;
     case dods_int16_c:
         d_buf = *(dods_int16*)val;
         break;
     case dods_int32_c:
         d_buf = *(dods_int32*)val;
         break;
     case dods_int64_c:
         d_buf = *(dods_int64*)val;
         break;
     default:
         assert(!"illegal type for D4Enum");
     }

    return width();
}

uint64_t D4Enum::buf2val(void **val)
{
    if (!val)
        throw InternalErr("NULL pointer");

    switch (d_element_type) {
     case dods_byte_c:
     case dods_uint8_c:
         if (!*val) *val = new dods_byte;
         *(dods_byte *) * val = d_buf;
         break;
     case dods_uint16_c:
         if (!*val) *val = new dods_uint16;
         *(dods_uint16 *) * val = d_buf;
         break;
     case dods_uint32_c:
         if (!*val) *val = new dods_uint32;
         *(dods_uint32 *) * val = d_buf;
         break;
     case dods_uint64_c:
         if (!*val) *val = new dods_uint64;
         *(dods_uint64 *) * val = d_buf;
         break;

     case dods_int8_c:
         if (!*val) *val = new dods_int8;
         *(dods_int8*) * val = d_buf;
         break;
     case dods_int16_c:
         if (!*val) *val = new dods_int16;
         *(dods_int16 *) * val = d_buf;
         break;
     case dods_int32_c:
         if (!*val) *val = new dods_int32;
         *(dods_int32 *) * val = d_buf;
         break;
     case dods_int64_c:
         if (!*val) *val = new dods_int64;
         *(dods_int64 *) * val = d_buf;
         break;
     default:
         assert(!"illegal type for D4Enum");
     }

    return width();
}

void D4Enum::print_val(ostream &out, string space, bool print_decl_p)
{
    if (print_decl_p) {
        print_decl(out, space, false);
        out << " = ";
    }

    DBG(cerr << "Enum union value: " << hex << d_buf << dec << endl);

    if (is_signed()) {
    	int64_t v;
    	value(&v);
    	out << v;
    }
    else {
    	uint64_t v;
    	value(&v);
    	out << v;
    }

    if (print_decl_p)
    	out << ";" << endl;
}

/** Write the XML representation of this variable. This method is used to
    build the DDX XML response.
    @param out Destination output stream
    @param space Use this to indent child declarations. Default is "".
    @param constrained If true, only print this if it's part part of the
    current projection. Default is False. */
void
D4Enum::print_xml_writer(XMLWriter &xml, bool constrained)
{
    if (constrained && !send_p())
        return;

    if (xmlTextWriterStartElement(xml.get_writer(), (const xmlChar*)"Enum") < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write Enum element");

    if (!name().empty())
        if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "name", (const xmlChar*)name().c_str()) < 0)
            throw InternalErr(__FILE__, __LINE__, "Could not write attribute for name");


    string path = d_enum_def->name();
    // Not every D4EnumDef is a member of an instance of D4EnumDefs - the D4EnumDefs instance
    // holds a reference to the D4Group that holds the Enum definitions.
    // TODO Should this be changed - so the EnumDef holds a reference to its parent Group?
    if (d_enum_def->parent()) {
    	// print the FQN for the enum def; D4Group::FQN() includes the trailing '/'
    	path = static_cast<D4Group*>(d_enum_def->parent()->parent())->FQN() + path;
    }
    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "enum", (const xmlChar*)path.c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for enum");

    attributes()->print_dap4(xml);

    if (get_attr_table().get_size() > 0)
        get_attr_table().print_xml_writer(xml);

    if (xmlTextWriterEndElement(xml.get_writer()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not end Enum element");
}


bool
D4Enum::ops(BaseType *b, int op)
{
    // Get the arg's value.
    if (!read_p() && !read())
        throw InternalErr(__FILE__, __LINE__, "This value not read!");

    // Get the second arg's value.
    if (!b->read_p() && !b->read())
        throw InternalErr(__FILE__, __LINE__, "This value not read!");

    switch (b->type()) {
        case dods_int8_c:
            return Cmp<dods_int64, dods_int8>(op, d_buf, static_cast<Int8*>(b)->value());
        case dods_byte_c:
            return Cmp<dods_int64, dods_byte>(op, d_buf, static_cast<Byte*>(b)->value());
        case dods_int16_c:
            return Cmp<dods_int64, dods_int16>(op, d_buf, static_cast<Int16*>(b)->value());
        case dods_uint16_c:
            return Cmp<dods_int64, dods_uint16>(op, d_buf, static_cast<UInt16*>(b)->value());
        case dods_int32_c:
            return Cmp<dods_int64, dods_int32>(op, d_buf, static_cast<Int32*>(b)->value());
        case dods_uint32_c:
            return Cmp<dods_int64, dods_uint32>(op, d_buf, static_cast<UInt32*>(b)->value());
#if 0
            // FIXME
        case dods_int64_c:
            return Cmp<dods_int64, dods_int64>(op, d_buf, static_cast<D4Enum*>(b)->value());
        case dods_uint64_c:
            return Cmp<dods_int64, dods_uint64>(op, d_buf, static_cast<D4Enum*>(b)->value());
#endif
        case dods_float32_c:
            return Cmp<dods_int64, dods_float32>(op, d_buf, static_cast<Float32*>(b)->value());
        case dods_float64_c:
            return Cmp<dods_int64, dods_float64>(op, d_buf, static_cast<Float64*>(b)->value());
        default:
            return false;
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
D4Enum::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "D4Enum::dump - (" << (void *) this << ")" << endl;
    DapIndent::Indent();
    BaseType::dump(strm);
    strm << DapIndent::LMarg << "value: " << d_buf << endl;
    DapIndent::UnIndent();
}

} // namespace libdap

