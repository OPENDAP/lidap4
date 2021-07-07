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

// Implementation for the class TestStructure. See TestByte.cc
//
// jhrg 1/12/95
//
// Note that the test code here to read values from a data file works only
// for single level sequences - that is, it does *not* work for sequences
// that contain other sequences. jhrg 2/2/98

//#define DODS_DEBUG

#include "config.h"
#include "D4Group.h"
#include "Constructor.h"
#include "D4Sequence.h"
#include "debug.h"

#include "TestSequence.h"
#include "TestD4Sequence.h"
#include "TestCommon.h"

using namespace libdap;

void TestSequence::_duplicate(const TestSequence &ts)
{
    d_current = ts.d_current;
    d_len = ts.d_len;
    d_series_values = ts.d_series_values;
}

BaseType *
TestSequence::ptr_duplicate()
{
    return new TestSequence(*this);
}

TestSequence::TestSequence(const string &n) :
        Sequence(n), d_len(4), d_set_length(true), d_current(0), d_series_values(false)
{
}

TestSequence::TestSequence(const string &n, const string &d) :
        Sequence(n, d), d_len(4), d_set_length(true), d_current(0), d_series_values(false)
{
}

TestSequence::TestSequence(const TestSequence &rhs) :
        Sequence(rhs), TestCommon(rhs)
{
    _duplicate(rhs);
}

TestSequence::~TestSequence()
{
}

TestSequence &
TestSequence::operator=(const TestSequence &rhs)
{
    if (this == &rhs) return *this;

    dynamic_cast<Sequence &>(*this) = rhs; // run Constructor=

    _duplicate(rhs);

    return *this;
}

void
TestSequence::transform_to_dap4(D4Group *root, Constructor *container)
{
    TestD4Sequence *dest = new TestD4Sequence(name());
    Constructor::transform_to_dap4(root, dest);
    container->add_var_nocopy(dest);
}

void TestSequence::output_values(std::ostream &out)
{
    print_val(out, "", false);
}

// Read values from text files. Sequence instances are stored on separate
// lines. Line can be no more than 255 characters long.

bool TestSequence::read()
{
    DBG(cerr << "Entering TestSequence::read for " << name() << endl);

    if (read_p()) return true;

    DBG(cerr << "current: " << d_current << ", length: " << d_len << endl);
    // When we get to the end of a Sequence, reset the row number counter so
    // that, in case this is an inner sequence, the next instance will be read
    // and the "Trying to back up in a Sequence" error won't be generated.
    if (++d_current > d_len) {
        DBG(cerr << "Leaving TestSequence::read for " << name()
                << " because d_current(" << d_current
                << ") > d_len(" << d_len << ")" << endl);
        d_current = 0;                  // reset
        set_unsent_data(false);
        reset_row_number();
        // jhrg original version from 10/9/13: return false; // No more values
        return true;
    }

    Vars_iter i = var_begin();
    while (i != var_end()) {
        if ((*i)->send_p() || (*i)->is_in_selection()) {
            DBG(cerr << "Calling " << (*i)->name() << "->read()" << endl);
            (*i)->read();
        }
        ++i;
    }

    set_unsent_data(true);
    DBG(cerr << "Leaving TestSequence::read for " << name() << endl);
    return false;
}

void TestSequence::set_series_values(bool sv)
{
    Vars_iter i = var_begin();
    while (i != var_end()) {
        dynamic_cast<TestCommon&>(*(*i)).set_series_values(sv);
        ++i;
    }

    d_series_values = sv;
}

uint64_t TestSequence::length() const
{
    return 5;
}
