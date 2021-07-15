// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

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

#ifndef D4GROUP_H_
#define D4GROUP_H_

#include <string>
#include <cstdint>

#include "Constructor.h"
#include "D4Dimensions.h"
#include "D4EnumDefs.h"

class Crc32;

namespace libdap {

class BaseType;
class Array;

/** A DAP4 Group object. A Group is-a Constructor, so it inherits a set of
 * BaseType objects and an attribute table, along with methods to search for
 * variables by name where dots (.) in a fully qualified name serve as
 * separators.
 */
class D4Group :public Constructor {
private:
    // Note that because Constructor is a BaseType, this class inherits
    // both a back pointer to its parent, an AttrTable and, directly from the
    // Constructor class, a vector of BaseTypes.

    // This instance of D4Dimensions holds the Group's definitions; the same
    // class is used by Array to hold the actual dimensions for a variable.
    D4Dimensions *d_dims;

    // This holds the Group's enumeration definitions; a different class is
    // used for the Enumeration type
    D4EnumDefs *d_enum_defs;

    // This is a pointer so that the factory class(es) that return pointers
    // work as expected when making Groups.
    vector<D4Group*> d_groups;

    BaseType *m_find_map_source_helper(const string &name);

protected:
    void m_duplicate(const D4Group &g);

public:
    typedef vector<D4Group*>::iterator groupsIter;
    typedef vector<D4Group*>::const_iterator groupsCIter;

    D4Group(const string &name);
    D4Group(const string &name, const string &dataset);

    D4Group(const D4Group &rhs);
    virtual ~D4Group();

    D4Group &operator=(const D4Group &rhs);

    // This method returned a D4Group * previously. jhrg 11/17/16
    virtual BaseType *ptr_duplicate();

    /// Get the dimensions defined for this Group
    D4Dimensions *dims() {
    	// If not built yet, make one and set this as parent.
        if (!d_dims) d_dims = new D4Dimensions(this);
        return d_dims;
    }

    virtual std::string FQN() const;

    D4Dimension *find_dim(const string &path);

    Array *find_map_source(const string &path);

    D4EnumDef *find_enum_def(const string &path);

    /// Get  the enumerations defined for this Group
    D4EnumDefs *enum_defs() {
        if (!d_enum_defs) {
        	d_enum_defs = new D4EnumDefs;
        	d_enum_defs->set_parent(this);
        }
        return d_enum_defs;
    }

    BaseType *find_first_var_that_uses_dimension(D4Dimension *dim);
    BaseType *find_first_var_that_uses_enumeration(D4EnumDef *enum_def);

    BaseType *find_var(const string &name);

    /// Get an iterator to the start of the values
    groupsIter grp_begin() { return d_groups.begin(); }

    /// Get an iterator to the end of the values
    groupsIter grp_end() { return d_groups.end(); }

    void add_group(const D4Group *g) {
    	add_group_nocopy(new D4Group(*g));
    }

    void add_group_nocopy(D4Group *g) {
    	g->set_parent(this);
        d_groups.push_back(g);
    }
    void insert_group_nocopy(D4Group *g, groupsIter i) {
    	g->set_parent(this);
        d_groups.insert(i, g);
    }

    D4Group *find_child_grp(const string &grp_name);

    long request_size(bool constrained);
    uint64_t request_size_kb(bool constrained);

    virtual void set_send_p(bool state);
    virtual void set_read_p(bool state);

    // DAP4
    virtual void intern_data(/*Crc32 &checksum, DMR &dmr, ConstraintEvaluator &eval*/);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    void print_dap4(XMLWriter &xml, bool constrained = false);

    virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table, bool show_shared_dims = false);
    //virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table, bool is_root);

};

} /* namespace libdap */
#endif /* D4GROUP_H_ */
