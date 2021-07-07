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

// Interface for the class Sequence. A sequence contains a single set
// of variables, all at the same lexical level just like a structure
// (and like a structure, it may contain other ctor types...). Unlike
// a structure, a sequence defines a pattern that is repeated N times
// for a sequence of N elements. Thus, Sequence { String name; Int32
// age; } person; means a sequence of N persons where each contain a
// name and age. The sequence can be arbitrarily long (i.e., you don't
// know N by looking at the sequence declaration.
//
// jhrg 9/14/94

#ifndef _sequence_h
#define _sequence_h 1

#include <stack>

#include "Constructor.h"

#ifndef S_XDRUtils_h
#include "XDRUtils.h"
#endif

namespace libdap {

class BaseType;
class ConstraintEvaluator;
class D4Group;

/** The type BaseTypeRow is used to store single rows of values in an
 instance of Sequence. Values are stored in instances of BaseType. */
typedef vector<BaseType *> BaseTypeRow;

/** This type holds all of the values of a Sequence. */
typedef vector<BaseTypeRow *> SequenceValues;

/** This is the interface for the class Sequence. A sequence contains
 a single set of variables, all at the same lexical level just like
 a Structure.  Like a Structure, a Sequence may contain other
 compound types, including other Sequences.  Unlike a Structure, a
 Sequence defines a pattern that is repeated N times for a sequence
 of N elements. It is useful to think of a Sequence as representing
 a table of values (like a relational database), with each row of
 the table corresponding to a Sequence ``instance.''  (This usage
 can be confusing, since ``instance'' also refers to a particular
 item of class Sequence.)  For example:

 <pre>
 Sequence {
 String name;
 Int32 age;
 } person;
 </pre>

 This represents a Sequence of ``person'' records, each instance of
 which contains a name and an age:

 <pre>
 Fred       34
 Ralph      23
 Andrea     29
 ...
 </pre>

 A Sequence can be arbitrarily long, which is to say that its
 length is not part of its declaration.  A Sequence can contain
 other Sequences:

 <pre>
 Sequence {
 String name;
 Int32 age;
 Sequence {
 String friend;
 } friend_list;
 } person;
 </pre>

 This is still represented as a single table, but each row contains
 the elements of both the main Sequence and the nested one:

 <pre>
 Fred       34     Norman
 Fred       34     Andrea
 Fred       34     Ralph
 Fred       34     Lisa
 Ralph      23     Norman
 Ralph      23     Andrea
 Ralph      23     Lisa
 Ralph      23     Marth
 Ralph      23     Throckmorton
 Ralph      23     Helga
 Ralph      23     Millicent
 Andrea     29     Ralph
 Andrea     29     Natasha
 Andrea     29     Norman
 ...        ..     ...
 </pre>

 Internally, the Sequence is represented by a vector of vectors. The
 members of the outer vector are the members of the Sequence. This
 includes the nested Sequences, as in the above example.

 NB: Note that in the past this class had a different behavior. It held
 only one row at a time and the deserialize(...) method had to be called
 from within a loop. This is <i>no longer true</i>. Now the
 deserailize(...) method should be called once and will read the entire
 sequence's values from the server. All the values are now stored in an
 instance of Sequence, not just a single row's.

 Because the length of a Sequence is indeterminate, there are
 changes to the behavior of the functions to read this class of
 data.  The <tt>read()</tt> function for Sequence must be written so that
 successive calls return values for successive rows of the Sequence.

 Similar to a C structure, you refer to members of Sequence
 elements with a ``.'' notation.  For example, if the Sequence has
 a member Sequence called ``Tom'' and Tom has a member Float32
 called ``shoe_size'', you can refer to Tom's shoe size as
 ``Tom.shoe_size''.

 @note This class contains the 'logic' for both the server- and client-side
 behavior. The field \e d_values is used by the client-side methods to store
 the entire Sequence. On the server-side, the read() method uses an underlying
 data system to read one row of data values which are then serialized using the
 serialize() methods of each variable.

 @todo Add an isEmpty() method which returns true if the Sequence is
 empty. This should work before and after calling deserialize().

 @brief Holds a sequence. */

class Sequence: public Constructor
{
private:
    // This holds the values read off the wire. Values are stored in
    // instances of BaseTypeRow objects which hold instances of BaseType.
    SequenceValues d_values;

    // The number of the row that has just been deserialized. Before
    // deserialized has been called, this field is -1.
    int d_row_number;

