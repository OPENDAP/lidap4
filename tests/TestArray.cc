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

// (c) COPYRIGHT URI/MIT 1995-1996,1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// Implementation for TestArray. See TestByte.cc
//
// jhrg 1/12/95

#include "config.h"

#include <cstring>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef WIN32
#else
#include <io.h>
#include <fcntl.h>
#include <process.h>
#endif

//#define DODS_DEBUG

#include "util.h"
#include "debug.h"

#include "TestInt8.h"

#include "TestByte.h"
#include "TestInt16.h"
#include "TestUInt16.h"
#include "TestInt32.h"
#include "TestUInt32.h"

#include "TestInt64.h"
#include "TestUInt64.h"

#include "TestD4Enum.h"

#include "TestFloat32.h"
#include "TestFloat64.h"

#include "TestStr.h"

#include "TestArray.h"
#include "TestCommon.h"

using std::cerr;
using std::endl;

extern int test_variable_sleep_interval;

void TestArray::_duplicate(const TestArray &ts)
{
    d_series_values = ts.d_series_values;
}

BaseType *
TestArray::ptr_duplicate()
{
    return new TestArray(*this);
}

TestArray::TestArray(const string &n, BaseType *v, bool is_dap4) :
        Array(n, v, is_dap4), d_series_values(false)
{
}

TestArray::TestArray(const string &n, const string &d, BaseType *v, bool is_dap4) :
        Array(n, d, v, is_dap4), d_series_values(false)
{
}

TestArray::TestArray(const TestArray &rhs) :
        Array(rhs), TestCommon(rhs)
{
    _duplicate(rhs);
}

TestArray::~TestArray()
{
}

TestArray &
TestArray::operator=(const TestArray &rhs)
{
    if (this == &rhs) return *this;

    dynamic_cast<Array &>(*this) = rhs;

    _duplicate(rhs);

    return *this;
}

// This code calls 'output_values()' because print_val() does not test
// the value of send_p(). We need to wrap a method around the calls to
// print_val() to ensure that only values for variables with send_p() set
// are called. In the serialize/deserialize case, the 'client' DDS only
// has variables sent by the 'server' but in the intern_data() case, the
// whole DDS is still present but only variables selected in the CE have
// values.

uint64_t TestArray::m_print_array(ostream &out, uint64_t index, unsigned int dims, unsigned int shape[])
{
    if (dims == 1) {
        out << "{";

        // Added this test for zero-length arrays. jhrg 1/28/16
        if (shape[0] >= 1) {
            for (unsigned i = 0; i < shape[0] - 1; ++i) {
                dynamic_cast<TestCommon&>(*var(index++)).output_values(out);
                out << ", ";
            }

            dynamic_cast<TestCommon&>(*var(index++)).output_values(out);
        }

        out << "}";

        return index;
    }
    else {
        out << "{";
        // Fixed an off-by-one error in the following loop. Since the array
        // length is shape[dims-1]-1 *and* since we want one less dimension
        // than that, the correct limit on this loop is shape[dims-2]-1. From
        // Todd Karakasian.
        //
        // The saga continues; the loop test should be `i < shape[0]-1'. jhrg
        // 9/12/96.
        //
        // Added this (primitive) guard against errors when a zero-length array
        // is declared with a shape like [0][4]. jhrg 1/28/16
        if (shape[0] > 0) {
            for (unsigned i = 0; i < shape[0] - 1; ++i) {
                index = m_print_array(out, index, dims - 1, shape + 1);
                out << ",";
            }

            index = m_print_array(out, index, dims - 1, shape + 1);
        }

        out << "}";

        return index;
    }
}

void TestArray::output_values(std::ostream &out)
{
    //unsigned int *shape = new unsigned int[dimensions(true)];

    vector<unsigned int> shape(dimensions(true));
    uint64_t index = 0;
    for (Dim_iter i = dim_begin(); i != dim_end() && index < dimensions(true); ++i)
        shape[index++] = dimension_size(i, true);

    m_print_array(out, 0, dimensions(true), &shape[0]);

    //delete[] shape;
    //shape = 0;
}

