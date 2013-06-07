
INCLUDES = @CFLAGS@ @GEOS_CFLAGS@ @LIBXML2_CFLAGS@
INCLUDES += -I$(top_srcdir)/src/headers

noinst_LTLIBRARIES = libsplite.la

libsplite_la_SOURCES = mbrcache.c \
	spatialite.c \
	spatialite_init.c \
	metatables.c \
	statistics.c \
	extra_tables.c \
	virtualdbf.c \
	virtualXL.c \
	virtualfdo.c \
	virtualspatialindex.c \
	virtualnetwork.c \
	virtualshape.c \
	virtualxpath.c

libsplite_la_CFLAGS = -fvisibility=hidden

MOSTLYCLEANFILES = *.gcna *.gcno *.gcda
