// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2014 OPeNDAP, Inc.
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

#include "config.h"

#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>

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

#include "DDS.h"
#include "DMR.h"
#include "XMLWriter.h"
#include "D4BaseTypeFactory.h"
#include "D4ParserSax2.h"

#include "GNURegex.h"
#include "GetOpt.h"
#include "util.h"
#include "debug.h"

#include "testFile.h"
#include "test_config.h"

static bool debug = false;
static bool debug2 = false;

#undef DBG
#define DBG(x) do { if (debug) {x;} } while(false)
#undef DBG2
#define DBG2(x) do { if (debug2) {x;} } while(false)

using namespace CppUnit;
using namespace std;
using namespace libdap;

static string THE_TESTS_DIR("/dmr-rt-testsuite/");

class DmrRoundTripTest: public TestFixture {
private:

public:
    DmrRoundTripTest()
    {
    }
    ~DmrRoundTripTest()
    {
    }

    void setUp()
    {
    }

    void tearDown()
    {
    }

    bool re_match(Regex &r, const string &s)
    {
        int match = r.match(s.c_str(), s.length());
        DBG(cerr << "Match: " << match << " should be: " << s.length() << endl);
        return match == static_cast<int>(s.length());
    }

    /**
     * Given the name of a DDS and optional DAS file, build a DMR using the
     * hackery known as transform_to_dap4 and the new DMR ctor.
     *
     * @param dds_file
     * @param attr
     * @return A pointer to the new DMR; caller must delete
     */
    DMR *build_dmr(const string &dds_file, const string &das_file = "")
    {
        DBG(cerr << __func__ << "() - BEGIN" << endl);
        DBG(cerr << __func__ << "() - dds_file: " << dds_file << endl);
        DBG(cerr << __func__ << "() - das_file: " << das_file << endl);

        try {
            string prefix = string(TEST_SRC_DIR) + THE_TESTS_DIR;

            BaseTypeFactory factory;
            DDS dds(&factory, dds_file);
            dds.parse(prefix + dds_file);
            DBG2(cerr << "SOURCE DDS: " << prefix + dds_file << endl; dds.print(cerr));

            if (!das_file.empty()) {
                DAS das;
                das.parse(prefix + das_file);
                dds.transfer_attributes(&das);
                DBG2(cerr << "SOURCE DAS: " << prefix + das_file << endl; das.print(cerr));

                DBG2(cerr << "dds.print_das(): " << endl; dds.print_das(cerr));
            }

            D4BaseTypeFactory d4_factory;
            DBG(cerr << __func__ << "() - END" << endl);
            return new DMR(&d4_factory, dds);
        }
        catch (Error &e) {
            CPPUNIT_FAIL(string("Caught Error: ") + e.get_error_message());
        }

        return 0;
    }

    void test_roundtrip_template(const string &dds_file, const string &dmr_baseline, const string &das_file = "")
    // bool expected_fail = false)
    {
        DBG(cerr << __func__ << "() - BEGIN" << endl);

        DMR *dmr = 0;
        try {
            dmr = build_dmr(dds_file, das_file);
            XMLWriter xml;
            dmr->print_dap4(xml);

            string prefix = string(TEST_SRC_DIR) + THE_TESTS_DIR;
            string result_dmr(xml.get_doc());
            string baseline_dmr = read_test_baseline(prefix + dmr_baseline);
            DBG(
                cerr << "BASELINE DMR(" << baseline_dmr.size() << " chars): " << prefix + dmr_baseline << endl
                    << baseline_dmr << endl);
            DBG(cerr << "RESULT DMR(" << result_dmr.size() << " chars): " << endl << result_dmr << endl);

            CPPUNIT_ASSERT(result_dmr == baseline_dmr);

            DDS *dds = dmr->getDDS(true);
            std::ostringstream result_dds;
            dds->print(result_dds);

            string source_dds = read_test_baseline(prefix + dds_file);
            DBG(
                cerr << "SOURCE DDS(" << source_dds.size() << " chars): " << prefix + dds_file << endl << source_dds
                    << endl);

            DBG(cerr << "RESULT DDS(" << result_dds.str().size() << " chars): " << endl << result_dds.str() << endl);
            CPPUNIT_ASSERT(result_dds.str() == source_dds);

            if (!das_file.empty()) {
                std::ostringstream result_das;
                dds->print_das(result_das);

                string source_das = read_test_baseline(prefix + das_file);
                DBG(
                    cerr << "SOURCE DAS(" << source_das.size() << " chars): " << prefix + das_file << endl << source_das
                        << endl);

                DBG(
                    cerr << "RESULT DAS(" << result_das.str().size() << " chars): " << endl << result_das.str()
                        << endl);
                CPPUNIT_ASSERT(result_das.str() == source_das);

            }
            delete dmr;
            delete dds;
        }
        catch (Error &e) {
            delete dmr;
            CPPUNIT_FAIL(string("Caught Error: ") + e.get_error_message());
        }
        DBG(cerr << __func__ << "() - END" << endl);
    }

#if 0
    void i_am_broken(string name)
    {
        cerr << endl;
        cerr << "###################################################################" << endl;
        cerr << "  THE CRUCIAL TEST: '" << name << "' IS BROKEN AND HAS BEEN DISABLED." << endl;
        cerr << "  Please enable the test '" << name << "', fix it, and check it in." << endl;
    }
#endif