/** Special names are ones that start with 'lat' or 'lon'. These indicate
 that the vector (this is only for vectors) is a vector of latitude or
 longitude values. */
bool TestArray::m_name_is_special()
{
    return (name().find("lat") != string::npos || name().find("lon") != string::npos);
}

void TestArray::m_build_special_values()
{
    int int_array_len;

    if (name().find("lat_reversed") != string::npos) {
        uint64_t array_len = length();
        int_array_len = static_cast<int>(array_len);
        //double *lat_data = new double[array_len];
        vector<double> lat_data(array_len);
        for (int i = 0; i < array_len; ++i) {
            lat_data[i] = -89 + (180 / int_array_len) * (i + 1);
        }
        libdap::set_array_using_double(this, &lat_data[0], array_len);
    }
    else if (name().find("lat") != string::npos) {
        uint64_t array_len = length();
        int_array_len = static_cast<int>(array_len);
        // double *lat_data = new double[array_len];
        vector<double> lat_data(array_len);
        for (int i = 0; i < array_len; ++i) {
            lat_data[i] = 90 - (180 / int_array_len) * (i + 1);
        }
        libdap::set_array_using_double(this, &lat_data[0], array_len);
    }
    else if (name().find("lon") != string::npos) {
        uint64_t array_len = length();
        int_array_len = static_cast<int>(array_len);
        //double *lon_data = new double[array_len];
        vector<double> lon_data(array_len);
        for (int i = 0; i < array_len; ++i) {
            lon_data[i] = (360 / int_array_len) * (i + 1);
        }
        libdap::set_array_using_double(this, &lon_data[0], array_len);
    }
    else {
        throw InternalErr(__FILE__, __LINE__, "Unrecognized name");
    }
}

int TestArray::m_offset(int y, Dim_iter X, int x)
{
    return y * dimension_size(X, false) + x;
}

/**
 * @brief Load an 2D array with values.
 * Use the read() function for the prototype element of the array to
 * get values and load them into an array, then constrain the array.
 * Thus if 'series values' are used and the array is constrained, the
 * result will 'make sense'
 *
 * @param constrained_array
 */
template<typename T, class C>
void TestArray::m_constrained_matrix(vector<T>&constrained_array)
{
    int unconstrained_size = 1;
    Dim_iter d = dim_begin();
    while (d != dim_end())
        unconstrained_size *= dimension_size(d++, false);

    vector<T> whole_array(unconstrained_size);
    for (uint64_t i = 0; i < unconstrained_size; ++i) {
        T v;
        var()->read();
#if 0
        if (var()->type() == dods_enum_c)
        static_cast<C*>(var())->value(&v);
        else
#endif
        v = static_cast<C*>(var())->value();

        whole_array[i] = v;
        var()->set_read_p(false); // pick up the next value
    }

    DBG(cerr << "whole_array: "; copy(whole_array.begin(), whole_array.end(), ostream_iterator<T>(cerr, ", ")); cerr << endl);

    Dim_iter Y = dim_begin();
    Dim_iter X = Y + 1;

    DBG(cerr << "dimension_start(Y): " << dimension_start(Y) << endl); DBG(cerr << "dimension_stop(Y): " << dimension_stop(Y) << endl); DBG(cerr << "dimension_start(X): " << dimension_start(X) << endl); DBG(cerr << "dimension_stop(X): " << dimension_stop(X) << endl);

    uint64_t constrained_size = 0;
    uint64_t y = dimension_start(Y);
    while (y < dimension_stop(Y) + 1) {
        int x = dimension_start(X);

        while (x < dimension_stop(X) + 1) {
            constrained_array[constrained_size++] = whole_array[m_offset(y, X, x)];
            x += dimension_stride(X);
        }

        y += dimension_stride(Y);
    }
}

