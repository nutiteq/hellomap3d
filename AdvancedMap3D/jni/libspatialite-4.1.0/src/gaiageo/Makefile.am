
SUBDIRS = flex lemon

INCLUDES = @CFLAGS@ @GEOS_CFLAGS@ @LIBXML2_CFLAGS@
INCLUDES += -I$(top_srcdir)/src/headers

noinst_LTLIBRARIES = libgaiageo.la

libgaiageo_la_SOURCES = gg_advanced.c \
	gg_endian.c \
	gg_geodesic.c \
	gg_geometries.c \
	gg_geoscvt.c \
	gg_relations.c \
	gg_lwgeom.c \
	gg_extras.c \
	gg_shape.c \
	gg_transform.c \
	gg_wkb.c \
	gg_wkt.c \
	gg_vanuatu.c \
	gg_ewkt.c \
	gg_geoJSON.c \
	gg_kml.c \
	gg_gml.c \
	gg_voronoj.c \
	gg_xml.c

MOSTLYCLEANFILES = *.gcna *.gcno *.gcda

EXTRA_DIST = Ewkt.h Ewkt.c lex.Ewkt.c \
	geoJSON.h geoJSON.c lex.GeoJson.c \
	Gml.h Gml.c lex.Gml.c \
	Kml.h Kml.c lex.Kml.c \
	vanuatuWkt.h vanuatuWkt.c lex.VanuatuWkt.c

