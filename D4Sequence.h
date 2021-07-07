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

#ifndef _d4sequence_h
#define _d4sequence_h 1

#include "Constructor.h"

// DAP2 Sequence supported subsetting using the array notation. This might
// be introduced into DAP4 later on.
#define INDEX_SUBSETTING 0

class Crc32;

namespace libdap
{
class BaseType;
class D4FilterClauseList;

/** The type BaseTypeRow is used to store single rows of values in an
    instance of D4Sequence. Values are stored in instances of BaseType. */
typedef vector<BaseType *> D4SeqRow;

/** This type holds all of the values of a D4Sequence. */
typedef vector<D4SeqRow *> D4SeqValues;

/** The type BaseTypeRow is used to store single rows of values in an
 instance of Sequence. Values are stored in instances of BaseType. */
typedef vector<BaseType *> BaseTypeRow;

/** This type holds all of the values of a Sequence. */
typedef vector<BaseTypeRow *> SequenceValues;


/** This is the interface for the class D4Sequence. A sequence contains
    a single set of variables, all at the same lexical level just like
    a Structure.  Like a Structure, a D4Sequence may contain other
    compound types, including other D4Sequences.  Unlike a Structure, a
    D4Sequence defines a pattern that is repeated N times for a sequence
    of N elements. It is useful to think of a D4Sequence as representing
    a table of values (like a relational database), with each row of
    the table corresponding to a D4Sequence ``instance.''  (This usage
    can be confusing, since ``instance'' also refers to a particular
    item of class D4Sequence.)  For example:

    <pre>
    D4Sequence {
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

    A D4Sequence can be arbitrarily long, which is to say that its
    length is not part of its declaration.  A D4Sequence can contain
    other D4Sequences:

    <pre>
    D4Sequence {
      String name;
      Int32 age;
      D4Sequence {
        String friend;
      } friend_list;
    } person;
    </pre>

    <pre>
    Fred       34     Norman
                      Andrea
                      Ralph
                      Lisa
    Ralph      23     Norman
                      Andrea
                      Lisa
                      Marth
                      Throckmorton
                      Helga
                      Millicent
    Andrea     29     Ralph
                      Natasha
                      Norman
    ...        ..     ...
    </pre>

    Internally, the D4Sequence is represented by a vector of vectors. The
    members of the outer vector are the members of the D4Sequence. This
    includes the nested D4Sequences, as in the above example.

    Because the length of a D4Sequence is indeterminate, there are
    changes to the behavior of the functions to read this class of
    data.  The <tt>read()</tt> function for D4Sequence must be written so that
    successive calls return values for successive rows of the D4Sequence.

    Similar to a C structure, you refer to members of D4Sequence
    elements with a ``.'' notation.  For example, if the D4Sequence has
    a member D4Sequence called ``Tom'' and Tom has a member Float32
    called ``shoe_size'', you can refer to Tom's shoe size as
    ``Tom.shoe_size''.

    @brief Holds a sequence. */

class D4Sequence: public Constructor
{
private:
    // This may be zero (nullptr) but the accessor (clauses()) allocates an
    // instance if that is the case.
    D4FilterClauseList *d_clauses;

    // Use this to control if ptr_duplicate(), ..., copy the filter clauses.
    // Because the values of a child sequence are held in copies of the Seq
    // object they clauses will bound to the 'master' instance will be copied
    // but the copies will never be used. This field can be used to control
    // that. ...purely an optimization.
    bool d_copy_clauses;



protected:
    // This holds the values of the sequence. Values are stored in
    // instances of BaseTypeRow objects which hold instances of BaseType.
    //
    // Allow these values to be accessed by subclasses
    D4SeqValues d_values;

    uint64_t d_length;	// How many elements are in the sequence; -1 if not currently known
    bool d_set_length;  // FALSE if length is unknown.

#if INDEX_SUBSETTING
    int d_starting_row_number;
    int d_row_stride;
    int d_ending_row_number;
#endif

    void m_duplicate(const D4Sequence &s);

    // Specialize this if you have a data source that requires read()
    // recursively call itself for child sequences.
    void read_sequence_values(bool filter);

    friend class D4SequenceTest;

public:

    D4Sequence(const string &n);
    D4Sequence(const string &n, const string &d);

    D4Sequence(const D4Sequence &rhs);

    virtual ~D4Sequence();

    D4Sequence &operator=(const D4Sequence &rhs);

    virtual BaseType *ptr_duplicate();

    virtual void clear_local_data();

