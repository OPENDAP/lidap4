
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

// (c) COPYRIGHT URI/MIT 1994-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Implementation for the class Structure
//
// jhrg 9/14/94

//#define DODS_DEBUG

#include "config.h"

#include <sstream>

#include "Byte.h"
#include "Int16.h"
#include "UInt16.h"
#include "Int32.h"
#include "UInt32.h"
#include "Float32.h"
#include "Float64.h"
#include "Str.h"
#include "Url.h"
#include "Array.h"
#include "Structure.h"
#include "Sequence.h"
#include "Grid.h"

#include "DDS.h"
#include "ConstraintEvaluator.h"

#include "D4Attributes.h"
#include "D4Group.h"

#include "XDRStreamMarshaller.h"
#include "util.h"
#include "debug.h"
#include "InternalErr.h"
#include "escaping.h"
#include "DapIndent.h"

using std::cerr;
using std::endl;

namespace libdap {

#if 0
/** This method is protected so it's hidden from the whole world, but
 * available to direct child classes. Because of that, we need a glue-routine
 * here so children of Structure can specialize it.
 */
void
Structure::m_duplicate(const Structure &s)
{
    Constructor::m_duplicate(s);
#if 0
    Structure &cs = const_cast<Structure &>(s);

    DBG(cerr << "Copying structure: " << name() << endl);

    for (Vars_iter i = cs.d_vars.begin(); i != cs.d_vars.end(); i++) {
        DBG(cerr << "Copying field: " << (*i)->name() << endl);
        // Jose Garcia
        // I think this assert here is part of a debugging
        // process since it is going along with a DBG call
        // I leave it here since it can be remove by defining NDEBUG.
        // assert(*i);
        BaseType *btp = (*i)->ptr_duplicate();
        btp->set_parent(this);
        d_vars.push_back(btp);
    }
#endif
}
#endif

/** The Structure constructor requires only the name of the variable
    to be created. The name may be omitted, which will create a
    nameless variable. This may be adequate for some applications.

    @param n A string containing the name of the variable to be
    created.
*/
Structure::Structure(const string &n) : Constructor(n, dods_structure_c)
{}

/** The Structure server-side constructor requires the name of the variable
    to be created and the dataset name from which this variable is being
    created. Used on server-side handlers.

    @param n A string containing the name of the variable to be
    created.
    @param d A string containing the name of the dataset from which this
    variable is being created.
*/
Structure::Structure(const string &n, const string &d)
    : Constructor(n, d, dods_structure_c)
{}

/** The Structure copy constructor. */
Structure::Structure(const Structure &rhs) : Constructor(rhs)
{
    DBG(cerr << "In Structure::copy_ctor for " << name() << endl);
    //m_duplicate(rhs);
}

Structure::~Structure()
{
}

BaseType *
Structure::ptr_duplicate()
{
    return new Structure(*this);
}

/**
 * Build a DAP4 Structure.
 *
 * This code must be subclassed for all but the most trivial cases.
 *
 * @param root
 * @param container
 * @return The new variable
 */
void
Structure::transform_to_dap4(D4Group *root, Constructor *container)
{
    DBG(cerr << __func__ <<"() -  BEGIN" << endl;);
	// Here we create a new Structure and then use it
    // as the target container for the transformed versions of
    // all the member variables by calling Constructor::transform_to_dap4() and
    // passing our new target Structure in as the target container.
	Structure *dest = new Structure(name());
    DBG(cerr << __func__ <<"() -  Calling Constructor::transform_to_dap4("<<
        "'" << root->name() << "':" << (void*)root << ","
        "'" << dest->name() << "':" << (void*)dest << ")"
        << endl; );
	Constructor::transform_to_dap4(root, dest);
	container->add_var_nocopy(dest);
	DBG(cerr << __func__ <<"() -  Added new Structure '" << dest->name() << "' (" << (void*)dest <<
	    ") to the container '" << container->name() <<"'" << endl;);
    DBG(cerr << __func__ <<"() -  END"<< endl;);
}


/** @brief DAP4 to DAP2 transform
 *
 * Return a DAP2 'copy' of the variable.
 *
 * @return A pointer to the transformed variable
 */
    std::vector<BaseType *> *
Structure::transform_to_dap2(AttrTable *, bool show_shared_dims)
{
    DBG(cerr << " " << __func__ << " BEGIN" << endl);
    Structure *dest = new Structure(name());

    // convert the Structure's d4 attributes to a dap2 attribute table.
#if 0
    AttrTable *attrs = this->attributes()->get_AttrTable(name());
#else
    if (dest->get_attr_table().get_size() == 0) {
        attributes()->transform_attrs_to_dap2(&dest->get_attr_table());
        dest->get_attr_table().set_name(name());
    }
#endif

    dest->set_is_dap4(false);

    vector<BaseType *> dropped_vars;
    for (Structure::Vars_citer i = var_begin(), e = var_end(); i != e; ++i) {
        vector<BaseType *> *new_vars = (*i)->transform_to_dap2(&dest->get_attr_table() /*attrs*/, show_shared_dims);
        if (new_vars) {  // Might be un-mappable
            // It's not so game on..
#if 0
            vector<BaseType*>::iterator vIter = new_vars->begin();
            vector<BaseType*>::iterator end = new_vars->end();
#endif
            for (vector<BaseType*>::iterator vi = new_vars->begin(), ve = new_vars->end(); vi != ve ; ++vi ) {
                BaseType *new_var = (*vi);
                new_var->set_parent(dest);
                dest->add_var_nocopy(new_var);
                (*vi) = NULL;
            }
            delete new_vars;

        }
        else {
            // Got a NULL, so we are dropping this var.
            dropped_vars.push_back(*i);
        }
    }

    AttrTable *dv_attr_table = make_dropped_vars_attr_table(&dropped_vars);
    if(dv_attr_table){
        dest->get_attr_table().append_container(dv_attr_table,dv_attr_table->get_name());
    }

    DBG(attrs->print(cerr,"",true););

#if 0
    // Since this does a copy we gotta delete the attrs when done
    // FIXME This is a bug. jhrg 6/17/19
    dest->set_attr_table(*attrs);
    delete attrs;
#endif

    vector<BaseType *> *result =  new vector<BaseType *>();
    result->push_back(dest);

    DBG(cerr << " " << __func__ << " END" << endl);

    return result;
}





Structure &
Structure::operator=(const Structure &rhs)
{
    DBG(cerr << "Entering Structure::operator=" << endl);
    if (this == &rhs)
        return *this;

    dynamic_cast<Constructor &>(*this) = rhs; // run Constructor=

    //m_duplicate(rhs);

    DBG(cerr << "Exiting Structure::operator=" << endl);
    return *this;
}

#if 0
int
Structure::element_count(bool leaves)
{
    if (!leaves)
        return d_vars.size();
    else {
        int i = 0;
        for (Vars_iter j = d_vars.begin(); j != d_vars.end(); j++) {
            i += (*j)->element_count(leaves);
        }
        return i;
    }
}
#endif

bool
Structure::is_linear()
{
    bool linear = true;
    for (Vars_iter i = d_vars.begin(); linear && i != d_vars.end(); i++) {
        if ((*i)->type() == dods_structure_c)
            linear = linear && static_cast<Structure*>((*i))->is_linear();
        else
            linear = linear && (*i)->is_simple_type();
    }

    return linear;
}

#if 0
void
Structure::set_send_p(bool state)
{
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        (*i)->set_send_p(state);
    }

