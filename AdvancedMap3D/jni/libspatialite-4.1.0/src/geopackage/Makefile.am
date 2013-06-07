AM_CFLAGS = $(GCOV_FLAGS)

INCLUDES = @CFLAGS@
INCLUDES += -I$(top_srcdir)/src/headers

noinst_LTLIBRARIES = libgeopackage.la 

libgeopackage_la_SOURCES = \
	gpkg_add_tile_triggers.c \
	gpkg_add_rt_metadata_triggers.c \
	gpkg_get_normal_row.c \
	gpkg_get_normal_zoom.c \
	gpkg_point_to_tile.c \
	gpkgGetImageType.c \
	gpkgCreateBaseTables.c \
	gpkgCreateTilesTable.c \
	gpkgCreateTilesZoomLevel.c

libgeopackage_la_CFLAGS = -fvisibility=hidden

MOSTLYCLEANFILES = *.gcna *.gcno *.gcda

EXTRA_DIST = geopackage_internal.h
