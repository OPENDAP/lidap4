
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

// Implementation for BaseType.
//
// jhrg 9/6/94

#include "config.h"

#include <cstdio>  // for stdin and stdout

#include <sstream>
#include <string>

//#define DODS_DEBUG

#include "BaseType.h"
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

#include "D4Attributes.h"
#include "DMR.h"
#include "XMLWriter.h"
#include "D4BaseTypeFactory.h"

#include "InternalErr.h"

#include "util.h"
#include "escaping.h"
#include "DapIndent.h"

#include "debug.h"

using namespace std;

namespace libdap {

// Protected copy mfunc

/** Perform a deep copy. Copies the values of \e bt into \c *this. Pointers
    are dereferenced and their values are copied into a newly allocated
    instance.

    @brief Perform a deep copy.
    @param bt The source object. */
void
BaseType::m_duplicate(const BaseType &bt)
{
    DBG(cerr << "In BaseType::m_duplicate for " << bt.name() << endl);

    d_name = bt.d_name;
    d_type = bt.d_type;
    d_dataset = bt.d_dataset;
    d_is_read = bt.d_is_read; // added, reza
    d_is_send = bt.d_is_send; // added, reza
    d_in_selection = bt.d_in_selection;
    d_is_synthesized = bt.d_is_synthesized; // 5/11/2001 jhrg

    d_parent = bt.d_parent; // copy pointers 6/4/2001 jhrg

    d_attr = bt.d_attr;  // Deep copy.

    if (bt.d_attributes)
        d_attributes = new D4Attributes(*bt.d_attributes); // deep copy
    else
        d_attributes = 0; // init to null if not used.

    d_is_dap4 = bt.d_is_dap4;

    DBG(cerr << "Exiting BaseType::m_duplicate for " << bt.name() << endl);
}

// Public mfuncs

/** The BaseType constructor needs a name  and a type.
    The BaseType class exists to provide data to
    type classes that inherit from it.  The constructors of those
    classes call the BaseType constructor; it is never called
    directly.

    @brief The BaseType constructor.

    @param n A string containing the name of the new variable.
    @param t The type of the variable.
    @param is_dap4 True if this is a DAP4 variable. Default is False
    @see Type */
BaseType::BaseType(const string &n, const Type &t, bool is_dap4)
: d_name(n), d_type(t), d_dataset(""), d_is_read(false), d_is_send(false),
  d_parent(0), d_attributes(0), d_is_dap4(is_dap4),
  d_in_selection(false), d_is_synthesized(false)
{}

/** The BaseType constructor needs a name, a dataset, and a type.
    The BaseType class exists to provide data to
    type classes that inherit from it.  The constructors of those
    classes call the BaseType constructor; it is never called
    directly.

    @brief The BaseType constructor.
    @param n A string containing the name of the new variable.
    @param d A string containing the dataset name.
    @param t The type of the variable. Default is False
    @param is_dap4 True if this is a DAP4 variable.
    @see Type */
BaseType::BaseType(const string &n, const string &d, const Type &t, bool is_dap4)
: d_name(n), d_type(t), d_dataset(d), d_is_read(false), d_is_send(false),
  d_parent(0), d_attributes(0), d_is_dap4(is_dap4),
  d_in_selection(false), d_is_synthesized(false)
{}

/** @brief The BaseType copy constructor. */
BaseType::BaseType(const BaseType &copy_from) : DapObj()
{
    DBG(cerr << "In BaseTpe::copy_ctor for " << copy_from.name() << endl);
    m_duplicate(copy_from);
}

BaseType::~BaseType()
{
    DBG2(cerr << "Entering ~BaseType (" << this << ")" << endl);

    if (d_attributes)
        delete d_attributes;

    DBG2(cerr << "Exiting ~BaseType" << endl);
}

BaseType &
BaseType::operator=(const BaseType &rhs)
{
    DBG(cerr << "Entering BaseType::operator=" << endl);
    if (this == &rhs)
        return *this;

    m_duplicate(rhs);

    DBG(cerr << "Exiting BaseType::operator=" << endl);
    return *this;
}

/** Write out the object's internal fields in a string. To be used for
    debugging when regular inspection w/ddd or gdb isn't enough.

    @return A string which shows the object's internal stuff. */
string BaseType::toString()
{
    ostringstream oss;
    oss << "BaseType (" << this << "):" << endl
        << "          _name: " << name() << endl
        << "          _type: " << type_name() << endl
        << "          _dataset: " << d_dataset << endl
        << "          _read_p: " << d_is_read << endl
        << "          _send_p: " << d_is_send << endl
        << "          _synthesized_p: " << d_is_synthesized << endl
        << "          d_parent: " << d_parent << endl
        << "          d_attr: " << hex << &d_attr << dec << endl;

    return oss.str();
}

/** @brief DAP2 to DAP4 transform
 *
 * For the current BaseType, return a DAP4 'copy' of the variable.
 *
 * @note For most DAP2 types, in this implementation of DAP4 the corresponding
 * DAP4 type is the same. The different types are Sequences (which are D4Sequences
 * in the DAP4 implementation), Grids (which are coverages) and Arrays (which use
 * shared dimensions).
 *
 * @param root The root group that should hold this new variable. Add Group-level
 * stuff here (e.g., D4Dimensions).
 * @param container Add the new variable to this container.
 *
 * @return A pointer to the transformed variable
 */
void
BaseType::transform_to_dap4(D4Group */*root*/, Constructor *container)
{
    BaseType *dest = ptr_duplicate();
    // If it's already a DAP4 object then we can just return it!
    if(!is_dap4()){
        dest->attributes()->transform_to_dap4(get_attr_table());
        dest->set_is_dap4(true);
    }
    container->add_var_nocopy(dest);
}


/** @brief DAP4 to DAP2 transform
 *
 * For the current BaseType, return a DAP2 'copy' of the variable.
 *
 * @note For most DAP4 types, in this implementation of DAP2 the corresponding
 * DAP4 type is the same.
 * These types have a different representations in DAP2 and DAP4:
 *  Sequences (which are D4Sequences in the DAP4 implementation),
 *  - Grids (which are semantically subsumed by coverages in DAP4)
 *  - Arrays (which use shared dimensions in DAP4)
 *
 *  Additionally DAP4 adds the following types:
 *  - UInt8, Int8, and Char which map to Byte in DAP2.
 *  - Int64, Unit64 which have no natural representation in DAP2.
 *  - Opaque Possible Byte stuff[] plus metadata?
 *  - Enum's can be represented as Int32.
 *
 *  - Groups, with the exception of the root group "disappear" into the
 *    names of their member variables. Specifically the Group name is add as a prefix
 *    followed by a "/" separator to the names of all of the Group's member groups
 *    variables.
 *
 * @param  The AttrTable pointer parent_attr_table is used by Groups, which disappear
 * from the DAP2 representation. Their children are returned in the the BAseType vector
 * their attributes are added to parent_attr_table;
 * @return A pointer to a vector of BaseType pointers (right?). In most cases this vector
 * will contain a single pointer but DAP4 types 'disappear' such as Group will return all
 * of their member variables in the vector. DAP4 types with no representation in DAP2
 * (ex: UInt64) the will return a NULL pointer and so this must be tested!
 */
std::vector<BaseType *> *
BaseType::transform_to_dap2(AttrTable *)
{
    BaseType *dest = this->ptr_duplicate();
    // convert the d4 attributes to a dap2 attribute table.
    // HK-403. jhrg 6/17/19
#if 0
    AttrTable *attrs = this->attributes()->get_AttrTable(name());
    dest->set_attr_table(*attrs);
#else
    if (dest->get_attr_table().get_size() == 0) {
        attributes()->transform_attrs_to_dap2(&dest->get_attr_table());
        dest->get_attr_table().set_name(name());
    }
#endif

    dest->set_is_dap4(false);

    vector<BaseType *> *result =  new vector<BaseType *>();
    result->push_back(dest);
    return result;
}


/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and then displays information
 * about this base type.
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void
BaseType::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "BaseType::dump - ("
        << (void *)this << ")" << endl ;
    DapIndent::Indent() ;

