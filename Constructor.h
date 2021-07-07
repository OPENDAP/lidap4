
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

#ifndef _constructor_h
#define _constructor_h 1

#include <vector>

#include "BaseType.h"

class Crc32;

namespace libdap
{

class DMR;
class XMLWriter;
class D4StreamUnMarshaller;

/** Common methods for all constructor types. */
class Constructor: public BaseType
{
private:
    Constructor();  // No default ctor.

protected:
    std::vector<BaseType *> d_vars;

    void m_duplicate(const Constructor &s);
    BaseType *m_leaf_match(const string &name, btp_stack *s = 0);
    BaseType *m_exact_match(const string &name, btp_stack *s = 0);

    Constructor(const string &name, const Type &type, bool is_dap4 = false);
    Constructor(const string &name, const string &d, const Type &type, bool is_dap4 = false);

    Constructor(const Constructor &copy_from);

public:
    typedef std::vector<BaseType *>::const_iterator Vars_citer ;
    typedef std::vector<BaseType *>::iterator Vars_iter ;
    typedef std::vector<BaseType *>::reverse_iterator Vars_riter ;

    virtual ~Constructor();

    Constructor &operator=(const Constructor &rhs);
    virtual void transform_to_dap4(D4Group *root, Constructor *dest);

    virtual std::string FQN() const;

    virtual int element_count(bool leaves = false);

    virtual void set_send_p(bool state);
    virtual void set_read_p(bool state);

    virtual uint64_t width(bool constrained = false) const;
#if 0
    virtual unsigned int width(bool constrained);
#endif
    // TODO Rewrite these methods to use the back pointers and keep this
    // for older code.
    /// btp_stack no longer needed; use back pointers (BaseType::get_parent())
    virtual BaseType *var(const string &name, bool exact_match = true, btp_stack *s = 0);
    /// @deprecated
    virtual BaseType *var(const string &n, btp_stack &s);

    Vars_iter var_begin();
    Vars_iter var_end();
    Vars_riter var_rbegin();
    Vars_riter var_rend();
    Vars_iter get_vars_iter(int i);
    BaseType *get_var_index(int i);

    virtual void add_var(BaseType *bt, Part part = nil);
    virtual void add_var_nocopy(BaseType *bt, Part part = nil);

    virtual void del_var(const string &name);
    virtual void del_var(Vars_iter i);

    virtual bool read();

    // DAP2
    virtual void intern_data(ConstraintEvaluator &eval, DDS &dds);
    virtual bool serialize(ConstraintEvaluator &eval, DDS &dds, Marshaller &m, bool ce_eval = true);
    virtual bool deserialize(UnMarshaller &um, DDS *dds, bool reuse = false);

    // DAP4
    virtual void compute_checksum(Crc32 &checksum);
    virtual void intern_data(/*Crc32 &checksum, DMR &dmr, ConstraintEvaluator &eval*/);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    // Do not store values in memory as for C; users work with the C++ objects
    virtual uint64_t val2buf(void *, bool) {
        throw InternalErr(__FILE__, __LINE__, "Never use this method; see the programmer's guide documentation.");
    }
    virtual uint64_t buf2val(void **) {
        throw InternalErr(__FILE__, __LINE__, "Never use this method; see the programmer's guide documentation.");
    }

    virtual bool is_linear();
    virtual void set_in_selection(bool state);

    virtual void print_decl(ostream &out, string space = "    ",
                            bool print_semi = true,
                            bool constraint_info = false,
                            bool constrained = false);

    virtual void print_xml(ostream &out, string space = "    ",
                           bool constrained = false);

    void print_dap4(XMLWriter &xml, bool constrained = false);

    virtual void print_xml_writer(XMLWriter &xml, bool constrained = false);

    virtual void print_decl(FILE *out, string space = "    ",
                            bool print_semi = true,
                            bool constraint_info = false,
                            bool constrained = false);
    virtual void print_xml(FILE *out, string space = "    ",
                           bool constrained = false);

    virtual void print_val(FILE *out, string space = "",
                           bool print_decl_p = true);
    virtual void print_val(ostream &out, string space = "",
                           bool print_decl_p = true);

    virtual bool check_semantics(string &msg, bool all = false);

    virtual void transfer_attributes(AttrTable *at);
    static AttrTable *make_dropped_vars_attr_table(vector<BaseType *> *dropped_vars);

    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif // _constructor_h
