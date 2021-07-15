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

#ifdef WIN32
#include <io.h>
#include <process.h>
#include <fstream>
#else
#include <unistd.h>    // for alarm and dup
#include <sys/wait.h>
#endif

#include <cassert>

#include <iostream>
#include <sstream>

//#define DODS_DEBUG
//#define DODS_DEBUG2

#include "D4Group.h"
#include "BaseType.h"
#include "Array.h"
#include "Grid.h"
#include "DMR.h"
#include "XMLWriter.h"
#include "D4BaseTypeFactory.h"
#include "D4Attributes.h"

#include "DDS.h"	// Included so DMRs can be built using a DDS for 'legacy' handlers

#include "debug.h"
#include "DapIndent.h"

/**
 * DapXmlNamespaces
 *
 * TODO  Replace all uses of the following variables with calls to DapXmlNamespaces
 */
const string c_xml_xsi = "http://www.w3.org/2001/XMLSchema-instance";
const string c_xml_namespace = "http://www.w3.org/XML/1998/namespace";

const string c_default_dap40_schema_location = "http://xml.opendap.org/dap/dap4.0.xsd";

const string c_dap40_namespace = "http://xml.opendap.org/ns/DAP/4.0#";

const string c_dap_40_n_sl = c_dap40_namespace + " " + c_default_dap40_schema_location;

using namespace std;