    void test_dds_to_dmr_to_dds_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_roundtrip_template("attr_test_00.dds", "attr_test_00.dmr", "attr_test_00.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void test_dds_to_dmr_to_dds_2()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_roundtrip_template("test.1", "test.1.full.dmr", "test.1.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void test_dds_to_dmr_to_dds_3()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_roundtrip_template("fnoc1.nc.dds", "fnoc1.nc.dmr.xml", "fnoc1.nc.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void test_dds_to_dmr_to_dds_4()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_roundtrip_template("3B42.980909.5.HDF.dds", "3B42.980909.5.HDF.full.dmr", "3B42.980909.5.HDF.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void test_dds_to_dmr_to_dds_5()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_roundtrip_template("S2000415.HDF.dds", "S2000415.HDF.full.dmr", "S2000415.HDF.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void test_grid_rt_01()
    {
        DBG(
            cerr << endl << __func__ << "() - BEGIN: " << "Testing simple Grid->D4Array->Grid with 'correct' DAS."
                << endl);
        //       i_am_broken(__func__);
        test_roundtrip_template("attr_test_01.dds", "attr_test_01.dmr", "attr_test_01.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void test_grid_rt_02()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN: " << "Testing Grid->D4Array->Grid with 'correct' DAS." << endl);
#if 0
        i_am_broken(__func__);
#endif
        test_roundtrip_template("coads_climatology.nc.dds", "coads_climatology.nc.full.dmr",
            "coads_climatology.nc.correct.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void test_grid_rt_03()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN: " << "Testing Grid->D4Array->Grid with flat DAS." << endl);
#if 0
        i_am_broken(__func__);
#endif
        test_roundtrip_template("coads_climatology.nc.dds", "coads_climatology.nc.full.dmr",
            "coads_climatology.nc.flat.das");
        DBG(cerr << __func__ << "() - END" << endl);
    }

CPPUNIT_TEST_SUITE (DmrRoundTripTest);

    CPPUNIT_TEST(test_dds_to_dmr_to_dds_1);
    CPPUNIT_TEST(test_dds_to_dmr_to_dds_2);
    CPPUNIT_TEST(test_dds_to_dmr_to_dds_3);
    CPPUNIT_TEST(test_dds_to_dmr_to_dds_4);
    CPPUNIT_TEST(test_dds_to_dmr_to_dds_5);
    CPPUNIT_TEST(test_grid_rt_01);
#if 0
    CPPUNIT_TEST(test_grid_rt_02);
    CPPUNIT_TEST(test_grid_rt_03);
#else
    CPPUNIT_TEST_FAIL(test_grid_rt_02);
    CPPUNIT_TEST_FAIL(test_grid_rt_03);
#endif

    CPPUNIT_TEST_SUITE_END()
    ;

};

CPPUNIT_TEST_SUITE_REGISTRATION(DmrRoundTripTest);

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
            cerr << "Usage: DmrRoundTripTest has the following tests:" << endl;
            const std::vector<Test*> &tests = DmrRoundTripTest::suite()->getTests();
            unsigned int prefix_len = DmrRoundTripTest::suite()->getName().append("::").length();
            for (std::vector<Test*>::const_iterator i = tests.begin(), e = tests.end(); i != e; ++i) {
                cerr << (*i)->getName().replace(0, prefix_len, "") << endl;
            }
            return 1;
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
            test = DmrRoundTripTest::suite()->getName().append("::").append(argv[i]);
            wasSuccessful = wasSuccessful && runner.run(test);
        }
    }

    return wasSuccessful ? 0 : 1;
}

