
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

//#define DODS_DEBUG

#include "TestD4Sequence.h"
#include "TestCommon.h"

#include "debug.h"

void
TestD4Sequence::m_duplicate(const TestD4Sequence &ts)
{
    d_current = ts.d_current;
    d_len = ts.d_len;
    d_series_values = ts.d_series_values;
    d_set_length = ts.d_set_length;
}

BaseType *
TestD4Sequence::ptr_duplicate()
{
    return new TestD4Sequence(*this);
}

TestD4Sequence::TestD4Sequence(const string &n) : D4Sequence(n), d_len(4), d_set_length(true),
	d_current(0), d_series_values(false)
{
}

TestD4Sequence::TestD4Sequence(const string &n, const string &d)
    : D4Sequence(n, d), d_len(4), d_set_length(true), d_current(0), d_series_values(false)
{
}

TestD4Sequence::TestD4Sequence(const TestD4Sequence &rhs) : D4Sequence(rhs), TestCommon(rhs)
{
    m_duplicate(rhs);
}

TestD4Sequence::~TestD4Sequence()
{
}

TestD4Sequence &
TestD4Sequence::operator=(const TestD4Sequence &rhs)
{
    if (this == &rhs)
        return *this;

    dynamic_cast<D4Sequence &>(*this) = rhs;

    m_duplicate(rhs);

    return *this;
}

void
TestD4Sequence::output_values(std::ostream &out)
{
    print_val(out, "", false);
}

/**
 * In DAP2, TestSequence::read() also reads the child sequence data. However,
 * in DAP4 it's up to each sequence to read its own data.
 */
bool
TestD4Sequence::read()
{
    if (read_p())
        return true;

    if (d_current < d_len) {
    	for (Vars_iter i = var_begin(), e = var_end(); i != e; ++i) {
            if ((*i)->type() != dods_sequence_c && ((*i)->send_p() || (*i)->is_in_selection())) {
                DBGN(cerr << __PRETTY_FUNCTION__ << "Calling " << (*i)->name() << "->read()" << endl);
                (*i)->read();
            }
    	}

    	++d_current;
    	return false;
    }
    else {
        return true;                // No more values
    }
}

void
TestD4Sequence::set_series_values(bool sv)
{
    Vars_iter i = var_begin();
    while (i != var_end()) {
        dynamic_cast<TestCommon&>(*(*i)).set_series_values(sv);
        ++i;
    }

    d_series_values = sv;
}
