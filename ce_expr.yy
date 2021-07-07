// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2002,2003,2020 OPeNDAP, Inc.
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

// (c) COPYRIGHT URI/MIT 1995-1999
// Please read the full copyright statement in the file COPYRIGHT_URI.
//
// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

/*
 This is the parser for the DODS constraint expression grammar. The parser
 calls various `helper' functions defined by the DAP classes which either
 implement the operations (in the case of relational ops) or store
 information (in the case of selection operations).

 jhrg 9/5/95
 */

%code requires {

#include "config.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <stack>
#include <memory>

// #define DODS_DEBUG

#include "debug.h"
#include "escaping.h"

#include "DDS.h"
#include "ConstraintEvaluator.h"

#include "BaseType.h"

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

#include "Error.h"

#include "util.h"
#include "parser.h"
#include "ce_parser.h"
#include "expr.h"
#include "RValue.h"
using std::cerr;
using std::endl;
using namespace libdap ;

#define EVALUATOR(arg) (static_cast<ce_parser_arg*>(arg)->get_eval())
#define DDS(arg) (static_cast<ce_parser_arg*>(arg)->get_dds())

#define YYERROR_VERBOSE 0

int ce_exprlex(void);           /* the scanner; see expr.lex */

// NB: never pass a variable name or other string from the CE to these functions.
// Only string literals are allowed. This is to prevent information in the CE that
// could be an attack from being transferred to the error response and then
// rendered/run by a browser (an XSS attack). jhrg 4/14/20
void ce_exprerror(const string &s);
// Some automatic invocations of yyerror pass ce_parser_arg. It is ignored in this
// function, however. jhrg 4/14/20
void ce_exprerror(ce_parser_arg *arg, const string &s);
void no_such_ident(const string &thing);

void no_such_func();

// dim_slice and slices are defined in expr.h
dim_slice *make_array_slice(value &v1, value &v2, value &v3);
dim_slice *make_array_slice(value &v1, value &v2);
dim_slice *make_array_slice(value &v1);

slices *make_array_slices(dim_slice *ds);
slices *append_array_slices(slices *s, dim_slice *ds);

void delete_array_slices(slices *s);
bool bracket_projection(DDS &table, const char *name, slices *s);

void process_sequence_slice(BaseType *variable, slices *s);

void process_array_slices(BaseType *variable, slices *s);
void process_grid_slices(BaseType *variable, slices *s);
void process_sequence_slices(BaseType *variable, slices *s);

/* Replace these with method calls. jhrg 8/31/06 */
bool is_array_t(BaseType *variable);
bool is_grid_t(BaseType *variable);
bool is_sequence_t(BaseType *variable);

BaseType *make_variable(ConstraintEvaluator &eval, const value &val);
bool_func get_function(const ConstraintEvaluator &eval, const char *name);
btp_func get_btp_function(const ConstraintEvaluator &eval, const char *name);
proj_func get_proj_function(const ConstraintEvaluator &eval, const char *name);

template<class arg_list, class arg_type>
arg_list make_fast_arg_list(unsigned long vector_size_hint, arg_type arg_value);

template<class arg_list, class arg_type>
arg_list make_fast_arg_list(arg_list int_values, arg_type arg_value);

template<class t, class T>
rvalue *build_constant_array(vector<t> *values, DDS *dds);

}

%require "2.4"

%parse-param {ce_parser_arg *arg}
%define api.prefix {ce_expr}
// %name-prefix "ce_expr"
%defines
%debug
%verbose

%union {
    bool boolean;
    int op;
    std::string *str;       // Strings - quoted or things that must be strings given the grammar

    char id[ID_MAX];        // Return identifiers

    libdap::dods_byte byte_value;
    libdap::dods_int16 int16_value;
    libdap::dods_uint16 uint16_value;
    libdap::dods_int32 int32_value;
    libdap::dods_uint32 uint32_value;
    libdap::dods_float32 float32_value;
    libdap::dods_float64 float64_value;

    libdap::byte_arg_list byte_values;
    libdap::int16_arg_list int16_values;
    libdap::uint16_arg_list uint16_values;
    libdap::int32_arg_list int32_values;
    libdap::uint32_arg_list uint32_values;
    libdap::float32_arg_list float32_values;
    libdap::float64_arg_list float64_values;
#if 0
    libdap::value val;               // value is defined in expr.h
#endif
    libdap::bool_func b_func;
    libdap::btp_func bt_func;

    libdap::dim_slice *dim_slice_ptr;
    libdap::slices *slices_ptr;

    libdap::rvalue *rval_ptr;
    libdap::rvalue_list *r_val_l_ptr;
}

%token <str> SCAN_STR

%token <id> SCAN_WORD

%token <op> SCAN_EQUAL
%token <op> SCAN_NOT_EQUAL
%token <op> SCAN_GREATER
%token <op> SCAN_GREATER_EQL
%token <op> SCAN_LESS
%token <op> SCAN_LESS_EQL
%token <op> SCAN_REGEXP

%token <op> SCAN_STAR

