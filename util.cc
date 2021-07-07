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

// Utility functions used by the api.
//
// jhrg 9/21/94

#include "config.h"

#include <fstream>

#include <cassert>
#include <cstring>
#include <climits>
#include <cstdlib>

#include <ctype.h>
#ifndef TM_IN_SYS_TIME
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef WIN32
#include <unistd.h>    // for stat
#else
#include <io.h>
#include <fcntl.h>
#include <process.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "BaseType.h"
#include "Byte.h"
#include "Int16.h"
#include "Int32.h"
#include "UInt16.h"
#include "UInt32.h"
#include "Float32.h"
#include "Float64.h"
#include "Str.h"
#include "Array.h"

#include "Int64.h"
#include "UInt64.h"
#include "Int8.h"

#include "Error.h"

#include "util.h"
#include "GNURegex.h"
#include "debug.h"

using namespace std;

namespace libdap {

/** @brief Does this host use big-endian byte order? */
bool is_host_big_endian()
{
#ifdef COMPUTE_ENDIAN_AT_RUNTIME

    dods_int16 i = 0x0100;
    char *c = reinterpret_cast<char*>(&i);
    return *c;

#else

#if WORDS_BIGENDIAN
    return true;
#else
    return false;
#endif

#endif
}

/** Given a BaseType pointer, extract the string value it contains

 @param arg The BaseType pointer
 @return A C++ string
 @exception Error thrown if the referenced BaseType object does not contain
 a DAP String. */
string extract_string_argument(BaseType *arg)
{
    assert(arg);

    if (arg->type() != dods_str_c) throw Error(malformed_expr, "The function requires a string argument.");

    if (!arg->read_p())
        throw InternalErr(__FILE__, __LINE__,
                "The CE Evaluator built an argument list where some constants held no values.");

    return static_cast<Str*>(arg)->value();
}

template<class T> static void set_array_using_double_helper(Array *a, double *src, uint64_t src_len)
{
    assert(a);
    assert(src);
    //assert(src_len > 0);

    vector<T> values(src_len);
    for (uint64_t i = 0; i < src_len; ++i)
        values[i] = (T) src[i];

    // This copies the values
    a->set_value(values, src_len);
}

/** Given an array that holds some sort of numeric data, load it with values
 using an array of doubles. This function makes several assumptions. First,
 it assumes the caller really wants to put the doubles into whatever types
 the array holds! Caveat emptor. Second, it assumes that if the size of
 source (\e src) array is different than the destination (\e dest) the
 caller has made a mistake. In that case it will throw an Error object.

 After setting the values, this method sets the \c read_p property for
 \e dest. Setting \e read_p tells the serialization methods in libdap
 that this variable already holds data values and, given that, the
 serialization code will not try to read the values.

 @note Support for DAP4 added.
 @param dest An Array. The values are written to this array, reusing
 its storage. Existing values are lost.
 @param src The source data.
 @param src_len The number of elements in the \e src array.
 @exception Error Thrown if \e dest is not a numeric-type array (Byte, ...,
 Float64) or if the number of elements in \e src does not match the number
 is \e dest. */
void set_array_using_double(Array *dest, double *src, uint64_t src_len)
{
    assert(dest);
    assert(src);
    //assert(src_len > 0);

    // Simple types are Byte, ..., Float64, String and Url.
    if ((dest->type() == dods_array_c && !dest->var()->is_simple_type()) || dest->var()->type() == dods_str_c
            || dest->var()->type() == dods_url_c)
        throw InternalErr(__FILE__, __LINE__, "The function requires a numeric-type array argument.");

    // Test sizes. Note that Array::length() takes any constraint into account
    // when it returns the length. Even if this was removed, the 'helper'
    // function this uses calls Vector::val2buf() which uses Vector::width()
    // which in turn uses length().
    if (dest->length() != src_len)
        throw InternalErr(__FILE__, __LINE__,
                "The source and destination array sizes don't match (" + long_to_string(src_len) + " versus "
                        + long_to_string(dest->length()) + ").");

    // The types of arguments that the CE Parser will build for numeric
    // constants are limited to Uint32, Int32 and Float64. See ce_expr.y.
    // Expanded to work for any numeric type so it can be used for more than
    // just arguments.
    switch (dest->var()->type()) {
    case dods_byte_c:
        set_array_using_double_helper<dods_byte>(dest, src, src_len);
        break;
    case dods_uint16_c:
        set_array_using_double_helper<dods_uint16>(dest, src, src_len);
        break;
    case dods_int16_c:
        set_array_using_double_helper<dods_int16>(dest, src, src_len);
        break;
    case dods_uint32_c:
        set_array_using_double_helper<dods_uint32>(dest, src, src_len);
        break;
    case dods_int32_c:
        set_array_using_double_helper<dods_int32>(dest, src, src_len);
        break;
    case dods_float32_c:
        set_array_using_double_helper<dods_float32>(dest, src, src_len);
        break;
    case dods_float64_c:
        set_array_using_double_helper<dods_float64>(dest, src, src_len);
        break;

        // DAP4 support
    case dods_uint8_c:
        set_array_using_double_helper<dods_byte>(dest, src, src_len);
        break;
    case dods_int8_c:
        set_array_using_double_helper<dods_int8>(dest, src, src_len);
        break;
    case dods_uint64_c:
        set_array_using_double_helper<dods_uint64>(dest, src, src_len);
        break;
    case dods_int64_c:
        set_array_using_double_helper<dods_int64>(dest, src, src_len);
        break;
    default:
        throw InternalErr(__FILE__, __LINE__,
                "The argument list built by the CE parser contained an unsupported numeric type.");
    }

    // Set the read_p property.
    dest->set_read_p(true);
}

template<class T> static double *extract_double_array_helper(Array * a)
{
    assert(a);

    uint64_t length = a->length();

    vector<T> b(length);
    a->value(&b[0]);    // Extract the values of 'a' to 'b'

    double *dest = new double[length];
    for (uint64_t i = 0; i < length; ++i)
        dest[i] = (double) b[i];

    return dest;
}

/**
 * Given a pointer to an Array which holds a numeric type, extract the
 * values and return in an array of doubles. This function allocates the
 * array using 'new double[n]' so delete[] MUST be used when you are done
 * the data.
 *
 * @note Support added for DAP4.
 * @param a Extract value from this Array.
 * @return A C++/C array of doubles.
 */
double *extract_double_array(Array * a)
{
    assert(a);

    // Simple types are Byte, ..., Float64, String and Url.
    if ((a->type() == dods_array_c && !a->var()->is_simple_type()) || a->var()->type() == dods_str_c
            || a->var()->type() == dods_url_c)
        throw Error(malformed_expr, "The function requires a DAP numeric-type array argument.");

    if (!a->read_p())
        throw InternalErr(__FILE__, __LINE__, string("The Array '") + a->name() + "'does not contain values.");

    // The types of arguments that the CE Parser will build for numeric
    // constants are limited to Uint32, Int32 and Float64. See ce_expr.y.
    // Expanded to work for any numeric type so it can be used for more than
    // just arguments.
    switch (a->var()->type()) {
    case dods_byte_c:
        return extract_double_array_helper<dods_byte>(a);
    case dods_uint16_c:
        return extract_double_array_helper<dods_uint16>(a);
    case dods_int16_c:
        return extract_double_array_helper<dods_int16>(a);
    case dods_uint32_c:
        return extract_double_array_helper<dods_uint32>(a);
    case dods_int32_c:
        return extract_double_array_helper<dods_int32>(a);
    case dods_float32_c:
        return extract_double_array_helper<dods_float32>(a);
    case dods_float64_c:
        // Should not be copying these values, just read them,
        // but older code may depend on the return of this function
        // being something that should be deleted, so leave this
        // alone. jhrg 2/24/15
        return extract_double_array_helper<dods_float64>(a);

        // Support for DAP4
    case dods_uint8_c:
        return extract_double_array_helper<dods_byte>(a);
    case dods_int8_c:
        return extract_double_array_helper<dods_int8>(a);
    case dods_uint64_c:
        return extract_double_array_helper<dods_uint64>(a);
    case dods_int64_c:
        return extract_double_array_helper<dods_int64>(a);
    default:
        throw InternalErr(__FILE__, __LINE__,
                "The argument list built by the CE parser contained an unsupported numeric type.");
    }
}

// This helper function assumes 'dest' is the correct size. This should not
// be called when the Array 'a' is a Float64, since the values are already
// in a double array!
template<class T> static void extract_double_array_helper(Array * a, vector<double> &dest)
{
    assert(a);
    assert(dest.size() == a->length());

    uint64_t length = a->length();

    vector<T> b(length);
    a->value(&b[0]);    // Extract the values of 'a' to 'b'

    for (uint64_t i = 0; i < length; ++i)
        dest[i] = (double) b[i];
}

/**
 * Given a pointer to an Array which holds a numeric type, extract the
 * values and return in an array of doubles. This function allocates the
 * array using 'new double[n]' so delete[] MUST be used when you are done
 * the data.
 *
 * @note Before you call this function, you must call read on the array,
 * or load the array with values and set the read_p property.
 *
 * @note Support added for DAP4.
 * @param a Extract value from this Array.
 * @param dest Put the values in this vector. A value-result parameter.
 * @return A C++/C array of doubles.
 */
void extract_double_array(Array *a, vector<double> &dest)
{
    assert(a);

    // Simple types are Byte, ..., Float64, String and Url.
    if ((a->type() == dods_array_c && !a->var()->is_simple_type()) || a->var()->type() == dods_str_c
            || a->var()->type() == dods_url_c)
        throw Error(malformed_expr, "The function requires a DAP numeric-type array argument.");

    if (!a->read_p())
        throw InternalErr(__FILE__, __LINE__, string("The Array '") + a->name() + "' does not contain values.");

    dest.resize(a->length());

    // The types of arguments that the CE Parser will build for numeric
    // constants are limited to Uint32, Int32 and Float64. See ce_expr.y.
    // Expanded to work for any numeric type so it can be used for more than
    // just arguments.
    switch (a->var()->type()) {
    case dods_byte_c:
        return extract_double_array_helper<dods_byte>(a, dest);
    case dods_uint16_c:
        return extract_double_array_helper<dods_uint16>(a, dest);
    case dods_int16_c:
        return extract_double_array_helper<dods_int16>(a, dest);
    case dods_uint32_c:
        return extract_double_array_helper<dods_uint32>(a, dest);
    case dods_int32_c:
        return extract_double_array_helper<dods_int32>(a, dest);
    case dods_float32_c:
        return extract_double_array_helper<dods_float32>(a, dest);
    case dods_float64_c:
        return a->value(&dest[0]);      // no need to copy the values
        // return extract_double_array_helper<dods_float64>(a, dest);

        // Support for DAP4
    case dods_uint8_c:
        return extract_double_array_helper<dods_byte>(a, dest);
    case dods_int8_c:
        return extract_double_array_helper<dods_int8>(a, dest);
    case dods_uint64_c:
        return extract_double_array_helper<dods_uint64>(a, dest);
    case dods_int64_c:
        return extract_double_array_helper<dods_int64>(a, dest);
    default:
        throw InternalErr(__FILE__, __LINE__,
                "The argument list built by the CE parser contained an unsupported numeric type.");
    }
}

/** Given a BaseType pointer, extract the numeric value it contains and return
 it in a C++ double.

 @note Support for DAP4 types added.

 @param arg The BaseType pointer
 @return A C++ double
 @exception Error thrown if the referenced BaseType object does not contain
 a DAP numeric value. */
double extract_double_value(BaseType *arg)
{
    assert(arg);

    // Simple types are Byte, ..., Float64, String and Url.
    if (!arg->is_simple_type() || arg->type() == dods_str_c || arg->type() == dods_url_c)
        throw Error(malformed_expr, "The function requires a numeric-type argument.");

    if (!arg->read_p())
        throw InternalErr(__FILE__, __LINE__,
                "The Evaluator built an argument list where some constants held no values.");

    // The types of arguments that the CE Parser will build for numeric
    // constants are limited to Uint32, Int32 and Float64. See ce_expr.y.
    // Expanded to work for any numeric type so it can be used for more than
    // just arguments.
    switch (arg->type()) {
    case dods_byte_c:
        return (double) (static_cast<Byte*>(arg)->value());
    case dods_uint16_c:
        return (double) (static_cast<UInt16*>(arg)->value());
    case dods_int16_c:
        return (double) (static_cast<Int16*>(arg)->value());
    case dods_uint32_c:
        return (double) (static_cast<UInt32*>(arg)->value());
    case dods_int32_c:
        return (double) (static_cast<Int32*>(arg)->value());
    case dods_float32_c:
        return (double) (static_cast<Float32*>(arg)->value());
    case dods_float64_c:
        return static_cast<Float64*>(arg)->value();

        // Support for DAP4 types.
    case dods_uint8_c:
        return (double) (static_cast<Byte*>(arg)->value());
    case dods_int8_c:
        return (double) (static_cast<Int8*>(arg)->value());
    case dods_uint64_c:
        return (double) (static_cast<UInt64*>(arg)->value());
    case dods_int64_c:
        return (double) (static_cast<Int64*>(arg)->value());

    default:
        throw InternalErr(__FILE__, __LINE__,
                "The argument list built by the parser contained an unsupported numeric type.");
    }
}

// Remove spaces from the start of a URL and from the start of any constraint
// expression it contains. 4/7/98 jhrg

/** Removed spaces from the front of a URL and also from the front of the CE.
 This function assumes that there are no holes in both the URL and the CE.
 It will remove \e leading space, but not other spaces.

 @param name The URL to process
 @return Returns a new string object that contains the pruned URL. */
string prune_spaces(const string &name)
{
    // If the URL does not even have white space return.
    if (name.find_first_of(' ') == name.npos)
        return name;
    else {
        // Strip leading spaces from http://...
        unsigned int i = name.find_first_not_of(' ');
        string tmp_name = name.substr(i);

        // Strip leading spaces from constraint part (following `?').
        unsigned int j = tmp_name.find('?') + 1;
        i = tmp_name.find_first_not_of(' ', j);
        tmp_name.erase(j, i - j);

        return tmp_name;
    }
}

// Compare elements in a list of (BaseType *)s and return true if there are
// no duplicate elements, otherwise return false.

bool unique_names(vector<BaseType *> l, const string &var_name, const string &type_name, string &msg)
{
    // copy the identifier names to a vector
    vector<string> names(l.size());

    int nelem = 0;
    typedef std::vector<BaseType *>::const_iterator citer;
    for (citer i = l.begin(); i != l.end(); i++) {
        assert(*i);
        names[nelem++] = (*i)->name();
        DBG(cerr << "NAMES[" << nelem - 1 << "]=" << names[nelem-1] << endl);
    }

    // sort the array of names
    sort(names.begin(), names.end());

    // sort the array of names
    sort(names.begin(), names.end());

    // look for any instance of consecutive names that are ==
    for (int j = 1; j < nelem; ++j) {
        if (names[j - 1] == names[j]) {
            ostringstream oss;
            oss << "The variable `" << names[j] << "' is used more than once in " << type_name << " `" << var_name
                    << "'";
            msg = oss.str();

            return false;
        }
    }

    return true;
}

const char *
libdap_root()
{
    return LIBDAP_ROOT;
}

/** Return the version string for this package.
 @note This function has C linkage so that it can be found using autoconf
 tests.
 @return The version string. */
extern "C" const char *
libdap_version()
{
    return PACKAGE_VERSION;
}

extern "C" const char *
libdap_name()
{
    return PACKAGE_NAME;
}

/**
 * Use the system time() function to get the current time. Return a string,
 * removing the trailing newline that time() includes in its response.
 * @return A C++ string with the current system time as formatted by time()
 */
string systime()
{
    time_t TimBin;

    if (time(&TimBin) == (time_t) -1)
        return string("time() error");
    else {
        char *ctime_value = ctime(&TimBin);
        if (ctime_value) {
            string TimStr = ctime_value;
            return TimStr.substr(0, TimStr.size() - 2); // remove the \n
        }
        else
            return "Unknown";
    }
}

/**
 * Downcase the source string. This function modifies its argument.
 * @param The string to modify
 */
void downcase(string &s)
{
    for (unsigned int i = 0; i < s.length(); i++)
        s[i] = tolower(s[i]);
}

/**
 * Is the string surrounded by double quotes?
 * @param s The source string
 * @reurn True if the string is quoted, false otherwise.
 */
bool is_quoted(const string &s)
{
    return (!s.empty() && s[0] == '\"' && s[s.length() - 1] == '\"');
}

/**
 * Return a new string that is not quoted. This will return a new string
 * regardless of whether the source string is actualy quoted.
 * @param s The source string
 * @return A new string without quotes
 */
string remove_quotes(const string &s)
{
    if (is_quoted(s))
        return s.substr(1, s.length() - 2);
    else
        return s;
}

/** Get the Type enumeration value which matches the given name. */
Type get_type(const char *name)
{
    if (strcmp(name, "Byte") == 0) return dods_byte_c;

    if (strcmp(name, "Char") == 0) return dods_char_c;

    if (strcmp(name, "Int8") == 0) return dods_int8_c;

    if (strcmp(name, "UInt8") == 0) return dods_uint8_c;

    if (strcmp(name, "Int16") == 0) return dods_int16_c;

    if (strcmp(name, "UInt16") == 0) return dods_uint16_c;

    if (strcmp(name, "Int32") == 0) return dods_int32_c;

    if (strcmp(name, "UInt32") == 0) return dods_uint32_c;

    if (strcmp(name, "Int64") == 0) return dods_int64_c;

    if (strcmp(name, "UInt64") == 0) return dods_uint64_c;

    if (strcmp(name, "Float32") == 0) return dods_float32_c;

    if (strcmp(name, "Float64") == 0) return dods_float64_c;

    if (strcmp(name, "String") == 0) return dods_str_c;

    // accept both spellings; this might be confusing since URL
    // could be filtered through code and come out Url. Don't know...
    // jhrg 8/15/13
    if (strcmp(name, "Url") == 0 || strcmp(name, "URL") == 0) return dods_url_c;

    if (strcmp(name, "Enum") == 0) return dods_enum_c;

    if (strcmp(name, "Opaque") == 0) return dods_opaque_c;

    if (strcmp(name, "Array") == 0) return dods_array_c;

    if (strcmp(name, "Structure") == 0) return dods_structure_c;

    if (strcmp(name, "Sequence") == 0) return dods_sequence_c;

    if (strcmp(name, "Grid") == 0) return dods_grid_c;

    return dods_null_c;
}

/**
 * @brief Returns the type of the class instance as a string.
 * Supports all DAP2 types and not the DAP4-only types. Also
 * returns Url (DAP2) and not "URL" (DAP4) for the URL type.
 * @param t The type code
 * @return The type name in a string
 */
string D2type_name(Type t)
{
    switch (t) {
    case dods_null_c:
        return string("Null");
    case dods_byte_c:
        return string("Byte");
    case dods_int16_c:
        return string("Int16");
    case dods_uint16_c:
        return string("UInt16");
    case dods_int32_c:
        return string("Int32");
    case dods_uint32_c:
        return string("UInt32");
    case dods_float32_c:
        return string("Float32");
    case dods_float64_c:
        return string("Float64");
    case dods_str_c:
        return string("String");
    case dods_url_c:
        return string("Url");

    case dods_array_c:
        return string("Array");
    case dods_structure_c:
        return string("Structure");
    case dods_sequence_c:
        return string("Sequence");
    case dods_grid_c:
        return string("Grid");

    default:
        throw InternalErr(__FILE__, __LINE__, "Unknown type.");
    }
}

/**
 * @brief Returns the type of the class instance as a string.
 * Supports all DAP4 types and not the DAP2-only types. Also
 * returns URL (DAP4) and not "Url" (DAP2) for the URL type.
 * @param t The type code
 * @return The type name in a string
 */
string D4type_name(Type t)
{
    switch (t) {
    case dods_null_c:
        return string("Null");
    case dods_byte_c:
        return string("Byte");
    case dods_char_c:
        return string("Char");
    case dods_int8_c:
        return string("Int8");
    case dods_uint8_c:
        return string("UInt8");
    case dods_int16_c:
        return string("Int16");
    case dods_uint16_c:
        return string("UInt16");
    case dods_int32_c:
        return string("Int32");
    case dods_uint32_c:
        return string("UInt32");
    case dods_int64_c:
        return string("Int64");
    case dods_uint64_c:
        return string("UInt64");
    case dods_enum_c:
        return string("Enum");

    case dods_float32_c:
        return string("Float32");
    case dods_float64_c:
        return string("Float64");

    case dods_str_c:
        return string("String");
    case dods_url_c:
        return string("URL");

    case dods_opaque_c:
        return string("Opaque");

    case dods_array_c:
        return string("Array");

    case dods_structure_c:
        return string("Structure");
    case dods_sequence_c:
        return string("Sequence");
    case dods_group_c:
        return string("Group");

    default:
        throw InternalErr(__FILE__, __LINE__, "Unknown type.");
    }
}

/** Return the type name. This function provides backward compatibility
 * for older code that predates, and has not been ported to, DAP4. It
 * is prejudiced toward DAP4, but if no D4 type name can be found, it
 * types D2. If neither would return a type name, and InternalErr object
 * is thrown.
 *
 * @param t The DAP2/DAP4 type
 * @return A string naming the type, suitable for humans
 * @exception InternalErr If not such type can be found
 */
string type_name(Type t)
{
    try {
        return D4type_name(t);
    }
    catch (...) {
        return D2type_name(t);
    }
}

/** @brief Returns true if the instance is a numeric, string or URL
 type variable.
 @return True if the instance is a scalar numeric, String or URL variable,
 False otherwise. Arrays (even of simple types) return False.
 @see is_vector_type() */
bool is_simple_type(Type t)
{
    switch (t) {

    case dods_byte_c:
    case dods_char_c:

    case dods_int8_c:
    case dods_uint8_c:

    case dods_int16_c:
    case dods_uint16_c:
    case dods_int32_c:
    case dods_uint32_c:

    case dods_int64_c:
    case dods_uint64_c:

    case dods_float32_c:
    case dods_float64_c:
    case dods_str_c:
    case dods_url_c:
    case dods_enum_c:
    case dods_opaque_c:
        return true;

    case dods_null_c:
    case dods_array_c:
    case dods_structure_c:
    case dods_sequence_c:
    case dods_grid_c:
    case dods_group_c:
    default:
        return false;
    }
}

/** @brief Returns true if the instance is a vector (i.e., array) type
 variable.
 @return True if the instance is an Array, False otherwise. */
bool is_vector_type(Type t)
{
    switch (t) {
    case dods_null_c:
    case dods_byte_c:
    case dods_char_c:

    case dods_int8_c:
    case dods_uint8_c:

    case dods_int16_c:
    case dods_uint16_c:

    case dods_int32_c:
    case dods_uint32_c:

    case dods_int64_c:
    case dods_uint64_c:

    case dods_float32_c:
    case dods_float64_c:

    case dods_str_c:
    case dods_url_c:
    case dods_enum_c:
    case dods_opaque_c:
        return false;

    case dods_array_c:
        return true;

    case dods_structure_c:
    case dods_sequence_c:
    case dods_grid_c:
    case dods_group_c:
    default:
        return false;
    }
}

/** @brief Returns true if the instance is a constructor (i.e., Structure,
 Sequence or Grid) type variable.
 @return True if the instance is a Structure, Sequence or Grid, False
 otherwise. */
bool is_constructor_type(Type t)
{
    switch (t) {
    case dods_null_c:
    case dods_byte_c:
    case dods_char_c:

    case dods_int8_c:
    case dods_uint8_c:

    case dods_int16_c:
    case dods_uint16_c:
    case dods_int32_c:
    case dods_uint32_c:

    case dods_int64_c:
    case dods_uint64_c:

    case dods_float32_c:
    case dods_float64_c:
    case dods_str_c:
    case dods_url_c:
    case dods_enum_c:
    case dods_opaque_c:

    case dods_array_c:
        return false;

    case dods_structure_c:
    case dods_sequence_c:
    case dods_grid_c:
    case dods_group_c:
    default:
        return true;
    }
}

/**
 * Is this an integer type?
 * @return True if the type holds an integer value, false otherwise.
 */
bool is_integer_type(Type t)
{
    switch (t) {
    case dods_byte_c:
    case dods_char_c:
    case dods_int8_c:
    case dods_uint8_c:
    case dods_int16_c:
    case dods_uint16_c:
    case dods_int32_c:
    case dods_uint32_c:
    case dods_int64_c:
    case dods_uint64_c:
        return true;
    default:
        return false;
    }
}

/**
 * Does the directory exist?
 *
 * @param dir The pathname to test.
 * @return True if the directory exists, false otherwise
 */
bool dir_exists(const string &dir)
{
    struct stat buf;

    return (stat(dir.c_str(), &buf) == 0) && (buf.st_mode & S_IFDIR);
}

// Jose Garcia
void append_long_to_string(long val, int base, string &str_val)
{
    // The array digits contains 36 elements which are the
    // posible valid digits for out bases in the range
    // [2,36]
    char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    // result of val / base
    ldiv_t r;

    if (base > 36 || base < 2) {
        // no conversion if wrong base
        std::invalid_argument ex("The parameter base has an invalid value.");
        throw ex;
    }
    if (val < 0) str_val += '-';
    r = ldiv(labs(val), base);

    // output digits of val/base first
    if (r.quot > 0) append_long_to_string(r.quot, base, str_val);

    // output last digit

    str_val += digits[(int) r.rem];
}

// base defaults to 10
string long_to_string(long val, int base)
{
    string s;
    append_long_to_string(val, base, s);
    return s;
}

// Jose Garcia
void append_double_to_string(const double &num, string &str)
{
    // s having 100 characters should be enough for sprintf to do its job.
    // I want to banish all instances of sprintf. 10/5/2001 jhrg
    ostringstream oss;
    oss.precision(9);
    oss << num;
    str += oss.str();
}

string double_to_string(const double &num)
{
    string s;
    append_double_to_string(num, s);
    return s;
}

// Given a pathname, return the file at the end of the path. This is used
// when reporting errors (maybe other times, too) to keep the server from
// revealing too much about its organization when sending error responses
// back to clients. 10/11/2000 jhrg
// MT-safe. 08/05/02 jhrg

#ifdef WIN32
static const char path_sep[] =
{   "\\"
};
#else
static const char path_sep[] = { "/" };
#endif

/** Get the filename part from a path. This function can be used to return a
 string that has the directory components stripped from a path. This is
 useful when building error message strings.

 If WIN32 is defined, use '\' as the path separator, otherwise use '/' as
 the path separator.

 @return A string containing only the filename given a path. */
string path_to_filename(string path)
{
    string::size_type pos = path.rfind(path_sep);

    return (pos == string::npos) ? path : path.substr(++pos);
}

#define CHECK_BIT( tab, bit ) ( tab[ (bit)/8 ] & (1<<( (bit)%8 )) )
#define BITLISTSIZE 16 /* bytes used for [chars] in compiled expr */

/*
 * globchars() - build a bitlist to check for character group match
 */

static void globchars(const char *s, const char *e, char *b)
{
    int neg = 0;

    memset(b, '\0', BITLISTSIZE);

    if (*s == '^') neg++, s++;

    while (s < e) {
        int c;

        if (s + 2 < e && s[1] == '-') {
            for (c = s[0]; c <= s[2]; c++)
                b[c / 8] |= (1 << (c % 8));
            s += 3;
        }
        else {
            c = *s++;
            b[c / 8] |= (1 << (c % 8));
        }
    }

    if (neg) {
        int i;
        for (i = 0; i < BITLISTSIZE; i++)
            b[i] ^= 0377;
    }

    /* Don't include \0 in either $[chars] or $[^chars] */

    b[0] &= 0376;
}

/**
 * glob:  match a string against a simple pattern
 *
 * Understands the following patterns:
 *
 *  *   any number of characters
 *  ?   any single character
 *  [a-z]   any single character in the range a-z
 *  [^a-z]  any single character not in the range a-z
 *  \x  match x
 *
 * @param c The pattern
 * @param s The string
 * @return 0 on success, -1 if the pattern is exhausted but there are
 * characters remaining in the string and 1 if the pattern does not match
 */
int glob(const char *c, const char *s)
{
    if (!c || !s) return 1;

    char bitlist[BITLISTSIZE];
    int i = 0;
    for (;;) {
        ++i;
        switch (*c++) {
        case '\0':
            return *s ? -1 : 0;

        case '?':
            if (!*s++) return i/*1*/;
            break;

        case '[': {
            /* scan for matching ] */

            const char *here = c;
            do {
                if (!*c++) return i/*1*/;
            } while (here == c || *c != ']');
            c++;

            /* build character class bitlist */

            globchars(here, c, bitlist);

            if (!CHECK_BIT(bitlist, *(unsigned char * )s)) return i/*1*/;
            s++;
            break;
        }

        case '*': {
            const char *here = s;

            while (*s)
                s++;

            /* Try to match the rest of the pattern in a recursive */
            /* call.  If the match fails we'll back up chars, retrying. */

            while (s != here) {
                int r;

                /* A fast path for the last token in a pattern */

                r = *c ? glob(c, s) : *s ? -1 : 0;

                if (!r)
                    return 0;
                else if (r < 0) return i/*1*/;

                --s;
            }
            break;
        }

        case '\\':
            /* Force literal match of next char. */

            if (!*c || *s++ != *c++) return i/*1*/;
            break;

        default:
            if (*s++ != c[-1]) return i/*1*/;
            break;
        }
    }
}

/** @name Security functions */
//@{
/** @brief sanitize the size of an array.
 Test for integer overflow when dynamically allocating an array.
 @param nelem Number of elements.
 @param sz size of each element.
 @return True if the \c nelem elements of \c sz size will overflow an array. */
bool size_ok(unsigned int sz, unsigned int nelem)
{
    return (sz > 0 && nelem < UINT_MAX / sz);
}

/** @brief Does the string name a potentially valid pathname?
 Test the given pathname to verify that it is a valid name. We define this
 as: Contains only printable characters; and Is less then 256 characters.
 If \e strict is true, test that the pathname consists of only letters,
 digits, and underscore, dash and dot characters instead of the more general
 case where a pathname can be composed of any printable characters.

 @note Using this function does not guarantee that the path is valid, only
 that the path \e could be valid. The intent is foil attacks where an
 exploit is encoded in a string then passed to a library function. This code
 does not address whether the pathname references a valid resource.

 @param path The pathname to test
 @param strict Apply more restrictive tests (true by default)
 @return true if the pathname consists of legal characters and is of legal
 size, false otherwise. */
bool pathname_ok(const string &path, bool strict)
{
    if (path.length() > 255) return false;

    Regex name("[-0-9A-z_./]+");
    if (!strict) name = "[:print:]+";

    string::size_type len = path.length();
    int result = name.match(path.c_str(), len);
    // Protect against casting too big an uint to int
    // if LEN is bigger than the max int32, the second test can't work
    if (len > INT_MAX || result != static_cast<int>(len)) return false;

    return true;
}

//@}

/**
 * Get the version of the DAP library.
 * @deprecated
 */
string dap_version()
{
    return (string) "OPeNDAP DAP/" + libdap_version() + ": compiled on " + __DATE__ + ":" + __TIME__;
}

/**
 * Using the given template, open a temporary file using the given
 * ofstream object. Uses mkstemp() in a 'safe' way.
 *
 * @param f Value-result parameter
 * @param name_template The template used to name the temporary file.
 * The template has the form templateXXXXXX where the six Xs will be
 * overwritten.
 * @param suffix If present, the template is 'templateXXXXXX.suffix'
 * @return The new file's name.
 * @exception Error if there is a problem.
 */
string open_temp_fstream(ofstream &f, const string &name_template, const string &suffix /* = "" */)
{
    vector<char> name;
    copy(name_template.begin(), name_template.end(), back_inserter(name));
    if (!suffix.empty())
        copy(suffix.begin(), suffix.end(), back_inserter(name));
    name.push_back('\0');

    // Use mkstemp to make and open the temp file atomically
    int tmpfile = mkstemps(&name[0], suffix.length());
    if (tmpfile == -1)
        throw Error(internal_error, "Could not make a temporary file.");
    // Open the file using C++ ofstream; get a C++ fstream object
    f.open(&name[0]);
    // Close the file descriptor; the file stays open because of the fstream object
    close(tmpfile);
    // Now test that the fstream object is valid
    if (f.fail())
        throw Error(internal_error, "Could not make a temporary file.");

    return string(&name[0]);
}


} // namespace libdap

