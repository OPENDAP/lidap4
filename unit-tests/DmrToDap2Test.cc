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
#include <memory>

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
#include "mime_util.h"
#include "debug.h"

#include "testFile.h"
#include "test_config.h"

static bool debug = false;
static bool mo_debug = false;

#undef DBG
#undef DBG2
#define DBG(x) do { if (debug) {x;} } while(false)
#define DBG2(x) do { if (mo_debug) {x;} } while(false)

using namespace CppUnit;
using namespace std;
using namespace libdap;

static string THE_TESTS_DIR("/dmr-to-dap2-testsuite/");

class DmrToDap2Test: public TestFixture {
private:

public:
    DmrToDap2Test()
    {
    }
    ~DmrToDap2Test()
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
        DBG2(cerr << "Match: " << match << " should be: " << s.length() << endl);
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
    DMR *build_dmr(const string &dmr_file)
    {
        DBG2(cerr << __func__ << "() - BEGIN" << endl);
        DBG2(cerr << __func__ << "() - dmr_file: " << dmr_file << endl);

        try {
            DMR *dmr = new DMR();
            dmr->set_filename(dmr_file);
            dmr->set_name(name_path(dmr_file));
            D4BaseTypeFactory BaseFactory;   // Use the factory for this handler's types
            dmr->set_factory(&BaseFactory);
            D4ParserSax2 parser;
            ifstream in(dmr_file.c_str(), ios::in);
            parser.intern(in, dmr, mo_debug);
            dmr->set_factory(0);
            DBG2(cerr << __func__ << "() - END" << endl);
            return dmr;
        }
        catch (Error &e) {
            CPPUNIT_FAIL(string("Caught Error: ") + e.get_error_message());
        }
        catch (...) {
            CPPUNIT_FAIL(string("CPPUNIT FAIL: OUCH! Caught unknown exception! "));
        }

        return 0;
    }

    void test_template(const string &test_base_name)
    {
        string prefix = string(TEST_SRC_DIR) + THE_TESTS_DIR;

        string dmr_file = prefix + test_base_name + ".dmr";
        string dds_file = prefix + test_base_name + ".dds";
        string das_file = prefix + test_base_name + ".das";

        DBG(cerr << __func__ << "() - BEGIN (test_base: " << test_base_name << ")" << endl);

        try {
            auto_ptr<DMR> dmr(build_dmr(dmr_file));
            CPPUNIT_ASSERT(dmr.get() != 0);
            XMLWriter xml;
            dmr->print_dap4(xml);
            string result_dmr(xml.get_doc());
            string baseline_dmr = read_test_baseline(dmr_file);

            DBG2(cerr << "BASELINE DMR(" << baseline_dmr.size() << " chars): " << dmr_file << endl << baseline_dmr << endl);
            DBG2(cerr << "RESULT DMR(" << result_dmr.size() << " chars): " << endl << result_dmr << endl);

            CPPUNIT_ASSERT(result_dmr == baseline_dmr);

            auto_ptr<DDS> dds(dmr->getDDS(true));
            ostringstream result_dds;
            dds->print(result_dds);
            string baseline_dds = read_test_baseline(dds_file);

            DBG2(cerr << "BASELINE DDS(" << baseline_dds.size() << " chars): " << dds_file << endl << baseline_dds << endl);
            DBG2(cerr << "RESULT DDS(" << result_dds.str().size() << " chars): " << endl << result_dds.str() << endl);

            CPPUNIT_ASSERT(result_dds.str() == baseline_dds);

            ostringstream result_das;
            dds->print_das(result_das);
            string source_das = read_test_baseline(das_file);

            DBG2(cerr << "BASELINE DAS(" << source_das.size() << " chars): " << das_file << endl << source_das << endl);
            DBG2(cerr << "RESULT DAS(" << result_das.str().size() << " chars): " << endl << result_das.str() << endl);

            CPPUNIT_ASSERT(result_das.str() == source_das);
        }
        catch (Error &e) {
            CPPUNIT_FAIL(string("Caught Error: ") + e.get_error_message());
        }
        catch (CPPUNIT_NS::Exception &e) {
            CPPUNIT_FAIL(string("CPPUNIT FAIL: ") + e.message().details());
        }
        catch (...) {
            CPPUNIT_FAIL(string("CPPUNIT FAIL: OUCH! Caught unknown exception! "));
        }

        DBG(cerr << __func__ << "() - END" << endl);
    }