    strm << DapIndent::LMarg << "name: " << name() << endl ;
    strm << DapIndent::LMarg << "type: " << type_name() << endl ;
    strm << DapIndent::LMarg << "dataset: " << d_dataset << endl ;
    strm << DapIndent::LMarg << "read_p: " << d_is_read << endl ;
    strm << DapIndent::LMarg << "send_p: " << d_is_send << endl ;
    strm << DapIndent::LMarg << "synthesized_p: " << d_is_synthesized << endl ;
    strm << DapIndent::LMarg << "parent: " << (void *)d_parent << endl ;
    strm << DapIndent::LMarg << "attributes: " << endl ;
    DapIndent::Indent() ;

    if (d_attributes)
        d_attributes->dump(strm);
    else
        d_attr.dump(strm) ;

    DapIndent::UnIndent() ;

    DapIndent::UnIndent() ;
}

/** @brief Returns the name of the class instance.
 */
string
BaseType::name() const
{
    return d_name;
}

/**
 * Return the FQN for this variable. This will include the D4 Group
 * component of the name.
 *
 * @return The FQN in a string
 */
string
BaseType::FQN() const
{
    if (get_parent() == 0)
        return name();
    else if (get_parent()->type() == dods_group_c)
        return get_parent()->FQN() + name();
    else
        return get_parent()->FQN() + "." + name();
}

/** @brief Sets the name of the class instance. */
void
BaseType::set_name(const string &n)
{
    string name = n;
    d_name = www2id(name); // www2id writes into its param.
}

/** @brief Returns the name of the dataset used to create this instance

    A dataset from which the data is to be read. The meaning of this string
    will vary among different types of data sources. It \e may be the name
    of a data file or an identifier used to read data from a relational
    database.
 */
string
BaseType::dataset() const
{
    return d_dataset;
}

/** @brief Returns the type of the class instance. */
Type
BaseType::type() const
{
    return d_type;
}

/** @brief Sets the type of the class instance. */
void
BaseType::set_type(const Type &t)
{
    d_type = t;
}

/** @brief Returns the type of the class instance as a string. */
string
BaseType::type_name() const
{
    if (is_dap4())
        return libdap::D4type_name(d_type);
    else
        return libdap::D2type_name(d_type);
}

