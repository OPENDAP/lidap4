
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

// Interface for the class Structure. A structure contains a single set of
// variables, all at the same lexical level. Of course, a structure may
// contain other structures... The variables contained in a structure are
// stored by instances of this class in a SLList of BaseType pointers.
//
// jhrg 9/14/94

#ifndef _structure_h
#define _structure_h 1

#include <vector>

#include "Constructor.h"

namespace libdap
{

class DDS;
class ConstraintEvaluator;
class D4Group;

/** This data type is used to hold a collection of related data types,
    in a manner roughly corresponding to a C structure.  The member
    types can be simple or compound types, and can include other
    Structures.

    The DAP2 structure is defined as a singly-linked list.  This means
    that Structure elements can be accessed either by name, with the
    <tt>var()</tt> function, or by their position in the list, either with
    the overloaded version of <tt>var()</tt>, or the combination of the
    <tt>first_var()</tt> and <tt>next_var()</tt> functions.

    The <tt>val2buf()</tt> and <tt>buf2val()</tt> functions only
    return the size of
    the structure.  To read parts of a DAP2 Structure into an
    application program, use the <tt>buf2val()</tt> function of the element
    of the Structure in question.

    Note that the predicate-setting functions <tt>set_send_p()</tt> and
    <tt>set_read_p()</tt> set their flags for the Structure as well as for
    each of the Structure's member elements.

    Similar to C, you can refer to members of Structure elements
    with a ``.'' notation.  For example, if the Structure has a member
    Structure called ``Tom'' and Tom has a member Float32 called
    ``shoe_size'', then you can refer to Tom's shoe size as
    ``Tom.shoe_size''.

    @brief Holds a structure (aggregate) type.
*/

class Structure: public Constructor
{
private:
protected:

public:
    Structure(const string &n);
    Structure(const string &n, const string &d);

    Structure(const Structure &rhs);
    virtual ~Structure();

    Structure &operator=(const Structure &rhs);
    virtual BaseType *ptr_duplicate();

    virtual void transform_to_dap4(D4Group *root, Constructor *container);
    virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table, bool show_shared_dims = false);

    virtual bool is_linear();

    virtual void set_leaf_sequence(int level = 1);

    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif // _structure_h
