
SUBDIRS = headers \
	gaiaaux \
	gaiaexif \
	gaiageo \
	geopackage \
	spatialite \
	shapefiles \
	srsinit \
	virtualtext \
	wfs \
	dxf \
	md5

INCLUDES = @CFLAGS@
INCLUDES += -I$(top_srcdir)/src/headers
INCLUDES += -I$(top_srcdir)

lib_LTLIBRARIES = libspatialite.la

libspatialite_la_SOURCES = versioninfo/version.c

libspatialite_la_LIBADD = ./gaiaaux/libgaiaaux.la \
	./gaiaexif/libgaiaexif.la \
	./gaiageo/libgaiageo.la \
	./geopackage/libgeopackage.la \
	./spatialite/libsplite.la \
	./shapefiles/libshapefiles.la \
	./dxf/libdxf.la \
	./md5/libmd5.la \
	./srsinit/libsrsinit.la \
	./virtualtext/libvirtualtext.la \
	./wfs/libwfs.la @LIBXML2_LIBS@

if MINGW
libspatialite_la_LDFLAGS = -version-info 4:1:0 -no-undefined
else 
if ANDROID
libspatialite_la_LDFLAGS = -version-info 6:0:1
libspatialite_la_LIBADD += -ldl
else
libspatialite_la_LDFLAGS = -version-info 6:0:1
libspatialite_la_LIBADD += -lpthread -ldl
endif
endif

MOSTLYCLEANFILES = *.gcna *.gcno *.gcda
