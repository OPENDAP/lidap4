
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

// Interface for the class TestSequence. See TestByte.h
//
// jhrg 1/12/95

#ifndef _testsequence_h
#define _testsequence_h 1

#include "Sequence.h"
#include "TestCommon.h"

using namespace libdap ;

namespace libdap {
	class D4Group;
	class Constructor;
}

class TestSequence: public Sequence, public TestCommon {
private:
    uint64_t d_len;
    bool d_set_length;
    uint64_t d_current;
    bool d_series_values;

    void _duplicate(const TestSequence &ts);

public:
    TestSequence(const string &n);
    TestSequence(const string &n, const string &d);
    TestSequence(const TestSequence &rhs);

    virtual ~TestSequence();

    TestSequence &operator=(const TestSequence &rhs);
    virtual BaseType *ptr_duplicate();
    virtual void transform_to_dap4(libdap::D4Group *root, libdap::Constructor *container);

    virtual bool read();

    virtual void output_values(std::ostream &out);

    void set_series_values(bool);
    bool get_series_values() { return d_series_values; }

    virtual uint64_t length() const;
};

#endif // _testsequence_h
