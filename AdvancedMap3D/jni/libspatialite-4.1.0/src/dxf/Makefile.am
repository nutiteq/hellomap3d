
INCLUDES = @CFLAGS@
INCLUDES += -I$(top_srcdir)/src/headers -I.

noinst_HEADERS = dxf_private.h
noinst_LTLIBRARIES = libdxf.la

libdxf_la_SOURCES = dxf_parser.c dxf_loader.c \
	dxf_load_distinct.c dxf_load_mixed.c \
	dxf_writer.c

MOSTLYCLEANFILES = *.gcna *.gcno *.gcda
