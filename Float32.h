
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

// (c) COPYRIGHT URI/MIT 1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Interface for Float32 type.
//
// 3/22/99 jhrg

#ifndef _float32_h
#define _float32_h 1


#ifndef _dods_datatypes_h
#include "dods-datatypes.h"
#endif

#ifndef _basetype_h
#include "BaseType.h"
#endif

#ifndef constraint_evaluator_h
#include "ConstraintEvaluator.h"
#endif

namespace libdap
{

class DMR;

/** @brief Holds a 32-bit floating point value.

    @see BaseType
    */
class Float32: public BaseType
{
protected:
    dods_float32 d_buf;

public:
    Float32(const string &n);
    Float32(const string &n, const string &d);

    Float32(const Float32 &copy_from);

    Float32 &operator=(const Float32 &rhs);

    virtual ~Float32()
    {}

    virtual BaseType *ptr_duplicate();
    virtual uint64_t width(bool constrained = false) const;

    // DAP2
    virtual bool serialize(ConstraintEvaluator &eval, DDS &dds,  Marshaller &m, bool ce_eval = true);
    virtual bool deserialize(UnMarshaller &um, DDS *dds, bool reuse = false);

    // DAP4
    virtual void compute_checksum(Crc32 &checksum);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    virtual uint64_t val2buf(void *val, bool reuse = false);
    virtual uint64_t buf2val(void **val);

    virtual dods_float32 value() const;
    virtual bool set_value(dods_float32 f);

    virtual void print_val(FILE *out, string space = "",
                           bool print_decl_p = true);
    virtual void print_val(ostream &out, string space = "",
                           bool print_decl_p = true);

    virtual bool ops(BaseType *b, int op);
    virtual bool d4_ops(BaseType *b, int op);

    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif // _float32_h