/** @brief Returns true if the instance is a numeric, string or URL
    type variable.
    @return True if the instance is a scalar numeric, String or URL variable,
    False otherwise. Arrays (even of simple types) return False.
    @see is_vector_type() */
bool
BaseType::is_simple_type() const
{
    return libdap::is_simple_type(type());
}

/** @brief Returns true if the instance is a vector (i.e., array) type
    variable.
    @return True if the instance is an Array, False otherwise. */
bool
BaseType::is_vector_type() const
{
    return libdap::is_vector_type(type());
}

/** @brief Returns true if the instance is a constructor (i.e., Structure,
    Sequence or Grid) type variable.
    @return True if the instance is a Structure, Sequence or Grid, False
    otherwise. */
bool
BaseType::is_constructor_type() const
{
    return libdap::is_constructor_type(type());
}

/** Return a count of the total number of variables in this variable.
    This is used to count the number of variables held by a constructor
    variable - for simple type and vector variables it always
    returns 1.

    For compound data types, there are two ways to count members.
    You can count the members, or you can count the simple members
    and add that to the count of the compound members.  For
    example, if a Structure contains an Int32 and another
    Structure that itself contains two Int32 members, the element
    count of the top-level structure could be two (one Int32 and
    one Structure) or three (one Int32 by itself and two Int32's
    in the subsidiary Structure).  Use the <i>leaves</i> parameter
    to control which kind of counting you desire.

    @brief Count the members of constructor types.
    @return Returns 1 for simple
    types.  For compound members, the count depends on the
    <i>leaves</i> argument.
    @param leaves This parameter is only relevant if the object
    contains other compound data types.  If FALSE, the function
    counts only the data variables mentioned in the object's
    declaration.  If TRUE, it counts the simple members, and adds
    that to the sum of the counts for the compound members.
    This parameter has no effect for simple type variables. */
int
BaseType::element_count(bool)
{
    return 1;
}

/** Returns true if the variable is a synthesized variable. A synthesized
    variable is one that is added to the dataset by the server (usually
    with a `projection function'. */
bool
BaseType::synthesized_p()
{
    return d_is_synthesized;
}

/** Set the synthesized flag. Before setting this flag be sure to set the
    <tt>read_p()</tt> state. Once this flag is set you cannot
    alter the state of the <tt>read_p</tt> flag!

    @see synthesized_p() */
void
BaseType::set_synthesized_p(bool state)
{
    d_is_synthesized = state;
}

// Return the state of d_is_read (true if the value of the variable has been
// read (and is in memory) false otherwise).

/** Returns true if the value(s) for this variable have been read from the
    data source, otherwise returns false. This method is used to determine
    when values need to be read using the read() method. When read_p()
    returns true, this library assumes that buf2val() (and other methods
    such as get_vec()) can be used to access the value(s) of a variable.

    @brief Has this variable been read?
    @return True if the variable's value(s) have been read, false otherwise. */
bool
BaseType::read_p()
{
    return d_is_read;
}

/** Sets the value of the <tt>read_p</tt> property. This indicates that the
    value(s) of this variable has/have been read. An implementation of the
    read() method should use this to set the \c read_p property to true.

    @note If the is_synthesized property is true, this method will _not_
    alter the is_read property. If you need that behavior, specialize the
    method in your subclasses if the various types.

    @note For most of the types the default implementation of this method is
    fine. However, if you're building a server which must handle data
    represented using nested sequences, then you may need to provide a
    specialization of Sequence::set_read_p(). By default Sequence::set_read_p()
    recursively sets the \e read_p property for all child variables to
    \e state. For servers where one Sequence reads an outer set of values
    and another reads an inner set, this is cumbersome. In such a case, it is
    easier to specialize Sequence::set_read_p() so that it does \e not
    recursively set the \e read_p property for the inner Sequence. Be sure
    to see the documentation for the read() method!

    @todo Look at making synthesized variables easier to implement and at
    making them more integrated into the overall CE evaluation process.
    Maybe the code that computes the synthesized var's value should be in the
    that variable's read() method? This might provide a way to get rid of the
    awkward 'projection functions' by replacing them with real children of
    BaseType. It would also provide a way to clean up the way the
    \e synthesized_p prop intrudes on the \e read_p prop.

    @see BaseType::read()
    @brief Sets the value of the \e read_p property.
    @param state Set the \e read_p property to this state. */
void
BaseType::set_read_p(bool state)
{
    // The this comment is/was wrong!
    // The is_synthesized property was not being used and the more I thought
    // about how this was coded, the more this code below seemed like a bad idea.
    // Once the property was set, the read_p property could not be changed.
    // That seems a little silly. Also, I think I need to use this is_synthesized
    // property for some of the server function code I'm working on for Raytheon,
    // and I'd like to be able to control the read_p property! jhrg 3/9/15

    // What's true: The is_synthesized property is used by
    // 'projection functions' in the freeform handler. It might be better
    // to modify the FFtypes to support this behavior, but for now I'm returning
    // the library to its old behavior. That this change (setting is_read
    // of the value of is_syn...) broke the FF handler was not detected
    // because the FF tests were not being run due to an error in the FF
    // bes-testsuite Makefile.am). jhrg 9/9/15

#if 1
    if (!d_is_synthesized) {
        d_is_read = state;
    }
#else
    d_is_read = state;
#endif
}