template<typename T>
void TestArray::m_enum_constrained_matrix(vector<T>&constrained_array)
{
    uint64_t unconstrained_size = 1;
    Dim_iter d = dim_begin();
    while (d != dim_end())
        unconstrained_size *= dimension_size(d++, false);

    vector<T> whole_array(unconstrained_size);
    for (uint64_t i = 0; i < unconstrained_size; ++i) {
        T v;
        var()->read();
        static_cast<D4Enum*>(var())->value(&v);
        whole_array[i] = v;
        var()->set_read_p(false); // pick up the next value
    }

    DBG(cerr << "whole_array: "; copy(whole_array.begin(), whole_array.end(), ostream_iterator<T>(cerr, ", ")); cerr << endl);

    Dim_iter Y = dim_begin();
    Dim_iter X = Y + 1;

    DBG(cerr << "dimension_start(Y): " << dimension_start(Y) << endl); DBG(cerr << "dimension_stop(Y): " << dimension_stop(Y) << endl); DBG(cerr << "dimension_start(X): " << dimension_start(X) << endl); DBG(cerr << "dimension_stop(X): " << dimension_stop(X) << endl);

    uint64_t constrained_size = 0;
    uint64_t y = dimension_start(Y);
    while (y < dimension_stop(Y) + 1) {
        uint64_t x = dimension_start(X);

        while (x < dimension_stop(X) + 1) {
            constrained_array[constrained_size++] = whole_array[m_offset(y, X, x)];
            x += dimension_stride(X);
        }

        y += dimension_stride(Y);
    }
}

/**
 * Load the variable's internal data buffer with values, simulating a read()
 * call to some data store. A private method.
 */
template<typename T, class C>
void TestArray::m_cardinal_type_read_helper()
{
    if (get_series_values()) {
        // Special case code for vectors that have specific names.
        // This is used to test code that works with lat/lon data.
        if (dimensions() == 1 && m_name_is_special()) {
            m_build_special_values();
        }
        else if (dimensions() == 2) {
            vector<T> tmp(length());
            m_constrained_matrix<T, C>(tmp);
            set_value(tmp, length());
        }
        else {
            vector<T> tmp(length());
            for (int64_t i = 0, end = length(); i < end; ++i) {
                var()->read();
                tmp[i] = static_cast<C*>(var())->value();
                var()->set_read_p(false); // pick up the next value
            }
            set_value(tmp, length());
        }
    }
    else {
        // read a value into the Array's prototype element
        var()->read();
        T value = static_cast<C*>(var())->value();
        vector<T> tmp(length());
        for (int64_t i = 0, end = length(); i < end; ++i) {
            tmp[i] = value;
        }

        set_value(tmp, length());
    }
}

/**
 * Load the variable's internal data buffer with values, simulating a read()
 * call to some data store. A private method.
 */
template<typename T>
void TestArray::m_enum_type_read_helper()
{
    if (get_series_values()) {
        if (dimensions() == 2) {
            vector<T> tmp(length());
            m_enum_constrained_matrix<T>(tmp);
            set_value(tmp, length());
        }
        else {
            vector<T> tmp(length());
            for (int64_t i = 0, end = length(); i < end; ++i) {
                var()->read();
                T v;
                static_cast<D4Enum*>(var())->value(&v);

                tmp[i] = v;
                var()->set_read_p(false); // pick up the next value
            }
            set_value(tmp, length());
        }
    }
    else {
        // read a value into the Array's prototype element
        var()->read();
        T value;
        static_cast<D4Enum*>(var())->value(&value);

        vector<T> tmp(length());
        for (int64_t i = 0, end = length(); i < end; ++i) {
            tmp[i] = value;
        }

        set_value(tmp, length());
    }
}