    BaseType::set_send_p(state);
}

void
Structure::set_read_p(bool state)
{
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        (*i)->set_read_p(state);
    }

    BaseType::set_read_p(state);
}
#endif
#if 0
/** Set the \e in_selection property for this variable and all of its
    children.

    @brief Set the \e in_selection property.
    @param state Set the property value to \e state. */
void
Structure::set_in_selection(bool state)
{
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        (*i)->set_in_selection(state);
    }

    BaseType::set_in_selection(state);
}
#endif
/** @brief Traverse Structure, set Sequence leaf nodes. */
void
Structure::set_leaf_sequence(int level)
{
    for (Vars_iter i = var_begin(); i != var_end(); i++) {
        if ((*i)->type() == dods_sequence_c)
        	static_cast<Sequence&>(**i).set_leaf_sequence(++level);
        else if ((*i)->type() == dods_structure_c)
        	static_cast<Structure&>(**i).set_leaf_sequence(level);
    }
}

#if 0
/** Adds an element to a Structure.

    @param bt A pointer to the DAP2 type variable to add to this Structure.
    @param part Not used by this class, defaults to nil */
void
Structure::add_var(BaseType *bt, Part)
{
    // Jose Garcia
    // Passing and invalid pointer to an object is a developer's error.
    if (!bt)
        throw InternalErr(__FILE__, __LINE__, "The BaseType parameter cannot be null.");

    if (bt->is_dap4_only_type())
        throw InternalErr(__FILE__, __LINE__, "Attempt to add a DAP4 type to a DAP2 Structure.");

    // Jose Garcia
    // Now we add a copy of bt so the external user is able to destroy bt as
    // he/she wishes. The policy is: "If it is allocated outside, it is
    // deallocated outside, if it is allocated inside, it is deallocated
    // inside"
    BaseType *btp = bt->ptr_duplicate();
    btp->set_parent(this);
    d_vars.push_back(btp);
}