    // Test a DDS with simple scalar types and no attributes
    void dmr_to_dap2_01()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("test.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    // Test a DDS with simple scalar types and no attributes
    void basic_dmr_to_dap2_0_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_0.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void basic_dmr_to_dap2_0_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_0.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void basic_dmr_to_dap2_1_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_1.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void basic_dmr_to_dap2_2_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_2.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void basic_dmr_to_dap2_2_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_2.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void basic_dmr_to_dap2_2_2()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_2.2");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void basic_dmr_to_dap2_2_3()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_2.3");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void basic_dmr_to_dap2_2_4()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_2.4");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void basic_dmr_to_dap2_2_5()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_2.5");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void basic_dmr_to_dap2_3_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("DMR_3.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_0_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_0.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_0_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_0.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_1_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_1.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_1_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_1.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_1_2()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_1.2");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_1_3()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_1.3");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_1_4()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_1.4");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_1_5()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_1.5");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_2_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_2.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_2_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_2.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_3_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_3.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_3_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_3.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_4_0()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_4.0");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void enum_dmr_to_dap2_4_1()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_4.1");
        DBG(cerr << __func__ << "() - END" << endl);
    }
    void enum_dmr_to_dap2_4_2()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("EnumDMR_4.2");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void dmr_to_grid_01()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("coads_climatology.nc");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void dmr_to_grid_02()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("sst.mnmean.nc.gz");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void dmr_to_grid_03()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("sresa1b_ncar_ccsm3_0_run1_200001.nc");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void dmr_to_grid_04()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("tos_O1_2001-2002.nc");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void dmr_to_grid_05()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("cami_0000-09-01_64x128_L26_c030918.nc");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    void big_airs_metadata()
    {
        DBG(cerr << endl << __func__ << "() - BEGIN" << endl);
        test_template("big_airs_metadata");
        DBG(cerr << __func__ << "() - END" << endl);
    }

    CPPUNIT_TEST_SUITE (DmrToDap2Test);

    // good (as in should be working) tests
    CPPUNIT_TEST(dmr_to_dap2_01);
    CPPUNIT_TEST(basic_dmr_to_dap2_0_0);
    CPPUNIT_TEST(basic_dmr_to_dap2_0_1);
    CPPUNIT_TEST(basic_dmr_to_dap2_2_0);
    CPPUNIT_TEST(basic_dmr_to_dap2_2_1);
    CPPUNIT_TEST(basic_dmr_to_dap2_2_2);
    CPPUNIT_TEST(basic_dmr_to_dap2_2_4);
    CPPUNIT_TEST(basic_dmr_to_dap2_2_3);
    CPPUNIT_TEST(basic_dmr_to_dap2_2_5); // Drops Variables

    CPPUNIT_TEST(enum_dmr_to_dap2_0_0);
    CPPUNIT_TEST(enum_dmr_to_dap2_0_1);
    CPPUNIT_TEST(enum_dmr_to_dap2_1_0);
    CPPUNIT_TEST(enum_dmr_to_dap2_1_1);
    CPPUNIT_TEST(enum_dmr_to_dap2_1_2);
    CPPUNIT_TEST(enum_dmr_to_dap2_1_3);
    CPPUNIT_TEST(enum_dmr_to_dap2_1_4);
    CPPUNIT_TEST(enum_dmr_to_dap2_2_0); // Drops Variables
    CPPUNIT_TEST(enum_dmr_to_dap2_2_1); // Drops Variables
    CPPUNIT_TEST(enum_dmr_to_dap2_3_0); // Drops Variables
    CPPUNIT_TEST(enum_dmr_to_dap2_3_1); // Drops Variables

    CPPUNIT_TEST(enum_dmr_to_dap2_4_0); // Drops Groups
    CPPUNIT_TEST(enum_dmr_to_dap2_4_1); // Drops Groups
    CPPUNIT_TEST(enum_dmr_to_dap2_4_1);
    CPPUNIT_TEST(enum_dmr_to_dap2_4_2);

    CPPUNIT_TEST(dmr_to_grid_01);
    CPPUNIT_TEST(dmr_to_grid_02);
    CPPUNIT_TEST(dmr_to_grid_03);
    CPPUNIT_TEST(dmr_to_grid_04);
    CPPUNIT_TEST(dmr_to_grid_05);

    // bad tests, here then is the woodshed of Testville.

    CPPUNIT_TEST_FAIL(big_airs_metadata); // Expect this test to fail. jhrg 6/17/19 HK-403
    CPPUNIT_TEST_FAIL(enum_dmr_to_dap2_1_5); // Broken: Parser issue with look-ahead

    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DmrToDap2Test);

int main(int argc, char *argv[])
{
    GetOpt getopt(argc, argv, "dDh");
    int option_char;

    while ((option_char = getopt()) != -1)
        switch (option_char) {
        case 'd':
            debug = 1;  // debug is a static global
            break;

        case 'D':
            mo_debug = 1;  // debug is a static global
            break;

        case 'h': {     // help - show test names
            cerr << "Usage: DmrToDap2Test has the following tests:" << endl;
            const std::vector<Test*> &tests = DmrToDap2Test::suite()->getTests();
            unsigned int prefix_len = DmrToDap2Test::suite()->getName().append("::").length();
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
            test = DmrToDap2Test::suite()->getName().append("::").append(argv[i]);
            wasSuccessful = wasSuccessful && runner.run(test);
        }
    }

    return wasSuccessful ? 0 : 1;
}