namespace libdap {

void
DMR::m_duplicate(const DMR &dmr)
{
    // This is needed because we use the factory to make a new instance of the root group
    assert(dmr.OK());

    d_factory = dmr.d_factory; // Shallow copy here

    d_name = dmr.d_name;
    d_filename = dmr.d_filename;

    d_dap_major = dmr.d_dap_major;
    d_dap_minor = dmr.d_dap_minor;
    d_dap_version = dmr.d_dap_version;       // String version of the protocol

    d_dmr_version = dmr.d_dmr_version;

    d_request_xml_base = dmr.d_request_xml_base;

    d_namespace = dmr.d_namespace;

    d_max_response_size_kb = dmr.d_max_response_size_kb;

    d_ce_empty = dmr.d_ce_empty;

    // Deep copy, using ptr_duplicate()
    // d_root can only be a D4Group, so the thing returned by ptr_duplicate() must be a D4Group.
    d_root = static_cast<D4Group*>(dmr.d_root->ptr_duplicate());
    DBG(cerr << "dmr.d_root: " << dmr.d_root << endl);
    DBG(cerr << "d_root (from ptr_dup(): " << d_root << endl);

    //d_root = static_cast<D4Group*>(dmr.d_factory->NewVariable(dods_group_c, dmr.d_root->name()));
}

/**
 * Make a DMR which uses the given BaseTypeFactory to create variables.
 *
 * @note The default DAP version is 4.0 - use the DDS class to make DAP2
 * things. The default DMR version is 1.0
 *
 * @param factory The D4BaseTypeFactory to use when creating instances of
 * DAP4 variables. The caller must ensure the factory's lifetime is at least
 * that of the DMR instance.
 * @param name The name of the DMR - usually derived from the name of the
 * pathname or table name of the dataset.
 */
DMR::DMR(D4BaseTypeFactory *factory, const string &name)
        : d_factory(factory), d_name(name), d_filename(""),
          d_dap_major(4), d_dap_minor(0),
          d_dmr_version("1.0"), d_request_xml_base(""),
          d_namespace(c_dap40_namespace), d_max_response_size_kb(0),
          d_ce_empty(false),d_root(0)
{
    // sets d_dap_version string and the two integer fields too
    set_dap_version("4.0");
}

/** @brief Build a DMR using a DAP2 DDS.
 *
 * Given a DDS from code written for DAP2, build a DAP4 DMR object. This
 * works because DAP4 subsumes DAP2, but there are a few quirks... For
 * each variable in the DDS, transform it to the equivalent DAP4 variable
 * type and then copy the variable's attributes. Most types convert easily.
 * Types that need special treatment are:
 * Array: DAP2 array dimensions must be morphed to DAP4
 * Sequence: Make a D4Sequence
 * Grid: Make a coverage; assume Grids with the same dimension names
 * have 'shared dimensions' and that maps with the same names are shared too.
 *
 * @note Assume that a DDS has only a root group. This is not actually
 * true for a DDS from the HDF5 handler, because it has Groups encoded
 * into the variable names. jhrg 3/18/14
 *
 * @param factory Factory class used to make new variables
 * @param dds Get the variables to convert from this DAP2 DDS.
 * @see BaseType::transform_to_dap4()
 */
DMR::DMR(D4BaseTypeFactory *factory, DDS &dds)
        : d_factory(factory), d_name(dds.get_dataset_name()),
          d_filename(dds.filename()), d_dap_major(4), d_dap_minor(0),
          d_dmr_version("1.0"), d_request_xml_base(""),
          d_namespace(c_dap40_namespace), d_max_response_size_kb(0),d_ce_empty(false), d_root(0)
{
    // sets d_dap_version string and the two integer fields too
    set_dap_version("4.0");

    build_using_dds(dds);
#if 0
    for (DDS::Vars_iter i = dds.var_begin(), e = dds.var_end(); i != e; ++i) {
    	BaseType *new_var = (*i)->transform_to_dap4(root() /*group*/, root() /*container*/);
    	// If the variable being transformed is a Grid,
    	// then Grid::transform_to_dap4() will add all the arrays to the
    	// container (root() in this case) and return null, indicating that
    	// this code does not need to do anything to add the transformed variable.
    	if (new_var)
    		root()->add_var_nocopy(new_var);
    }

    // Now copy the global attributes
    root()->attributes()->transform_to_dap4(dds.get_attr_table());
#endif
}

/**
 * Make a DMR which uses the given BaseTypeFactory to create variables.
 *
 * @note The default DAP version is 4.0 - use the DDS class to make DAP2
 * things. The default DMR version is 1.0
 */
DMR::DMR()
        : d_factory(0), d_name(""), d_filename(""), d_dap_major(4), d_dap_minor(0),
          d_dap_version("4.0"), d_dmr_version("1.0"), d_request_xml_base(""),
          d_namespace(c_dap40_namespace), d_max_response_size_kb(0), d_ce_empty(false),d_root(0)
{
    // sets d_dap_version string and the two integer fields too
    set_dap_version("4.0");
}

/** The DMR copy constructor. */
DMR::DMR(const DMR &rhs) : DapObj()
{
    m_duplicate(rhs);
}

/** Delete a DMR. The BaseType factory is not freed, while the contained
 * group is.
 */
DMR::~DMR()
{
#if 1
    delete d_root;
#endif
}

DMR &
DMR::operator=(const DMR &rhs)
{
    if (this == &rhs)
        return *this;

    m_duplicate(rhs);

    return *this;
}

/**
 * If we have a DDS that includes Attributes, use it to build the DMR. This
 * will copy all of the variables in the DDS into the DMR using BaseType::transform_to_dap4(),
 * so the actual types added can be controlled by code that specializes
 * the various type classes.
 *
 * @param dds Read variables and Attributes from this DDS
 */
void DMR::build_using_dds(DDS &dds)
{
    set_name(dds.get_dataset_name());
    set_filename(dds.filename());

    D4Group *root_grp = root();
    for (DDS::Vars_iter i = dds.var_begin(), e = dds.var_end(); i != e; ++i) {
        BaseType *d4_var = root()->var((*i)->name());
        // Don't add duplicate variables. We have to make this check
        // because some of the child variables may add arrays
        // to the root object. For example, this happens in
        // Grid with the Map Arrays - ndp - 05/08/17
        if(!d4_var){
            // no variable of this name is in the root group at this point. Add it.
            DBG(cerr << __func__ << "() - Transforming top level variable: " <<
                " (" << (*i)->type_name() << ":'" << (*i)->name() << "':"<<(void *)(*i) <<
                ") (root:"<< root_grp << ")"<< endl; );
            (*i)->transform_to_dap4(root_grp, root_grp);
            DBG(cerr << __func__ << "() - top level variable: '" <<
                (*i)->name() << "' (type:" << (*i)->type_name() << ") Transformed"<< endl; );
        }
        else {
            DBG(cerr << __func__ << "() - Skipping variable: " <<
                d4_var->type_name() << " " << d4_var->name() << " because a variable with" <<
                " this name already exists in the root group." << endl; );
        }
    }

    // Now copy the global attributes
    root()->attributes()->transform_to_dap4(dds.get_attr_table());
}

#if 0
/**
 * If we have a DMR that includes Attributes, use it to build the DDS. This
 * will copy all of the variables in the DMR into the DDS using
 * BaseType::transform_to_dap2(), so the actual types added can be
 * controlled by code that specializes the various type classes.
 *
 * @todo Fix this so that it does not take a reference to a DMR - since
 * this is a method of DMR and seems to operate on *this.
 *
 * @param dds Read variables and Attributes from this DDS
 */
DDS *DMR::getDDS(DMR &dmr)
{
    DBG( cerr << __func__ << "() - BEGIN" << endl);
    D4Group *root = dmr.root();

    BaseTypeFactory *btf = new BaseTypeFactory();
    DDS *dds = new DDS(btf, dmr.name());
    dds->filename(dmr.filename());
    AttrTable *dds_at = &(dds->get_attr_table());

    // Now copy the global attributes
    // D4Attributes::load_AttrTable(dds_at,root->attributes());

    // TODO Make this a unique_ptr<> and let the compiler delete it. jhrg 6/17/19
    vector<BaseType *> *top_vars = root->transform_to_dap2(dds_at, true);
    vector<BaseType *>::iterator vIter = top_vars->begin();
    vector<BaseType *>::iterator vEnd = top_vars->end();
    for (; vIter != vEnd; vIter++) {
        dds->add_var_nocopy(*vIter);
    }
    delete top_vars;

#if 0
    set<string> shared_dim_candidates;

    vector<BaseType *> dropped_vars;
    for (D4Group::Vars_iter i = root->var_begin(), e = root->var_end(); i != e; ++i)
    {
        DBG( cerr << __func__ << "() - Processing top level variable '"<< (*i)->type_name() << " " << (*i)->name() << "' to DDS." << endl; );
        vector<BaseType *> *new_vars = (*i)->transform_to_dap2(&(dds->get_attr_table()));
        if(new_vars!=0) {
            vector<BaseType*>::iterator vIter = new_vars->begin();
            vector<BaseType*>::iterator end = new_vars->end();
            for(; vIter!=end; vIter++ ) {
                BaseType *new_var = (*vIter);
                DBG( cerr << __func__ << "() - Adding variable name: '"<< new_var->name() << "' " <<
                    "type: " << new_var->type() << " " <<
                    "type_name: " << new_var->type_name() << " to DDS." << endl; );
                dds->add_var_nocopy(new_var);
                Grid *grid = dynamic_cast <Grid *>(new_var);
                if(grid) {
                    Grid::Map_iter m = grid->map_begin();
                    for(; m != grid->map_end(); m++) {
                        shared_dim_candidates.insert((*m)->name());
                    }
                }
                (*vIter) = 0;
            }
            delete new_vars;
        }
        else {
            DBG( cerr << __func__ << "Adding variable '"<< (*i)->type_name() << " " << (*i)->name() << "' to drop list." << endl; );
            dropped_vars.push_back((*i));
        }
    }
    AttrTable *dv_table = Constructor::make_dropped_vars_attr_table(&dropped_vars);
    if(dv_table) {
        DBG( cerr << __func__ << "() - Adding dropped variable AttrTable." << endl);
        dds_at->append_container(dv_table,dv_table->get_name());
    }

    // Get all the child groups.
    D4Group::groupsIter gIter = root->grp_begin();
    D4Group::groupsIter gEnd = root->grp_end();
    for(; gIter!=gEnd; gIter++) {
        D4Group *grp = *gIter;
        DBG( cerr << __func__ << "() - Processing D4Group " << grp->name() << endl);
        vector<BaseType *> *d2_vars = grp->transform_to_dap2(dds_at);
        if(d2_vars) {
            DBG( cerr << __func__ << "() - Processing " << grp->name() << " Member Variables." << endl);
            vector<BaseType *>::iterator vIter = d2_vars->begin();
            vector<BaseType *>::iterator vEnd = d2_vars->end();
            for(; vIter!=vEnd; vIter++) {
                DBG( cerr << __func__ << "() - Processing " << grp->name() << " Member Variable: " << (*vIter)->name() << endl);
                dds->add_var(*vIter);
            }
        }
    }
#endif

    DBG( cerr << __func__ << "() - END" << endl);
    return dds;
}
#endif

/**
 * @brief Build a DDS from a DMR
 *
 * Build a DDS from a DMR, collapsing DAP4 Groups, transforming Arrays to Grids
 * as needed, and moving the attributes around to match the new variables. All
 * of the variables in the returned DDS object are newly allocated with separate
 * lifetimes from the objects in the DMR. They are made using ptr_duplicate()
 * so that all of the variables mirror specializations for the various types.
 * That is, if the HDF5 handler built the DMR, then the resulting DDS will hold
 * instances of H5Int32, etc.
 *
 * @note The caller is responsible for deleting the resulting DDS object.
 *
 * @return A pointer to the newly allocated DDS.
 */
    DDS *
DMR::getDDS(bool show_shared_dims)
{
#if 0
    return DMR::getDDS(*this);
#else
    DBG( cerr << __func__ << "() - BEGIN" << endl);

#if 0
    BaseTypeFactory *btf = new BaseTypeFactory();
#endif
    BaseTypeFactory btf;
    DDS *dds = new DDS(&btf, name());

    // Rename DDS container if there is a group with explicit container's name
    string cname = "";
    for (D4Group::groupsIter i = root()->grp_begin(), e = root()->grp_end(); i != e; ++i) {
        size_t pos = (*i)->name().find("_excon");
        if (pos != string::npos) {
            cname = (*i)->name().substr(0,pos);
        }
    }
    if(!cname.empty()) dds->container_name(cname);

    dds->filename(filename());

    // Now copy the global attributes
    // TODO Make this a unique_ptr<> and let the compiler delete it. jhrg 6/17/19
    vector<BaseType *> *top_vars = root()->transform_to_dap2(&(dds->get_attr_table())/*, true*/, show_shared_dims);
    for (vector<BaseType *>::iterator i = top_vars->begin(), e = top_vars->end(); i != e; i++) {
        dds->add_var_nocopy(*i);
    }
    delete top_vars;

    DBG( cerr << __func__ << "() - END" << endl);
    
    dds->set_factory(0);
    return dds;
#endif
}

/**
 * Get the root group for this DMR. This accessor allocates the root group
 * if one does not exist using the factory class bound to this DMR
 *
 * @return A pointer to the root group.
 */
D4Group *
DMR::root()
{
	if (!d_root) d_root = static_cast<D4Group*>(d_factory->NewVariable(dods_group_c, "/"));
	return d_root;
}

/**
 * Given the DAP protocol version, parse that string and set the DMR fields.
 *
 * @param v The version string.
 */
void
DMR::set_dap_version(const string &v)
{
    istringstream iss(v);

    int major = -1, minor = -1;
    char dot;
    if (!iss.eof() && !iss.fail())
        iss >> major;
    if (!iss.eof() && !iss.fail())
        iss >> dot;
    if (!iss.eof() && !iss.fail())
        iss >> minor;

    if (major == -1 || minor == -1 or dot != '.')
        throw InternalErr(__FILE__, __LINE__, "Could not parse dap version. Value given: " + v);

    d_dap_version = v;

    d_dap_major = major;
    d_dap_minor = minor;

    // Now set the related XML constants. These might be overwritten if
    // the DMR instance is being built from a document parse, but if it's
    // being constructed by a server the code to generate the XML document
    // needs these values to match the DAP version information.
    switch (d_dap_major) {
        case 4:
            d_namespace = c_dap40_namespace;
            break;
        default:
            d_namespace = "";
            break;
    }
}

/** Get the size of a response, in kilobytes. This method looks at the
 * variables in the DMR a computes the number of bytes in the response.
 *
 * @note This version of the method does a poor job with Arrays that
 * have varying dimensions.
 *
 * @param constrained Should the size of the whole DMR be used or should the
 * current constraint be taken into account?
 * @return The size of the request in kilobytes
 */
[[deprecated("Use DMR::request_size_kb()")]]
long DMR::request_size(bool constrained)
{
    return d_root->request_size(constrained);
}

/** Get the size of a response, in kilobytes. This method looks at the
 * variables in the DMR a computes the number of bytes in the response.
 *
 * @note This version of the method does a poor job with Arrays that
 * have varying dimensions.
 *
 * @param constrained Should the size of the whole DMR be used or should the
 * current constraint be taken into account?
 * @return The size of the request in kilobytes
 */
uint64_t DMR::request_size_kb(bool constrained)
{
    return d_root->request_size_kb(constrained);
}




/**
 * Print the DAP4 DMR object.
 *
 * @param xml use this XMLWriter to build the XML.
 * @param constrained Should the DMR be subject to a constraint? Defaults to
 * False
 */
void
DMR::print_dap4(XMLWriter &xml, bool constrained)
{
    if (xmlTextWriterStartElement(xml.get_writer(), (const xmlChar*) "Dataset") < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write Dataset element");

#if 0
    // Reintroduce these if they are really useful. jhrg 4/15/13
    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "xmlns:xml",
            (const xmlChar*) c_xml_namespace.c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for xmlns:xml");

    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "xmlns:xsi", (const xmlChar*) c_xml_xsi.c_str())
            < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for xmlns:xsi");

    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "xsi:schemaLocation",
            (const xmlChar*) c_dap_40_n_sl.c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for xmlns:schemaLocation");
#endif

    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "xmlns", (const xmlChar*) get_namespace().c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for xmlns");

    if (!request_xml_base().empty()) {
        if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "xml:base",
                (const xmlChar*)request_xml_base().c_str()) < 0)
            throw InternalErr(__FILE__, __LINE__, "Could not write attribute for xml:base");
    }

    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "dapVersion",  (const xmlChar*)dap_version().c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for dapVersion");

    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "dmrVersion", (const xmlChar*)dmr_version().c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for dapVersion");

    if (xmlTextWriterWriteAttribute(xml.get_writer(), (const xmlChar*) "name", (const xmlChar*)name().c_str()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not write attribute for name");

    root()->print_dap4(xml, constrained);

    if (xmlTextWriterEndElement(xml.get_writer()) < 0)
        throw InternalErr(__FILE__, __LINE__, "Could not end the top-level Group element");
}


/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and then calls parent dump
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void
DMR::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "DMR::dump - ("
    << (void *)this << ")" << endl ;
    DapIndent::Indent() ;
    strm << DapIndent::LMarg << "factory: " << (void *)d_factory << endl ;
    strm << DapIndent::LMarg << "name: " << d_name << endl ;
    strm << DapIndent::LMarg << "filename: " << d_filename << endl ;
    strm << DapIndent::LMarg << "protocol major: " << d_dap_major << endl;
    strm << DapIndent::LMarg << "protocol minor: " << d_dap_minor << endl;

    DapIndent::UnIndent() ;
}

} // namespace libdap
