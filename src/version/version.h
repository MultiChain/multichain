// Copyright (c) 2014-2017 Coin Sciences Ltd
// Rk code distributed under the GPLv3 license, see COPYING file.

#ifndef RKVERSION_H
#define	RKVERSION_H

#define RK_VERSION_MAJOR     1
#define RK_VERSION_MINOR     0
#define RK_VERSION_REVISION  2
#define RK_VERSION_STAGE     9
#define RK_VERSION_BUILD     1

#define RK_PROTOCOL_VERSION 10009
#define RK_MIN_DOWNGRADE_PROTOCOL_VERSION 10008


#ifndef STRINGIZE
#define STRINGIZE(X) DO_STRINGIZE(X)
#endif

#ifndef DO_STRINGIZE
#define DO_STRINGIZE(X) #X
#endif

#define RK_BUILD_DESC_WITH_SUFFIX(maj, min, rev, build, suffix) \
    DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build) "-" DO_STRINGIZE(suffix)

#define RK_BUILD_DESC_FROM_UNKNOWN(maj, min, rev, build) \
    DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build)


#define RK_BUILD_DESC "1.0.2"
#define RK_BUILD_DESC_NUMERIC 10002901

#ifndef RK_BUILD_DESC
#ifdef BUILD_SUFFIX
#define RK_BUILD_DESC RK_BUILD_DESC_WITH_SUFFIX(RK_VERSION_MAJOR, RK_VERSION_MINOR, RK_VERSION_REVISION, RK_VERSION_BUILD, BUILD_SUFFIX)
#else
#define RK_BUILD_DESC RK_BUILD_DESC_FROM_UNKNOWN(RK_VERSION_MAJOR, RK_VERSION_MINOR, RK_VERSION_REVISION, RK_VERSION_BUILD)
#endif
#endif

#define RK_FULL_DESC(build, protocol) \
    "build " build " protocol " DO_STRINGIZE(protocol)


#ifndef RK_FULL_VERSION
#define RK_FULL_VERSION RK_FULL_DESC(RK_BUILD_DESC, RK_PROTOCOL_VERSION)
#endif


#endif	/* RKVERSION_H */

