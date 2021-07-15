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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#ifndef _d4_opaque_h
#define _d4_opaque_h 1

#include <vector>

#include "BaseType.h"
#include "InternalErr.h"

class Crc32;

namespace libdap
{

class D4Opaque: public BaseType
{
public:
	typedef std::vector<uint8_t> dods_opaque;

protected:
    dods_opaque d_buf;

public:
    D4Opaque(const std::string &n) : BaseType(n, dods_opaque_c, true /*is_dap4*/), d_buf(0) { }
    D4Opaque(const std::string &n, const std::string &d)  : BaseType(n, d, dods_opaque_c, true /*is_dap4*/), d_buf(0) { }

    virtual ~D4Opaque()  { }

    D4Opaque(const D4Opaque &copy_from) : BaseType(copy_from) {
        d_buf = copy_from.d_buf;
    }

    D4Opaque &operator=(const D4Opaque &rhs);

    virtual BaseType *ptr_duplicate() {  return new D4Opaque(*this); }

    virtual void clear_local_data();

    virtual unsigned int width(bool = false) const { return sizeof(vector<uint8_t>); }

    // Return the length of the stored data or zero if no string has been
    // stored in the instance's internal buffer.
    virtual int length() const { return d_buf.size(); }

    // DAP2
    virtual bool serialize(ConstraintEvaluator &, DDS &, Marshaller &, bool = true) {
    	throw InternalErr(__FILE__, __LINE__, "Unimplemented method");
    }
    virtual bool deserialize(UnMarshaller &, DDS *, bool = false) {
    	throw InternalErr(__FILE__, __LINE__, "Unimplemented method");
    }

    // DAP4
    virtual void compute_checksum(Crc32 &checksum);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
#if 0
    virtual void serialize_no_release(D4StreamMarshaller &m, DMR &dmr, bool filter = false);
#endif
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    virtual unsigned int val2buf(void *val, bool reuse = false);
    virtual unsigned int buf2val(void **val);

    virtual bool set_value(const dods_opaque &value);
    virtual dods_opaque value() const;

    virtual void print_val(FILE *, std::string = "", bool = true)  {
    	throw InternalErr(__FILE__, __LINE__, "Unimplemented method");
    }
    virtual void print_val(std::ostream &out, std::string space = "", bool print_decl_p = true);

    //virtual void print_dap4(XMLWriter &xml, bool constrained = false);

    virtual bool ops(BaseType *, int) {
        throw InternalErr(__FILE__, __LINE__, "Unimplemented method");
    }

    virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table, bool show_shared_dims);

    virtual void dump(std::ostream &strm) const ;

};

} // namespace libdap

#endif // _d4_opaque_h