bool TestArray::read()
{
    if (read_p()) return true;

    if (test_variable_sleep_interval > 0) sleep(test_variable_sleep_interval);

    int64_t array_len = length(); // elements in the array

    switch (var()->type()) {
    // These are the DAP2 types and the classes that implement them all define
    // the old buf2val() and val2buf() methods. For the new DAP4 types see below.
    //case dods_byte_c:
    //case dods_uint8_c:
    case dods_int16_c:
        m_cardinal_type_read_helper<dods_int16, Int16>();
        set_read_p(true);
        break;

    case dods_uint16_c:
        m_cardinal_type_read_helper<dods_uint16, UInt16>();
        set_read_p(true);
        break;

    case dods_int32_c:
        m_cardinal_type_read_helper<dods_int32, Int32>();
        set_read_p(true);
        break;

    case dods_uint32_c:
        m_cardinal_type_read_helper<dods_uint32, UInt32>();
        set_read_p(true);
        break;

    case dods_float32_c:
        m_cardinal_type_read_helper<dods_float32, Float32>();
        set_read_p(true);
        break;

    case dods_float64_c:
        m_cardinal_type_read_helper<dods_float64, Float64>();
        set_read_p(true);
        break;

    case dods_int8_c:
        m_cardinal_type_read_helper<dods_int8, Int8>();
        set_read_p(true);
        break;

    case dods_byte_c:
    case dods_char_c:
    case dods_uint8_c:
        m_cardinal_type_read_helper<dods_byte, Byte>();
        set_read_p(true);
        break;

    case dods_int64_c:
        m_cardinal_type_read_helper<dods_int64, Int64>();
        set_read_p(true);
        break;

    case dods_uint64_c:
        m_cardinal_type_read_helper<dods_uint64, UInt64>();
        set_read_p(true);
        break;

    case dods_enum_c:
        switch (static_cast<D4Enum*>(var())->element_type()) {
        case dods_byte_c:
        case dods_char_c:
        case dods_uint8_c:
            m_enum_type_read_helper<dods_byte>();
            break;
        case dods_int8_c:
            m_enum_type_read_helper<dods_int8>();
            break;
        case dods_int16_c:
            m_enum_type_read_helper<dods_int16>();
            break;
        case dods_uint16_c:
            m_enum_type_read_helper<dods_uint16>();
            break;
        case dods_int32_c:
            m_enum_type_read_helper<dods_int32>();
            break;
        case dods_uint32_c:
            m_enum_type_read_helper<dods_uint32>();
            break;
        case dods_int64_c:
            m_enum_type_read_helper<dods_int64>();
            break;
        case dods_uint64_c:
            m_enum_type_read_helper<dods_uint64>();
            break;
        default:
            throw InternalErr(__FILE__, __LINE__, "Enum with undefined type.");
        }
        set_read_p(true);
        break;

    case dods_str_c:
    case dods_url_c: {
        vector<string> tmp(array_len);

        if (get_series_values()) {
            for (int64_t i = 0; i < array_len; ++i) {
                var()->read();
                // URL isa Str
                tmp[i] = static_cast<Str*>(var())->value();
                var()->set_read_p(false); // pick up the next value
            }
        }
        else {
            var()->read();
            string value = static_cast<Str*>(var())->value();

            for (unsigned i = 0; i < array_len; ++i)
                tmp[i] = value;
        }

        set_value(tmp, array_len);
        set_read_p(true);
        break;
    }

    case dods_opaque_c:
    case dods_structure_c:
        vec_resize(array_len);
        for (unsigned i = 0; i < array_len; ++i) {
            // Copy the prototype and read a value into it
            BaseType *elem = var()->ptr_duplicate();
            elem->read();
            // Load the new value into this object's array
            set_vec_nocopy(i, elem);   // Use set_vec_nocopy() TODO (and below)
        }
        set_read_p(true);
        break;

    case dods_sequence_c:
        // No sequence arrays in DAP2
        if (!is_dap4()) throw InternalErr(__FILE__, __LINE__, "Bad data type");

        vec_resize(array_len);
        for (unsigned i = 0; i < array_len; ++i) {
            // Load the new BaseType (a D4Sequence) into the array element
            set_vec_nocopy(i, var()->ptr_duplicate());
        }

        break;

        // No Grids in DAP4; No arrays of arrays and no null-typed vars in DAP2 or 4
    case dods_grid_c:
    case dods_array_c:
    case dods_null_c:
    default:
        throw InternalErr(__FILE__, __LINE__, "Bad data type");
    }

    // set_read_p(true);

    return true;
}

void TestArray::set_series_values(bool sv)
{
    dynamic_cast<TestCommon&>(*var()).set_series_values(sv);
    d_series_values = sv;
}
