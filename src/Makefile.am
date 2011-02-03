bin_PROGRAMS = pdfcube

pdfcube_SOURCES = pdfcube.cc
pdfcube_CXXFLAGS = -Wall -Weffc++ 

LDADD = $(glib_LIBS) $(gtk_LIBS) $(gtkglext_LIBS) $(poppler_glib_LIBS) -lglut 
INCLUDES = $(glib_CFLAGS) $(gtk_CFLAGS) $(gtkglext_CFLAGS) $(poppler_glib_CFLAGS) $(BOOST_LDFLAGS)

AM_CXXFLAGS = $(BOOST_CPPFLAGS) 
AM_LDFLAGS=$(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB)
