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

#include "config.h"

//#define DODS_DEBUG
//#define DODS_DEBUG2

#include <algorithm>
#include <string>
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

#include "Marshaller.h"
#include "UnMarshaller.h"

#include "debug.h"
#include "Error.h"
#include "InternalErr.h"
#include "Sequence.h"
#include "DDS.h"
#include "DataDDS.h"
#include "util.h"
#include "InternalErr.h"
#include "escaping.h"

#include "D4Attributes.h"
#include "D4Sequence.h"
#include "D4Group.h"
#include "Constructor.h"
#include "DMR.h"
#include "DapIndent.h"

#undef CLEAR_LOCAL_DATA

using namespace std;

namespace libdap {

static const unsigned char end_of_sequence = 0xA5; // binary pattern 1010 0101
static const unsigned char start_of_instance = 0x5A; // binary pattern 0101 1010

// Private member functions

void Sequence::m_duplicate(const Sequence &s)
{
    DBG(cerr << "In Sequence::m_duplicate" << endl);

    d_row_number = s.d_row_number;
    d_starting_row_number = s.d_starting_row_number;
    d_ending_row_number = s.d_ending_row_number;
    d_row_stride = s.d_row_stride;
    d_leaf_sequence = s.d_leaf_sequence;
    d_unsent_data = s.d_unsent_data;
    d_wrote_soi = s.d_wrote_soi;
    d_top_most = s.d_top_most;

    Sequence &cs = const_cast<Sequence &>(s);

    // Copy the BaseType objects used to hold values.
    for (vector<BaseTypeRow *>::iterator rows_iter = cs.d_values.begin(); rows_iter != cs.d_values.end(); rows_iter++) {
        // Get the current BaseType Row
        BaseTypeRow *src_bt_row_ptr = *rows_iter;
        // Create a new row.
        BaseTypeRow *dest_bt_row_ptr = new BaseTypeRow;
        // Copy the BaseType objects from a row to new BaseType objects.
        // Push new BaseType objects onto new row.
        for (BaseTypeRow::iterator bt_row_iter = src_bt_row_ptr->begin(); bt_row_iter != src_bt_row_ptr->end();
                bt_row_iter++) {
            BaseType *src_bt_ptr = *bt_row_iter;
            BaseType *dest_bt_ptr = src_bt_ptr->ptr_duplicate();
            dest_bt_row_ptr->push_back(dest_bt_ptr);
        }
        // Push new row onto d_values.
        d_values.push_back(dest_bt_row_ptr);
    }
}

static void write_end_of_sequence(Marshaller &m)
{
    m.put_opaque((char *) &end_of_sequence, 1);
}

static void write_start_of_instance(Marshaller &m)
{
    m.put_opaque((char *) &start_of_instance, 1);
}

static unsigned char read_marker(UnMarshaller &um)
{
    unsigned char marker;
    um.get_opaque((char *) &marker, 1);

    return marker;
}

static bool is_start_of_instance(unsigned char marker)
{
    return (marker == start_of_instance);
}

static bool is_end_of_sequence(unsigned char marker)
{
    return (marker == end_of_sequence);
}

// Public member functions

/** The Sequence constructor requires only the name of the variable
 to be created.  The name may be omitted, which will create a
 nameless variable.  This may be adequate for some applications.

 @param n A string containing the name of the variable to be
 created.

 @brief The Sequence constructor. */
Sequence::Sequence(const string &n) :
        Constructor(n, dods_sequence_c), d_row_number(-1), d_starting_row_number(-1), d_row_stride(1), d_ending_row_number(
                -1), d_unsent_data(false), d_wrote_soi(false), d_leaf_sequence(false), d_top_most(false)
{
}

/** The Sequence server-side constructor requires the name of the variable
 to be created and the dataset name from which this variable is being
 created.

 @param n A string containing the name of the variable to be
 created.
 @param d A string containing the name of the dataset from which this
 variable is being created.

 @brief The Sequence server-side constructor. */
Sequence::Sequence(const string &n, const string &d) :
		Constructor(n, d, dods_sequence_c), d_row_number(-1), d_starting_row_number(-1),
		d_row_stride(1), d_ending_row_number(-1), d_unsent_data(false),
		d_wrote_soi(false), d_leaf_sequence(false), d_top_most(false)
{
}

/** @brief The Sequence copy constructor. */
Sequence::Sequence(const Sequence &rhs) :
        Constructor(rhs)
{
    m_duplicate(rhs);
}

BaseType *
Sequence::ptr_duplicate()
{
    return new Sequence(*this);
}

/**
 * Build a D4Sequence from a DAP2 Sequence.
 *
 * Because DAP4 uses a different type for sequences, this code must
 * be subclassed by anything other than trivial test code or client
 * side-only uses of the library.
 *
 * @note This version of transformto_dap4() builds a new type of object,
 * so it must be subclassed.
 *
 * @param root Use this as the environment for D4Dimensions
 * @param container Load the result into this container
 * @return The new D4Sequence
 */
void
Sequence::transform_to_dap4(D4Group *root, Constructor *container)
{
    D4Sequence *dest;
    // If it's already a DAP4 object then we can just return it!
    if(is_dap4()){
        dest = static_cast<D4Sequence*>(ptr_duplicate());
        dest->set_length(-1);
        container->add_var_nocopy(dest);
        return;
    }
    dest = new D4Sequence(name());
    Constructor::transform_to_dap4(root, dest);
    dest->set_length(-1);
    container->add_var_nocopy(dest);
}

static inline void delete_bt(BaseType *bt_ptr)
{
    delete bt_ptr;
    bt_ptr = 0;
}

static inline void delete_rows(BaseTypeRow *bt_row_ptr)
{
    for_each(bt_row_ptr->begin(), bt_row_ptr->end(), delete_bt);

    delete bt_row_ptr;
    bt_row_ptr = 0;
}

Sequence::~Sequence()
{
    clear_local_data();
}

void Sequence::clear_local_data()
{
    if (!d_values.empty()) {
        for_each(d_values.begin(), d_values.end(), delete_rows);
        d_values.resize(0);
    }

    set_read_p(false);
}

Sequence &
Sequence::operator=(const Sequence &rhs)
{
    if (this == &rhs) return *this;

    dynamic_cast<Constructor &>(*this) = rhs; // run Constructor=

    m_duplicate(rhs);

    return *this;
}

/**
 * The Sequence class will be streamlined for DAP4.
 */
bool Sequence::is_dap2_only_type()
{
    return true;
}

string Sequence::toString()
{
    ostringstream oss;

    oss << BaseType::toString();

    for (Vars_iter i = d_vars.begin(); i != d_vars.end(); i++) {
        oss << (*i)->toString();
    }

    oss << endl;

    return oss.str();
}

bool Sequence::is_linear()
{
    bool linear = true;
    bool seq_found = false;
    for (Vars_iter iter = d_vars.begin(); linear && iter != d_vars.end(); iter++) {
        if ((*iter)->type() == dods_sequence_c) {
            // A linear sequence cannot have more than one child seq. at any
            // one level. If we've already found a seq at this level, return
            // false.
            if (seq_found) {
                linear = false;
                break;
            }
            seq_found = true;
            linear = static_cast<Sequence *>((*iter))->is_linear();
        }
        else if ((*iter)->type() == dods_structure_c) {
            linear = static_cast<Structure*>((*iter))->is_linear();
        }
        else {
            // A linear sequence cannot have Arrays, Lists or Grids.
            linear = (*iter)->is_simple_type();
        }
    }

    return linear;
}

/** @brief Get a whole row from the sequence.
 @param row Get row number <i>row</i> from the sequence.
 @return A BaseTypeRow object (vector<BaseType *>). Null if there's no such
 row number as \e row. */
BaseTypeRow *
Sequence::row_value(size_t row)
{
    if (row >= d_values.size()) return 0;   //nullptr
    return d_values[row];
}

/** Set value of this Sequence. This does not perform a deep copy, so data
 should be allocated on the heap and freed only when the Sequence dtor is
 called.
 @see SequenceValues
 @see BaseTypeRow
 @param values Set the value of this Sequence. */
void Sequence::set_value(SequenceValues &values)
{
    d_values = values;
}

/** Get the value for this sequence.
 @return The SequenceValues object for this Sequence. */
SequenceValues Sequence::value()
{
    return d_values;
}

/** Get the value for this sequence.
 @return The SequenceValues object for this Sequence. */
SequenceValues &
Sequence::value_ref()
{
    return d_values;
}

/** @brief Get the BaseType pointer to the named variable of a given row.
 @param row Read from <i>row</i> in the sequence.
 @param name Return <i>name</i> from <i>row</i>.
 @return A BaseType which holds the variable and its value.
 @see number_of_rows */
BaseType *
Sequence::var_value(size_t row, const string &name)
{
    BaseTypeRow *bt_row_ptr = row_value(row);
    if (!bt_row_ptr) return 0;

    BaseTypeRow::iterator bt_row_iter = bt_row_ptr->begin();
    BaseTypeRow::iterator bt_row_end = bt_row_ptr->end();
    while (bt_row_iter != bt_row_end && (*bt_row_iter)->name() != name)
        ++bt_row_iter;

    if (bt_row_iter == bt_row_end)
        return 0;
    else
        return *bt_row_iter;
}

/** @brief Get the BaseType pointer to the $i^{th}$ variable of <i>row</i>.
 @param row Read from <i>row</i> in the sequence.
 @param i Return the $i^{th}$ variable from <i>row</i>.
 @return A BaseType which holds the variable and its value.
 @see number_of_rows */
BaseType *
Sequence::var_value(size_t row, size_t i)
{
    BaseTypeRow *bt_row_ptr = row_value(row);
    if (!bt_row_ptr) return 0;

    if (i >= bt_row_ptr->size()) return 0;

    return (*bt_row_ptr)[i];
}

// This version returns -1. Each API-specific subclass should define a more
// reasonable version. jhrg 5/24/96

/** Returns the number of elements in a Sequence object. Note that
 this is <i>not</i> the number of items in a row, but the number
 of rows in the complete sequence object. To be meaningful, this
 must be computed after constraint expression (CE) evaluation.
 The purpose of this function is to facilitate translations
 between Sequence objects and Array objects, particularly when
 the Sequence is too large to be transferred from the server to
 the client in its entirety.

 This function, to be useful, must be specialized for the API and
 data format in use.

 @return The base implementation returns -1, indicating that the
 length is not known.  Sub-classes specific to a particular API
 will have a more complete implementation. */
uint64_t Sequence::length() const
{
    return 0;
    //return -1;
}

// Hmmm. how is this different from length()?
int Sequence::number_of_rows() const
{
    return d_values.size();
}

/** When reading a nested sequence, use this method to reset the internal
 row number counter. This is necessary so that the second, ... instances
 of the inner/nested sequence will start off reading row zero. */
void Sequence::reset_row_number()
{
    d_row_number = -1;
}

/**
 * @brief A recursive version of reset_row_number()
 *
 * @param recur If true, reset the row number of child sequences as well
 */
void Sequence::reset_row_number(bool recur)
{
    reset_row_number();

    if (recur)
        for (Vars_iter i = var_begin(), e = var_end(); i != e; ++i)
            if ((*i)->type() == dods_sequence_c)
                reset_row_number(true);
}

// Notes:
// Assume that read() is implemented so that, when reading data for a nested
// sequence, only the outer most level is *actually* read.
// This is a consequence of our current (12/7/99) implementation of
// the JGOFS server (which is the only server to actually use nested
// sequences). 12/7/99 jhrg
//
// Stop assuming this. This logic is being moved into the JGOFS server
// itself. 6/1/2001 jhrg

// The read() function returns a boolean value, with TRUE
// indicating that read() should be called again because there's
// more data to read, and FALSE indicating there's no more data
// to read. Note that this behavior is necessary to properly
// handle variables that contain Sequences. Jose Garcia If an
// error exists while reading, the implementers of the surrogate
// library SHOULD throw an Error object which will propagate
// beyond this point to to the original caller.
// Jose Garcia

/** Read row number <i>row</i> of the Sequence. The values of the row
 are obtained by calling the read() method of the sequence. The
 current \e row just read is stored in the Sequence instance
 along with its row number. If a selection expression has been
 supplied, rows are counted only if they satisfy that expression.

 Note that we can only advance in a Sequence. It is not possible to back up
 and read a row numbered lower than the current row. If you need that
 you will need to replace the serialize() method with one of your own.

 Used on the server side.

 @note The first row is row number zero. A Sequence with 100 rows will
 have row numbers 0 to 99.

 @todo This code ignores the main reason for nesting the sequences, that
 if the outer Sequence's current instance fails the CE, there's no need to
 look at the values of the inner Sequence. But in the code that calls this
 method (serialize() and intern_data()) the CE is not evaluated until the
 inner-most Sequence (i.e., the leaf Sequence) is read. That means that
 each instance of the inner Sequence is read and the CE evaluated for each
 of those reads. To fix this, and the overall problem of complexity here,
 we need to re-think Sequences and how they behave. 11/13/2007 jhrg

 @return A boolean value, with TRUE indicating that read_row
 should be called again because there's more data to be read.
 FALSE indicates the end of the Sequence.
 @param row The row number to read.
 @param dds A reference to the DDS for this dataset.
 @param eval Use this as the constraint expression evaluator.
 @param ce_eval If True, evaluate any CE, otherwise do not.
 */
bool Sequence::read_row(int row, DDS &dds, ConstraintEvaluator &eval, bool ce_eval)
{
    DBG2(cerr << "Entering Sequence::read_row for " << name() << ", row number " << row << ", current row " << d_row_number << endl);
    if (row < d_row_number) throw InternalErr("Trying to back up inside a sequence!");

    if (row == d_row_number) {
        DBG2(cerr << "Leaving Sequence::read_row for " << name() << endl);
        return false;
    }

    bool eof = false;  // Start out assuming EOF is false.
    while (!eof && d_row_number < row) {
        if (!read_p()) {
            // jhrg original version from 10/9/13 : eof = (read() == false);
            eof = read();
        }

        // Advance the row number if ce_eval is false (we're not supposed to
        // evaluate the selection) or both ce_eval and the selection are
        // true.
        if (!eof && (!ce_eval || eval.eval_selection(dds, dataset()))) d_row_number++;

        set_read_p(false); // ...so that the next instance will be read
    }

    // Once we finish the above loop, set read_p to true so that the caller
    // knows that data *has* been read. This is how the read() methods of the
    // elements of the sequence know to not call read() but instead look for
    // data values inside themselves.
    set_read_p(true);

    // Return true if we have valid data, false if we've read to the EOF.
    DBG2(cerr << "Leaving Sequence::read_row for " << name() << " with eof: " << eof << endl);
    return !eof; // jhrg 10/10/13 was: eof == 0;
}

// Private. This is used to process constraints on the rows of a sequence.
// Starting with 3.2 we support constraints like Sequence[10:2:20]. This
// odd-looking logic first checks if d_ending_row_number is the sentinel
// value of -1. If so, the sequence was not constrained by row number and
// this method should never return true (which indicates that we're at the
// end of a row-number constraint). If d_ending_row_number is not -1, then is
// \e i at the end point? 6/1/2001 jhrg
inline bool Sequence::is_end_of_rows(int i)
{
    return ((d_ending_row_number == -1) ? false : (i > d_ending_row_number));
}

/** Serialize a Sequence.

 Leaf Sequences must be marked as such (see DDS::tag_nested_sequence()),
 as must the top most Sequence.

 How the code works. Methods called for various functions are named in
 brackets:
 <ol>
 <li>Sending a one-level sequence:
 <pre>
 Dataset {
     Sequence {
         Int x;
         Int y;
     } flat;
 } case_1;
 </pre>

 Serialize case_1 by reading successive rows and sending all of those that
 satisfy the CE. Before each row, send a start of instance (SOI) marker.
 Once all rows have been sent, send an End of Sequence (EOS)
 marker.[serialize_leaf].</li>

 <li>Sending a nested sequence:
 <pre>
 Dataset {
     Sequence {
         Int t;
         Sequence {
             Int z;
         } inner;
     } outer;
 } case_2;
 </pre>

 Serialize case_2 by reading the first row of outer and storing the values. Do
 not evaluate the CE [serialize_parent_part_one]. Call serialize() for inner
 and read each row for it, evaluating the CE for each row that is read.
 After the first row of inner is read and satisfies the CE, write out the
 SOI marker and values for outer [serialize_parent_part_two], then write
 the SOI and values for the first row of inner. Continue to read and send
 rows of inner until the last row has been read. Send EOS for inner
 [serialize_leaf]. Now read the next row of outer and repeat. Once outer
 is completely read, send its EOS marker.</li>
 </ol>

 Notes:
 <ol>
 <li>For a nested Sequence, the child sequence must follow all other types
 in the parent sequence (like the example). There may be only one nested
 Sequence per level.</li>

 <li>CE evaluation happens only in a leaf sequence.</li>

 <li>When no data satisfies a CE, the empty Sequence is signaled by a
 single EOS marker, regardless of the level of nesting of Sequences. That
 is, the EOS marker is sent for only the outer Sequence in the case of a
 completely empty response.</li>
 </ol>
 */
bool Sequence::serialize(ConstraintEvaluator &eval, DDS &dds, Marshaller &m, bool ce_eval)
{
    // Special case leaf sequences!
    bool status = false;

    if (is_leaf_sequence())
        status = serialize_leaf(dds, eval, m, ce_eval);
    else
        status = serialize_parent_part_one(dds, eval, m);

    return status;
}

// We know this is not a leaf Sequence. That means that this Sequence holds
// another Sequence as one of its fields _and_ that child Sequence triggers
// the actual transmission of values.

bool Sequence::serialize_parent_part_one(DDS &dds, ConstraintEvaluator &eval, Marshaller &m)
{
    DBG2(cerr << "Entering serialize_parent_part_one for " << name() << endl);

    int i = (d_starting_row_number != -1) ? d_starting_row_number : 0;

    // read_row returns true if valid data was read, false if the EOF was
    // found. 6/1/2001 jhrg
    // Since this is a parent sequence, read the row ignoring the CE (all of
    // the CE clauses will be evaluated by the leaf sequence).
    bool status = read_row(i, dds, eval, false);
    DBG2(cerr << "Sequence::serialize_parent_part_one::read_row() status: " << status << endl);

    while (status && !is_end_of_rows(i)) {
        i += d_row_stride;

        // DBG(cerr << "Writing Start of Instance marker" << endl);
        // write_start_of_instance(sink);

        // In this loop serialize will signal an error with an exception.
        for (Vars_iter iter = d_vars.begin(); iter != d_vars.end(); iter++) {
            // Only call serialize for child Sequences; the leaf sequence
            // will trigger the transmission of values for its parents (this
            // sequence and maybe others) once it gets some valid data to
            // send.
            // Note that if the leaf sequence has no variables in the current
            // projection, its serialize() method will never be called and that's
            // the method that triggers actually sending values. Thus the leaf
            // sequence must be the lowest level sequence with values whose send_p
            // property is true.
            if ((*iter)->send_p() && (*iter)->type() == dods_sequence_c) (*iter)->serialize(eval, dds, m);
        }

        set_read_p(false); // ...so this will read the next instance

        status = read_row(i, dds, eval, false);
        DBG(cerr << "Sequence::serialize_parent_part_one::read_row() status: " << status << endl);
    }
    // Reset current row number for next nested sequence element.
    d_row_number = -1;

    // Always write the EOS marker? 12/23/04 jhrg
    // Yes. According to DAP2, a completely empty response is signaled by
    // a return value of only the EOS marker for the outermost sequence.
    if (d_top_most || d_wrote_soi) {
        DBG(cerr << "Writing End of Sequence marker" << endl);
        write_end_of_sequence(m);
        d_wrote_soi = false;
    }

    return true;  // Signal errors with exceptions.
}

// If we are here then we know that this is 'parent sequence' and that the
// leaf sequence has found valid data to send. We also know that
// serialize_parent_part_one has been called so data are in the instance's
// fields. This is where we send data. Whereas ..._part_one() contains a
// loop to iterate over all of rows in a parent sequence, this does not. This
// method assumes that the serialize_leaf() will call it each time it needs
// to be called.
//
// NB: This code only works if the child sequences appear after all other
// variables.
void Sequence::serialize_parent_part_two(DDS &dds, ConstraintEvaluator &eval, Marshaller &m)
{
    DBG(cerr << "Entering serialize_parent_part_two for " << name() << endl);

    BaseType *btp = get_parent();
    if (btp && btp->type() == dods_sequence_c) static_cast<Sequence&>(*btp).serialize_parent_part_two(dds, eval, m);

    if (d_unsent_data) {
        DBG(cerr << "Writing Start of Instance marker" << endl);
        d_wrote_soi = true;
        write_start_of_instance(m);

        // In this loop serialize will signal an error with an exception.
        for (Vars_iter iter = d_vars.begin(); iter != d_vars.end(); iter++) {
            // Send all the non-sequence variables
            DBG(cerr << "Sequence::serialize_parent_part_two(), serializing "
                    << (*iter)->name() << endl);
            if ((*iter)->send_p() && (*iter)->type() != dods_sequence_c) {
                DBG(cerr << "Send P is true, sending " << (*iter)->name() << endl);
                (*iter)->serialize(eval, dds, m, false);
            }
        }

        d_unsent_data = false;              // read should set this.
    }
}

// This code is only run by a leaf sequence. Note that a one level sequence
// is also a leaf sequence.
bool Sequence::serialize_leaf(DDS &dds, ConstraintEvaluator &eval, Marshaller &m, bool ce_eval)
{
    DBG(cerr << "Entering Sequence::serialize_leaf for " << name() << endl);
    int i = (d_starting_row_number != -1) ? d_starting_row_number : 0;

    // read_row returns true if valid data was read, false if the EOF was
    // found. 6/1/2001 jhrg
    bool status = read_row(i, dds, eval, ce_eval);
    DBG(cerr << "Sequence::serialize_leaf::read_row() status: " << status << endl);

    // Once the first valid (satisfies the CE) row of the leaf sequence has
    // been read, we know we're going to send data. Send the current instance
    // of the parent/ancestor sequences now, if there are any. We only need
    // to do this once, hence it's not inside the while loop, but we only
    // send the parent seq data _if_ there's data in the leaf to send, that's
    // why we wait until after the first call to read_row() here in the leaf
    // sequence.
    //
    // NB: It's important to only call serialize_parent_part_two() for a
    // Sequence that really is the parent of a leaf sequence.
    if (status && !is_end_of_rows(i)) {
        BaseType *btp = get_parent();
        if (btp && btp->type() == dods_sequence_c) static_cast<Sequence&>(*btp).serialize_parent_part_two(dds, eval, m);
    }

    d_wrote_soi = false;
    while (status && !is_end_of_rows(i)) {
        i += d_row_stride;

        DBG(cerr << "Writing Start of Instance marker" << endl);
        d_wrote_soi = true;
        write_start_of_instance(m);

        // In this loop serialize will signal an error with an exception.
        for (Vars_iter iter = d_vars.begin(); iter != d_vars.end(); iter++) {
            DBG(cerr << "Sequence::serialize_leaf(), serializing "
                    << (*iter)->name() << endl);
            if ((*iter)->send_p()) {
                DBG(cerr << "Send P is true, sending " << (*iter)->name() << endl);
                (*iter)->serialize(eval, dds, m, false);
            }
        }

        set_read_p(false); // ...so this will read the next instance

        status = read_row(i, dds, eval, ce_eval);
        DBG(cerr << "Sequence::serialize_leaf::read_row() status: " << status << endl);
    }

    // Only write the EOS marker if there's a matching Start Of Instance
    // Marker in the stream.
    if (d_wrote_soi || d_top_most) {
        DBG(cerr << "Writing End of Sequence marker" << endl);
        write_end_of_sequence(m);
    }

    return true;  // Signal errors with exceptions.
}

/** This method is used to evaluate a constraint and based on those results
 load the Sequence variable with data. This simulates having a server call
 the serialize() method and a client call the deserialize() method without
 the overhead of any IPC. Use this method on the server-side to 'load the
 d_values field with data' so that other code and work with those data.

 The somewhat odd algorithm used by serialize() is largely copied here, so
 comments about logic in serialize() and the related methods apply here
 as well.

 @note Even though each Sequence variable has a \e values field, only the
 top-most Sequence in a hierarchy of Sequences holds values. The field
 accessed by the var_value() method is a completely linked object; access
 the values of nested Sequences using the BaseType objects returned by
 var_value().

 @note Only call this method for top-most Sequences. Never call it for
 Sequences which have a parent (directly or indirectly) variable that is
 a Sequence.

 @param eval Use this constraint evaluator
 @param dds This DDS holds the variables for the data source */
void Sequence::intern_data(ConstraintEvaluator &eval, DDS &dds)
{
    DBG(cerr << "Sequence::intern_data - for " << name() << endl); DBG2(cerr << "    intern_data, values: " << &d_values << endl);

    // Why use a stack instead of return values? We need the stack because
    // Sequences nested three of more levels deep will loose the middle
    // instances when the intern_data_parent_part_two() code is run.
    sequence_values_stack_t sequence_values_stack;

    sequence_values_stack.push(&d_values);

    intern_data_private(eval, dds, sequence_values_stack);
}

void Sequence::intern_data_private(ConstraintEvaluator &eval, DDS &dds, sequence_values_stack_t &sequence_values_stack)
{
    DBG(cerr << "Entering intern_data_private for " << name() << endl);

    if (is_leaf_sequence())
        intern_data_for_leaf(dds, eval, sequence_values_stack);
    else
        intern_data_parent_part_one(dds, eval, sequence_values_stack);
}

void Sequence::intern_data_parent_part_one(DDS & dds, ConstraintEvaluator & eval,
        sequence_values_stack_t & sequence_values_stack)
{
    DBG(cerr << "Entering intern_data_parent_part_one for " << name() << endl);

    int i = (get_starting_row_number() != -1) ? get_starting_row_number() : 0;

    // read_row returns true if valid data was read, false if the EOF was
    // found. 6/1/2001 jhrg
    // Since this is a parent sequence, read the row ignoring the CE (all of
    // the CE clauses will be evaluated by the leaf sequence).
    bool status = read_row(i, dds, eval, false);

    // Grab the current size of the value stack. We do this because it is
    // possible that no nested sequences for this row happened to be
    // selected because of a constraint evaluation or the last row is not
    // selected because of a constraint evaluation. In either case, no
    // nested sequence d_values are pushed onto the stack, so there is
    // nothing to pop at the end of this function. pcw 07/14/08
    SequenceValues::size_type orig_stack_size = sequence_values_stack.size();

    while (status && (get_ending_row_number() == -1 || i <= get_ending_row_number())) {
        i += get_row_stride();
        for (Vars_iter iter = var_begin(); iter != var_end(); iter++) {
            if ((*iter)->send_p()) {
                switch ((*iter)->type()) {
                case dods_sequence_c:
                    static_cast<Sequence&>(**iter).intern_data_private(eval, dds, sequence_values_stack);
                    break;

                default:
                    (*iter)->intern_data(eval, dds);
                    break;
                }
            }
        }

        set_read_p(false);      // ...so this will read the next instance

        status = read_row(i, dds, eval, false);
    }

    // Reset current row number for next nested sequence element.
    reset_row_number();

    // if the size of the stack is larger than the original size (retrieved
    // above) then pop the top set of d_values from the stack. If it's the
    // same, then no nested sequences, or possibly the last nested sequence,
    // were pushed onto the stack, so there is nothing to pop.
    if (sequence_values_stack.size() > orig_stack_size) {
        DBG2(cerr << "    popping d_values (" << sequence_values_stack.top()
                << ") off stack; size: " << sequence_values_stack.size() << endl);
        sequence_values_stack.pop();
    }

    DBG(cerr << "Leaving intern_data_parent_part_one for " << name() << endl);
}

void Sequence::intern_data_parent_part_two(DDS &dds, ConstraintEvaluator &eval,
        sequence_values_stack_t &sequence_values_stack)
{
    DBG(cerr << "Entering intern_data_parent_part_two for " << name() << endl);

    BaseType *btp = get_parent();
    if (btp && btp->type() == dods_sequence_c) {
        static_cast<Sequence&>(*btp).intern_data_parent_part_two(dds, eval, sequence_values_stack);
    }

    DBG2(cerr << "    stack size: " << sequence_values_stack.size() << endl);
    SequenceValues *values = sequence_values_stack.top();
    DBG2(cerr << "    using values = " << (void *)values << endl);

    if (get_unsent_data()) {
        BaseTypeRow *row_data = new BaseTypeRow;

        // In this loop transfer_data will signal an error with an exception.
        for (Vars_iter iter = var_begin(); iter != var_end(); iter++) {

            if ((*iter)->send_p() && (*iter)->type() != dods_sequence_c) {
                row_data->push_back((*iter)->ptr_duplicate());
            }
            else if ((*iter)->send_p()) { //Sequence; must be the last variable
                Sequence *tmp = dynamic_cast<Sequence*>((*iter)->ptr_duplicate());
                if (!tmp) {
                    delete row_data;
                    throw InternalErr(__FILE__, __LINE__, "Expected a Sequence.");
                }
                row_data->push_back(tmp);
                DBG2(cerr << "    pushing d_values of " << tmp->name()
                        << " (" << &(tmp->d_values)
                        << ") on stack; size: " << sequence_values_stack.size()
                        << endl);
                // This pushes the d_values field of the newly created leaf
                // Sequence onto the stack. The code then returns to intern
                // _data_for_leaf() where this value will be used.
                sequence_values_stack.push(&(tmp->d_values));
            }
        }

        DBG2(cerr << "    pushing values for " << name()
                << " to " << values << endl);
        values->push_back(row_data);
        set_unsent_data(false);
    }

    DBG(cerr << "Leaving intern_data_parent_part_two for " << name() << endl);
}

void Sequence::intern_data_for_leaf(DDS &dds, ConstraintEvaluator &eval, sequence_values_stack_t &sequence_values_stack)
{
    DBG(cerr << "Entering intern_data_for_leaf for " << name() << endl);

    int i = (get_starting_row_number() != -1) ? get_starting_row_number() : 0;

    DBG2(cerr << "    reading row " << i << endl);
    bool status = read_row(i, dds, eval, true);
    DBG2(cerr << "    status: " << status << endl); DBG2(cerr << "    ending row number: " << get_ending_row_number() << endl);

    if (status && (get_ending_row_number() == -1 || i <= get_ending_row_number())) {
        BaseType *btp = get_parent();
        if (btp && btp->type() == dods_sequence_c) {
            // This call will read the values for the parent sequences and
            // then allocate a new instance for the leaf and push that onto
            // the stack.
            static_cast<Sequence&>(*btp).intern_data_parent_part_two(dds, eval, sequence_values_stack);
        }

        // intern_data_parent_part_two pushes the d_values field of the leaf
        // onto the stack, so this operation grabs that value and then loads
        // data into it.
        SequenceValues *values = sequence_values_stack.top();
        DBG2(cerr << "    using values = " << values << endl);

        while (status && (get_ending_row_number() == -1 || i <= get_ending_row_number())) {
            i += get_row_stride();

            // Copy data from the object's fields to this new BaeTypeRow instance
            BaseTypeRow *row_data = new BaseTypeRow;
            for (Vars_iter iter = var_begin(); iter != var_end(); iter++) {
                if ((*iter)->send_p()) {
                    row_data->push_back((*iter)->ptr_duplicate());
                }
            }

            DBG2(cerr << "    pushing values for " << name()
                    << " to " << values << endl);
            // Save the row_data to values().
            values->push_back(row_data);

            set_read_p(false);      // ...so this will read the next instance
            // Read the ith row into this object's fields
            status = read_row(i, dds, eval, true);
        }

        DBG2(cerr << "    popping d_values (" << sequence_values_stack.top()
                << ") off stack; size: " << sequence_values_stack.size() << endl);
        sequence_values_stack.pop();
    }

    DBG(cerr << "Leaving intern_data_for_leaf for " << name() << endl);
}

/** @brief Deserialize (read from the network) the entire Sequence.

 This method used to read a single row at a time. Now the entire
 sequence is read at once. The method used to return True to indicate
 that more data needed to be deserialized and False when the sequence
 was completely read. Now it simply returns false. This might seem odd,
 but making this method return false breaks existing software the least.

 @param um An UnMarshaller that knows how to deserialize data
 @param dds A DataDDS from which to read.
 @param reuse Passed to child objects when they are deserialized. Some
 implementations of deserialize() use this to determine if new storage should
 be allocated or existing storage reused.
 @exception Error if a sequence stream marker cannot be read.
 @exception InternalErr if the <tt>dds</tt> param is not a DataDDS.
 @return A return value of false indicates that an EOS ("end of
 Sequence") marker was found, while a value of true indicates
 that there are more rows to be read. This version always reads the
 entire sequence, so it always returns false.
 */
bool Sequence::deserialize(UnMarshaller &um, DDS *dds, bool reuse)
{
#if 0
    // Nathan's tip - this is something that should never happen
    DataDDS *dd = dynamic_cast<DataDDS *>(dds);
    if (!dd) throw InternalErr("Expected argument 'dds' to be a DataDDS!");

    DBG2(cerr << "Reading from server/protocol version: "
            << dd->get_protocol_major() << "." << dd->get_protocol_minor()
            << endl);

    // Check for old servers.
    if (dd->get_protocol_major() < 2) {
        throw Error(
                string("The protocl version (") + dd->get_protocol()
                        + ") indicates that this\nis an old server which may not correctly transmit Sequence variables.\nContact the server administrator.");
    }
#endif
    while (true) {
        // Grab the sequence stream's marker.
        unsigned char marker = read_marker(um);
        if (is_end_of_sequence(marker))
            break;  // EXIT the while loop here!!!
        else if (is_start_of_instance(marker)) {
            d_row_number++;
            DBG2(cerr << "Reading row " << d_row_number << " of "
                    << name() << endl);
            BaseTypeRow *bt_row_ptr = new BaseTypeRow;
            // Read the instance's values, building up the row
            for (Vars_iter iter = d_vars.begin(); iter != d_vars.end(); iter++) {
                BaseType *bt_ptr = (*iter)->ptr_duplicate();
                bt_ptr->deserialize(um, dds, reuse);
                DBG2(cerr << "Deserialized " << bt_ptr->name() << " ("
                        << bt_ptr << ") = "); DBG2(bt_ptr->print_val(stderr, ""));
                bt_row_ptr->push_back(bt_ptr);
            }
            // Append this row to those accumulated.
            d_values.push_back(bt_row_ptr);
        }
        else
            throw Error("I could not read the expected Sequence data stream marker!");
    };

    return false;
}

// Return the current row number.

/** Return the starting row number if the sequence was constrained using
 row numbers (instead of, or in addition to, a relational constraint).
 If a relational constraint was also given, the row number corresponds
 to the row number of the sequence <i>after</i> applying the relational
 constraint.

 If the bracket notation was not used to constrain this sequence, this
 method returns -1.

 @brief Get the starting row number.
 @return The starting row number. */
int Sequence::get_starting_row_number()
{
    return d_starting_row_number;
}

/** Return the row stride number if the sequence was constrained using
 row numbers (instead of, or in addition to, a relational constraint).
 If a relational constraint was also given, the row stride is applied
 to the sequence <i>after</i> applying the relational constraint.

 If the bracket notation was not used to constrain this sequence, this
 method returns -1.

 @brief Get the row stride.
 @return The row stride. */
int Sequence::get_row_stride()
{
    return d_row_stride;
}

/** Return the ending row number if the sequence was constrained using
 row numbers (instead of, or in addition to, a relational constraint).
 If a relational constraint was also given, the row number corresponds
 to the row number of the sequence <i>after</i> applying the
 relational constraint.

 If the bracket notation was not used to constrain this sequence, this
 method returns -1.

 @brief Get the ending row number.
 @return The ending row number. */
int Sequence::get_ending_row_number()
{
    return d_ending_row_number;
}

/** Set the start, stop and stride for a row-number type constraint.
 This should be used only when the sequence is constrained using the
 bracket notation (which supplies start, stride and stop information).
 If omitted, the stride defaults to 1.

 @param start The starting row number. The first row is row zero.
 @param stop The ending row number. The 20th row is row 19.
 @param stride The stride. A stride of two skips every other row. */
void Sequence::set_row_number_constraint(int start, int stop, int stride)
{
    if (stop < start) throw Error(malformed_expr, "Starting row number must precede the ending row number.");

    d_starting_row_number = start;
    d_row_stride = stride;
    d_ending_row_number = stop;
}

void Sequence::print_one_row(FILE *out, int row, string space, bool print_row_num)
{
    ostringstream oss;
    print_one_row(oss, row, space, print_row_num);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

void Sequence::print_one_row(ostream &out, int row, string space, bool print_row_num)
{
    if (print_row_num) out << "\n" << space << row << ": ";

    out << "{ ";

    int elements = element_count();
    int j = 0;
    BaseType *bt_ptr = 0;

    // This version of print_one_row() works for both data read with
    // deserialize(), where each variable is assumed to have valid data, and
    // intern_data(), where some/many variables do not. Because of that, it's
    // not correct to assume that all of the elements will be printed, which
    // is what the old code did.
    // Print the first value
    while (j < elements && !bt_ptr) {
        bt_ptr = var_value(row, j++);
        if (bt_ptr) {  // data
            if (bt_ptr->type() == dods_sequence_c)
                static_cast<Sequence*>(bt_ptr)->print_val_by_rows(out, space + "    ", false, print_row_num);
            else
                bt_ptr->print_val(out, space, false);
        }
    }

    // Print the remaining values
    while (j < elements) {
        bt_ptr = var_value(row, j++);
        if (bt_ptr) {  // data
            out << ", ";
            if (bt_ptr->type() == dods_sequence_c)
                static_cast<Sequence*>(bt_ptr)->print_val_by_rows(out, space + "    ", false, print_row_num);
            else
                bt_ptr->print_val(out, space, false);
        }
    }

    out << " }";
}

void Sequence::print_val_by_rows(FILE *out, string space, bool print_decl_p, bool print_row_numbers)
{
    ostringstream oss;
    print_val_by_rows(oss, space, print_decl_p, print_row_numbers);
    fwrite(oss.str().data(), sizeof(char), oss.str().length(), out);
}

void Sequence::print_val_by_rows(ostream &out, string space, bool print_decl_p, bool print_row_numbers)
{
    if (print_decl_p) {
        print_decl(out, space, false);
        out << " = ";
    }

    out << "{ ";

    int rows = number_of_rows() - 1;
    int i;
    for (i = 0; i < rows; ++i) {
        print_one_row(out, i, space, print_row_numbers);
        out << ", ";
    }
    print_one_row(out, i, space, print_row_numbers);

    out << " }";

    if (print_decl_p) out << ";\n";
}

void Sequence::print_val(FILE *out, string space, bool print_decl_p)
{
    print_val_by_rows(out, space, print_decl_p, false);
}

void Sequence::print_val(ostream &out, string space, bool print_decl_p)
{
    print_val_by_rows(out, space, print_decl_p, false);
}

void Sequence::set_leaf_p(bool state)
{
    d_leaf_sequence = state;
}

bool Sequence::is_leaf_sequence()
{
    return d_leaf_sequence;
}

/** @brief Mark the Sequence which holds the leaf elements.

 In a nested Sequence, the Sequence which holds the leaf elements is special
 because it during the serialization of this Sequence's data that constraint
 Expressions must be evaluated. If CEs are evaluated at the upper levels,
 then valid data may not be sent because it was effectively hidden from the
 serialization and evaluation code (see the documentation for the serialize_leaf()
 method).

 The notion of the leaf Sequence needs to be modified to mean the lowest level
 of a Sequence where data are to be sent. Suppose there's a two level Sequence,
 but that only fields from the top level are to be sent. Then that top level
 is also the leaf Sequence and should be marked as such. If the lower level is
 marked as a leaf Sequence, then no values will ever be sent since the send_p
 property will always be false for each field and it's the call to
 serialize_leaf() that actually triggers transmission of values (because it's
 not until the code makes it into serialize_leaf() that it knows there are
 values to be sent.

 @note This method \e must not be called before the CE is parsed.

 @param lvl The current level of the Sequence. a \e lvl of 1 indicates the
 topmost Sequence. The default value is 1.
 @see Sequence::serialize_leaf() */
void Sequence::set_leaf_sequence(int lvl)
{
    bool has_child_sequence = false;

    if (lvl == 1) d_top_most = true;

    DBG2(cerr << "Processing sequence " << name() << endl);

    for (Vars_iter iter = d_vars.begin(); iter != d_vars.end(); iter++) {
        // About the test for send_p(): Only descend into a sequence if it has
        // fields that might be sent. Thus if, in a two-level sequence, nothing
        // in the lower level is to be sent, the upper level is marked as the
        // leaf sequence. This ensures that values _will_ be sent (see the comment
        // in serialize_leaf() and serialize_parent_part_one()).
        if ((*iter)->type() == dods_sequence_c && (*iter)->send_p()) {
            if (has_child_sequence)
                throw Error("This implementation does not support more than one nested sequence at a level. Contact the server administrator.");

            has_child_sequence = true;
            static_cast<Sequence&>(**iter).set_leaf_sequence(++lvl);
        }
        else if ((*iter)->type() == dods_structure_c) {
            static_cast<Structure&>(**iter).set_leaf_sequence(lvl);
        }
    }

    if (!has_child_sequence)
        set_leaf_p(true);
    else
        set_leaf_p(false);

    DBG2(cerr << "is_leaf_sequence(): " << is_leaf_sequence() << " (" << name() << ")" << endl);
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance and information about this
 * instance.
 *
 * @param strm C++ i/o stream to dump the information to
 * @return void
 */
void Sequence::dump(ostream &strm) const
{
    strm << DapIndent::LMarg << "Sequence::dump - (" << (void *) this << ")" << endl;
    DapIndent::Indent();
    Constructor::dump(strm);
    strm << DapIndent::LMarg << "# rows deserialized: " << d_row_number << endl;
    strm << DapIndent::LMarg << "bracket notation information:" << endl;
    DapIndent::Indent();
    strm << DapIndent::LMarg << "starting row #: " << d_starting_row_number << endl;
    strm << DapIndent::LMarg << "row stride: " << d_row_stride << endl;
    strm << DapIndent::LMarg << "ending row #: " << d_ending_row_number << endl;
    DapIndent::UnIndent();

    strm << DapIndent::LMarg << "data been sent? " << d_unsent_data << endl;
    strm << DapIndent::LMarg << "start of instance? " << d_wrote_soi << endl;
    strm << DapIndent::LMarg << "is leaf sequence? " << d_leaf_sequence << endl;
    strm << DapIndent::LMarg << "top most in hierarchy? " << d_top_most << endl;
    DapIndent::UnIndent();
}

} // namespace libdap