    // If a client asks for certain rows of a sequence using the bracket
    // notation (<tt>[<start>:<stride>:<stop>]</tt>) primarily intended for
    // arrays
    // and grids, record that information in the next three fields. This
    // information can be used by the translation software. s.a. the accessor
    // and mutator methods for these members. Values of -1 indicate that
    // these have not yet been set.
    int d_starting_row_number;
    int d_row_stride;
    int d_ending_row_number;

    // Used to track if data has not already been sent.
    bool d_unsent_data;

    // Track if the Start Of Instance marker has been written. Needed to
    // properly send EOS for only the outer Sequence when a selection
    // returns an empty Sequence.
    bool d_wrote_soi;

    // This signals whether the sequence is a leaf or parent.
    bool d_leaf_sequence;

    // In a hierarchy of sequences, is this the top most?
    bool d_top_most;

    bool is_end_of_rows(int i);

    friend class SequenceTest;

protected:
    void m_duplicate(const Sequence &s);
    typedef stack<SequenceValues*> sequence_values_stack_t;

    virtual bool serialize_parent_part_one(DDS &dds, ConstraintEvaluator &eval, Marshaller &m);
    virtual void serialize_parent_part_two(DDS &dds, ConstraintEvaluator &eval, Marshaller &m);
    virtual bool serialize_leaf(DDS &dds, ConstraintEvaluator &eval, Marshaller &m, bool ce_eval);

    virtual void intern_data_private(ConstraintEvaluator &eval, DDS &dds,
            sequence_values_stack_t &sequence_values_stack);
    virtual void intern_data_for_leaf(DDS &dds, ConstraintEvaluator &eval,
            sequence_values_stack_t &sequence_values_stack);

    virtual void intern_data_parent_part_one(DDS &dds, ConstraintEvaluator &eval,
            sequence_values_stack_t &sequence_values_stack);

    virtual void intern_data_parent_part_two(DDS &dds, ConstraintEvaluator &eval,
            sequence_values_stack_t &sequence_values_stack);

public:

    Sequence(const string &n);
    Sequence(const string &n, const string &d);

    Sequence(const Sequence &rhs);

    virtual ~Sequence();

    Sequence &operator=(const Sequence &rhs);

    virtual BaseType *ptr_duplicate();

    virtual void clear_local_data();

    virtual void transform_to_dap4(D4Group *root, Constructor *container);

    virtual bool is_dap2_only_type();

    virtual string toString();

    virtual bool is_linear();

    virtual uint64_t length() const;

    virtual int number_of_rows() const;

    virtual bool read_row(int row, DDS &dds, ConstraintEvaluator &eval, bool ce_eval = true);

    virtual void intern_data(ConstraintEvaluator &eval, DDS &dds);
    virtual bool serialize(ConstraintEvaluator &eval, DDS &dds, Marshaller &m, bool ce_eval = true);
    virtual bool deserialize(UnMarshaller &um, DDS *dds, bool reuse = false);

    /// Rest the row number counter
    void reset_row_number();
    // I added a second method instead of a param with a default value because I think
    // this will result only in an addition to the ABI/API, not a change. 5/16/15 jhrg
    void reset_row_number(bool recur);
    void increment_row_number(unsigned int i) { d_row_number += i; }
    int get_row_number() const { return d_row_number; }

    int get_starting_row_number();

    virtual int get_row_stride();

    virtual int get_ending_row_number();

    virtual void set_row_number_constraint(int start, int stop, int stride = 1);

    /// Get the unsent data property
    bool get_unsent_data() const
    {
        return d_unsent_data;
    }

    /// Set the unsent data property
    void set_unsent_data(bool usd)
    {
        d_unsent_data = usd;
    }

    virtual void set_value(SequenceValues &values);
    virtual SequenceValues value();
    virtual SequenceValues &value_ref();

    virtual BaseType *var_value(size_t row, const string &name);

    virtual BaseType *var_value(size_t row, size_t i);

    virtual BaseTypeRow *row_value(size_t row);
    virtual void print_one_row(ostream &out, int row, string space, bool print_row_num = false);
    virtual void print_val_by_rows(ostream &out, string space = "", bool print_decl_p = true, bool print_row_numbers =
            true);
    virtual void print_val(ostream &out, string space = "", bool print_decl_p = true);

    virtual void print_one_row(FILE *out, int row, string space, bool print_row_num = false);
    virtual void print_val_by_rows(FILE *out, string space = "", bool print_decl_p = true,
            bool print_row_numbers = true);
    virtual void print_val(FILE *out, string space = "", bool print_decl_p = true);

    virtual void set_leaf_p(bool state);

    virtual bool is_leaf_sequence();

    virtual void set_leaf_sequence(int lvl = 1);

    virtual void dump(ostream &strm) const;
};

} // namespace libdap

#endif //_sequence_h
