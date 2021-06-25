
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

// Copyright (c) 1996, California Institute of Technology.
// ALL RIGHTS RESERVED.   U.S. Government Sponsorship acknowledged.
//
// Please read the full copyright notice in the file COPYRIGHT_URI
// in this directory.
//
// Author: Todd Karakashian, NASA/Jet Propulsion Laboratory
//         Todd.K.Karakashian@jpl.nasa.gov
//
// $RCSfile: escaping.cc,v $ - Miscellaneous routines for OPeNDAP HDF server
//
// These two routines are for escaping/unescaping strings that are identifiers
// in DAP2
// id2www() -- escape (using WWW hex codes) non-allowable characters in a
// DAP2 identifier
// www2id() -- given an WWW hexcode escaped identifier, restore it
//
// These two routines are for escaping/unescaping strings storing attribute
// values.  They use traditional octal escapes (\nnn) because they are
// intended to be viewed by a user
// escattr() -- escape (using traditional octal backslash) non-allowable
// characters in the value of a DAP2 attribute
// unescattr() -- given an octally escaped string, restore it
//
// These are routines used by the above, not intended to be called directly:
//
// hexstring()
// unhexstring()
// octstring()
// unoctstring()
//
// -Todd

#include "config.h"

#include <ctype.h>

#include <iomanip>
#include <string>
#include <sstream>

#include "GNURegex.h"
#include "Error.h"
#include "InternalErr.h"
//#define DODS_DEBUG
#include "debug.h"

using namespace std;

