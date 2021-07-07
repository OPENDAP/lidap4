#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/CompilerOutputter.h>

#include <iostream>
#include "TestArray.h"
#include "TestInt16.h"
#include "TestTypeFactory.h"

#include "util.h"
#include "debug.h"

#include "GetOpt.h"

using std::cerr;
using std::endl;

using namespace CppUnit;

int test_variable_sleep_interval = 0; // Used in Test* classes for testing
// timeouts.

static bool debug = false;

class arrayT: public CppUnit::TestFixture {

CPPUNIT_TEST_SUITE (arrayT);
    CPPUNIT_TEST(arrayT_test);CPPUNIT_TEST_SUITE_END( )
    ;

private:
    /* TEST PRIVATE DATA */
    TestTypeFactory *factory;

public:
    void setUp()
    {
        factory = new TestTypeFactory;
    }

    void tearDown()
    {
        delete factory;
        factory = 0;
    }

    void arrayT_test()
    {
        BaseType *bt = factory->NewInt16();

        TestArray ar("My Array", bt);

        int l = ar.length();
        CPPUNIT_ASSERT(l == 0);

        try {
            int w = ar.width(true);
            DBG(cerr << "w = " << w << endl);DBG(cerr << "(int)bt->width() " << (int)bt->width() << endl);DBG(cerr << "L " << l << endl);
            CPPUNIT_ASSERT(w == (l * (int ) bt->width()));
        }
        catch (InternalErr &e) {
            CPPUNIT_FAIL("Unable to retrieve width");
        }

        ar.append_dim(4, "dim1");

        l = ar.length();
        CPPUNIT_ASSERT(l == 4);

        try {
            int w = ar.width();
            CPPUNIT_ASSERT(w == (l * (int ) bt->width()));
        }
        catch (InternalErr &e) {
            CPPUNIT_FAIL("Unable to retrieve width");
        }

        ar.append_dim(3, "dim2");

        l = ar.length();
        CPPUNIT_ASSERT(l == 12);

        try {
            int w = ar.width();
            CPPUNIT_ASSERT(w == (l * (int ) bt->width()));
        }
        catch (InternalErr &e) {
            CPPUNIT_FAIL("Unable to retrieve width");
        }

        ar.append_dim(2, "dim3");

        l = ar.length();
        CPPUNIT_ASSERT(l == 24);

        try {
            int w = ar.width();
            CPPUNIT_ASSERT(w == (l * (int ) bt->width()));
        }
        catch (InternalErr &e) {
            CPPUNIT_FAIL("Unable to retrieve width");
        }

        vector<string> dims;
        typedef vector<string>::const_iterator citer;
        dims.push_back("dim1");
        dims.push_back("dim2");
        dims.push_back("dim3");

        vector<int> dimsize;
        typedef vector<int>::const_iterator dsiter;
        dimsize.push_back(4);
        dimsize.push_back(3);
        dimsize.push_back(2);

        citer i = dims.begin();
        dsiter d = dimsize.begin();
        Array::Dim_iter diter = ar.dim_begin();
        i = dims.begin();
        d = dimsize.begin();
        for (; diter != ar.dim_end() && i != dims.end(); diter++, i++, d++) {
            CPPUNIT_ASSERT(ar.dimension_name(diter) == (*i));
            if (ar.dimension_name(diter) == (*i)) {
                CPPUNIT_ASSERT(ar.dimension_size(diter) == (*d));
                CPPUNIT_ASSERT(ar.dimension_start(diter) == 0);
                CPPUNIT_ASSERT(ar.dimension_stop(diter) == (*d) - 1);
                CPPUNIT_ASSERT(ar.dimension_stride(diter) == 1);
            }
        }
        if (diter != ar.dim_end() && i == dims.end()) {
            CPPUNIT_FAIL("too many dimensions");
        }
        else if (diter == ar.dim_end() && i != dims.end()) {
            CPPUNIT_FAIL("not enough dimensions");
        }

        unsigned int numdims = ar.dimensions();
        CPPUNIT_ASSERT(numdims == 3);

        diter = ar.dim_begin();
        ar.add_constraint(diter, 0, 2, 3, false);

        l = ar.length();
        CPPUNIT_ASSERT(l == 12);

        diter = ar.dim_begin();
        diter++;
        ar.add_constraint(diter, 0, 2, 2, false);

        l = ar.length();
        CPPUNIT_ASSERT(l == 8);

        ar.reset_constraint();

        l = ar.length();
        CPPUNIT_ASSERT(l == 24);

        ar.rename_dim("dim3", "myDim");

        bool is_read = ar.read();
        CPPUNIT_ASSERT(is_read == true);

        cout << ar << endl;

        delete bt;
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(arrayT);

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
            cerr << "Usage: arrayT has the following tests:" << endl;
            const std::vector<Test*> &tests = arrayT::suite()->getTests();
            unsigned int prefix_len = arrayT::suite()->getName().append("::").length();
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
            test = arrayT::suite()->getName().append("::").append(argv[i]);
            wasSuccessful = wasSuccessful && runner.run(test);
        }
    }

    return wasSuccessful ? 0 : 1;
}
