// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2005,2018 OPeNDAP, Inc.
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

#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstring>
#include <string>

#include "GNURegex.h"

#include "Array.h"
#include "Int16.h"
#include "Str.h"
#include "Structure.h"
#include "D4Dimensions.h"

#include "debug.h"
#include "GetOpt.h"

using namespace CppUnit;
using namespace std;

static bool debug = false;

namespace libdap {

class ArrayTest: public TestFixture {
private:
    Array *d_cardinal, *d_string, *d_structure;
    Int16 *d_int16;
    Str *d_str;
    Structure *d_struct;

    string svalues[4];
    char a[1024];
    
public:
    ArrayTest()
    {
        svalues[0] = "0 String";
        svalues[1] = "1 String";
        svalues[2] = "2 String";
        svalues[3] = "3 String";
    }

    ~ArrayTest()
    {
    }

    void setUp()
    {
        d_int16 = new Int16("Int16");
        DBG(cerr << "d_int16: " << d_int16 << endl);
        d_cardinal = new Array("Array_of_Int16", d_int16);
        d_cardinal->append_dim(4, "dimension");
        dods_int16 buffer[4] = { 0, 1, 2, 3 };
        d_cardinal->val2buf(buffer);
#ifdef DODS_DEBUG
        for (int i = 0; i < 4; ++i)
        cerr << "buffer[" << i << "]: " << buffer[i] << endl;
#endif

        d_str = new Str("Str");
        d_string = new Array("Array_of_String", d_str);
        d_string->append_dim(4, "dimension");
        string sbuffer[4] = { "0 String", "1 String", "2 String", "3 String" };
        d_string->val2buf(sbuffer);
#ifdef DODS_DEBUG
        for (int i = 0; i < 4; ++i)
        cerr << "sbuffer[" << i << "]: " << sbuffer[i] << endl;
#endif

        d_struct = new Structure("Structure");
        d_struct->add_var(d_int16);
        d_structure = new Array("Array_of_Strctures", d_struct);
        d_structure->append_dim(4, "dimension");
        ostringstream oss;
        for (int i = 0; i < 4; ++i) {
            oss.str("");
            oss << "field" << i;
            Int16 *n = new Int16(oss.str());
            DBG(cerr << "n " << i << ": " << n << endl);
            oss.str("");
            oss << "element" << i;
            Structure *s = new Structure(oss.str());
            s->add_var(n);
            d_structure->set_vec(i, s);
            delete n;
            n = 0;
            delete s;
            s = 0;
        }

        delete d_int16;
        d_int16 = 0;
        delete d_str;
        d_str = 0;
        delete d_struct;
        d_struct = 0;
    }

    void tearDown()
    {
        delete d_cardinal;
        delete d_string;
        delete d_structure;
    }

    bool re_match(Regex &r, const char *s)
    {
        int match_position = r.match(s, strlen(s));
        DBG(cerr << "match position: " << match_position
            << " string length: " << (int)strlen(s) << endl);
        return match_position == (int) strlen(s);
    }

    CPPUNIT_TEST_SUITE (ArrayTest);

    CPPUNIT_TEST (cons_test);
    CPPUNIT_TEST (equals_test);
    CPPUNIT_TEST (prepend_dim_test);
    CPPUNIT_TEST (prepend_dim_2_test);
    CPPUNIT_TEST (clear_dims_test);
    CPPUNIT_TEST (error_handling_test);
    CPPUNIT_TEST (error_handling_2_test);
    CPPUNIT_TEST (print_test);
    CPPUNIT_TEST (duplicate_cardinal_test);
    CPPUNIT_TEST (duplicate_string_test);
    CPPUNIT_TEST (duplicate_structure_test);

    CPPUNIT_TEST_SUITE_END();

    void cons_test()
    {
        Array a1 = Array("a", "b", d_int16, true);
        CPPUNIT_ASSERT(a1.name() == "a");
    }

    void equals_test()
    {
        Array a1 = Array("a", d_int16);
        a1 = *d_cardinal;
        Array::Dim_iter i = a1.dim_begin();        
        CPPUNIT_ASSERT(a1.dimension_size(i) == 4);
        a1 = a1;
        i = a1.dim_begin();        
        CPPUNIT_ASSERT(a1.dimension_size(i) == 4);
    }

