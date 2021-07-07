
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

// (c) COPYRIGHT URI/MIT 1995-1997,1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Interface definition for TestArray. See TestByte.h for more information
//
// jhrg 1/12/95

#ifndef _testarray_h
#define _testarray_h 1

#include "Array.h"
#include "TestCommon.h"

using namespace libdap ;

class TestArray: public Array, public TestCommon {
    bool d_series_values;
    void _duplicate(const TestArray &ts);
    uint64_t m_print_array(ostream &out, uint64_t index, unsigned int dims, unsigned int shape[]);

    bool m_name_is_special();
    void m_build_special_values();

    int m_offset(int y, Dim_iter Y, int x);

    template <typename T, class C> void m_constrained_matrix(vector<T> &constrained_array);
    template <typename T> void m_enum_constrained_matrix(vector<T> &constrained_array);

    template <typename T, class C> void m_cardinal_type_read_helper();
    template <typename T> void m_enum_type_read_helper();

public:
    TestArray(const string &n, BaseType *v, bool is_dap4 = false);
    TestArray(const string &n, const string &d, BaseType *v, bool is_dap4 = false);
    TestArray(const TestArray &rhs);

    virtual ~TestArray();

    TestArray &operator=(const TestArray &rhs);

    virtual BaseType *ptr_duplicate();

    virtual bool read();

    virtual void output_values(std::ostream &out);

    void set_series_values(bool);
    bool get_series_values() { return d_series_values; }
};

#endif // _testarray_h

