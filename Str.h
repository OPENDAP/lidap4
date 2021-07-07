
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

// Interface for Str type.
//
// jhrg 9/7/94

#ifndef _str_h
#define _str_h 1

#include <string>

#include "dods-limits.h"
#include "BaseType.h"

namespace libdap
{

// max_str_len should be large since we always send strings with length bytes
// as a prefix (so xdr_string will always know how much memory to malloc) but
// if deserialize gets confused and thinks a ctor (in particular) is a string
// xdr_string in turn will max_str_len if it cannot get a length byte. A long
// term solution is to fix libdap, but strings should not routinely be > 32k
// for the time being... jhrg 4/30/97

const unsigned int max_str_len = DODS_USHRT_MAX - 1;

/** @brief Holds character string data.

    @see BaseType
    @see Url
    */

class Str: public BaseType
{
protected:
    string d_buf;

public:
    Str(const string &n);
    Str(const string &n, const string &d);

    virtual ~Str()
    {}

    Str(const Str &copy_from);

    Str &operator=(const Str &rhs);

    virtual BaseType *ptr_duplicate();

    virtual uint64_t width(bool constrained = false) const;

    // Return the length of the stored string or zero if no string has been
    // stored in the instance's internal buffer.
    virtual uint64_t length() const;

    // DAP2
    virtual bool serialize(ConstraintEvaluator &eval, DDS &dds, Marshaller &m, bool ce_eval = true);
    virtual bool deserialize(UnMarshaller &um, DDS *dds, bool reuse = false);

    // DAP4
    virtual void compute_checksum(Crc32 &checksum);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    virtual uint64_t val2buf(void *val, bool reuse = false);
    virtual uint64_t buf2val(void **val);

    virtual bool set_value(const string &value);
    virtual string value() const;

    virtual void print_val(FILE *out, string space = "",
                           bool print_decl_p = true);
    virtual void print_val(ostream &out, string space = "",
                           bool print_decl_p = true);

    virtual bool ops(BaseType *b, int op);
    virtual bool d4_ops(BaseType *b, int op);

    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif // _str_h