    void prepend_dim_test()
    {
        Array a1 = Array("a", d_int16);
        Array::Dim_iter i = a1.dim_begin();
        CPPUNIT_ASSERT(a1.dimension_size(i) == 0);
        a1.prepend_dim(2, "dim_a");
        i = a1.dim_begin();
        CPPUNIT_ASSERT(a1.dimension_size(i) == 2);
        a1.prepend_dim(3, "dim_b");
    }

    void prepend_dim_2_test()
    {
        Array a1 = Array("a", d_int16);
        string expected_name[2] = {"dim_b", "myDim"};
        int j = 0;
        D4Dimensions *dims = new D4Dimensions();
        D4Dimension *d = new D4Dimension("dim_b", 2, dims);
        a1.prepend_dim(2, "dim_a");
        a1.prepend_dim(d);
        a1.rename_dim("dim_a", "myDim");
        for (Array::Dim_iter i = a1.dim_begin(); i != a1.dim_end(); i++, j++) {
            CPPUNIT_ASSERT(a1.dimension_name(i) == expected_name[j]);
        }
        delete dims;
        delete d;
    }

    void clear_dims_test()
    {
        Array a1 = Array("a", d_int16);
        a1.prepend_dim(2, "dim_a");
        a1.prepend_dim(3, "dim_b");
        a1.clear_all_dims();
        Array::Dim_iter i = a1.dim_begin();
        CPPUNIT_ASSERT(a1.dimension_size(i) == 0);
    }

    void error_handling_test()
    {
        Array a1 = Array("a", d_int16);
        Array::Dim_iter i = a1.dim_begin();
        string msg;
        CPPUNIT_ASSERT_THROW(a1.dimension_name(i), InternalErr);
        CPPUNIT_ASSERT(!a1.check_semantics(msg));
    }

    void error_handling_2_test()
    {
        Array a1 = Array("a", d_int16);
        a1.append_dim(2, "dim_a");
        Array::Dim_iter i = a1.dim_begin();
        CPPUNIT_ASSERT(a1.dimension_size(i) == 2);
        CPPUNIT_ASSERT_THROW(a1.add_constraint(i, 2, 1, 1, false), Error);
        CPPUNIT_ASSERT_THROW(a1.add_constraint(i, 0, 1, 2, false), Error);
        CPPUNIT_ASSERT_THROW(a1.add_constraint(i, 0, 3, 1, false), Error);
    }

    void print_test()
    {
        // Array a1 = Array("a", d_int16);
        // FILE *fp;
        // a1.append_dim(2, "dim_a");
        // CPPUNIT_ASSERT(fp = fopen("ArrayTest.output", "w"));
        // a1.print_xml(fp, " ", true);
        // fclose(fp);
        // ifstream ifs("ArrayTest.output");
        // while(!ifs.eof())
        //     ifs >> a;
        // ifs.close();
        // cout<<a;
//        CPPUNIT_ASSERT(!strcmp(a, "22;"));
    }

    void duplicate_structure_test()
    {
        Array::Dim_iter i = d_structure->dim_begin();
        CPPUNIT_ASSERT(d_structure->dimension_size(i) == 4);
#ifdef DODS_DEBUG
        for (int i = 0; i < 4; ++i) {
            Structure *s = dynamic_cast<Structure*>(d_structure->var(i));
            DBG(cerr << "s: " << s << endl);
            if (s)
            s->print_decl(cerr);
        }
#endif

        Array *a = new Array(*d_structure);
        // a = *d_structure; I test operator= in duplicate_cardinal_test().
        i = a->dim_begin();
        CPPUNIT_ASSERT(a->dimension_size(i) == 4);
        for (int i = 0; i < 4; ++i) {
            // The point of this test is to ensure that the const ctor
            // performs a deep copy; first test to make sure the pointers
            // to BaseType instnaces are different in the two objects.
            Structure *src = dynamic_cast<Structure*>(d_structure->var(i));
            Structure *dest = dynamic_cast<Structure*>(a->var(i));
            CPPUNIT_ASSERT(src != dest);

            // However, for the deep copy to be correct, the two arrays must
            // have equivalent elements. We know there's only one field...
            CPPUNIT_ASSERT(src->type() == dods_structure_c && dest->type() == dods_structure_c);
            Constructor::Vars_iter s = src->var_begin();
            Constructor::Vars_iter d = dest->var_begin();
            CPPUNIT_ASSERT((*s)->type() == dods_int16_c && (*d)->type() == dods_int16_c);
            CPPUNIT_ASSERT((*s)->name() == (*d)->name());
        }
        delete a;
        a = 0;
    }