/** Returns the state of the \c send_p property. If true, this variable
    should be sent to the client, if false, it should not. If no constraint
    expression (CE) has been evaluated, this property is true for all
    variables in a data source (i.e., for all the variables listed in a DDS).
    If a CE has been evaluated, this property is true only for those
    variables listed in the <em>projection part</em> of the CE.

    @brief Should this variable be sent?
    @return True if the variable should be sent to the client, false
    otherwise. */
bool
BaseType::send_p()
{
    return d_is_send;
}

/** Sets the value of the <tt>send_p</tt> flag.  This
    function is meant to be called from within the constraint evaluator of
    other code which determines that this variable should be returned to the
    client.  Data are ready to be sent when <i>both</i> the <tt>d_is_send</tt>
    and <tt>d_is_read</tt> flags are set to TRUE.

    @param state The logical state to set the <tt>send_p</tt> flag.
 */
void
BaseType::set_send_p(bool state)
{
    DBG2(cerr << "Calling BaseType::set_send_p() for: " << this->name()
        << endl);
    d_is_send = state;
}


/** Get this variable's AttrTable. It's generally a bad idea to return a
    reference to a contained object, but in this case it seems that building
    an interface inside BaseType is overkill.

    Use the AttrTable methods to manipulate the table. */
AttrTable &
BaseType::get_attr_table()
{
    return d_attr;
}

/** Set this variable's attribute table.
    @param at Source of the attributes. */
void
BaseType::set_attr_table(const AttrTable &at)
{
    d_attr = at;
}

/** DAP4 Attribute methods
 * @{
 */
D4Attributes *
BaseType::attributes()
{
    if (!d_attributes) d_attributes = new D4Attributes();
    return d_attributes;
}

void
BaseType::set_attributes(D4Attributes *attrs)
{
    d_attributes = new D4Attributes(*attrs);
}

void
BaseType::set_attributes_nocopy(D4Attributes *attrs)
{
    d_attributes = attrs;
}
///@}

/**
 * Transfer attributes from a DAS object into this variable. Because of the
 * rough history of the DAS object and the way that various server code built
 * the DAS, this is necessarily a heuristic process. The intent is that this
 * method will be overridden by handlers that need to look for certain patterns
 * in the DAS (e.g., hdf4's odd variable_dim_n; where n = 0, 1, 2, ...)
 * attribute containers.
 *
 * There should be a one-to-one
 * mapping between variables and attribute containers. However, in some cases
 * one variable has attributes spread across several top level containers and
 * in some cases one container is used by several variables
 *
 * @note This method is technically \e unnecessary because a server (or
 * client) can easily add attributes directly using the DDS::get_attr_table
 * or BaseType::get_attr_table methods and then poke values in using any
 * of the methods AttrTable provides. This method exists to ease the
 * transition to DDS objects which contain attribute information for the
 * existing servers (Since they all make DAS objects separately from the
 * DDS). They could be modified to use the same AttrTable methods but
 * operate on the AttrTable instances in a DDS/BaseType instead of those in
 * a DAS.
 *
 * @param at_container Transfer attributes from this container.
 * @return void
 */
void BaseType::transfer_attributes(AttrTable *at_container) {

    DBG(cerr << __func__ << "() -  BEGIN name:'" << name() << "'" << endl);

    AttrTable *at = at_container->get_attr_table(name());
    DBG(cerr << __func__ << "() - at: "<< (void *) at << endl);


    if (at) {
        at->set_is_global_attribute(false);
        DBG(cerr << __func__ << "() - Processing AttrTable: " << at->get_name() << endl);

        AttrTable::Attr_iter at_p = at->attr_begin();
        while (at_p != at->attr_end()) {
            DBG(cerr << __func__ << "() -  Attribute '" << at->get_name(at_p) << "' is type: " << at->get_type(at_p) << endl);
            if (at->get_attr_type(at_p) == Attr_container){
                // An attribute container may actually represent a child member variable. When
                // that's the case we don't want to add the container to the parent type, but
                // rather let any child of BaseType deal with those containers in the child's
                // overridden transfer_attributes() method.
                // We capitalize on the magic of the BaseType API and utilize the var() method
                // to check for a child variable of the same name and, if one exists, we'll skip
                // this AttrTable and let a child constructor class like Grid or Constructor
                // deal with it.
                BaseType *bt = var(at->get_name(at_p),true);
                if(bt==0){
                    DBG(cerr << __func__ << "() - Adding container '" << at->get_name(at_p) << endl);
                    get_attr_table().append_container(new AttrTable(*at->get_attr_table(at_p)), at->get_name(at_p));
                }
                else {
                    DBG(cerr << __func__ << "() - Found child var: '"<< bt->type_name()<< " " << bt->name() << " (address:" << (void *) bt << ")" << endl);
                    DBG(cerr << __func__ << "() -  Skipping container '" << at->get_name(at_p) << endl);
                }
            }
            else {
                DBG(cerr << __func__ << "() - Adding Attribute '" << at->get_name(at_p) << endl);
                get_attr_table().append_attr(at->get_name(at_p), at->get_type(at_p), at->get_attr_vector(at_p));
            }
            at_p++;
        }
    }
    else {
        DBG(cerr << __func__ << "() - Unable to locate AttrTable '" << name() << "'  SKIPPING" << endl);

    }
}