/** Adds an element to a Structure.

    @param bt A pointer to the DAP2 type variable to add to this Structure.
    @param part Not used by this class, defaults to nil */
void
Structure::add_var_nocopy(BaseType *bt, Part)
{
    if (!bt)
        throw InternalErr(__FILE__, __LINE__, "The BaseType parameter cannot be null.");

    if (bt->is_dap4_only_type())
        throw InternalErr(__FILE__, __LINE__, "Attempt to add a DAP4 type to a DAP2 Structure.");

    bt->set_parent(this);
    d_vars.push_back(bt);
}


/** Removed an element from a Structure.

    @param n name of the variable to remove */
void
Structure::del_var(const string &n)
{
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        if ((*i)->name() == n) {
            BaseType *bt = *i ;
            d_vars.erase(i) ;
            delete bt ; bt = 0;
            return;
        }
    }
}
#endif
#if 0
/** @brief simple implementation of read that iterates through vars
 *  and calls read on them
 *
 * @return returns false to signify all has been read
 */
bool Structure::read()
{
    if (!read_p()) {
        for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
            (*i)->read();
        }
        set_read_p(true);
    }

    return false;
}
#endif
#if 0
// TODO Recode to use width(bool)
unsigned int
Structure::width()
{
    unsigned int sz = 0;

    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        sz += (*i)->width();
    }

    return sz;
}

/** This version of width simply returns the same thing as width() for simple
    types and Arrays. For Structure it returns the total size if constrained
    is false, or the size of the elements in the current projection if true.

    @param constrained If true, return the size after applying a constraint.
    @return  The number of bytes used by the variable.
 */
unsigned int
Structure::width(bool constrained)
{
    unsigned int sz = 0;

    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
    	if (constrained) {
    		if ((*i)->send_p())
    			sz += (*i)->width(constrained);
    	}
    	else {
    		sz += (*i)->width(constrained);
    	}
    }

    return sz;
}
#endif

#if 0
void
Structure::intern_data(ConstraintEvaluator & eval, DDS & dds)
{
    DBG(cerr << "Structure::intern_data: " << name() << endl);
    if (!read_p())
        read();          // read() throws Error and InternalErr

    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        if ((*i)->send_p()) {
            (*i)->intern_data(eval, dds);
        }
    }
}

bool
Structure::serialize(ConstraintEvaluator &eval, DDS &dds,
                     Marshaller &m, bool ce_eval)
{
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_on();
#endif
    if (!read_p())
        read();  // read() throws Error and InternalErr

#if EVAL
    if (ce_eval && !eval.eval_selection(dds, dataset()))
        return true;
#endif
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_off();
#endif
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        if ((*i)->send_p()) {
#ifdef CHECKSUMS
            XDRStreamMarshaller *sm = dynamic_cast<XDRStreamMarshaller*>(&m);
            if (sm && sm->checksums() && (*i)->type() != dods_structure_c && (*i)->type() != dods_grid_c)
                sm->reset_checksum();

            (*i)->serialize(eval, dds, m, false);

            if (sm && sm->checksums() && (*i)->type() != dods_structure_c && (*i)->type() != dods_grid_c)
                sm->get_checksum();
#else
            (*i)->serialize(eval, dds, m, false);
#endif
        }
    }

    return true;
}

