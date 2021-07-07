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

#include <string>
#include <sstream>
#include <iterator>

//#define DODS_DEBUG 

#include "D4CEScanner.h"
#include "D4ConstraintEvaluator.h"
#include "d4_ce_parser.tab.hh"

#include "DMR.h"
#include "D4Group.h"
#include "D4Dimensions.h"
#include "D4Maps.h"
#include "BaseType.h"
#include "Array.h"
#include "Constructor.h"
#include "D4Sequence.h"

#include "D4RValue.h"
#include "D4FilterClause.h"

#include "escaping.h"
#include "parser.h"		// for get_ull()
#include "debug.h"

// Always define this for a production release.
#define PREVENT_XXS_VIA_CE 1
#if NDEBUG && !PREVENT_XXS_VIA_CE
#error("Never release libdap with PREVENT_XXS_VIA_CE turned off")
#endif

namespace libdap {

bool D4ConstraintEvaluator::parse(const std::string &expr)
{
    
    d_expr = expr;	// set for error messages. See the %initial-action section of .yy

    DBG(cerr << "Entering D4ConstraintEvaluator::parse: "  << endl);
    std::istringstream iss(expr);
    D4CEScanner scanner(iss);
    D4CEParser parser(scanner, *this /* driver */);

    if (trace_parsing()) {
        parser.set_debug_level(1);
        parser.set_debug_stream(std::cerr);
    }

    if(expr.empty()) 
        d_dmr->set_ce_empty(true);

    return parser.parse() == 0;
}

/**
 * print an error message. If PREVENT_XXS_VIA_CE is true (it should be), then
 * id won't be printed. The value of 'ident' is a literal that identifies the
 * parse rule to help locate the source of the error.
 * @param ident
 */
void D4ConstraintEvaluator::throw_not_found(const string &/* id */, const string &/* ident */)
{
#if PREVENT_XXS_VIA_CE
    throw Error(no_such_variable, string("The constraint expression referenced a variable that was not found in the dataset."));
#else
    throw Error(no_such_variable, d_expr + ": The variable " + id + " was not found in the dataset (" + ident + ").");
#endif
}

void D4ConstraintEvaluator::throw_not_array(const string &/* id */, const string &/* ident */)
{
#if PREVENT_XXS_VIA_CE
    throw Error(no_such_variable, string("The constraint expression referenced an Array that was not found in the dataset."));
#else
    throw Error(no_such_variable, d_expr + ": The variable '" + id + "' is not an Array variable (" + ident + ").");
#endif
}

void D4ConstraintEvaluator::search_for_and_mark_arrays(BaseType *btp)
{
    DBG(cerr << "Entering D4ConstraintEvaluator::search_for_and_mark_arrays...(" << btp->name() << ")" << endl);

    assert(btp->is_constructor_type());

    Constructor *ctor = static_cast<Constructor*>(btp);
    for (Constructor::Vars_iter i = ctor->var_begin(), e = ctor->var_end(); i != e; ++i) {
        switch ((*i)->type()) {
        case dods_array_c:
            DBG(cerr << "Found an array: " << (*i)->name() << endl);
            mark_array_variable(*i);
            break;
        case dods_structure_c:
        case dods_sequence_c:
            DBG(cerr << "Found a ctor: " << (*i)->name() << endl);
            search_for_and_mark_arrays(*i);
            break;
        default:
            break;
        }
    }
}

/**
 * When an identifier is used in a CE, is becomes part of the 'current projection,'
 * which means it is part of the set of variables to be sent back to the client. This
 * method sets a flag in the variable (send_p; pronounced 'send predicate') indicating that.
 *
 * @note This will check if the variable is an array and set it's slices accordingly
 * @param btp BaseType pointer to the variable. Must be non-null
 * @return The BaseType* to the variable; the send_p flag is set as a side effect.
 */
BaseType *
D4ConstraintEvaluator::mark_variable(BaseType *btp)
{
    assert(btp);

    DBG(cerr << "In D4ConstraintEvaluator::mark_variable... (" << btp->name() << "; " << btp->type_name() << ")" << endl);

    btp->set_send_p(true);

    if (btp->type() == dods_array_c) {
        mark_array_variable(btp);
    }

    // Test for Constructors and marks arrays they contain
    if (btp->is_constructor_type()) {
        search_for_and_mark_arrays(btp);
    }
    else if (btp->type() == dods_array_c && btp->var() && btp->var()->is_constructor_type()) {
        search_for_and_mark_arrays(btp->var());
    }

    // Now set the parent variables
    BaseType *parent = btp->get_parent();
    while (parent) {
        parent->BaseType::set_send_p(true); // Just set the parent using BaseType's impl.
        parent = parent->get_parent();
    }

    return btp;
}

static bool array_uses_shared_dimension(Array *map, D4Dimension *source_dim)
{
    for (Array::Dim_iter d = map->dim_begin(), e = map->dim_end(); d != e; ++d) {
        if (source_dim->name() == (*d).name) return true;
    }

    return false;
}

/**
 * Add an array to the current projection with slicing. Calling this method will result
 * in the array being returned with anonymous dimensions.
 *
 * @note If btp is an array that has shared dimensions and uses '[]' where a shared dimension
 * is found and if that shared dimension has been sliced, then the slice is used as the array's
 * slice for that dimension (there must be an easier way to explain that...)
 *
 * @param btp
 * @return The BaseType* to the Array variable; the send_p and slicing information is
 * set as a side effect.
 */

// Note: If a Map is not part of the current projection, do not include mention of it
// in the response DMR (CDMR)
BaseType *
D4ConstraintEvaluator::mark_array_variable(BaseType *btp)
{
    assert(btp->type() == dods_array_c);

    Array *a = static_cast<Array*>(btp);

    // If an array appears in a CE without the slicing operators ([]) we still have to
    // call add_constraint(...) for all of it's sdims for them to appear in
    // the Constrained DMR.
    if (d_indexes.empty()) {
        for (Array::Dim_iter d = a->dim_begin(), de = a->dim_end(); d != de; ++d) {
            D4Dimension *dim = a->dimension_D4dim(d);
            if (dim) {
                a->add_constraint(d, dim);
            }
        }
    }
    else {
        // Test that the indexes and dimensions match in number
        if (d_indexes.size() != a->dimensions())
            throw Error(malformed_expr, "The index constraint for '" + btp->name() + "' does not match its rank.");

        Array::Dim_iter d = a->dim_begin();
        for (vector<index>::iterator i = d_indexes.begin(), e = d_indexes.end(); i != e; ++i) {
            if ((*i).stride > (unsigned long long) (a->dimension_stop(d, false) - a->dimension_start(d, false)) + 1)
                throw Error(malformed_expr,
                    "For '" + btp->name()
                        + "', the index stride value is greater than the number of elements in the Array");
            if (!(*i).rest
                && ((*i).stop) > (unsigned long long) (a->dimension_stop(d, false) - a->dimension_start(d, false)) + 1)
                throw Error(malformed_expr,
                    "For '" + btp->name()
                        + "', the index stop value is greater than the number of elements in the Array");

            D4Dimension *dim = a->dimension_D4dim(d);

            // In a DAP4 CE, specifying '[]' as an array dimension slice has two meanings.
            // It can mean 'all the elements' of the dimension or 'apply the slicing inherited
            // from the shared dimension'. The latter might be provide 'all the elements'
            // but regardless, the Array object must record the CE correctly.

            if (dim && (*i).empty) {
                // This case corresponds to a CE that uses the '[]' notation for a
                // particular dimension - meaning, use the Shared Dimension size for
                // this dimension's 'slice'.
                a->add_constraint(d, dim);  // calls set_used_by_projected_var(true) + more
            }
            else {
                // This case corresponds to a 'local dimension slice' (See sections 8.6.2 and
                // 8.7 of the spec as of 4/12/16). When a local dimension slice is used, drop
                // the Map(s) that include that dimension. This enables people to constrain
                // an Array when some of the Array's dimensions don't use Shared Dimensions
                // but others do.

                // First apply the constraint to the Array's dimension
                //a->add_constraint(d, (*i).start, (*i).stride, (*i).rest ? -1 : (*i).stop, false);
                if ((*i).rest) {
                    a->add_constraint(d, (*i).start, (*i).stride, 0, true);
                } else {
                    a->add_constraint(d, (*i).start, (*i).stride, (*i).stop, false);
                }


                // Then, if the Array has Maps, scan those Maps for any that use dimensions
                // that match the name of this particular dimension. If any such Maps are found
                // remove them. This ensure that the Array can be constrained using the  'local
                // dimension slice' without the constrained DMR containing references to Maps
                // that don't exist (or are otherwise nonsensical).
                //
                // This code came about as a fix for problems discovered during testing of
                // local dimension slices. See https://opendap.atlassian.net/browse/HYRAX-98
                // jhrg 4/12/16
                if (!a->maps()->empty()) {
                    for (D4Maps::D4MapsIter m = a->maps()->map_begin(), e = a->maps()->map_end(); m != e; ++m) {
                        if ((*m)->array() == 0)
                            throw Error(malformed_expr,
                                "An array with Maps was found, but one of the Maps was not defined correctly.");

                        Array *map = const_cast<Array*>((*m)->array()); // Array lacks const iterator support
                        // Added a test to ensure 'dim' is not null. This could be the case if
                        // execution gets here and the index *i was not empty. jhrg 4/18/17
                        if (dim && array_uses_shared_dimension(map, dim)) {
                            D4Map *map_to_be_removed = *m;
                            a->maps()->remove_map(map_to_be_removed); // Invalidates the iterator
                            delete map_to_be_removed;   // removed from container; delete
                            break; // must leave the for loop because 'm' is now invalid
                        }
                    }
                }
            }

            ++d;
        }
    }

    d_indexes.clear();  // Clear the info so the next slice expression can be parsed.

    return btp;
}

/**
 * Add an array to the current projection with slicing. Calling this method will result
 * in the array being returned with anonymous dimensions.
 *
 * @param id
 * @return The BaseType* to the Array variable; the send_p and slicing information is
 * set as a side effect.
 */
D4Dimension *
D4ConstraintEvaluator::slice_dimension(const std::string &id, const index &i)
{
    D4Dimension *dim = dmr()->root()->find_dim(id);

    if (i.stride > dim->size())
        throw Error(malformed_expr,
            "For '" + id + "', the index stride value is greater than the size of the dimension");
    if (!i.rest && (i.stop > dim->size() - 1))
        throw Error(malformed_expr, "For '" + id + "', the index stop value is greater than the size of the dimension");

    dim->set_constraint(i.start, i.stride, i.rest ? dim->size() - 1 : i.stop);

    return dim;
}

D4ConstraintEvaluator::index D4ConstraintEvaluator::make_index(const std::string &i)
{
    unsigned long long v = get_int64(i.c_str());
    return index(v, 1, v, false, false /*empty*/, "");
}

D4ConstraintEvaluator::index D4ConstraintEvaluator::make_index(const std::string &i, const std::string &s,
    const std::string &e)
{
    return index(get_int64(i.c_str()), get_int64(s.c_str()), get_int64(e.c_str()), false, false /*empty*/, "");
}

D4ConstraintEvaluator::index D4ConstraintEvaluator::make_index(const std::string &i, unsigned long long s,
    const std::string &e)
{
    return index(get_int64(i.c_str()), s, get_int64(e.c_str()), false, false /*empty*/, "");
}

D4ConstraintEvaluator::index D4ConstraintEvaluator::make_index(const std::string &i, const std::string &s)
{
    return index(get_int64(i.c_str()), get_int64(s.c_str()), 0, true, false /*empty*/, "");
}

D4ConstraintEvaluator::index D4ConstraintEvaluator::make_index(const std::string &i, unsigned long long s)
{
    return index(get_uint64(i.c_str()), s, 0, true, false /*empty*/, "");
}

static string expr_msg(const std::string &op, const std::string &arg1, const std::string &arg2)
{
    return "(" + arg1 + " " + op + " " + arg2 + ").";
}

/**
 * @brief Return the D4FilterClause constant for an operator
 *
 * Here are the strings returned by the parser:
 *   GREATER ">"
 *   LESS_EQUAL "<="
 *   GREATER_EQUAL ">="
 *   EQUAL "=="
 *   NOT_EQUAL "!="
 *   REGEX_MATCH "~="
 *
 *   LESS_BBOX "<<"
 *   GREATER_BBOX ">>"
 *
 *   MASK "@="
 *   ND "ND"
 */
static D4FilterClause::ops get_op_code(const std::string &op)
{
    DBGN(cerr << "Entering " << __PRETTY_FUNCTION__ << endl << "op: " << op << endl);

    if (op == "<")
        return D4FilterClause::less;
    else if (op == ">")
        return D4FilterClause::greater;
    else if (op == "<=")
        return D4FilterClause::less_equal;
    else if (op == ">=")
        return D4FilterClause::greater_equal;
    else if (op == "==")
        return D4FilterClause::equal;
    else if (op == "!=")
        return D4FilterClause::not_equal;
    else if (op == "~=")
        return D4FilterClause::match;
    else
        throw Error(malformed_expr, "The opertator '" + op + "' is not supported.");
}

/**
 * @brief Add a D4FilterClause
 *
 * This method adds a filter clause to the D4Sequence that is on the top of the
 * parser's stack. If there is not a D4Sequence on the stack, an exception is
 * thrown. Similarly, if the filter clause parameters are not valid, then an
 * exception is thrown.
 *
 * Filter clause rules: One of the parameters must be a variable in a D4Sequence
 * and the other must be a constant. The operator must be one of the valid relops.
 * Note that the D4FilterClause objects use the same numerical codes as the DAP2
 * parser/evaluator.
 *
 * @note The parser will have pushed the Sequence onto the BaseType stack during
 * the parse, so variables can be looked up using the top_basetype() (which
 * must be a D4Sequence).
 *
 * @param arg1 The first argument; a D4Sequence variable or a constant.
 * @param arg2 The second argument; a D4Sequence variable or a constant.
 * @param op The infix relop
 */
void D4ConstraintEvaluator::add_filter_clause(const std::string &op, const std::string &arg1, const std::string &arg2)
{
    DBG(cerr << "Entering: " << __PRETTY_FUNCTION__ << endl);

    // Check that there really is a D4Sequence associated with this filter clause.
    D4Sequence *s = dynamic_cast<D4Sequence*>(top_basetype());
    if (!s)
        throw Error(malformed_expr,
            "When a filter expression is used, it must be bound to a Sequence variable: " + expr_msg(op, arg1, arg2));

    DBG(cerr << "s->name(): " << s->name() << endl);

    // Check that arg1 and 2 are valid
    BaseType *a1 = s->var(arg1);
    BaseType *a2 = s->var(arg2);
    DBG(cerr << "a1: " << a1 << ", a2: " << a2 << endl);

    if (a1 && a2)
        throw Error(malformed_expr,
            "One of the arguments in a filter expression must be a constant: " + expr_msg(op, arg1, arg2));
    if (!(a1 || a2))
        throw Error(malformed_expr,
            "One of the arguments in a filter expression must be a variable in a Sequence: "
                + expr_msg(op, arg1, arg2));

    // Now we know a1 XOR a2 is true
    if (a1) {
        s->clauses().add_clause(new D4FilterClause(get_op_code(op), new D4RValue(a1), D4RValueFactory(arg2)));
    }
    else {
        s->clauses().add_clause(new D4FilterClause(get_op_code(op), D4RValueFactory(arg1), new D4RValue(a2)));
    }
}

/**
 * @brief If the string has surrounding quotes, remove them.
 *
 * @param src The source string, passed by reference and modified in place
 * @return A reference to the sting parameter.
 */
string &
D4ConstraintEvaluator::remove_quotes(string &s)
{
    if (*s.begin() == '\"' && *(s.end() - 1) == '\"') {
        s.erase(s.begin());
        s.erase(s.end() - 1);
    }

    return s;
}

// This method is called from the parser (see d4_ce_parser.yy, down in the code
// section). This will be called during the call to D4CEParser::parse(), that
// is inside D4ConstraintEvaluator::parse(...)
//
// Including the value passed in for 'l' allows the CE text to leak into
// the error message, a potential XSS attack vector. jhrg 4/15/20
void D4ConstraintEvaluator::error(const libdap::location &, const std::string &m)
{
    ostringstream oss;
#if PREVENT_XXS_VIA_CE
    oss << "Constraint expression parse error: " << m << ends;
#else
    oss << l << ": " << m << ends;
#endif
    throw Error(malformed_expr, oss.str());
}

} /* namespace libdap */