namespace libdap {

// The next four functions were originally defined static, but I removed that
// to make testing them (see generalUtilTest.cc) easier to write. 5/7/2001
// jhrg

string
hexstring(unsigned char val)
{
    ostringstream buf;
    buf << hex << setw(2) << setfill('0') << static_cast<unsigned int>(val);

    return buf.str();
}

string
unhexstring(string s)
{
    int val;
    istringstream ss(s);
    ss >> hex >> val;
    char tmp_str[2];
    tmp_str[0] = static_cast<char>(val);
    tmp_str[1] = '\0';
    return string(tmp_str);
}

string
octstring(unsigned char val)
{
    ostringstream buf;
    buf << oct << setw(3) << setfill('0')
    << static_cast<unsigned int>(val);

    return buf.str();
}

string
unoctstring(string s)
{
    int val;

    istringstream ss(s);
    ss >> oct >> val;

    DBG(cerr << "unoctstring: " << val << endl);

    char tmp_str[2];
    tmp_str[0] = static_cast<char>(val);
    tmp_str[1] = '\0';
    return string(tmp_str);
}

/** Replace characters that are not allowed in DAP2 identifiers.

    -In the DAP itself, id2www() is called in:
     -# Array::print_decl() where dimension names are escaped
     -# AttrTable::print() (which calls AttrTable::simple_print()) where
        attribute names are escaped
     -# BaseType::print_decl() where variable names are escaped.
     -# Constructor::print_decl() where the name of the constructor type is
        printed.
     -# DDS::print() and DDS::print_constrained() where the name of the
        dataset is printed.
     -# Grid::print_decl() where the name of the grid is printed.

    -In the client code:
     -# id2www_ce() is called five times in the five methods that are used to
        request responses where a CE is appended to a URL
        (Connect::request_version, request_protocol, request_das, request_dds,
        request_data).

    @param in Replace characters in this string.
    @param allowable The set of characters that are allowed in a URI.
    default: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+_/.\\*"
    @see id2www_ce()
    @return The modified identifier. */
string
id2www(string in, const string &allowable)
{
    string::size_type i = 0;
    DBG(cerr<<"Input string: [" << in << "]" << endl);
    while ((i = in.find_first_not_of(allowable, i)) != string::npos) {
	DBG(cerr<<"Found escapee: [" << in[i] << "]");
        in.replace(i, 1, "%" + hexstring(in[i]));
        DBGN(cerr<<" now the string is: " << in << endl);
        i += 3;//i++;
    }

    return in;
}

/** Replace characters that are not allowed in WWW URLs using rules specific
    to Constraint Expressions. This has changed over time and now the only
    difference is that '*' is escaped by this function while it is not
    escaped by id2www().

    @param in The string in which to replace characters.
    @param allowable The set of characters that are allowed in a URI.
    default: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+_/.\\"
    @see id2www()
    @return The modified identifier. */
string
id2www_ce(string in, const string &allowable)
{
    return id2www(in, allowable);


}

/** Given a string that contains WWW escape sequences, translate those escape
    sequences back into the ASCII characters they represent. Return the
    modified string.

    -Places in the dap code where www2id() is called:
     -# Array::append_dim() the name is decoded before it is added
     -# AttrTable::set_name(), AttrTable::append_attr(),
        AttrTable::append_container(), AttrTable::del_attr(),
        AttrTable::add_container_alias(), AttrTable::add_value_alias()
        names are decoded before that are set/used.
     -# BaseType::set_name() Names are decoded before they are set
     -# When the constraint expression parser looks for a variable, the name is
        first decoded.
     -# DAS::DAS() Named attribute containers are decoded
     -# DDS::var() When a DDS searches for a variable, the name is first decoded.
     -# Grid::var(), Sequence::var(), Structure::var() Variable names are decoded.

   -In the server code:
     -# ResponseBuilder::initialize() The dataset name is decoded except that %20
        is not removed.
     -# ResponseBuilder::set_ce() The CE is decoded, except for spaces (%20).
     -# ResponseBuilder::set_dataset_name() same logic as the first case.
     -# The ResponseBuilder methods supersede methods with the same names
        from DODSFilter, which is still in the code although deprecated.

    @param in The string to modify.
    @param escape The character used to signal the beginning of an escape
    sequence. default: "%"
    @param except If there are some escape codes that should not be removed by
    this call (e.g., you might not want to remove spaces, %20) use this
    parameter to specify those codes. The function will then transform all
    escapes \e except those given. For example, to suppress translation of both
    spaces and the ampersand, pass "%20%26" for 'except'. default: ""
    @return The modified string. */
string
www2id(const string &in, const string &escape, const string &except)
{
    string::size_type i = 0;
    string res = in;
    while ((i = res.find_first_of(escape, i)) != string::npos) {
        if (except.find(res.substr(i, 3)) != string::npos) {
            i += 3;
            continue;
        }
        res.replace(i, 3, unhexstring(res.substr(i + 1, 2)));
        ++i;
    }

    return res;
}

static string
entity(char c)
{
    switch (c) {
    case '>': return "&gt;";
    case '<': return "&lt;";
    case '&': return "&amp;";
    case '\'': return "&apos;";
    case '\"': return "&quot;";
    default:
        throw InternalErr(__FILE__, __LINE__, "Unrecognized character.");
    }
}

// Assumption: There are always exactly two octal digits in the input
// and two hex digits in the result.
string
octal_to_hex(const string &octal_digits)
{
    int val;

    istringstream ss(octal_digits);
    ss >> oct >> val;

    ostringstream ds;
    ds << hex << setw(2) << setfill('0') << val;
    return ds.str();
}

/** Replace characters that are not allowed in XML

    @param in The string in which to replace characters.
    @param not_allowed The set of characters that are not allowed in XML.
    default: ><&'(single quote)"(double quote)
    @return The modified identifier. */
string
id2xml(string in, const string &not_allowed)
{
    string::size_type i = 0;

    while ((i = in.find_first_of(not_allowed, i)) != string::npos) {
        in.replace(i, 1, entity(in[i]));
        ++i;
    }
#if 0
    // Removed the encoding of octal escapes. This function is used by
    // AttrTable to encode the stuff that is the value of the <value>
    // element in the DDX. The problem is that some of the values are not
    // valid UTF-8 and that makes a XML parser gag.; ticket 1512.
    // jhrg 3/19/10

    // OK, now scan for octal escape sequences like \\012 (where the '\'
    // is itself escaped). This type of attribute value comes from the netCDF
    // handler and maybe others. Assumption: The '\' will always appear as
    // in its escaped form: '\\'. NB: Both backslashes must be escaped in the
    // C++ string.
    string octal_escape = "\\\\";
    i = 0;
    string::size_type length = in.length();
    while ((i = in.find(octal_escape, i)) != string::npos) {
        // Get the three octal digits following the '\\0'
        string::size_type j = i + 2;
        if (j + 1 >= length)  // Check that we're not past the end
            break;
        string octal_digits = in.substr(j, 3);
        // convert to a &#xdd; XML escape
        string hex_escape = string("&#x");
        hex_escape.append(octal_to_hex(octal_digits));
        hex_escape.append(string(";"));

        // replace the octal escape with an XML/hex escape
        in.replace(i, 5, hex_escape);

        // increment i
        i += 6;
    }
#endif
    return in;
}

/** Given a string that contains XML escape sequences (i.e., entities),
    translate those back into ASCII characters. Return the modified string.

    @param in The string to modify.
    @return The modified string. */
string
xml2id(string in)
{
    string::size_type i = 0;

    while ((i = in.find("&gt;", i)) != string::npos)
        in.replace(i, 4, ">");

    i = 0;
    while ((i = in.find("&lt;", i)) != string::npos)
        in.replace(i, 4, "<");

    i = 0;
    while ((i = in.find("&amp;", i)) != string::npos)
        in.replace(i, 5, "&");

    i = 0;
    while ((i = in.find("&apos;", i)) != string::npos)
        in.replace(i, 6, "'");

    i = 0;
    while ((i = in.find("&quot;", i)) != string::npos)
        in.replace(i, 6, "\"");

    return in;
}

/** Return a string that has all the \c %&lt;hex digit&gt;&lt;hex digit&gt;
    sequences replaced with underscores (`_').

    @param s The string to transform
    @return The modified string. */
string
esc2underscore(string s)
{
    string::size_type pos;
    while ((pos = s.find('%')) != string::npos)
        s.replace(pos, 3, "_");

    return s;
}


/** Escape non-printable characters and quotes from an HDF attribute.
    @param s The attribute to modify.
    @return The modified attribute. */
string
escattr(string s)
{
    const string printable = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789~`!@#$%^&*()_-+={[}]|\\:;<,>.?/'\"\n\t\r";
    const string ESC = "\\";
#if 0
    const string DOUBLE_ESC = ESC + ESC;
    const string QUOTE = "\"";
    const string ESCQUOTE = ESC + QUOTE;

    // escape \ with a second backslash
    string::size_type ind = 0;
    while ((ind = s.find(ESC, ind)) != s.npos) {
        s.replace(ind, 1, DOUBLE_ESC);
        ind += DOUBLE_ESC.length();
    }

    // escape non-printing characters with octal escape
    ind = 0;
    while ((ind = s.find_first_not_of(printable, ind)) != s.npos)
        s.replace(ind, 1, ESC + octstring(s[ind]));

    // escape " with backslash
    ind = 0;
    while ((ind = s.find(QUOTE, ind)) != s.npos) {
        s.replace(ind, 1, ESCQUOTE);
        ind += ESCQUOTE.length();
    }
#endif
    // escape non-printing characters with octal escape
    size_t ind = 0;
    while ((ind = s.find_first_not_of(printable, ind)) != s.npos)
        s.replace(ind, 1, ESC + octstring(s[ind]));

    return s;
}

/** Un-escape special characters, quotes and backslashes from an HDF
    attribute.

    Note: A regex to match one \ must be defined as: Regex foo = "\\\\";
    because both C++ strings and GNU's Regex also employ \ as an escape
    character!

    @param s The escaped attribute. @return The unescaped attribute. */
string
unescattr(string s)
{
    Regex octal("\\\\[0-3][0-7][0-7]");  // matches 4 characters
    Regex esc_quote("\\\\\"");  // matches 3 characters
    Regex esc_esc("\\\\\\\\");      // matches 2 characters
    const string ESC = "\\";
    const string QUOTE = "\"";
    int matchlen;
    unsigned int index;

    DBG(cerr << "0XX" << s << "XXX" << endl);
    // unescape any escaped backslashes
    index = esc_esc.search(s.c_str(), s.length(), matchlen, 0);
    while (index < s.length()) {
        DBG(cerr << "1aXX" << s << "XXX index: " << index << endl);
        s.replace(index, 2, ESC);
        DBG(cerr << "1bXX" << s << "XXX index: " << index << endl);
        index = esc_esc.search(s.c_str(), s.length(), matchlen, 0);
    }

    // unescape any escaped double quote characters
    index = esc_quote.search(s.c_str(), s.length(), matchlen, 0);
    while (index < s.length()) {
        s.replace(index, 2, QUOTE);
        DBG(cerr << "2XX" << s << "XXX index: " << index << endl);
        index = esc_quote.search(s.c_str(), s.length(), matchlen, 0);
    }

    // unescape octal characters
    index = octal.search(s.c_str(), s.length(), matchlen, 0);
    while (index < s.length()) {
        s.replace(index, 4, unoctstring(s.substr(index + 1, 3)));
        DBG(cerr << "3XX" << s << "XXX index: " << index << endl);
        index = octal.search(s.c_str(), s.length(), matchlen, 0);
    }

    DBG(cerr << "4XX" << s << "XXX" << endl);
    return s;
}

string
munge_error_message(string msg)
{
    // First, add enclosing quotes if needed.
    if (*msg.begin() != '"')
        msg.insert(msg.begin(), '"');
    if (*(msg.end() - 1) != '"')
        msg += "\"";

    // Now escape any internal double quotes that aren't escaped.
    string::iterator miter;
    for (miter = msg.begin() + 1; miter != msg.end() - 1; miter++)
        if (*miter == '"' && *(miter - 1) != '\\')
            miter = msg.insert(miter, '\\');

    return msg;
}

/** Rip through a string and replace all the double quotes with \" sequences.
    @param source
    @return result
 */
string
escape_double_quotes(string source)
{
    string::size_type idx = 0;
    while((idx = source.find('\"', idx)) != string::npos) {
        source.replace(idx, 1, "\\\""); // a backslash and a double quote
        idx += 2;
    }

    return source;
}

/** Rip through a string and replace all the escaped double quotes with
    regular double quotes.
    @param source
    @return result
 */
string
unescape_double_quotes(string source)
{
    string::size_type idx = 0;
    while((idx = source.find("\\\"", idx)) != string::npos) {
        source.replace(idx, 2, "\""); // a backslash and a double quote
        ++idx;
    }

    return source;
}

} // namespace libdap

