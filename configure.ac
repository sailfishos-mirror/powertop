#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([powertop], [2.15], [powertop@lists.01.org], [], [https://github.com/fenrus75/powertop])
AM_INIT_AUTOMAKE([
	-Wall
	1.12.2
	foreign
])
AC_LANG([C++])
AC_CONFIG_FILES([
	Makefile
	Doxyfile
	src/Makefile
	po/Makefile.in
	doc/Makefile
	scripts/bash-completion/Makefile
])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_HEADERS([config.h])
GETTEXT_PACKAGE=powertop
AC_SUBST([GETTEXT_PACKAGE])
AM_SILENT_RULES([yes])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18.2])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AX_REQUIRE_DEFINED([AX_ADD_FORTIFY_SOURCE])
AX_REQUIRE_DEFINED([AX_CXX_COMPILE_STDCXX])
AX_REQUIRE_DEFINED([AX_PTHREAD])
# Checks for programs.
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O
AX_ADD_FORTIFY_SOURCE
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])
PKG_PROG_PKG_CONFIG

# Checks for libraries.
AX_PTHREAD([
    LIBS="$PTHREAD_LIBS $LIBS"
    CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
    CC="$PTHREAD_CC"
  ], [
    AC_MSG_ERROR([Could not configure pthreads support])
])

# Checks for header files.
AC_CHECK_HEADERS([ \
	fcntl.h \
	libintl.h \
	limits.h \
	locale.h \
	malloc.h \
	ncurses.h \
	stdint.h \
	stdlib.h \
	string.h \
	sys/ioctl.h \
	sys/socket.h \
	sys/statfs.h \
	sys/time.h \
	termios.h \
	unistd.h \
])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_FUNC_STRTOD
AC_CHECK_FUNCS([ \
	clock_gettime \
	fdatasync \
	getpagesize \
	gettimeofday \
	memmove \
	memset \
	mkdir \
	munmap \
	pow \
	realpath \
	regcomp \
	select \
	setlocale \
	socket \
	sqrt \
	strcasecmp \
	strchr \
	strrchr \
	strdup \
	strerror \
	strncasecmp \
	strstr \
	strtoul \
	strtoull \
])

AC_SEARCH_LIBS([clock_gettime], [rt])

PKG_CHECK_MODULES([ncursesw], [ncursesw],
	[NCURSES_CFLAGS="$ncursesw_CFLAGS"; NCURSES_LIBS="$ncursesw_LIBS"], [
	PKG_CHECK_MODULES([NCURSES], [ncurses], [], [
		AC_SEARCH_LIBS([delwin], [ncursesw ncurses], [], [
			AC_MSG_ERROR([ncurses is required but was not found])
		])
	])
])

has_libpci=0
PKG_CHECK_MODULES([PCIUTILS], [libpci],[has_libpci=1], [
	AC_SEARCH_LIBS([pci_get_dev], [pci],[has_libpci=1], [has_libpci=0])
])
 
has_libtracefs=0
PKG_CHECK_MODULES([LIBTRACEFS], [libtracefs],[has_libtracefs=1], [
	AC_SEARCH_LIBS([tracefs_local_events], [libtracefs], [], [
		AC_MSG_ERROR([libtracefs is required but was not found])
	], [])
])

has_libnl_ver=0
dnl libnl-2 provides only libnl-2.0.pc file, so we check for separate
dnl libnl-genl-3.0.pc pkg-config file just for libnl-3.0 case.
PKG_CHECK_MODULES([LIBNL], [libnl-3.0 >= 3.0 libnl-genl-3.0 >= 3.0], [has_libnl_ver=3], [
	PKG_CHECK_MODULES([LIBNL], [libnl-2.0 >= 2.0], [has_libnl_ver=2], [
		PKG_CHECK_MODULES([LIBNL], [libnl-1], [has_libnl_ver=1], [has_libnl_ver=0])
	])
])
AS_IF([test "$has_libnl_ver" -eq 0], [
	AC_MSG_ERROR([libnl and libnl-genl are required but were not found])
])
AS_IF([test "$has_libnl_ver" -gt 1], [
	AC_DEFINE([HAVE_LIBNL20], [1], [Define if you have libnl-2.0 or higher])
])

AS_IF([test "$has_libpci" -eq 0], [
	AC_DEFINE([HAVE_NO_PCI], [1], [Define if pci is not supported])
        AC_MSG_WARN([
			************* LIBPCI SUPPORT NOT CONFIGURED**************
			If you need or want pci support, please install libpci 
			and re-configure PowerTOP. 
			*********************************************************
	])
])

AC_SEARCH_LIBS([inet_aton], [resolv], [], [
	AC_MSG_ERROR([libresolv is required but was not found])
], [])


AS_IF([`${PKG_CONFIG} --exists bash-completion`], [
	bashcompletiondir=`${PKG_CONFIG} --variable=completionsdir --define-variable=prefix=${prefix} bash-completion`
], [
	bashcompletiondir=${datadir}/bash-completion/completions
])
AC_SUBST([bashcompletiondir], [$bashcompletiondir])

AC_OUTPUT
