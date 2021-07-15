
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2012 OPeNDAP, Inc.
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

#ifndef _int8_h
#define _int8_h 1

#ifndef _dods_datatypes_h
#include "dods-datatypes.h"
#endif

#ifndef _basetype_h
#include "BaseType.h"
#endif

namespace libdap
{

/** @brief Holds an 8-bit signed integer value. */

class Int8: public BaseType
{
	// This is used in BaseType *Vector::var(unsigned int i)
	virtual unsigned int val2buf(void *val, bool)  {
    	set_value(*reinterpret_cast<dods_int8*>(val));
    	return sizeof(dods_int8);
    }
    virtual unsigned int buf2val(void **) { throw InternalErr(__FILE__, __LINE__, "buf2val: Not implemented for Int8"); }
    virtual void print_val(FILE *, string , bool) { throw InternalErr(__FILE__, __LINE__, "print_val: Not implemented for Int8"); }

protected:
    dods_int8 d_buf;

public:
    Int8(const string &n);
    Int8(const string &n, const string &d);
    virtual ~Int8()
    {}

    Int8(const Int8 &copy_from);

    Int8 &operator=(const Int8 &rhs);

    virtual BaseType *ptr_duplicate();

    virtual unsigned int width(bool constrained = false) const;

    // DAP4
    virtual void compute_checksum(Crc32 &checksum);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    virtual dods_int8 value() const;
    virtual bool set_value(dods_int8 val);

    virtual void print_val(ostream &out, string space = "", bool print_decl_p = true);

    virtual bool ops(BaseType *b, int op);
    virtual bool d4_ops(BaseType *b, int op);
    virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table, bool show_shared_dims = false);

    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif // _int16_h