/** Does this variable appear in either the selection part or as a function
    argument in the current constrain expression. If this property is set
    (true) then implementations of the read() method should read this
    variable.

    @note This method does not check, nor does it know about the semantics of,
    string arguments passed to functions. Those functions might include
    variable names in strings; they are responsible for reading those variables.
    See the grid (func_grid_select()) for an example.
    @see BaseType::read()
    @brief Is this variable part of the current selection? */
bool
BaseType::is_in_selection()
{
    return d_in_selection;
}

/** Set the \e in_selection property to \e state. This property indicates
    that the variable is used as a parameter to a constraint expression
    function or that it appears as an argument in a selection sub-expression.
    If set (true), implementations of the BaseType::read() method should read
    this variable.

    @param state Set the \e in_selection property to this state.
    @see BaseType::read()
    @see BaseType::is_in_selection() for more information. */
void
BaseType::set_in_selection(bool state)
{
    d_in_selection = state;
}

// Protected method.
/** Set the <tt>parent</tt> property for this variable.

    @note Added ability to set parent to null. 10/19/12 jhrg

    @param parent Pointer to the Constructor of Vector parent variable or null
    if the variable has no parent (if it is at the top-level of a DAP2/3 DDS).
    @exception InternalErr thrown if called with anything other than a
    Constructor, Vector or Null. */
void
BaseType::set_parent(BaseType *parent)
{
    if (!dynamic_cast<Constructor *>(parent)
        && !dynamic_cast<Vector *>(parent)
        && parent != 0)
        throw InternalErr("Call to set_parent with incorrect variable type.");

    d_parent = parent;
}

// Public method.

/** Return a pointer to the Constructor or Vector which holds (contains)
    this variable. If this variable is at the top level, this method
    returns null.

    @return A BaseType pointer to the variable's parent. */
BaseType *
BaseType::get_parent() const
{
    return d_parent;
}

// Documented in the header file.
BaseType *
BaseType::var(const string &/*name*/, bool /*exact_match*/, btp_stack */*s*/)
{
    return static_cast<BaseType *>(0);
}

/** This version of var(...) searches for <i>name</i> and returns a
    pointer to the BaseType object if found. It uses the same search
    algorithm as BaseType::var(const string &, bool, btp_stack *) when
    <i>exact_match</i> is false. In addition to returning a pointer to
    the variable, it pushes onto <i>s</i> a BaseType pointer to each
    constructor type that ultimately contains <i>name</i>.

    @note The BaseType implementation always returns null. There are no default
    values for the parameters. If var() is called w/o any params, the three
    parameter version will be used.

    @deprecated This method is deprecated because it tries first to use
    exact_match and, if that fails, then tries leaf_match. It's better to use
    the alternate form of var(...) and specify exactly what you'd like to do.

    @return A pointer to the named variable. */
BaseType *
BaseType::var(const string &, btp_stack &)
{
    return static_cast<BaseType *>(0);
}

/** Adds a variable to an instance of a constructor class, such as Array,
    Structure <em>et cetera</em>. This function is only used by those
    classes. For constructors with more than one variable, the variables
    appear in the same order in which they were added (i.e., the order in
    which add_var() was called). Since this method is only for use by Vectors
    and Constructors, the BaseType implementation throws InternalErr.

    @note For the implementation of this method in Structure, Sequence, et c.,
    first copy \e bt and then insert the copy. If \e bt is itself a constructor
    type you must either use the var() method to get a pointer to the actual
    instance added to \c *this or you must first add all of <em>bt</em>'s
    children to it before adding it to \c *this. The implementations should use
    m_duplicate() to perform a deep copy of \e bt.

    @brief Add a variable.

    @todo We should get rid of the Part parameter and adopt the convention
    that the first variable is the Array and all subsequent ones are Maps
    (when dealing with a Grid, the only time Part matters). This would enable
    several methods to migrate from Structure, Sequence and Grid to
    Constructor.

    @param bt The variable to be added to this instance. The caller of this
    method <i>must</i> free memory it allocates for <tt>v</tt>. This method
    will make a deep copy of the object pointed to by <tt>v</tt>.
    @param part The part of the constructor data to be modified. Only
    meaningful for Grid variables.

    @see Part */
void
BaseType::add_var(BaseType *, Part)
{
    throw InternalErr(__FILE__, __LINE__, "BaseType::add_var unimplemented");
}

void
BaseType::add_var_nocopy(BaseType *, Part)
{
    throw InternalErr(__FILE__, __LINE__, "BaseType::add_var_nocopy unimplemented");
}