    /**
     * @brief The number of elements in a Sequence object.
     * @note This is not the number of items in a row, but the number
     * of rows in the complete sequence object.
     *
     * @return 0 if the number of elements is unknown, else
     * return the number of elements.
     */
    virtual uint64_t length() const { return d_length; }

    /**
     * Set the length of the sequence.
     * @param count
     */
    virtual void set_length(uint64_t count) { d_length = count; }

    virtual bool read_next_instance(bool filter);

    virtual void intern_data(ConstraintEvaluator &, DDS &) {
    	throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4");
    }
    virtual bool serialize(ConstraintEvaluator &, DDS &, Marshaller &, bool ) {
    	throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4");
    }
    virtual bool deserialize(UnMarshaller &, DDS *, bool ) {
    	throw InternalErr(__FILE__, __LINE__, "Not implemented for DAP4");
    }

    // DAP4
    virtual void intern_data(/*Crc32 &checksum, DMR &dmr, ConstraintEvaluator &eval*/);
    virtual void serialize(D4StreamMarshaller &m, DMR &dmr, /*ConstraintEvaluator &eval,*/ bool filter = false);
    virtual void deserialize(D4StreamUnMarshaller &um, DMR &dmr);

    D4FilterClauseList &clauses();

#if INDEX_SUBSETTING
    /** Return the starting row number if the sequence was constrained using
        row numbers (instead of, or in addition to, a relational constraint).
        If a relational constraint was also given, the row number corresponds
        to the row number of the sequence <i>after</i> applying the relational
        constraint.

        If the bracket notation was not used to constrain this sequence, this
        method returns -1.

        @brief Get the starting row number.
        @return The starting row number. */
    virtual int get_starting_row_number() const { return d_starting_row_number; }

    /** Return the row stride number if the sequence was constrained using
        row numbers (instead of, or in addition to, a relational constraint).
        If a relational constraint was also given, the row stride is applied
        to the sequence <i>after</i> applying the relational constraint.

        If the bracket notation was not used to constrain this sequence, this
        method returns -1.

        @brief Get the row stride.
        @return The row stride. */
    virtual int get_row_stride() const { return d_row_stride; }

    /** Return the ending row number if the sequence was constrained using
        row numbers (instead of, or in addition to, a relational constraint).
        If a relational constraint was also given, the row number corresponds
        to the row number of the sequence <i>after</i> applying the
        relational constraint.

        If the bracket notation was not used to constrain this sequence, this
        method returns -1.

        @brief Get the ending row number.
        @return The ending row number. */
    virtual int get_ending_row_number() const { return d_ending_row_number; }

    virtual void set_row_number_constraint(int start, int stop, int stride = 1);
#endif

    /**
     * @brief Set the internal value.
     * The 'values' of a D4Sequence is a vector of vectors of BaseType* objects.
     * Using this method does not perform a deep copy; the BaseType*s are
     * copied so the caller should not free them. Note that this does set
     * d_length but the read_p flag for the BaseTypes should all be set to
     * keep the serializer from trying to read each of them.
     * @param values
     */
    virtual void set_value(D4SeqValues &values) { d_values = values; d_length = d_values.size(); }

    /**
     * @brief Get the values for this D4Sequence
     * This method returns a reference to the values held by the instance.
     * You should make sure that the instance really holds values before
     * calling it! Do not free the BaseType*s contained in the vector of
     * vectors.
     * @return A reference tp the vector of vector of BaseType*
     */
    virtual D4SeqValues value() const { return d_values; }

    /**
     * @brief Get the sequence values by reference
     * This method returns a reference to the D4Sequence's values,
     * eliminating the copy of all the pointers. For large sequences,
     * that could be a substantial number of values (even though
     * they are 'just' pointers).
     * @return A reference to the vector of vector of BaseType*
     */
    virtual D4SeqValues &value_ref() { return d_values; }

    virtual D4SeqRow *row_value(size_t row);
    virtual BaseType *var_value(size_t row, const string &name);
    virtual BaseType *var_value(size_t row, size_t i);

    virtual void print_one_row(ostream &out, int row, string space,
                               bool print_row_num = false);
    virtual void print_val_by_rows(ostream &out, string space = "",
                                   bool print_decl_p = true,
                                   bool print_row_numbers = true);
    virtual void print_val(ostream &out, string space = "",
                           bool print_decl_p = true);

    virtual void dump(ostream &strm) const ;
};

} // namespace libdap

#endif //_sequence_h
