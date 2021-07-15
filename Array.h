
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

// Class for array variables. The dimensions of the array are stored in the
// list SHAPE.
//
// jhrg 9/6/94

#ifndef _array_h
#define _array_h 1

#include <string>
#include <vector>

#ifndef _dods_limits_h
#include "dods-limits.h"
#endif

#ifndef _vector_h
#include "Vector.h"
#endif

//#include "D4Dimensions.h"

namespace libdap
{
class D4Group;
class D4Maps;
class XMLWriter;
class D4Dimension;
class D4Dimensions;

const int DODS_MAX_ARRAY = DODS_INT_MAX;

/** This class is used to hold arrays of data. The elements of the array can
    be simple or compound data types. There is no limit on the number of
    dimensions an array can have, or on the size of each dimension.

    If desired, the user can give each dimension of an array a name. You can,
    for example, have a 360x180 array of temperatures, covering the whole
    globe with one-degree squares. In this case, you could name the first
    dimension \e Longitude and the second dimension \e Latitude. This can
    help prevent a great deal of confusion.

    The Array is used as part of the Grid class, where the dimension names
    are crucial to its structure. The dimension names correspond to \e Map
    vectors, holding the actual values for that column of the array.

    In DAP4, the Array may be a Coverage or a simple Array. In the former case
    the Array will have both named dimensions and maps, where the maps (instances
    of D4Map) are what make the Array a Coverage. Coverages are a generalization
    of DAP2 Grids.

    Each array dimension carries with it its own projection information. The
    projection information takes the form of three integers: the start, stop,
    and stride values. This is clearest with an example. Consider a
    one-dimensional array 10 elements long. If the start value of the
    dimension constraint is 3, then the constrained array appears to be seven
    elements long. If the stop value is changed to 7, then the array appears
    to be five elements long. If the stride is changed to two, the array will
    appear to be 3 elements long. Array constraints are written as:
    <b>[start:stride:stop]</b>.

    \verbatim
    A = [1 2 3 4 5 6 7 8 9 10]

    A[3::] = [4 5 6 7 8 9 10]

    A[3::7] = [4 5 6 7 8]

    A[3:2:7] = [4 6 8]

    A[0:3:9] = [1 4 7 10]
    \endverbatim

    @note Arrays use zero-based indexing.
    @note This class is used for both DAP2 and DAP4.

    @brief A multidimensional array of identical data types.
    @see Grid
    @see Vector
    @see dimension */

class Array: public Vector
{
public:
    /** Information about a dimension. Each Array has one or more dimensions.
        For each of an Array's dimensions, a corresponding instance of this
        struct holds the natural size, name, constraint information and
        constrained size.

        @note Instead of using this struct's fields directly, use Array's
        dimension accessor methods.

        @note This struct is public because its type is used in public
        typedefs. */
    struct dimension
    {
    	// In DAP2, the name and size of a dimension is stored here, along
    	// with information about any constraint. In DAP4, either the name
    	// and size are stored in the two fields below _or_ the name and
    	// size information comes from a dimension object defined in a
    	// group that is referenced by the 'dim' pointer. Do not free this
    	// pointer; it is shared between the array and the Group where the
    	// Dimension is defined. To keep Array manageable to implement, size
    	// will be set here using the value from 'dim' if it is not null.
        int size;  ///< The unconstrained dimension size.
        string name;    ///< The name of this dimension.

        D4Dimension *dim; ///< If not null, a weak pointer to the D4Dimension

        // when a DMR is printed for a data response, if an array uses shared
        // dimensions and those sdims have been sliced, make sure to use those
        // and get the syntax correct. That's what this field does - in every
        // case the array records the sizes of its dimensions and their slices
        // regardless of whether they were provided explicitly in a CE or inherited
        // from a sliced sdim.
        bool use_sdim_for_slice; ///< Used to control printing the DMR in data responses

        int start;  ///< The constraint start index
        int stop;  ///< The constraint end index
        int stride;  ///< The constraint stride
        int c_size;  ///< Size of dimension once constrained

        dimension() : size(0), name(""), dim(0), use_sdim_for_slice(false) {
            // this information changes with each constraint expression
            start = 0;
            stop = 0;
            stride = 1;
            c_size = size;
        }

        dimension(unsigned long s, string n) : size(s), name(n), dim(0), use_sdim_for_slice(false) {
            start = 0;
            stop = size - 1;
            stride = 1;
            c_size = size;
        }