/** This method should be implemented for each of the data type classes (Byte,
    ..., Grid) when using the DAP class library to build a server. This
    method is only for DAP servers. The library provides a default
    definition here which throws an InternalErr exception \e unless the read_p
    property has been set. In that case it returns false, indicating that all
    the data have been read. The latter case can happen when building a
    constant value that needs to be passed to a function. The variable/constant
    is loaded with a value when it is created.

    When implementing a new DAP server, the Byte, ..., Grid data type classes
    are usually specialized. In each of those specializations read() should
    be defined to read values from the data source and store them in the
    object's local buffer. The read() method is called by other methods in
    this library. When writing read(), follow these rules:

    <ul>
    <li> read() should throw Error if it encounters an error. The message
   should be verbose enough to be understood by someone running a
   client on a different machine.</li>
    <li> The value(s) should be read if and only if either send_p() or
          is_in_selection() return true. If neither of these return true, the
   value(s) should not be read. This is important when writing read()
   for a Constructor type such as Grid where a client may ask for only
   the map vectors (and thus reading the much larger Array part is not
   needed).</li>
    <li> For each specialization of read(), the method should first test
          the value of the \c read_p property (using the read_p() method)
          and read values only if the value of read_p() is false. Once the
          read() method reads data and stores it in the instance, it must
          set the value of the \c read_p property to true using set_read_p().
          If your read() methods fail to do this data may not serialize
          correctly.</li>
    <li> The Array::read() and Grid::read() methods should take into account
   any restrictions on Array sizes.</li>
    <li> If you are writing Sequence::read(), be sure to check the
          documentation for Sequence::read_row() and Sequence::serialize()
          so you understand how Sequence::read() is being called.</li>
    <li> For Sequence::read(), your specialization must correctly manage the
          \c unsent_data property and row count in addition to the \c read_p
          property (handle the \c read_p property as describe above). For a
          Sequence to serialize correctly, once all data from the Sequence
          has been read, \c unsent_data property must be set to false (use
          Sequence::set_unsent_data()). Also, at that time the row number
          counter must be reset (use Sequence::reset_row_counter()). Typically
          the correct time to set \c unsent_data to false and reset the row
          counter is the time when Sequence::read() return false indicating
          that all the data for the Sequence have been read. Failure to
          handle these tasks will break serialization of nested Sequences. Note
          that when Sequence::read() returns with a result of true (indicating
          there is more data to send, the value of the \c unsent_data property
          should be true.

          Also, if you server must handle nested sequences, be sure to read
          about subclassing set_read_p().</li>
    </ul>

    @brief Read data into a local buffer.

	@todo Modify the D4 serialize code so that it supports the true/false
	behavior of read() for arrays.

	@todo Modify all of the stock handlers so they conform to this!

    @return False means more data remains to be read, True indicates that no
    more data need to be read. For Sequence and D4Sequence, this method will
    generally read one instance of the Sequence; for other types it will generally
    read the entire variable modulo any limitations due to a constraint. However,
    the library should be written so that read can return less than all of the data
    for a variable - serialize() would then call the function until it returns
    True.

    @see BaseType */
bool
BaseType::read()
{
    if (d_is_read)
        return true;

    throw InternalErr("Unimplemented BaseType::read() method called for the variable named: " + name());
}

void
BaseType::intern_data(ConstraintEvaluator &, DDS &/*dds*/)
{
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_on();
#endif
    DBG2(cerr << "BaseType::intern_data: " << name() << endl);
    if (!read_p())
        read();          // read() throws Error and InternalErr
#if USE_LOCAL_TIMEOUT_SCHEME
    dds.timeout_off();
#endif
}

/**
 * @brief Read data into this variable
 * @param eval Evaluator for a constraint expression
 * @param dmr DMR for the whole dataset
 */
void
BaseType::intern_data(/*Crc32 &checksum, DMR &, ConstraintEvaluator &*/)
{
    if (!read_p())
        read();          // read() throws Error and InternalErr
#if 0
    compute_checksum(checksum);
#endif
}

bool
BaseType::serialize(ConstraintEvaluator &, DDS &,  Marshaller &, bool)
{
    throw InternalErr(__FILE__, __LINE__, "The DAP2 serialize() method has not been implemented for " + type_name());
}

bool
BaseType::deserialize(UnMarshaller &, DDS *, bool)
{
    throw InternalErr(__FILE__, __LINE__, "The DAP2 deserialize() method has not been implemented for " + type_name());
}

void
BaseType::serialize(D4StreamMarshaller &, DMR &, /*ConstraintEvaluator &,*/ bool)
{
    throw InternalErr(__FILE__, __LINE__, "The DAP4 serialize() method has not been implemented for " + type_name());
}

void
BaseType::deserialize(D4StreamUnMarshaller &, DMR &)
{
    throw InternalErr(__FILE__, __LINE__, "The DAP4 deserialize() method has not been implemented for " + type_name());
}

/** Write the variable's declaration in a C-style syntax. This
    function is used to create textual representation of the Data
    Descriptor Structure (DDS).  See <i>The DODS User Manual</i> for
    information about this structure.

    A simple array declaration might look like this:
    \verbatim
    Float64 lat[lat = 180];
    \endverbatim
    While a more complex declaration (for a Grid, in this case),
    would look like this:
    \verbatim
    Grid {
    ARRAY:
    Int32 sst[time = 404][lat = 180][lon = 360];
    MAPS:
    Float64 time[time = 404];
    Float64 lat[lat = 180];
    Float64 lon[lon = 360];
    } sst;
    \endverbatim

    @brief Print an ASCII representation of the variable structure.
    @param out The output stream on which to print the
    declaration.
    @param space Each line of the declaration will begin with the
    characters in this string.  Usually used for leading spaces.
    @param print_semi A boolean value indicating whether to print a
    semicolon at the end of the declaration.
    @param constraint_info A boolean value indicating whether
    constraint information is to be printed with the declaration.
    If the value of this parameter is TRUE, <tt>print_decl()</tt> prints
    the value of the variable's <tt>send_p()</tt> flag after the
    declaration.
    @param constrained If this boolean value is TRUE, the variable's
    declaration is only printed if is the <tt>send_p()</tt> flag is TRUE.
    If a constraint expression is in place, and this variable is not
    requested, the <tt>send_p()</tt> flag is FALSE.

    @see DDS
    @see DDS::CE
 */
