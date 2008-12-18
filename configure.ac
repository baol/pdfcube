dnl --------------------------------
dnl Initialization macros.
dnl --------------------------------

AC_INIT(src/pdfcube.cc)
dnl AM_CONFIG_HEADER(config.h)

dnl -----------------------------------------------
dnl Package name and version number (user defined)
dnl -----------------------------------------------

AM_INIT_AUTOMAKE(pdfcube, 0.0.3)

AC_CONFIG_MACRO_DIR([m4])

dnl -----------------------------------------------
dnl Checks for programs.
dnl -----------------------------------------------

AC_PROG_CC
AC_PROG_CXX
AM_PROG_LIBTOOL
AM_SANITY_CHECK

dnl -----------------------------------------------
dnl Checks for libraries.
dnl -----------------------------------------------

AC_LANG_CPLUSPLUS

PKG_CHECK_MODULES(glib, glib-2.0 >= 2.0.0)
AC_SUBST(glib_CFLAGS)
AC_SUBST(glib_LIBS)

PKG_CHECK_MODULES(gtk, gtk+-2.0 >= 2.0.0)
AC_SUBST(gtk_CFLAGS)
AC_SUBST(gtk_LIBS)

PKG_CHECK_MODULES(gtkglext, gtkglext-1.0 >= 1.0.0)
AC_SUBST(gtkglext_CFLAGS)
AC_SUBST(gtkglext_LIBS)

PKG_CHECK_MODULES(poppler, poppler >= 0.4.5)
AC_SUBST(poppler_CFLAGS)
AC_SUBST(poppler_LIBS)

PKG_CHECK_MODULES(poppler_glib, poppler-glib >= 0.4.5)
AC_SUBST(poppler_glib_CFLAGS)
AC_SUBST(poppler_glib_LIBS)

AC_CHECK_HEADERS(GL/gl.h)
AC_CHECK_HEADERS(GL/glu.h)
AC_CHECK_HEADERS(GL/glx.h)

AX_BOOST_BASE([1.33.1])
AX_BOOST_PROGRAM_OPTIONS()

dnl -----------------------------------------------
dnl Generates Makefiles.
dnl -----------------------------------------------

AC_OUTPUT(Makefile \
          src/Makefile \
	  m4/Makefile
)