%token <op> SCAN_HASH_BYTE
%token <op> SCAN_HASH_INT16
%token <op> SCAN_HASH_UINT16
%token <op> SCAN_HASH_INT32
%token <op> SCAN_HASH_UINT32
%token <op> SCAN_HASH_FLOAT32
%token <op> SCAN_HASH_FLOAT64

%type <boolean> constraint_expr projection proj_clause proj_function
%type <boolean> array_projection selection clause bool_function arg_length_hint
%type <id> array_proj_clause name
%type <op> rel_op

%type <dim_slice_ptr> array_index
%type <slices_ptr> array_indices

%type <rval_ptr> r_value id_or_const array_const_special_form array_projection_rvalue
%type <r_val_l_ptr> r_value_list arg_list

%type <byte_value> fast_byte_arg
%type <byte_values> fast_byte_arg_list

%type <int16_value> fast_int16_arg
%type <int16_values> fast_int16_arg_list

%type <uint16_value> fast_uint16_arg
%type <uint16_values> fast_uint16_arg_list

%type <int32_value> fast_int32_arg
%type <int32_values> fast_int32_arg_list

%type <uint32_value> fast_uint32_arg
%type <uint32_values> fast_uint32_arg_list

%type <float32_value> fast_float32_arg
%type <float32_values> fast_float32_arg_list

%type <float64_value> fast_float64_arg
%type <float64_values> fast_float64_arg_list

%code {
/* This global is used by the rule 'arg_length_hint' so that the hint can
   be used during the parent rule's parse. See fast_int32_arg_list. */
unsigned long arg_length_hint_value = 0;
}

%%

constraint_expr: /* empty constraint --> send all */
{
    DBG(cerr << "Mark all variables" << endl);
    DDS(arg)
        ->mark_all(true);
    $$ = true;
}
/* projection only */
| projection
  /* selection only --> project everything */
| '&' { DDS(arg)
          ->mark_all(true); } selection
          {
              $$ = $3;
          }
| projection '&' selection
{
    $$ = $1 && $3;
}
;

projection: proj_clause
| proj_clause ',' projection
{
    $$ = $1 && $3;
}
;

proj_clause: name
{
    BaseType *var = DDS(arg)->var($1);
    if (var) {
        DBG(cerr << "Marking " << $1 << endl);
        $$ = DDS(arg)
            ->mark($1, true);
        DBG(cerr << "result: " << $$ << endl);
    }
    else {
        no_such_ident("identifier");
    }
}
| proj_function
{
    $$ = $1;
}
| array_projection
{
    $$ = $1;
}
| array_const_special_form
{
    Array *array = dynamic_cast<Array*>($1->bvalue(*DDS(arg)));
    if (array) {
        /* When the special form appears here (not as a function argument)
           set send_p so the data will be sent and add it to the DDS. This
           streamlines testing (and is likely what is intended). */

        array->set_send_p(true);
        DDS(arg)
            ->add_var_nocopy(array);

        return true;
    }
    else {
        ce_exprerror("Could not create the anonymous vector using the # special form.");
        return false;
    }
}
;

/* The value parsed by arg_length_hint is stored in a global variable by that rule
   so that it can be used during the parse of fast_byte_arg_list. */

/* return a rvalue */
array_const_special_form: SCAN_HASH_BYTE '(' arg_length_hint ':' fast_byte_arg_list ')'
{
    $$ = build_constant_array<dods_byte, Byte>($5, DDS(arg)
                                               );
}
;

array_const_special_form: SCAN_HASH_INT16 '(' arg_length_hint ':' fast_int16_arg_list ')'
{
    $$ = build_constant_array<dods_int16, Int16>($5, DDS(arg)
                                                 );
}
;

array_const_special_form: SCAN_HASH_UINT16 '(' arg_length_hint ':' fast_uint16_arg_list ')'
{
    $$ = build_constant_array<dods_uint16, UInt16>($5, DDS(arg)
                                                   );
}
;

array_const_special_form: SCAN_HASH_INT32 '(' arg_length_hint ':' fast_int32_arg_list ')'
{
    $$ = build_constant_array<dods_int32, Int32>($5, DDS(arg)
                                                 );
}
;

array_const_special_form: SCAN_HASH_UINT32 '(' arg_length_hint ':' fast_uint32_arg_list ')'
{
    $$ = build_constant_array<dods_uint32, UInt32>($5, DDS(arg)
                                                   );
}
;

array_const_special_form: SCAN_HASH_FLOAT32 '(' arg_length_hint ':' fast_float32_arg_list ')'
{
    $$ = build_constant_array<dods_float32, Float32>($5, DDS(arg)
                                                     );
}
;

array_const_special_form: SCAN_HASH_FLOAT64 '(' arg_length_hint ':' fast_float64_arg_list ')'
{
    $$ = build_constant_array<dods_float64, Float64>($5, DDS(arg)
                                                     );
}
;

/* Here the arg length hint is stored in a global so it can be used by the
   function that allocates the vector. The value is passed to vector::reserve(). */