        dimension(D4Dimension *d);
    };

    D4Maps *d_maps;

private:
    std::vector<dimension> _shape; // list of dimensions (i.e., the shape)

    void update_dimension_pointers(D4Dimensions *old_dims, D4Dimensions *new_dims);

    friend class ArrayTest;
    friend class D4Group;

    bool is_dap2_grid();

protected:
    void _duplicate(const Array &a);

    unsigned int print_array(FILE *out, unsigned int index,
                             unsigned int dims, unsigned int shape[]);

    unsigned int print_array(ostream &out, unsigned int index,
                             unsigned int dims, unsigned int shape[]);

public:
    /** A constant iterator used to access the various dimensions of an
        Array.

        @see dim_begin()
        @see dim_end() */
    typedef std::vector<dimension>::const_iterator Dim_citer;

    /** An iterator used to access the various dimensions of an
        Array. Most of the methods that access various properties of a
        dimension use an instance of Dim_iter.

        @see dim_begin()
        @see dim_end() */
    typedef std::vector<dimension>::iterator Dim_iter;

    Array(const string &n, BaseType *v, bool is_dap4 = false);
    Array(const string &n, const string &d, BaseType *v, bool is_dap4 = false);
    Array(const Array &rhs);
    virtual ~Array();

    Array &operator=(const Array &rhs);
    virtual BaseType *ptr_duplicate();

    virtual void transform_to_dap4(D4Group *root, Constructor *container);
    virtual std::vector<BaseType *> *transform_to_dap2(AttrTable *parent_attr_table, bool show_shared_dims = false);

    void add_var(BaseType *v, Part p = nil);
    void add_var_nocopy(BaseType *v, Part p = nil);

    void append_dim(int size, const string &name = "");
    void append_dim(D4Dimension *dim);
    void prepend_dim(int size, const string& name = "");
    void prepend_dim(D4Dimension *dim);
    void clear_all_dims();
    void rename_dim(const string &oldName = "", const string &newName = "");

    virtual void add_constraint(Dim_iter i, int start, int stride, int stop);
    virtual void add_constraint(Dim_iter i, D4Dimension *dim);
    virtual void reset_constraint();

    virtual void clear_constraint(); // deprecated

    virtual void update_length(int size = 0); // should be used internally only

    Dim_iter dim_begin() ;
    Dim_iter dim_end() ;

    virtual int dimension_size(Dim_iter i, bool constrained = false);
    virtual int dimension_start(Dim_iter i, bool constrained = false);
    virtual int dimension_stop(Dim_iter i, bool constrained = false);
    virtual int dimension_stride(Dim_iter i, bool constrained = false);
    virtual string dimension_name(Dim_iter i);
    virtual D4Dimension *dimension_D4dim(Dim_iter i);

    virtual unsigned int dimensions(bool constrained = false);

    virtual D4Maps *maps();

    virtual void print_dap4(XMLWriter &xml, bool constrained = false);

    // These are all DAP2 output methods

    virtual void print_decl(ostream &out, string space = "    ",
                            bool print_semi = true,
                            bool constraint_info = false,
                            bool constrained = false);

    virtual void print_xml(ostream &out, string space = "    ",
                           bool constrained = false);

    virtual void print_xml_writer(XMLWriter &xml, bool constrained = false);
    virtual void print_xml_writer_core(XMLWriter &out, bool constrained, string tag);
    virtual void print_as_map_xml_writer(XMLWriter &xml, bool constrained);

    virtual void print_xml_core(FILE *out, string space, bool constrained, string tag);
    virtual void print_xml_core(ostream &out, string space, bool constrained, string tag);

    // not used (?)
    virtual void print_as_map_xml(ostream &out, string space = "    ",
                                  bool constrained = false);

    virtual void print_val(ostream &out, string space = "",
                           bool print_decl_p = true);

    virtual void print_xml(FILE *out, string space = "    ",
                           bool constrained = false);
    virtual void print_as_map_xml(FILE *out, string space = "    ",
                                  bool constrained = false);
    virtual void print_val(FILE *out, string space = "",
                           bool print_decl_p = true);
    virtual void print_decl(FILE *out, string space = "    ",
                            bool print_semi = true,
                            bool constraint_info = false,
                            bool constrained = false);

    virtual bool check_semantics(string &msg, bool all = false);


    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif // _array_h