void
BaseType::print_decl(FILE *out, string space, bool print_semi,
    bool constraint_info, bool constrained)
{
    ostringstream oss;
    print_decl(oss, space, print_semi, constraint_info, constrained);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

/** Write the variable's declaration in a C-style syntax. This
    function is used to create textual representation of the Data
    Descriptor Structure (DDS).  See <i>The DODS User Manual</i> for
    information about this structure.

    A simple array declaration might look like this:
    \verbatim
    Float64 lat[lat = 180];
    \endverbatim
    While a more complex declaration (for a Grid, in this case),
    would look like this:
    \verbatim
    Grid {
    ARRAY:
    Int32 sst[time = 404][lat = 180][lon = 360];
    MAPS:
    Float64 time[time = 404];
    Float64 lat[lat = 180];
    Float64 lon[lon = 360];
    } sst;
    \endverbatim

    @brief Print an ASCII representation of the variable structure.
    @param out The output stream on which to print the
    declaration.
    @param space Each line of the declaration will begin with the
    characters in this string.  Usually used for leading spaces.
    @param print_semi A boolean value indicating whether to print a
    semicolon at the end of the declaration.
    @param constraint_info A boolean value indicating whether
    constraint information is to be printed with the declaration.
    If the value of this parameter is TRUE, <tt>print_decl()</tt> prints
    the value of the variable's <tt>send_p()</tt> flag after the
    declaration.
    @param constrained If this boolean value is TRUE, the variable's
    declaration is only printed if is the <tt>send_p()</tt> flag is TRUE.
    If a constraint expression is in place, and this variable is not
    requested, the <tt>send_p()</tt> flag is FALSE.

    @see DDS
    @see DDS::CE
 */
void
BaseType::print_decl(ostream &out, string space, bool print_semi,
    bool constraint_info, bool constrained)
{
    // if printing the constrained declaration, exit if this variable was not
    // selected.
    if (constrained && !send_p())
        return;

    out << space << type_name() << " " << id2www(name()) ;

    if (constraint_info) {
        if (send_p())
            out << ": Send True" ;
        else
            out << ": Send False" ;
    }

    if (print_semi)
        out << ";\n" ;
}

/** Prints the value of the variable, with its declaration. This
function is primarily intended for debugging DODS
applications. However, it can be overloaded and used to do
some useful things. Take a look at the asciival and writeval
clients, both of which overload this to output the values of
variables in different ways.

@brief Prints the value of the variable.

@param out The output stream on which to print the value.
@param space This value is passed to the print_decl()
function, and controls the leading spaces of the output.
@param print_decl_p A boolean value controlling whether the
variable declaration is printed as well as the value. */
void
BaseType::print_val(FILE *out, string space, bool print_decl_p)
{
    ostringstream oss;
    print_val(oss, space, print_decl_p);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

/** Write the XML representation of this variable. This method is used to
    build the DDX XML response.
    @param out Destination.
    @param space Use this to indent child declarations. Default is "".
    @param constrained If true, only print this if it's part part of the
    current projection. Default is False.
    @deprecated */
void
BaseType::print_xml(FILE *out, string space, bool constrained)
{
    XMLWriter xml(space);
    print_xml_writer(xml, constrained);
    fwrite(xml.get_doc(), sizeof(char), xml.get_doc_size(), out);
}

/** Write the XML representation of this variable. This method is used to
    build the DDX XML response.
    @param out Destination output stream
    @param space Use this to indent child declarations. Default is "".
    @param constrained If true, only print this if it's part part of the
    current projection. Default is False.
    @deprecated */
void
BaseType::print_xml(ostream &out, string space, bool constrained)
{
    XMLWriter xml(space);
    print_xml_writer(xml, constrained);
    out << xml.get_doc();
}

/** Write the XML representation of this variable. This method is used to
    build the DDX XML response.
    @param out Destination output stream
    @param space Use this to indent child declarations. Default is "".
    @param constrained If true, only print this if it's part part of the
    current projection. Default is False. */
void
BaseType::print_xml_writer(XMLWriter &xml, bool constrained)
{
    if (constrained && !send_p())
        return;

    if (xmlTextWriterStartElement(xml.get_writer(), (const xmlChar*)type_name().c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write " + type_name() + " element");

    if (!name().empty())
        if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "name", (const xmlChar*)name().c_str()) < 0)
            throw InternalErr(__FILE__, __LINE__, "Could not write attribute for name");

    if (is_dap4())
        attributes()->print_dap4(xml);

    if (!is_dap4() && get_attr_table().get_size() > 0)
        get_attr_table().print_xml_writer(xml);

    if (xmlTextWriterEndElement(xml.get_writer()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not end " + type_name() + " element");
}

/** Write the DAP4 XML representation for this variable. This method is used
 * to build the DAP4 DMR response object.
 *
 * @param xml An XMLWriter that will do the serialization
 * @param constrained True if the response should show the variables subject
 * to the current constraint expression.
 */
void
BaseType::print_dap4(XMLWriter &xml, bool constrained)
{
    print_xml_writer(xml, constrained);
}

// Compares the object's current state with the semantics of a particular
// type. This will typically be defined in ctor classes (which have
// complicated semantics). For BaseType, an object is semantically correct if
// it has both a non-null name and type.
//
// NB: This is not the same as an invariant -- during the parse objects exist
// but have no name. Also, the bool ALL defaults to false for BaseType. It is
// used by children of CtorType.
//
// Returns: true if the object is semantically correct, false otherwise.

/** This function checks the class instance for internal
    consistency.  This is important to check for complex constructor
    classes.  For BaseType, an object is semantically correct if it
    has both a non-null name and type.

    For example, an Int32 instance would return FALSE if it had no
    name or no type defined.  A Grid instance might return FALSE for
    more complex reasons, such as having Map arrays of the wrong
    size or shape.

    This function is used by the DDS class, and will rarely, if
    ever, be explicitly called by a DODS application program.  A
    variable must pass this test before it is sent, but there may be
    many other stages in a retrieve operation where it would fail.

    @brief Compare an object's current state with the semantics of its
    type.
    @return Returns FALSE when the current state violates some
    aspect of the type semantics, TRUE otherwise.

    @param msg A returned string, containing a message indicating
    the source of any problem.
    @param all For complex constructor types (Grid,
    Sequence, Structure), this flag indicates whether to check the
    semantics of the member variables, too.

    @see DDS::check_semantics
 */
bool
BaseType::check_semantics(string &msg, bool)
{
    bool sem = (d_type != dods_null_c && name().length());

    if (!sem)
        msg = "Every variable must have both a name and a type\n";

    return sem;
}

/** This method contains the relational operators used by the constraint
    expression evaluator in the DDS class. Each class that wants to be able
    to evaluate relational expressions must overload this function. The
    implementation in BaseType throws an InternalErr exception. The DAP
    library classes Byte, ..., Url provide specializations of this method. It
    is not meaningful for classes such as Array because relational
    expressions using Array are not supported.

    The <i>op</i> argument refers to a table generated by bison from
    the constraint expression parser.  Use statements like the
    following to correctly interpret its value:

    \verbatim
    switch (op) {
        case EQUAL: return i1 == i2;
        case NOT_EQUAL: return i1 != i2;
        case GREATER: return i1 > i2;
        case GREATER_EQL: return i1 >= i2;
        case LESS: return i1 < i2;
        case LESS_EQL: return i1 <= i2;
        case REGEXP: throw Error("Regular expressions are not supported for integer values");
        default: throw Error("Unknown operator");
    }
    \endverbatim

    This function is used by the constraint expression evaluator.

    @brief Evaluate relational operators.
    @param b Compare the value of this instance with \e b.
    @param op An integer index indicating which relational operator
    is implied. Choose one from the following: <tt>EQUAL</tt>,
    <tt>NOT_EQUAL</tt>, <tt>GREATER</tt>, <tt>GREATER_EQL</tt>,
    <tt>LESS</tt>, <tt>LESS_EQL</tt>, and <tt>REGEXP</tt>.
    @return The boolean value of the comparison.
    @see BaseType::d4_ops(BaseType *, int)
 */
bool
BaseType::ops(BaseType *, int)
{
    // Even though ops is a public method, it can never be called because
    // they will never have a BaseType object since this class is abstract,
    // however any of the child classes could by mistake call BaseType::ops
    // so this is an internal error. Jose Garcia
    throw InternalErr(__FILE__, __LINE__, "Unimplemented operator.");
}

/**
 * @brief Evaluator a relop for DAP4
 *
 * This method is used by the filter expression evaluation code in DAP4.
 * Each of the 'data type' classes that support relops must overload this
 * method. In an expression of the form arg1 op arg2, this object is arg1,
 * the parameter 'b' is arg2 and op is the relational operator.
 *
 * @note I used the same relop codes for DAP4 as in the DAP2 parser/scanner
 * which makes for some coupling between them, but cuts way down on the
 * duplication of the evaluator logic, which is somewhat involved.
 *
 * @param b The second argument in the relational expression
 * @param op The infix relational operator
 * @return True if the expression is true, False otherwise.
 */
bool
BaseType::d4_ops(BaseType *, int)
{
    throw InternalErr(__FILE__, __LINE__, "Unimplemented operator.");
}

/**
 * @brief How many bytes does this variable use
 * Return the number of bytes of storage this variable uses. For scalar types,
 * this is pretty simple (an int32 uses 4 bytes, etc.). For arrays and Constructors,
 * it is a bit more complex. Note that a scalar String variable uses sizeof(String*)
 * bytes, not the length of the string value. The width() of a String array returns
 * the number of elements in the array times sizeof(String*).
 *
 * @param constrained Should the current constraint be taken into account?
 * @return Bytes of storage
 */
uint64_t
BaseType::width(bool /* constrained */) const
{
    throw InternalErr(__FILE__, __LINE__, "not implemented");
}

} // namespace libdap