bool
Structure::deserialize(UnMarshaller &um, DDS *dds, bool reuse)
{
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        (*i)->deserialize(um, dds, reuse);
    }

    return false;
}
#endif
#if 0
/**  @brief Never call this

     This method cannot be used to change values of a Structure since
     the values of a Constructor type must be set using methods in
     Constructor. See the Constructor::var_begin() and related
     methods.

     @todo Make this throw an exception
     @return Returns the size of the structure. */
unsigned int
Structure::val2buf(void *, bool)
{
    return sizeof(Structure);
}

/** @brief Never call this
    @see val2buf()
    @return Returns the size of the structure. */
unsigned int
Structure::buf2val(void **)
{
    return sizeof(Structure);
}
#endif

#if 0
BaseType *
Structure::var(const string &name, bool exact_match, btp_stack *s)
{
    string n = www2id(name);

    if (exact_match)
        return m_exact_match(n, s);
    else
        return m_leaf_match(n, s);
}

/** @deprecated See comment in BaseType */
BaseType *
Structure::var(const string &n, btp_stack &s)
{
    string name = www2id(n);

    BaseType *btp = m_exact_match(name, &s);
    if (btp)
        return btp;

    return m_leaf_match(name, &s);
}
#endif
#if 0
// Private method to find a variable using the shorthand name. This
// should be moved to Constructor.
BaseType *
Structure::m_leaf_match(const string &name, btp_stack *s)
{
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        if ((*i)->name() == name) {
            if (s) {
                DBG(cerr << "Pushing " << this->name() << endl);
                s->push(static_cast<BaseType *>(this));
            }
            return *i;
        }
        if ((*i)->is_constructor_type()) {
            BaseType *btp = (*i)->var(name, false, s);
            if (btp) {
                if (s) {
                    DBG(cerr << "Pushing " << this->name() << endl);
                    s->push(static_cast<BaseType *>(this));
                }
                return btp;
            }
        }
    }

    return 0;
}

// Breadth-first search for NAME. If NAME contains one or more dots (.)
// TODO The btp_stack is not needed since there are 'back pointers' in
// BaseType.
BaseType *
Structure::m_exact_match(const string &name, btp_stack *s)
{
    // Look for name at the top level first.
    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        if ((*i)->name() == name) {
            if (s)
                s->push(static_cast<BaseType *>(this));

            return *i;
        }
    }

    // If it was not found using the simple search, look for a dot and
    // search the hierarchy.
    string::size_type dot_pos = name.find("."); // zero-based index of `.'
    if (dot_pos != string::npos) {
        string aggregate = name.substr(0, dot_pos);
        string field = name.substr(dot_pos + 1);

        BaseType *agg_ptr = var(aggregate);
        if (agg_ptr) {
            if (s)
                s->push(static_cast<BaseType *>(this));

            return agg_ptr->var(field, true, s); // recurse
        }
        else
            return 0;  // qualified names must be *fully* qualified
    }

    return 0;
}
#endif
#if 0
void
Structure::print_val(FILE *out, string space, bool print_decl_p)
{
    ostringstream oss;
    print_val(oss, space, print_decl_p);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

void
Structure::print_val(ostream &out, string space, bool print_decl_p)
{
    if (print_decl_p) {
        print_decl(out, space, false);
	out << " = " ;
    }

    out << "{ " ;
    for (Vars_citer i = d_vars.begin(); i != d_vars.end();
         i++, (void)(i != d_vars.end() && out << ", ")) {
        (*i)->print_val(out, "", false);
    }

    out << " }" ;

    if (print_decl_p)
	out << ";\n" ;
}
#endif

#if 0
bool
Structure::check_semantics(string &msg, bool all)
{
    if (!BaseType::check_semantics(msg))
        return false;

    bool status = true;

    if (!unique_names(d_vars, name(), type_name(), msg))
        return false;

    if (all) {
        for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
            //assert(*i);
            if (!(*i)->check_semantics(msg, true)) {
                status = false;
                goto exit;
            }
        }
    }

exit:
    return status;
}
#endif

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and information about this
 * instance.
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void
Structure::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "Structure::dump - ("
    << (void *)this << ")" << endl ;
    DapIndent::Indent() ;
    Constructor::dump(strm) ;
    DapIndent::UnIndent() ;
}

} // namespace libdap