arg_length_hint: SCAN_WORD
{
    if (!check_int32($1))
        throw Error(malformed_expr, "$<type>(hint, value, ...) special form expected hint to be an integer");

    arg_length_hint_value = atoi($1);
    $$ = true;
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_byte_arg_list: fast_byte_arg
{
    $$ = make_fast_arg_list<byte_arg_list, dods_byte>(arg_length_hint_value, $1);
}
| fast_byte_arg_list ',' fast_byte_arg
{
    $$ = make_fast_arg_list<byte_arg_list, dods_byte>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_int8() in parser-util.cc. jhrg 2.11.20 */
fast_byte_arg: SCAN_WORD
{
    $$ = strtol($1, 0, 0);
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_int16_arg_list: fast_int16_arg
{
    $$ = make_fast_arg_list<int16_arg_list, dods_int16>(arg_length_hint_value, $1);
}
| fast_int16_arg_list ',' fast_int16_arg
{
    $$ = make_fast_arg_list<int16_arg_list, dods_int16>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_int16() in parser-util.cc. jhrg 2.11.20 */
fast_int16_arg: SCAN_WORD
{
    $$ = strtol($1, 0, 0);
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_uint16_arg_list: fast_uint16_arg
{
    $$ = make_fast_arg_list<uint16_arg_list, dods_uint16>(arg_length_hint_value, $1);
}
| fast_uint16_arg_list ',' fast_uint16_arg
{
    $$ = make_fast_arg_list<uint16_arg_list, dods_uint16>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_uint16() in parser-util.cc. jhrg 2.11.20 */
fast_uint16_arg: SCAN_WORD
{
    $$ = strtoul($1, 0, 0);
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_int32_arg_list: fast_int32_arg
{
    $$ = make_fast_arg_list<int32_arg_list, dods_int32>(arg_length_hint_value, $1);
}
| fast_int32_arg_list ',' fast_int32_arg
{
    $$ = make_fast_arg_list<int32_arg_list, dods_int32>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_int32() in parser-util.cc. jhrg 2.11.20 */
fast_int32_arg: SCAN_WORD
{
    $$ = strtol($1, 0, 0);
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_uint32_arg_list: fast_uint32_arg
{
    $$ = make_fast_arg_list<uint32_arg_list, dods_uint32>(arg_length_hint_value, $1);
}
| fast_uint32_arg_list ',' fast_uint32_arg
{
    $$ = make_fast_arg_list<uint32_arg_list, dods_uint32>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_uint32() in parser-util.cc. jhrg 2.11.20 */
fast_uint32_arg: SCAN_WORD
{
    $$ = strtoul($1, 0, 0);
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_float32_arg_list: fast_float32_arg
{
    $$ = make_fast_arg_list<float32_arg_list, dods_float32>(arg_length_hint_value, $1);
}
| fast_float32_arg_list ',' fast_float32_arg
{
    $$ = make_fast_arg_list<float32_arg_list, dods_float32>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_float32() in parser-util.cc. jhrg 2.11.20 */
fast_float32_arg: SCAN_WORD
{
    $$ = strtof($1, 0);
}
;

/* return an int_arg_list (a std::vector<int>*) */
fast_float64_arg_list: fast_float64_arg
{
    $$ = make_fast_arg_list<float64_arg_list, dods_float64>(arg_length_hint_value, $1);
}
| fast_float64_arg_list ',' fast_float64_arg
{
    $$ = make_fast_arg_list<float64_arg_list, dods_float64>($1, $3);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
/* TODO Use get_float64() in parser-util.cc. jhrg 2.11.20 */
fast_float64_arg: SCAN_WORD
{
    $$ = strtod($1, 0);
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
proj_function:  SCAN_WORD '(' arg_list ')'
{
    proj_func p_f = 0;
    btp_func f = 0;

    if ((f = get_btp_function(*(EVALUATOR(arg)), $1))) {
        EVALUATOR(arg)->append_clause(f, $3);
        $$ = true;
    }
    else if ((p_f = get_proj_function(*(EVALUATOR(arg)), $1))) {
        DDS &dds = dynamic_cast<DDS&>(*(DDS(arg)));
        BaseType **args = build_btp_args( $3, dds );
        (*p_f)(($3) ? $3->size():0, args, dds, *(EVALUATOR(arg)
                                                 ));
        delete[] args;
        $$ = true;
    }
    else {
        no_such_func();
    }
}
;

selection:      clause
| selection '&' clause
{
    $$ = $1 && $3;
}
;

clause:         r_value rel_op '{' r_value_list '}'
{
    if ($1) {
        EVALUATOR(arg)->append_clause($2, $1, $4);
        $$ = true;
    }
    else
        $$ = false;
}
| r_value rel_op r_value
{
    if ($1) {
        rvalue_list *rv = new rvalue_list;
        rv->push_back($3);
        EVALUATOR(arg)->append_clause($2, $1, rv);
        $$ = true;
    }
    else
        $$ = false;
}
| bool_function
{
    $$ = $1;
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
bool_function: SCAN_WORD '(' arg_list ')'
{
    bool_func b_func = get_function((*EVALUATOR(arg)), $1);
    if (!b_func) {
        no_such_func();
    }
    else {
        EVALUATOR(arg)->append_clause(b_func, $3);
        $$ = true;
    }
}
;

/* This rule does not check SCAN_WORD nor does it perform www escaping */
r_value: id_or_const
| SCAN_WORD '(' arg_list ')'
{
    btp_func func = get_btp_function((*EVALUATOR(arg)), $1);
    if (func) {
        $$ = new rvalue(func, $3);
    }
    else {
        no_such_func();
    }
}
| array_const_special_form
{
    $$ = $1;
}
| array_projection_rvalue
{
    $$ = $1;
}
;

r_value_list:   r_value
{
    if ($1)
        $$ = make_rvalue_list($1);
    else
        $$ = 0;
}
| r_value_list ',' r_value
{
    if ($1 && $3)
        $$ = append_rvalue_list($1, $3);
    else
        $$ = 0;
}
;

arg_list: r_value_list
{
    $$ = $1;
}
| /* Null, argument lists may be empty */
{
    $$ = 0;
}
;

id_or_const: SCAN_WORD
{
    BaseType *btp = DDS(arg)->var(www2id($1));
    if (btp) {
        btp->set_in_selection(true);
        $$ = new rvalue(btp);
    }
    else {
#if 0
        value val;
        if (check_int32($1)) {
            val.type = dods_int32_c;
            val.v.i = atoi($1);
        }
        else if (check_uint32($1)) {
            val.type = dods_uint32_c;
            val.v.ui = atoi($1);
        }
        else if (check_float64($1)) {
            val.type = dods_float64_c;
            val.v.f = atof($1);
        }
        else {
            val.type = dods_str_c;
            // Filter the string value, then put the filtered value in new storage
            val.v.s = new string(www2id($1));
        }
#else
        value val($1, false);
#endif
        BaseType *btp = make_variable((*EVALUATOR(arg)), val);
        $$ = new rvalue(btp);
    }
}
| SCAN_STR
{
    if ($1 == 0 || $1->empty())
        ce_exprerror("Malformed string");

    BaseType *var = DDS(arg)->var(www2id(*($1)));
    if (var) {
        $$ = new rvalue(var);
    }
    else {
#if 0
        value tmp;

        tmp.type = dods_str_c;
        *($1) = www2id(*($1));
        tmp.v.s = $1;
#else
        *($1) = www2id(*($1));
        value tmp(*($1), false, dods_str_c);
#endif
        var = make_variable((*EVALUATOR(arg)), tmp);
        $$ = new rvalue(var);
        delete (tmp.v.s);
    }
    // When the scanner (ce_expr.lex) returns the SCAN_STR token type
    // it makes a local copy of the string in a new std::string object
    // that we must delete. Fix for a bug report by Aron.Bartle@mechdyne.com
    // See ticket 2240. jhrg 7/30/14
    delete $1;
}
;

/* this must return an rvalue. It should run bracket_projection()
   and then return the BaseType of the Array wrapped in a RValue
   object. */
array_projection_rvalue : name array_indices
{
    if (!bracket_projection((*DDS(arg)), $1, $2))
        no_such_ident("array, grid or sequence");

    DDS(arg)->mark($1, true);
    $$ = new rvalue(DDS(arg)->var($1));
}
;

array_projection : array_proj_clause
{
    $$ = (*DDS(arg)).mark($1, true);
}
;

array_proj_clause: name array_indices
{
    if (!bracket_projection((*DDS(arg)), $1, $2))
        no_such_ident("array, grid or sequence");

    strncpy($$, $1, ID_MAX-1);
    $$[ID_MAX-1] = '\0';
}
| array_proj_clause name
{
    string name = string($1).append($2); //  + string($2);
    strncpy($$, name.c_str(), ID_MAX-1);
    $$[ID_MAX-1] = '\0';
}
| array_proj_clause name array_indices
{
    string name = string($1).append($2); // + string($2);
    if (!bracket_projection((*DDS(arg)), name.c_str(), $3))
        no_such_ident("array, grid or sequence");

    strncpy($$, name.c_str(), ID_MAX-1);
    $$[ID_MAX-1] = '\0';
}
;

name: SCAN_WORD
{
    strncpy($$, www2id($1).c_str(), ID_MAX-1);
    $$[ID_MAX-1] = '\0';
}
| SCAN_STR
{
    if ($1 == 0 || $1->empty())
        ce_exprerror("Malformed string");

    strncpy($$, www2id(*($1)).c_str(), ID_MAX-1);
    // See comment about regarding the scanner's behavior WRT SCAN_STR.
    // jhrg 7/30/14
    delete $1;

    $$[ID_MAX-1] = '\0';
}
;

array_indices: array_index
{
    $$ = make_array_slices($1);
}
| array_indices array_index
{
    $$ = append_array_slices($1, $2);
}
;

/*
 * We added [*], [n:*] and [n:m:*] to the syntax for array projections.
 * These mean, resp., all the elements, elements from n to the end, and
 * from n to the end with a stride of m. To encode this with as little
 * disruption as possible, we represent the star with -1. jhrg 12/20/12
 */
array_index:

'[' SCAN_WORD ']'
{
    if (!check_uint32($2))
        throw Error(malformed_expr, "Expected an array index.");
#if 0
    value i;
    i.type = dods_uint32_c;
    i.v.i = atoi($2);
#endif
    value i($2, false, dods_uint32_c);
    $$ = make_array_slice(i);
}
| '[' SCAN_STAR ']'
{
#if 0
    value i;
    i.type = dods_int32_c;
    i.v.i =-1;
#else
    value i(-1, false, dods_int32_c);
#endif
    $$ = make_array_slice(i);
}
|'[' SCAN_WORD ':' SCAN_WORD ']'
{
    if (!check_uint32($2))
        throw Error(malformed_expr, "Expected an array index.");
    if (!check_uint32($4))
        throw Error(malformed_expr, "Expected an array index.");
#if 0
    value i,j;
    i.type = j.type = dods_uint32_c;
    i.v.i = atoi($2);
    j.v.i = atoi($4);
#endif

    value i($2, false, dods_uint32_c);
    value j($4, false, dods_uint32_c);

    $$ = make_array_slice(i, j);
}
|'[' '(' SCAN_WORD ')' ':' '(' SCAN_WORD ')' ']'
{
    // TODO These tests are redundant - the value ctor performs them, too
    if (!(check_int32($3) || check_float64($3)))
        throw Error(malformed_expr, "Expected an array index.");
    if (!(check_int32($7) || check_float64($3)))
        throw Error(malformed_expr, "Expected an array index.");

#if 0
    value i,j;
    i.type = j.type = dods_int32_c;
    i.v.i = atoi($3);
    j.v.i = atoi($7);
    $$ = make_array_slice(i, j);
#endif

    value i($3, true);
    value j($7, true);

    $$ = make_array_slice(i, j);
}
|'[' SCAN_WORD ':' SCAN_STAR ']'
{
    if (!check_uint32($2))
        throw Error(malformed_expr, "Expected an array index.");
#if 0
    value i,j;
    i.type = dods_uint32_c;
    j.type = dods_int32_c; /* signed */
    i.v.i = atoi($2);
    j.v.i = -1;
#endif

    value i($2, false, dods_uint32_c);
    value j(-1, false, dods_int32_c);

    $$ = make_array_slice(i, j);
}
| '[' SCAN_WORD ':' SCAN_WORD ':' SCAN_WORD ']'
{
    if (!check_uint32($2))
        throw Error(malformed_expr, "Expected an array index.");
    if (!check_uint32($4))
        throw Error(malformed_expr, "Expected an array index.");
    if (!check_uint32($6))
        throw Error(malformed_expr, "Expected an array index.");
#if 0
    value i, j, k;
    i.type = j.type = k.type = dods_uint32_c;
    i.v.i = atoi($2);
    j.v.i = atoi($4);
    k.v.i = atoi($6);
#endif

    value i($2, false, dods_uint32_c);
    value j($4, false, dods_uint32_c);
    value k($6, false, dods_uint32_c);

    $$ = make_array_slice(i, j, k);
}
| '[' SCAN_WORD ':' SCAN_WORD ':' SCAN_STAR ']'
{
    if (!check_uint32($2))
        throw Error(malformed_expr, "Expected an array index.");
    if (!check_uint32($4))
        throw Error(malformed_expr, "Expected an array index.");

    value i($2, false, dods_uint32_c);
    value j($4, false, dods_uint32_c);
    value k(-1, false, dods_int32_c);

    $$ = make_array_slice(i, j, k);
}
;

rel_op: SCAN_EQUAL
| SCAN_NOT_EQUAL
| SCAN_GREATER
| SCAN_GREATER_EQL
| SCAN_LESS
| SCAN_LESS_EQL
| SCAN_REGEXP
;

%%

// All these error reporting function now throw instances of Error. The expr
// parser no longer returns an error code to indicate and error. jhrg 2/16/2000

// As moted above, never pass text from the CE like an identifier into these
// functions since their text could be rendered in a browser. A string passed
// into the CE could be used in a XSS attack since some errors are shown in HTML
// pages and thus rendered by browsers. jhrg 4/14/20

void
ce_exprerror(const string &s)
{
    throw Error(malformed_expr, string("Constraint expression parse error: ").append(s));
}

void ce_exprerror(ce_parser_arg *, const string &s)
{
    throw Error(malformed_expr, string("Constraint expression parse error: ").append(s));
}

void no_such_ident(const string &thing)
{
    throw Error(no_such_variable, string("Constraint expression parse error: the expression referenced a ").append(thing).append(" not found in the dataset."));
}

void no_such_func()
{
    throw Error(malformed_expr, "One or more functions using the constraint expression was not defined.");
}

/* If we're calling this, assume var is not a Sequence. But assume that the
   name contains a dot and it's a separator. Look for the rightmost dot and
   then look to see if the name to the left is a sequence. Return a pointer
   to the sequence if it is, otherwise return null. Uses tail-recursion to
   'walk' back from right to left looking at each dot. This way the sequence
   will be found even if there are structures between the field and the
   Sequence. */
static Sequence *
parent_is_sequence(DDS &table, const string &n)
{
    string::size_type dotpos = n.find_last_of('.');
    if (dotpos == string::npos) return 0;

    string s = n.substr(0, dotpos);

    // If the thing returned by table.var is not a Sequence, this cast
    // will yield null.
    Sequence *seq = dynamic_cast<Sequence*>(table.var(s));
    if (seq)
        return seq;
    else
        return parent_is_sequence(table, s);
}

// TODO Modify this to process the natural axes projections. jhrg 2/10/20
bool bracket_projection(DDS &table, const char *name, slices *s)
{
    BaseType *var = table.var(name);
    Sequence *seq; // used in last else-if clause
    bool ret_val = true;
    try {
        if (!var) {
            ret_val = false;
        }
        else if (is_array_t(var)) {
            /* calls to set_send_p should be replaced with
               calls to DDS::mark so that arrays of Structures,
               etc. will be processed correctly when individual
               elements are projected using short names.
               9/1/98 jhrg */
            /* var->set_send_p(true); */
            //table.mark(name, true);
            // We don't call mark() here for an array. Instead it is called from
            // within the parser. jhrg 10/10/08
            process_array_slices(var, s); // throws on error
        }
        else if (is_grid_t(var)) {
            process_grid_slices(var, s); // indicial subset
            table.mark(name, true);
        }
        else if (is_sequence_t(var)) {
            table.mark(name, true);
            process_sequence_slices(var, s);
        }
        else if ((seq = parent_is_sequence(table, name))) {
            process_sequence_slices(seq, s);
            table.mark(name, true);
        }
        else {
            ret_val = false;
        }
        delete_array_slices(s);
    }
    catch (...) {
        delete_array_slices(s);
        throw;
    }
    return ret_val;
}

// Given three values (I1, I2, I3), all of which must be integers, build an
// int_list which contains those values.
//
// Note that we added support for * in the rightmost position of an index
// (i.e., [*], [n:*], [n:m:*]) and indicate that star using -1 as an index value.
// Bescause of this change, the test for the type of the rightmost value in
// the index subexpr was changed to include signed int.
// jhrg 12/20/12
//
// Returns: A pointer to an int_list of three integers or NULL if any of the
// values are not integers.

dim_slice *
make_array_slice(value &v1, value &v2, value &v3)
{
    auto_ptr<dim_slice> ds(new dim_slice);
    ds->push_back(v1);
    ds->push_back(v2);
    ds->push_back(v3);

    return ds.release();
}

dim_slice *
make_array_slice(value &v1, value &v2)
{
    value one((unsigned int)1, false, dods_uint32_c);
#if 0
    one.is_range_value = false;
    one.type = dods_uint32_c;
    one.v.ui = 1;
#endif
    return make_array_slice(v1, one, v2);
}

dim_slice *
make_array_slice(value &v1)
{
    // When the CE is Array[*] that means all of the elements, but the value
    // of i1 will be -1. Make the projection triple be 0:1:-1 which is a
    // pattern that libdap::Array will recognize.

    value one((unsigned int)1, false, dods_uint32_c);
#if 0
    one.is_range_value = false;
    one.type = dods_uint32_c;
    one.v.ui = 1;
#endif

    // The parser above looks for the special value '*' and sets the 'value' to -1
    // The projection is set to 0:1:-1 and will be recognized by the code that sets
    // the project for a given dimension
    if ((v1.type == dods_int32_c && v1.v.i == -1)) {
        value zero((unsigned int)0, false, dods_uint32_c);
#if 0
        minus_one.is_range_value = false;
        minus_one.type = dods_uint32_c;
        minus_one.v.ui = -1;
#endif
        return make_array_slice(zero, one, v1);
    }
    else {
        return make_array_slice(v1, one, v1);
    }
}

slices *
make_array_slices(dim_slice *ds)
{
    auto_ptr<slices> s(new slices);
    s->push_back(ds);
    return s.release();
}

slices *
append_array_slices(slices *s, dim_slice *ds)
{
    s->push_back(ds);
    return s;
}

void delete_array_slices(slices *s)
{
    assert(s);

    for (slices::iterator i = s->begin(); i != s->end(); i++) {
        dim_slice *ds = *i;
        assert(ds);
        delete ds;
    }

    delete s;
}

bool is_array_t(BaseType *variable)
{
    assert(variable);

    if (variable->type() != dods_array_c)
        return false;
    else
        return true;
}

bool is_grid_t(BaseType *variable)
{
    assert(variable);

    if (variable->type() != dods_grid_c)
        return false;
    else
        return true;
}

bool is_sequence_t(BaseType *variable)
{
    assert(variable);

    if (variable->type() != dods_sequence_c)
        return false;
    else
        return true;
}

bool has_range_values(slices *s)
{
    for (slices::iterator i = s->begin(), e = s->end(); i != e; ++i) {
        for (dim_slice::iterator j = (*i)->begin(), ej = (*i)->end(); j != ej; ++j)
            if ((*j).is_range_value)
                return true;
    }

    return false;
}

void process_array_slices(BaseType *variable, slices *s)
{
    assert(variable);

    Array *a = dynamic_cast<Array *>(variable); // replace with dynamic cast
    if (!a) throw Error(malformed_expr, "The constraint expression evaluator expected an array.");

    if (a->dimensions(true) != (unsigned) s->size())
        throw Error(malformed_expr,
                    string("Error: The number of dimensions in the constraint must match the number in the array."));

    DBG(cerr << "Before applying projection to array:" << endl);
    DBG(a->print_decl(cerr, "", true, false, true));

    bool setStop;
    Array::Dim_iter r = a->dim_begin();
    slices::iterator p = s->begin();    // p is used after the loop
    for (; p != s->end() && r != a->dim_end(); p++, r++) {
        dim_slice *ds = *p;

	dim_slice::iterator q = ds->begin();
        assert(q != ds->end());

        int start = q->v.i;

        q++;
        int stride = q->v.i;

        q++;
        uint64_t stop;
        int ce_stop = q->v.i;
        if (ce_stop == -1) {
            setStop = true;
            stop = 0;
        } else {
            setStop = false;
            stop = ce_stop;
        }

        q++;
        if (q != ds->end()) throw Error(malformed_expr, string("Too many values in index list for one or more variables."));

        DBG(cerr << "process_array_indices: Setting constraint on " << a->name() << "[" << start << ":" << stop << "]" << endl);

        // It's possible that an array will appear more than once in a CE
        // (for example, if an array of structures is constrained so that
        // only two fields are projected and there's an associated hyperslab).
        // However, in this case the two hyperslabs must be equal; test for
        // that here.
        //
        // We added '*' to mean 'the last element' in the array and use an index of -1
        // to indicate that. If 'stop' is -1, don't test it here because dimension_stop()
        // won't be -1 but the actual ending index of the array. jhrg 12/20/12

        if (a->send_p()
            && (a->dimension_start(r, true) != start || a->dimension_stop(r, true) != stop || a->dimension_stride(r, true) != stride))
            throw Error(malformed_expr, string("One or more Array variables were projected multiple times in the constraint expression."));

        a->add_constraint(r, start, stride, stop, setStop);

        DBG(cerr << "Set Constraint: " << a->dimension_size(r, true) << endl);
    }

    DBG(cerr << "After applying projection to array:" << endl);
    DBG(a->print_decl(cerr, "", true, false, true));

    if (p != s->end() && r == a->dim_end()) throw Error(malformed_expr, string("Too many indices in constraint for one or more variables."));
}

#define set_indicial_value(lhs, rhs) do { \
    if ((*rhs).type == dods_int32_c) \
        lhs = (*rhs).v.i; \
    else if ((*rhs).type == dods_uint32_c) \
        lhs = (*rhs).v.ui; \
    else \
        throw InternalErr("Expected an integer value for an array, grid or sequence subset.", __FILE__, __LINE__); } while(false)

/**
 * This method processes the slices when we know they do not contain range-value
 * subsetting.
 */
void process_grid_indicial_slices(Grid *g, slices *s)
{
    Array *a = g->get_array();

    if (a->dimensions(true) != (unsigned) s->size())
        throw Error(malformed_expr,
                    string("Error: The number of dimensions in the constraint must match the number in the grid."));

    // First do the constraints on the ARRAY in the grid.
    process_array_slices(g->array_var(), s);

    // Now process the maps.
    // Suppress all maps by default.
    for (Grid::Map_iter r = g->map_begin(); r != g->map_end(); ++r) {
        (*r)->set_send_p(false);
    }

    // Add specified maps to the current projection.
    //assert(indices);
    //int_list_citer p = indices->begin();

    slices::iterator p = s->begin();
    Grid::Map_iter r = g->map_begin();

    bool setStop;
    for (; p != s->end() && r != g->map_end(); ++p, ++r) {
        dim_slice *slice = *p;
        //assert(index);

	dim_slice::iterator q = slice->begin();
        //assert(q != index->end());
        int start = (*q).v.ui;

        q++;
        int stride = (*q).v.i;

        q++;
        int ce_stop = (*q).v.i;
        uint64_t stop;
        if (ce_stop == -1) {
            setStop = true;
            stop = 0;
        } else {
            setStop = false;
            stop = ce_stop;
        }

        BaseType *btp = *r;
        assert(btp);
        assert(btp->type() == dods_array_c);
        Array *a = dynamic_cast<Array*>(btp);
        a->set_send_p(true);
        a->reset_constraint();

        q++;
        if (q != slice->end()) {
            throw Error(malformed_expr, string("Too many values in index list for one or more variables."));
        }

        DBG(cerr << "process_grid_indices: Setting constraint on " << a->name() << "[" << start << ":" << stop << "]" << endl);

        Array::Dim_iter si = a->dim_begin();
        a->add_constraint(si, start, stride, stop, setStop);

    }

    DBG(cout << "Grid Constraint: ";
        for (Array::Dim_iter dp = ((Array *) g->array_var())->dim_begin(); dp != ((Array *) g->array_var())->dim_end(); dp++)
            cout << ((Array *) g->array_var())->dimension_size(dp, true) << " ";
        cout << endl
        );

    if (p != s->end() && r == g->map_end()) {
        throw Error(malformed_expr, string("Too many indices in constraint for one or more variables."));
    }
}

/**
 * @brief Process a indicial subset for a Grid.
 */
void process_grid_slices(BaseType *variable, slices *s)
{
    assert(variable);
    assert(variable->type() == dods_grid_c);
    Grid *g = dynamic_cast<Grid *>(variable);
    if (!g) throw Error(unknown_error, "Expected a Grid variable");

    if (has_range_values(s)) {
        throw InternalErr(__FILE__, __LINE__, "Range-value subsets not implemented");
    }
    else {
        process_grid_indicial_slices(g, s);
    }
}

// TODO Add support for value based slicing of Sequences if reasonable jhrg 4/19/19
/**
 * Use a slice to subset a Sequence. For a Sequence, it only make sense
 * to have one dimension of slicing. Thus, if \arg s has more than one
 * slice, it's an error.
 */
void process_sequence_slices(BaseType *variable, slices *s)
{
    assert(variable);
    assert(variable->type() == dods_sequence_c);
    Sequence *seq = dynamic_cast<Sequence *>(variable);
    if (!seq) throw Error(malformed_expr, "Expected a Sequence variable");

    if (s->size() != 1) throw Error(string("Expected only one slice when subsetting the Sequence '") + seq->name() + "'.");

    dim_slice *ds = *(s->begin());

    dim_slice::iterator q = ds->begin(); // *q is an instance of 'value'
    int start, stop, stride;

    if ((*q).is_range_value) throw Error("Range-value subsetting not applicable to Sequences, use filter expressions instead.");

    if ((*q).type != dods_uint32_c)
        throw Error(string("Expected an integer value for the bracket subset operator used with Sequence '") + seq->name() + "'.");
    else
        start = (*q).v.ui;

    q++;
    if ((*q).is_range_value) throw Error("Range-value subsetting not applicable to Sequences, use filter expressions instead.");

    if ((*q).type != dods_uint32_c)
        throw Error(string("Expected an integer value for the bracket subset operator used with Sequence '") + seq->name() + "'.");
    else
        stride = (*q).v.ui;

    q++;
    if ((*q).is_range_value) throw Error("Range-value subsetting not applicable to Sequences, use filter expressions instead.");

    // The last of the three can be a -1
    if ((*q).type == dods_uint32_c)
        stop = (*q).v.ui;
    else if ((*q).type != dods_int32_c)
        stop = (*q).v.i;
    else
        throw Error(string("Expected an integer value for the bracket subset operator used with Sequence '") + seq->name() + "'.");

    seq->set_row_number_constraint(start, stop, stride);
}

// Given a value, wrap it up in a BaseType and return a pointer to the same.

BaseType *
make_variable(ConstraintEvaluator &eval, const value &val)
{
    BaseType *var;
    switch (val.type) {
      case dods_int32_c: {
          var = new Int32("dummy");
          var->val2buf((void *) &val.v.i);
          break;
      }

      case dods_uint32_c: {
          var = new UInt32("dummy");
          var->val2buf((void *) &val.v.i);
          break;
      }

      case dods_float64_c: {
          var = new Float64("dummy");
          var->val2buf((void *) &val.v.f);
          break;
      }

      case dods_str_c: {
          var = new Str("dummy");
          var->val2buf((void *) val.v.s);
          break;
      }

      default:
        var = (BaseType *) 0;
        return var;
    }

    var->set_read_p(true); // ...so the evaluator will know it has data
    eval.append_constant(var);

    return var;
}

// Given a string (passed in VAL), consult the DDS CE function lookup table
// to see if a function by that name exists.
// NB: function arguments are type-checked at run-time.
//
// Returns: A pointer to the function or NULL if not such function exists.

bool_func get_function(const ConstraintEvaluator &eval, const char *name)
{
    bool_func f;

    if (eval.find_function(name, &f))
        return f;
    else
        return 0;
}

btp_func get_btp_function(const ConstraintEvaluator &eval, const char *name)
{
    btp_func f;

    if (eval.find_function(name, &f))
        return f;
    else
        return 0;
}

proj_func get_proj_function(const ConstraintEvaluator &eval, const char *name)
{
    proj_func f;

    if (eval.find_function(name, &f))
        return f;
    else
        return 0;
}

template<class arg_type_list, class arg_type>
arg_type_list make_fast_arg_list(unsigned long vector_size_hint, arg_type value)
{
    arg_type_list args = new std::vector<arg_type>;

    if (vector_size_hint > 0) args->reserve(vector_size_hint);

    args->push_back(value);
    return args;
}

template<class arg_type_list, class arg_type>
arg_type_list make_fast_arg_list(arg_type_list values, arg_type value)
{
    values->push_back(value);
    return values;
}

template<class t, class T>
rvalue *build_constant_array(vector<t> *values, DDS *dds)
{
    T i("");
    Array *array = new Array("", &i);
    array->append_dim(values->size());

    // TODO Make set_value_nocopy() methods so that values' pointers can be copied
    // instead of allocating memory twice. jhrg 7/5/13

    array->set_value(*values, values->size());
    delete values;
    array->set_read_p(true);

    static unsigned long counter = 1;
    string name;
    do {
        name = "g" + long_to_string(counter++);
    } while (dds->var(name));
    array->set_name(name);

    return new rvalue(array);
}