    void duplicate_string_test()
    {
        Array::Dim_iter i = d_string->dim_begin();
        CPPUNIT_ASSERT(d_string->dimension_size(i) == 4);
        string *s = new string[4];
        d_string->buf2val((void**) &s);
        for (int i = 0; i < 4; ++i) {
            CPPUNIT_ASSERT(s[i] == svalues[i]);
            DBG(cerr << "s[" << i << "]: " << s[i] << endl);
        }

        Array a = *d_string;
        i = a.dim_begin();
        CPPUNIT_ASSERT(a.dimension_size(i) == 4);

        string *s2 = new string[4];
        d_string->buf2val((void**) &s2);
        for (int i = 0; i < 4; ++i) {
            CPPUNIT_ASSERT(s2[i] == svalues[i]);
            DBG(cerr << "s2[" << i << "]: " << s2[i] << endl);
        }

        delete[] s;
        s = 0;
        delete[] s2;
        s2 = 0;
    }

    void duplicate_cardinal_test()
    {
        Array::Dim_iter i = d_cardinal->dim_begin();
        CPPUNIT_ASSERT(d_cardinal->dimension_size(i) == 4);
        dods_int16 *b = new dods_int16[4];
        d_cardinal->buf2val((void**) &b);
        for (int i = 0; i < 4; ++i) {
            CPPUNIT_ASSERT(b[i] == i);
            DBG(cerr << "b[" << i << "]: " << b[i] << endl);
        }
        delete[] b;
        b = 0;

        Array a = *d_cardinal;
        i = a.dim_begin();
        CPPUNIT_ASSERT(a.dimension_size(i) == 4);

        dods_int16 *b2 = new dods_int16[4];
        d_cardinal->buf2val((void**) &b2);
        for (int i = 0; i < 4; ++i) {
            CPPUNIT_ASSERT(b2[i] == i);
            DBG(cerr << "b2[" << i << "]: " << b2[i] << endl);
        }
        delete[] b2;
        b2 = 0;
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION (ArrayTest);

} // namespace libdap

int main(int argc, char *argv[])
{
    GetOpt getopt(argc, argv, "dh");
    int option_char;

    while ((option_char = getopt()) != -1)
        switch (option_char) {
        case 'd':
            debug = 1;  // debug is a static global
            break;

        case 'h': {     // help - show test names
            cerr << "Usage: ArrayTest has the following tests:" << endl;
            const std::vector<Test*> &tests = libdap::ArrayTest::suite()->getTests();
            unsigned int prefix_len = libdap::ArrayTest::suite()->getName().append("::").length();
            for (std::vector<Test*>::const_iterator i = tests.begin(), e = tests.end(); i != e; ++i) {
                cerr << (*i)->getName().replace(0, prefix_len, "") << endl;
            }
            break;
        }

        default:
            break;
        }

    CppUnit::TextTestRunner runner;
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());

    bool wasSuccessful = true;
    string test = "";
    int i = getopt.optind;
    if (i == argc) {
        // run them all
        wasSuccessful = runner.run("");
    }
    else {
        for (; i < argc; ++i) {
            if (debug) cerr << "Running " << argv[i] << endl;
            test = libdap::ArrayTest::suite()->getName().append("::").append(argv[i]);
            wasSuccessful = wasSuccessful && runner.run(test);
        }
    }

    return wasSuccessful ? 0 : 1;
}
