[![Build Status](https://travis-ci.org/OPENDAP/libdap4.svg?branch=master)](https://travis-ci.org/OPENDAP/libdap4)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=OPENDAP-libdap4&metric=alert_status)](https://sonarcloud.io/dashboard?id=OPENDAP-libdap4)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.2566512.svg)](https://doi.org/10.5281/zenodo.2566512) 

# libdap - An implementation of the Data Access Protocol in C++#

### What's in this Directory? ###

This directory contains the OPeNDAP C++ implementation of the Data
Access Protocol version 2 (DAP2) with some extensions that will be
part of DAP3.  Documentation for this software can be found on the
OPeNDAP home page at http://www.opendap.org/. The NASA/ESE RFC which
describes DAP2, implemented by the library, can be found at
http://spg.gsfc.nasa.gov/rfc/004/.

The DAP2 is used to provide a uniform way of accessing a variety of
different types of data across the Internet. It was originally part of
the DODS and then NVODS projects. The focus of those projects was
access to Earth-Science data, so much of the software developed using
the DAP2 to date has centered on that discipline. However, the DAP2
data model is very general (and similar to a modern structured
programming language) so it can be applied to a wide variety of
fields.

The DAP2 is implemented as a set of C++ classes that can be used to
build data servers and clients. The classes may be specialized to
mimic the behavior of other data access APIs, such as netCDF. In this
way, programs originally meant to work with local data in those
formats can be re-linked and equipped to work with data stored
remotely in many different formats.  The classes can also by
specialized to build standalone client programs.

The DAP2 is contained in a single library: libdap++.a. Also included
in the library are classes and utility functions which simplify
building clients and servers.

### What else is there? ###

The file README.dodsrc describes the client-side behavior which can be
controlled using the .dodsrc file. This includes client-side caching,
proxy servers, et c., and is described in a separate file so it's easy
to include in your clients.

The file README.AIS describes the prototype Ancillary Information
Service (AIS) included in this version of the library. The AIS is
(currently) a client-side capability which provides a way to augment
DAP attributes. This is a very useful feature because it can be used
to add missing metadata to a data source. The AIS is accessed by using
the AISConnect class in place of Connect in your client.

This directory also contains test programs for the DAP2, a sample
specialization of the classes, getdap (a useful command-line web
client created with DAP2) and dap-config (a utility script to simplify
linking with libdap.a). Also included as of version 3.5.2 is
libdap.m4, an autoconf macro which developers can use along with
autoconf to test for libdap. This macro will be installed in
${prefix}/share/aclocal and can be by any package which uses autoconf
for its builds. See the file for more information.

We also have Java and C versions of the DAP2 library which
inter-operate with software which uses this library. In other words,
client programs built with the Java DAP2 implementation can
communicate with servers built with this (C++) implementation of the
DAP2. The C DAP2 library, called the Ocapi, only implements the
client-side part of the protocol. Clients written using the Ocapi are
interoperable with both the Java and C++ DAP2 libraries. Note that the
Ocapi is in early beta and available only from CVS at this time (5 May
2005).
  
### Thread Safety ###

We don't need to do this since the STL is also not thread safe. Users
of libdap have to be sure that multiple threads never make
simultaneous and/or overlapping calls to a single copy of libdap. If
several threads are part of a program and each will make calls to
libdap, either those threads must synchronize their calls or arrange
to each use their own copy of libdap.  Some aspects of the library
are thread-safe: the singleton classes are all protected as is the
HTTP cache (which uses the local file system).

### Installation Instructions ###

See the file INSTALL in this directory for information on building the
library and the geturl client.

### Copyright Information ###

The OPeNDAP DAP library is copyrighted using the GNU Lesser GPL. See
the file COPYING or contact the Free Software Foundation, Inc., at 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA. Older versions of
the DAP were copyrighted by the University of Rhode Island and
Massachusetts Institute of Technology; see the file COPYRIGHT_URI. The
file deflate.c is also covered by COPYRIGHT_W3C.
