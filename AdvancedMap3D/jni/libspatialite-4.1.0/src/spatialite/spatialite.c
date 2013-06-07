/*

 spatialite.c -- SQLite3 spatial extension

 version 4.1, 2013 May 8

 Author: Sandro Furieri a.furieri@lqt.it

 ------------------------------------------------------------------------------
 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1
 
 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is the SpatiaLite library

The Initial Developer of the Original Code is Alessandro Furieri
 
Portions created by the Initial Developer are Copyright (C) 2008-2013
the Initial Developer. All Rights Reserved.

Contributor(s):
Pepijn Van Eeckhoudt <pepijnvaneeckhoudt@luciad.com>
(implementing Android support)

Alternatively, the contents of this file may be used under the terms of
either the GNU General Public License Version 2 or later (the "GPL"), or
the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
in which case the provisions of the GPL or the LGPL are applicable instead
of those above. If you wish to allow use of your version of this file only
under the terms of either the GPL or the LGPL, and not to allow others to
use your version of this file under the terms of the MPL, indicate your
decision by deleting the provisions above and replace them with the notice
and other provisions required by the GPL or the LGPL. If you do not delete
the provisions above, a recipient may use your version of this file under
the terms of any one of the MPL, the GPL or the LGPL.
 
*/

/*
 
CREDITS:

this module has been partly funded by:
Regione Toscana - Settore Sistema Informativo Territoriale ed Ambientale
(exposing liblwgeom APIs as SpatiaLite own SQL functions) 

*/

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <locale.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#include "config-msvc.h"
#else
#include "config.h"
#endif

#if defined(_WIN32) || defined(WIN32)
#include <io.h>
#define isatty	_isatty
#else
#include <unistd.h>
#endif


#include <spatialite/sqlite.h>
#include <spatialite/debug.h>

#include <spatialite/gaiaaux.h>
#include <spatialite/gaiageo.h>
#include <spatialite/gaiaexif.h>
#include <spatialite/geopackage.h>
#include <spatialite/spatialite.h>
#include <spatialite/gg_dxf.h>
#include <spatialite.h>
#include <spatialite_private.h>

#ifndef OMIT_GEOS		/* including GEOS */
#include <geos_c.h>
#endif

#ifndef OMIT_PROJ		/* including PROJ.4 */
#include <proj_api.h>
#endif

#ifdef _WIN32
#define strcasecmp	_stricmp
#endif /* not WIN32 */

/* 64 bit integer: portable format for printf() */
#if defined(_WIN32) && !defined(__MINGW32__)
#define FRMT64 "%I64d"
#define FRMT64_WO_PCT "I64d"
#else
#define FRMT64 "%lld"
#define FRMT64_WO_PCT "lld"
#endif

#define GAIA_UNUSED() if (argc || argv) argc = argc;

struct gaia_geom_chain_item
{
/* a struct used to store a chain item */
    gaiaGeomCollPtr geom;
    struct gaia_geom_chain_item *next;
};

struct gaia_geom_chain
{
/* a struct used to store a dynamic chain of GeometryCollections */
    int all_polygs;
    struct gaia_geom_chain_item *first;
    struct gaia_geom_chain_item *last;
};

#ifndef OMIT_GEOCALLBACKS	/* supporting RTree geometry callbacks */
struct gaia_rtree_mbr
{
/* a struct used by R*Tree GeometryCallback functions [MBR] */
    double minx;
    double miny;
    double maxx;
    double maxy;
};
#endif /* end RTree geometry callbacks */

struct stddev_str
{
/* a struct to implement StandardVariation and Variance aggregate functions */
    int cleaned;
    double mean;
    double quot;
    double count;
};

struct fdo_table
{
/* a struct to implement a linked-list for FDO-ORG table names */
    char *table;
    struct fdo_table *next;
};

static void
fnct_spatialite_version (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ spatialite_version()
/
/ return a text string representing the current SpatiaLite version
*/
    int len;
    const char *p_result = spatialite_version ();
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    len = strlen (p_result);
    sqlite3_result_text (context, p_result, len, SQLITE_TRANSIENT);
}

static void
fnct_geos_version (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ geos_version()
/
/ return a text string representing the current GEOS version
/ or NULL if GEOS is currently unsupported
*/

#ifndef OMIT_GEOS		/* GEOS version */
    int len;
    const char *p_result = GEOSversion ();
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    len = strlen (p_result);
    sqlite3_result_text (context, p_result, len, SQLITE_TRANSIENT);
#else
    sqlite3_result_null (context);
#endif
}

static void
fnct_proj4_version (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ proj4_version()
/
/ return a text string representing the current PROJ.4 version
/ or NULL if PROJ.4 is currently unsupported
*/

#ifndef OMIT_PROJ		/* PROJ.4 version */
    int len;
    const char *p_result = pj_get_release ();
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    len = strlen (p_result);
    sqlite3_result_text (context, p_result, len, SQLITE_TRANSIENT);
#else
    sqlite3_result_null (context);
#endif
}

static void
fnct_has_proj (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasProj()
/
/ return 1 if built including Proj.4; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_PROJ		/* PROJ.4 is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_geos (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasGeos()
/
/ return 1 if built including GEOS; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_GEOS		/* GEOS is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_geos_advanced (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ HasGeosAdvanced()
/
/ return 1 if built including GEOS-ADVANCED; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifdef GEOS_ADVANCED		/* GEOS-ADVANCED is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_geos_trunk (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasGeosTrunk()
/
/ return 1 if built including GEOS-TRUNK; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifdef GEOS_TRUNK		/* GEOS-TRUNK is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_lwgeom_version (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ lwgeom_version()
/
/ return a text string representing the current LWGEOM version
/ or NULL if LWGEOM is currently unsupported
*/

#ifdef ENABLE_LWGEOM		/* LWGEOM version */
    int len;
    const char *p_result = splite_lwgeom_version ();
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    len = strlen (p_result);
    sqlite3_result_text (context, p_result, len, SQLITE_TRANSIENT);
#else
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    sqlite3_result_null (context);
#endif
}

static void
fnct_libxml2_version (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ libxml2_version()
/
/ return a text string representing the current LIBXML2 version
/ or NULL if LIBXML2 is currently unsupported
*/

#ifdef ENABLE_LIBXML2		/* LIBXML2 version */
    int len;
    const char *p_result = gaia_libxml2_version ();
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    len = strlen (p_result);
    sqlite3_result_text (context, p_result, len, free);
#else
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    sqlite3_result_null (context);
#endif
}

static void
fnct_has_lwgeom (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasLwGeom()
/
/ return 1 if built including LWGEOM; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifdef ENABLE_LWGEOM		/* LWGEOM is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_iconv (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasIconv()
/
/ return 1 if built including ICONV; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_ICONV		/* ICONV is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_math_sql (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasMathSql()
/
/ return 1 if built including MATHSQL; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_MATHSQL		/* MATHSQL is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_geo_callbacks (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ HasGeoCallbacks()
/
/ return 1 if built enabling GEOCALLBACKS; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_GEOCALLBACKS	/* GEO-CALLBACKS are supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_freeXL (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasFreeXL()
/
/ return 1 if built including FreeXL; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_FREEXL		/* FreeXL is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_epsg (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasEpsg()
/
/ return 1 if built including EPSG; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifndef OMIT_EPSG		/* EPSG is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_has_libxml2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ HasLibXML2()
/
/ return 1 if built including LIBXML2; otherwise 0
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
#ifdef ENABLE_LIBXML2		/* LIBXML2 is supported */
    sqlite3_result_int (context, 1);
#else
    sqlite3_result_int (context, 0);
#endif
}

static void
fnct_GeometryConstraints (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ GeometryConstraints(BLOBencoded geometry, geometry-type, srid)
/ GeometryConstraints(BLOBencoded geometry, geometry-type, srid, dimensions)
/
/ checks geometry constraints, returning:
/
/ -1 - if some error occurred
/ 1 - if geometry constraints validation passes
/ 0 - if geometry constraints validation fails
/
*/
    int little_endian;
    int endian_arch = gaiaEndianArch ();
    unsigned char *p_blob = NULL;
    int n_bytes = 0;
    int srid;
    int geom_srid = -1;
    const char *type;
    int xtype;
    int geom_type = -1;
    int geom_normalized_type;
    const unsigned char *dimensions;
    int dims = GAIA_XY;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_BLOB
	|| sqlite3_value_type (argv[0]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_TEXT)
	type = (const char *) sqlite3_value_text (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  /* current metadata style >= v.4.0.0 */
	  type = "UNKNOWN";
	  switch (sqlite3_value_int (argv[1]))
	    {
	    case 0:
		type = "GEOMETRY";
		dims = GAIA_XY;
		break;
	    case 1:
		type = "POINT";
		dims = GAIA_XY;
		break;
	    case 2:
		type = "LINESTRING";
		dims = GAIA_XY;
		break;
	    case 3:
		type = "POLYGON";
		dims = GAIA_XY;
		break;
	    case 4:
		type = "MULTIPOINT";
		dims = GAIA_XY;
		break;
	    case 5:
		type = "MULTILINESTRING";
		dims = GAIA_XY;
		break;
	    case 6:
		type = "MULTIPOLYGON";
		dims = GAIA_XY;
		break;
	    case 7:
		type = "GEOMETRYCOLLECTION";
		dims = GAIA_XY;
		break;
	    case 1000:
		type = "GEOMETRY";
		dims = GAIA_XY_Z;
		break;
	    case 1001:
		type = "POINT";
		dims = GAIA_XY_Z;
		break;
	    case 1002:
		type = "LINESTRING";
		dims = GAIA_XY_Z;
		break;
	    case 1003:
		type = "POLYGON";
		dims = GAIA_XY_Z;
		break;
	    case 1004:
		type = "MULTIPOINT";
		dims = GAIA_XY_Z;
		break;
	    case 1005:
		type = "MULTILINESTRING";
		dims = GAIA_XY_Z;
		break;
	    case 1006:
		type = "MULTIPOLYGON";
		dims = GAIA_XY_Z;
		break;
	    case 1007:
		type = "GEOMETRYCOLLECTION";
		dims = GAIA_XY_Z;
		break;
	    case 2000:
		type = "GEOMETRY";
		dims = GAIA_XY_M;
		break;
	    case 2001:
		type = "POINT";
		dims = GAIA_XY_M;
		break;
	    case 2002:
		type = "LINESTRING";
		dims = GAIA_XY_M;
		break;
	    case 2003:
		type = "POLYGON";
		dims = GAIA_XY_M;
		break;
	    case 2004:
		type = "MULTIPOINT";
		dims = GAIA_XY_M;
		break;
	    case 2005:
		type = "MULTILINESTRING";
		dims = GAIA_XY_M;
		break;
	    case 2006:
		type = "MULTIPOLYGON";
		dims = GAIA_XY_M;
		break;
	    case 2007:
		type = "GEOMETRYCOLLECTION";
		dims = GAIA_XY_M;
		break;
	    case 3000:
		type = "GEOMETRY";
		dims = GAIA_XY_Z_M;
		break;
	    case 3001:
		type = "POINT";
		dims = GAIA_XY_Z_M;
		break;
	    case 3002:
		type = "LINESTRING";
		dims = GAIA_XY_Z_M;
		break;
	    case 3003:
		type = "POLYGON";
		dims = GAIA_XY_Z_M;
		break;
	    case 3004:
		type = "MULTIPOINT";
		dims = GAIA_XY_Z_M;
		break;
	    case 3005:
		type = "MULTILINESTRING";
		dims = GAIA_XY_Z_M;
		break;
	    case 3006:
		type = "MULTIPOLYGON";
		dims = GAIA_XY_Z_M;
		break;
	    case 3007:
		type = "GEOMETRYCOLLECTION";
		dims = GAIA_XY_Z_M;
		break;
	    };
      }
    else
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[2]);
    else
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (argc == 4)
      {
	  /* explicit dimensions - supporting XYZM */
	  dimensions = sqlite3_value_text (argv[3]);
	  if (strcasecmp ((char *) dimensions, "XYZ") == 0)
	      dims = GAIA_XY_Z;
	  else if (strcasecmp ((char *) dimensions, "XYM") == 0)
	      dims = GAIA_XY_M;
	  else if (strcasecmp ((char *) dimensions, "XYZM") == 0)
	      dims = GAIA_XY_Z_M;
	  else
	      dims = GAIA_XY;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_BLOB)
      {
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
      }
    if (p_blob)
      {
	  /* quick Geometry validation */
	  if (n_bytes < 45)
	      goto illegal_geometry;	/* cannot be an internal BLOB WKB geometry */
	  if (*(p_blob + 0) != GAIA_MARK_START)
	      goto illegal_geometry;	/* failed to recognize START signature */
	  if (*(p_blob + (n_bytes - 1)) != GAIA_MARK_END)
	      goto illegal_geometry;	/* failed to recognize END signature */
	  if (*(p_blob + 38) != GAIA_MARK_MBR)
	      goto illegal_geometry;	/* failed to recognize MBR signature */
	  if (*(p_blob + 1) == GAIA_LITTLE_ENDIAN)
	      little_endian = 1;
	  else if (*(p_blob + 1) == GAIA_BIG_ENDIAN)
	      little_endian = 0;
	  else
	      goto illegal_geometry;	/* unknown encoding; neither little-endian nor big-endian */
	  geom_type = gaiaImport32 (p_blob + 39, little_endian, endian_arch);
	  geom_srid = gaiaImport32 (p_blob + 2, little_endian, endian_arch);
	  goto valid_geometry;
	illegal_geometry:
	  sqlite3_result_int (context, -1);
	  return;
      }
  valid_geometry:
    xtype = GAIA_UNKNOWN;
    if (strcasecmp ((char *) type, "POINT") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_POINTZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_POINTM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_POINTZM;
		break;
	    default:
		xtype = GAIA_POINT;
		break;
	    };
      }
    if (strcasecmp ((char *) type, "LINESTRING") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_LINESTRINGZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_LINESTRINGM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_LINESTRINGZM;
		break;
	    default:
		xtype = GAIA_LINESTRING;
		break;
	    };
      }
    if (strcasecmp ((char *) type, "POLYGON") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_POLYGONZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_POLYGONM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_POLYGONZM;
		break;
	    default:
		xtype = GAIA_POLYGON;
		break;
	    };
      }
    if (strcasecmp ((char *) type, "MULTIPOINT") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_MULTIPOINTZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_MULTIPOINTM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_MULTIPOINTZM;
		break;
	    default:
		xtype = GAIA_MULTIPOINT;
		break;
	    };
      }
    if (strcasecmp ((char *) type, "MULTILINESTRING") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_MULTILINESTRINGZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_MULTILINESTRINGM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_MULTILINESTRINGZM;
		break;
	    default:
		xtype = GAIA_MULTILINESTRING;
		break;
	    };
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGON") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_MULTIPOLYGONZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_MULTIPOLYGONM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_MULTIPOLYGONZM;
		break;
	    default:
		xtype = GAIA_MULTIPOLYGON;
		break;
	    };
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTION") == 0)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_GEOMETRYCOLLECTIONZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_GEOMETRYCOLLECTIONM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_GEOMETRYCOLLECTIONZM;
		break;
	    default:
		xtype = GAIA_GEOMETRYCOLLECTION;
		break;
	    };
      }
    switch (geom_type)
      {
	  /* adjusting COMPRESSED Geometries */
      case GAIA_COMPRESSED_LINESTRING:
	  geom_normalized_type = GAIA_LINESTRING;
	  break;
      case GAIA_COMPRESSED_LINESTRINGZ:
	  geom_normalized_type = GAIA_LINESTRINGZ;
	  break;
      case GAIA_COMPRESSED_LINESTRINGM:
	  geom_normalized_type = GAIA_LINESTRINGM;
	  break;
      case GAIA_COMPRESSED_LINESTRINGZM:
	  geom_normalized_type = GAIA_LINESTRINGZM;
	  break;
      case GAIA_COMPRESSED_POLYGON:
	  geom_normalized_type = GAIA_POLYGON;
	  break;
      case GAIA_COMPRESSED_POLYGONZ:
	  geom_normalized_type = GAIA_POLYGONZ;
	  break;
      case GAIA_COMPRESSED_POLYGONM:
	  geom_normalized_type = GAIA_POLYGONM;
	  break;
      case GAIA_COMPRESSED_POLYGONZM:
	  geom_normalized_type = GAIA_POLYGONZM;
	  break;
      default:
	  geom_normalized_type = geom_type;
	  break;
      };
    if (strcasecmp ((char *) type, "GEOMETRY") == 0)
	xtype = -1;
    if (xtype == GAIA_UNKNOWN)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = 1;
	  if (p_blob)
	    {
		/* skipping NULL Geometry; this is assumed to be always good */
		if (geom_srid != srid)
		    ret = 0;
		if (xtype == -1)
		    ;
		else if (xtype != geom_normalized_type)
		    ret = 0;
	    }
	  sqlite3_result_int (context, ret);
      }
}

static void
fnct_RTreeAlign (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ RTreeAlign(RTree-table-name, PKID-value, BLOBencoded geometry)
/
/ attempts to update the associated R*Tree, returning:
/
/ -1 - if some invalid arg was passed
/ 1 - succesfull update
/ 0 - update failure
/
*/
    unsigned char *p_blob = NULL;
    int n_bytes = 0;
    sqlite3_int64 pkid;
    const char *rtree_table;
    char *table_name;
    int len;
    char pkv[64];
    gaiaGeomCollPtr geom = NULL;
    int ret;
    char *sql_statement;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
	rtree_table = (const char *) sqlite3_value_text (argv[0]);
    else
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	pkid = sqlite3_value_int64 (argv[1]);
    else
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_BLOB
	|| sqlite3_value_type (argv[2]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_BLOB)
      {
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[2]);
	  n_bytes = sqlite3_value_bytes (argv[2]);
	  geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
      }

    if (geom == NULL)
      {
	  /* NULL geometry: nothing to do */
	  sqlite3_result_int (context, 1);
      }
    else
      {
	  /* INSERTing into the R*Tree */
	  if (*(rtree_table + 0) == '"'
	      && *(rtree_table + strlen (rtree_table) - 1) == '"')
	    {
		/* earlier versions may pass an already quoted name */
		char *dequoted_table_name;
		len = strlen (rtree_table);
		table_name = malloc (len + 1);
		strcpy (table_name, rtree_table);
		dequoted_table_name = gaiaDequotedSql (table_name);
		free (table_name);
		if (dequoted_table_name == NULL)
		  {
		      sqlite3_result_int (context, -1);
		      return;
		  }
		table_name = gaiaDoubleQuotedSql (dequoted_table_name);
		free (dequoted_table_name);
	    }
	  else
	      table_name = gaiaDoubleQuotedSql (rtree_table);
	  sprintf (pkv, FRMT64, pkid);
	  sql_statement =
	      sqlite3_mprintf
	      ("INSERT INTO \"%s\" (pkid, xmin, ymin, xmax, ymax) "
	       "VALUES (%s, %1.12f, %1.12f, %1.12f, %1.12f)", table_name,
	       pkv, geom->MinX, geom->MinY, geom->MaxX, geom->MaxY);
	  gaiaFreeGeomColl (geom);
	  ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, NULL);
	  sqlite3_free (sql_statement);
	  free (table_name);
	  if (ret != SQLITE_OK)
	      sqlite3_result_int (context, 0);
	  else
	      sqlite3_result_int (context, 1);
      }
}

static void
fnct_IsValidNoDataPixel (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ IsValidNoDataPixel(BLOBencoded pixel, text sample_type, int num_bands)
/
/ basic version intended to be overloaded by RasterLite-2
/ always return 0 (FALSE)
/ or -1 (INVALID ARGS)
/
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
	sqlite3_result_int (context, -1);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
	sqlite3_result_int (context, -1);
    if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
	sqlite3_result_int (context, -1);
    sqlite3_result_int (context, 0);
}

static void
fnct_IsPopulatedCoverage (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ IsPopulatedCoverage()
/
/ check if a RasterCoverage is already populated 
/ returns 1 if TRUE, 0 if FALSE
/ -1 on invalid arguments
*/
    const char *coverage;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    coverage = (const char *) sqlite3_value_text (argv[0]);
    ret = checkPopulatedCoverage (sqlite, coverage);
    sqlite3_result_int (context, ret);
    return;
}

SPATIALITE_PRIVATE int
checkSpatialMetaData (const void *handle)
{
/* internal utility function:
/
/ for FDO-OGR interoperability and cross-version seamless compatibility:
/ tests the SpatialMetadata type, returning:
/
/ 0 - if no valid SpatialMetaData where found
/ 1 - if SpatiaLite-like (legacy) SpatialMetadata where found
/ 2 - if FDO-OGR-like SpatialMetadata where found
/ 3 - if SpatiaLite-like (current) SpatialMetadata where found
/
*/
    sqlite3 *sqlite = (sqlite3 *) handle;
    int spatialite_legacy_rs = 0;
    int spatialite_rs = 0;
    int fdo_rs = 0;
    int spatialite_legacy_gc = 0;
    int spatialite_gc = 0;
    int fdo_gc = 0;
    int rs_srid = 0;
    int auth_name = 0;
    int auth_srid = 0;
    int srtext = 0;
    int ref_sys_name = 0;
    int proj4text = 0;
    int f_table_name = 0;
    int f_geometry_column = 0;
    int geometry_type = 0;
    int coord_dimension = 0;
    int gc_srid = 0;
    int geometry_format = 0;
    int type = 0;
    int spatial_index_enabled = 0;
    char sql[1024];
    int ret;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;
/* checking the GEOMETRY_COLUMNS table */
    strcpy (sql, "PRAGMA table_info(geometry_columns)");
    ret = sqlite3_get_table (sqlite, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "f_table_name") == 0)
		    f_table_name = 1;
		if (strcasecmp (name, "f_geometry_column") == 0)
		    f_geometry_column = 1;
		if (strcasecmp (name, "geometry_type") == 0)
		    geometry_type = 1;
		if (strcasecmp (name, "coord_dimension") == 0)
		    coord_dimension = 1;
		if (strcasecmp (name, "srid") == 0)
		    gc_srid = 1;
		if (strcasecmp (name, "geometry_format") == 0)
		    geometry_format = 1;
		if (strcasecmp (name, "type") == 0)
		    type = 1;
		if (strcasecmp (name, "spatial_index_enabled") == 0)
		    spatial_index_enabled = 1;
	    }
      }
    sqlite3_free_table (results);
    if (f_table_name && f_geometry_column && type && coord_dimension
	&& gc_srid && spatial_index_enabled)
	spatialite_legacy_gc = 1;
    if (f_table_name && f_geometry_column && geometry_type && coord_dimension
	&& gc_srid && spatial_index_enabled)
	spatialite_gc = 1;
    if (f_table_name && f_geometry_column && geometry_type && coord_dimension
	&& gc_srid && geometry_format)
	fdo_gc = 1;
/* checking the SPATIAL_REF_SYS table */
    strcpy (sql, "PRAGMA table_info(spatial_ref_sys)");
    ret = sqlite3_get_table (sqlite, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "srid") == 0)
		    rs_srid = 1;
		if (strcasecmp (name, "auth_name") == 0)
		    auth_name = 1;
		if (strcasecmp (name, "auth_srid") == 0)
		    auth_srid = 1;
		if (strcasecmp (name, "srtext") == 0)
		    srtext = 1;
		if (strcasecmp (name, "ref_sys_name") == 0)
		    ref_sys_name = 1;
		if (strcasecmp (name, "proj4text") == 0)
		    proj4text = 1;
		if (strcasecmp (name, "srtext") == 0)
		    srtext = 1;
	    }
      }
    sqlite3_free_table (results);
    if (rs_srid && auth_name && auth_srid && ref_sys_name && proj4text
	&& srtext)
	spatialite_rs = 1;
    if (rs_srid && auth_name && auth_srid && ref_sys_name && proj4text)
	spatialite_legacy_rs = 1;
    if (rs_srid && auth_name && auth_srid && srtext)
	fdo_rs = 1;
/* verifying the MetaData format */
    if (spatialite_legacy_gc && spatialite_legacy_rs)
	return 1;
    if (fdo_gc && fdo_rs)
	return 2;
    if (spatialite_gc && spatialite_rs)
	return 3;
  unknown:
    return 0;
}

static void
add_fdo_table (struct fdo_table **first, struct fdo_table **last,
	       const char *table, int len)
{
/* adds an FDO-OGR styled Geometry Table to corresponding linked list */
    struct fdo_table *p = malloc (sizeof (struct fdo_table));
    p->table = malloc (len + 1);
    strcpy (p->table, table);
    p->next = NULL;
    if (!(*first))
	(*first) = p;
    if ((*last))
	(*last)->next = p;
    (*last) = p;
}

static void
free_fdo_tables (struct fdo_table *first)
{
/* memory cleanup; destroying the FDO-OGR tables linked list */
    struct fdo_table *p;
    struct fdo_table *pn;
    p = first;
    while (p)
      {
	  pn = p->next;
	  if (p->table)
	      free (p->table);
	  free (p);
	  p = pn;
      }
}

static void
fnct_AutoFDOStart (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AutoFDOStart(void)
/
/ for FDO-OGR interoperability:
/ tests the SpatialMetadata type, then automatically
/ creating a VirtualFDO table for each FDO-OGR main table 
/ declared within FDO-styled SpatialMetadata
/
*/
    int ret;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;
    char *sql_statement;
    int count = 0;
    struct fdo_table *first = NULL;
    struct fdo_table *last = NULL;
    struct fdo_table *p;
    int len;
    char *xname;
    char *xxname;
    char *xtable;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (checkSpatialMetaData (sqlite) == 2)
      {
	  /* ok, creating VirtualFDO tables */
	  sql_statement = "SELECT DISTINCT f_table_name FROM geometry_columns";
	  ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows,
				   &columns, NULL);
	  if (ret != SQLITE_OK)
	      goto error;
	  if (rows < 1)
	      ;
	  else
	    {
		for (i = 1; i <= rows; i++)
		  {
		      name = results[(i * columns) + 0];
		      if (name)
			{
			    len = strlen (name);
			    add_fdo_table (&first, &last, name, len);
			}
		  }
	    }
	  sqlite3_free_table (results);
	  p = first;
	  while (p)
	    {
		/* destroying the VirtualFDO table [if existing] */
		xxname = sqlite3_mprintf ("fdo_%s", p->table);
		xname = gaiaDoubleQuotedSql (xxname);
		sqlite3_free (xxname);
		sql_statement =
		    sqlite3_mprintf ("DROP TABLE IF EXISTS \"%s\"", xname);
		free (xname);
		ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, NULL);
		sqlite3_free (sql_statement);
		if (ret != SQLITE_OK)
		    goto error;
		/* creating the VirtualFDO table */
		xxname = sqlite3_mprintf ("fdo_%s", p->table);
		xname = gaiaDoubleQuotedSql (xxname);
		sqlite3_free (xxname);
		xtable = gaiaDoubleQuotedSql (p->table);
		sql_statement =
		    sqlite3_mprintf
		    ("CREATE VIRTUAL TABLE \"%s\" USING VirtualFDO(\"%s\")",
		     xname, xtable);
		free (xname);
		free (xtable);
		ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, NULL);
		sqlite3_free (sql_statement);
		if (ret != SQLITE_OK)
		    goto error;
		count++;
		p = p->next;
	    }
	error:
	  free_fdo_tables (first);
	  sqlite3_result_int (context, count);
	  return;
      }
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_AutoFDOStop (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AutoFDOStop(void)
/
/ for FDO-OGR interoperability:
/ tests the SpatialMetadata type, then automatically
/ removes any VirtualFDO table 
/
*/
    int ret;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;
    char *sql_statement;
    int count = 0;
    struct fdo_table *first = NULL;
    struct fdo_table *last = NULL;
    struct fdo_table *p;
    int len;
    char *xname;
    char *xxname;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (checkSpatialMetaData (sqlite) == 2)
      {
	  /* ok, creating VirtualFDO tables */
	  sql_statement = "SELECT DISTINCT f_table_name FROM geometry_columns";
	  ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows,
				   &columns, NULL);
	  if (ret != SQLITE_OK)
	      goto error;
	  if (rows < 1)
	      ;
	  else
	    {
		for (i = 1; i <= rows; i++)
		  {
		      name = results[(i * columns) + 0];
		      if (name)
			{
			    len = strlen (name);
			    add_fdo_table (&first, &last, name, len);
			}
		  }
	    }
	  sqlite3_free_table (results);
	  p = first;
	  while (p)
	    {
		/* destroying the VirtualFDO table [if existing] */
		xxname = sqlite3_mprintf ("fdo_%s", p->table);
		xname = gaiaDoubleQuotedSql (xxname);
		sqlite3_free (xxname);
		sql_statement =
		    sqlite3_mprintf ("DROP TABLE IF EXISTS \"%s\"", xname);
		free (xname);
		ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, NULL);
		sqlite3_free (sql_statement);
		if (ret != SQLITE_OK)
		    goto error;
		count++;
		p = p->next;
	    }
	error:
	  free_fdo_tables (first);
	  sqlite3_result_int (context, count);
	  return;
      }
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_CheckSpatialMetaData (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ CheckSpatialMetaData(void)
/
/ for FDO-OGR interoperability:
/ tests the SpatialMetadata type, returning:
/
/ 0 - if no valid SpatialMetaData where found
/ 1 - if SpatiaLite-legacy SpatialMetadata where found
/ 2- if FDO-OGR-like SpatialMetadata where found
/ 3 - if SpatiaLite-current SpatialMetadata where found
/
*/
    sqlite3 *sqlite;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    sqlite = sqlite3_context_db_handle (context);
    ret = checkSpatialMetaData (sqlite);
    if (ret == 3)
      {
	  /* trying to create the advanced metadata tables >= v.4.0.0 */
	  createAdvancedMetaData (sqlite);
      }
    sqlite3_result_int (context, ret);
    return;
}

static void
fnct_InitSpatialMetaData (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ InitSpatialMetaData()
/     or
/ InitSpatialMetaData(text mode)
/     or
/ InitSpatialMetaData(integer transaction)
/     or
/ InitSpatialMetaData(integer transaction, text mode)
/
/ creates the SPATIAL_REF_SYS and GEOMETRY_COLUMNS tables
/ returns 1 on success
/ 0 on failure
*/
    char sql[8192];
    char *errMsg = NULL;
    int ret;
    int transaction = 0;
    const char *xmode;
    int mode = GAIA_EPSG_ANY;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 1)
      {
	  if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
	    {
		xmode = (const char *) sqlite3_value_text (argv[0]);
		if (strcasecmp (xmode, "NONE") == 0
		    || strcasecmp (xmode, "EMPTY") == 0)
		    mode = GAIA_EPSG_NONE;
		if (strcasecmp (xmode, "WGS84") == 0
		    || strcasecmp (xmode, "WGS84_ONLY") == 0)
		    mode = GAIA_EPSG_WGS84_ONLY;
	    }
	  else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
	      transaction = sqlite3_value_int (argv[0]);
	  else
	    {
		spatialite_e
		    ("InitSpatialMetaData() error: argument 1 is not of the String or Integer type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_INTEGER)
	    {
		spatialite_e
		    ("InitSpatialMetaData() error: argument 1 is not of the Integer type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
	    {
		spatialite_e
		    ("InitSpatialMetaData() error: argument 2 is not of the String type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  transaction = sqlite3_value_int (argv[0]);
	  xmode = (const char *) sqlite3_value_text (argv[1]);
	  if (strcasecmp (xmode, "NONE") == 0
	      || strcasecmp (xmode, "EMPTY") == 0)
	      mode = GAIA_EPSG_NONE;
	  if (strcasecmp (xmode, "WGS84") == 0
	      || strcasecmp (xmode, "WGS84_ONLY") == 0)
	      mode = GAIA_EPSG_WGS84_ONLY;
      }

    if (transaction)
      {
	  /* starting a Transaction */
	  ret = sqlite3_exec (sqlite, "BEGIN", NULL, NULL, &errMsg);
	  if (ret != SQLITE_OK)
	      goto error;
      }

/* creating the SPATIAL_REF_SYS table */
    strcpy (sql, "CREATE TABLE spatial_ref_sys (\n");
    strcat (sql, "srid INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "auth_name TEXT NOT NULL,\n");
    strcat (sql, "auth_srid INTEGER NOT NULL,\n");
    strcat (sql, "ref_sys_name TEXT NOT NULL DEFAULT 'Unknown',\n");
    strcat (sql, "proj4text TEXT NOT NULL,\n");
    strcat (sql, "srtext TEXT NOT NULL DEFAULT 'Undefined')");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &errMsg);
    if (ret != SQLITE_OK)
	goto error;
    strcpy (sql, "CREATE UNIQUE INDEX idx_spatial_ref_sys \n");
    strcat (sql, "ON spatial_ref_sys (auth_srid, auth_name)");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &errMsg);
    if (ret != SQLITE_OK)
	goto error;
    updateSpatiaLiteHistory (sqlite, "spatial_ref_sys", NULL,
			     "table successfully created");

/* creating the GEOMETRY_COLUMNS table */
    if (!createGeometryColumns (sqlite))
	goto error;

/* creating the GEOM_COLS_REF_SYS view */
    strcpy (sql, "CREATE VIEW geom_cols_ref_sys AS\n");
    strcat (sql, "SELECT f_table_name, f_geometry_column, geometry_type,\n");
    strcat (sql, "coord_dimension, spatial_ref_sys.srid AS srid,\n");
    strcat (sql, "auth_name, auth_srid, ref_sys_name, proj4text, srtext\n");
    strcat (sql, "FROM geometry_columns, spatial_ref_sys\n");
    strcat (sql, "WHERE geometry_columns.srid = spatial_ref_sys.srid");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &errMsg);
    updateSpatiaLiteHistory (sqlite, "geom_cols_ref_sys", NULL,
			     "view 'geom_cols_ref_sys' successfully created");
    if (ret != SQLITE_OK)
	goto error;
    if (!createAdvancedMetaData (sqlite))
	goto error;
/* creating the SpatialIndex VIRTUAL TABLE */
    strcpy (sql, "CREATE VIRTUAL TABLE SpatialIndex ");
    strcat (sql, "USING VirtualSpatialIndex()");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &errMsg);
    if (ret != SQLITE_OK)
	goto error;
    if (spatial_ref_sys_init2 (sqlite, mode, 0))
      {
	  if (mode == GAIA_EPSG_NONE)
	      updateSpatiaLiteHistory (sqlite, "spatial_ref_sys", NULL,
				       "table successfully created [empty]");
	  else
	      updateSpatiaLiteHistory (sqlite, "spatial_ref_sys", NULL,
				       "table successfully populated");
      }

    if (transaction)
      {
	  /* confirming the still pending Transaction */
	  ret = sqlite3_exec (sqlite, "COMMIT", NULL, NULL, &errMsg);
	  if (ret != SQLITE_OK)
	      goto error;
      }

    sqlite3_result_int (context, 1);
    return;
  error:
    spatialite_e (" InitSpatiaMetaData() error:\"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    if (transaction)
      {
	  /* performing a Rollback */
	  ret = sqlite3_exec (sqlite, "ROLLBACK", NULL, NULL, &errMsg);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e (" InitSpatiaMetaData() error:\"%s\"\n", errMsg);
		sqlite3_free (errMsg);
	    }
      }
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_InsertEpsgSrid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ InsertEpsgSrid(int srid)
/
/ returns 1 on success: 0 on failure
*/
    int srid;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[0]);
    else
      {
	  sqlite3_result_int (context, 0);
	  return;
      }
    ret = insert_epsg_srid (sqlite, srid);
    if (!ret)
	sqlite3_result_int (context, 0);
    else
	sqlite3_result_int (context, 1);
}

static int
recoverGeomColumn (sqlite3 * sqlite, const char *table,
		   const char *column, int xtype, int dims, int srid)
{
/* checks if TABLE.COLUMN exists and has the required features */
    int ok = 1;
    int type;
    sqlite3_stmt *stmt;
    gaiaGeomCollPtr geom;
    const void *blob_value;
    int len;
    int ret;
    int i_col;
    int is_nullable = 1;
    char *p_table;
    char *p_column;
    char *sql_statement;

/* testing if NOT NULL */
    p_table = gaiaDoubleQuotedSql (table);
    sql_statement = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", p_table);
    free (p_table);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("recoverGeomColumn: \"%s\"\n", sqlite3_errmsg (sqlite));
	  return 0;
      }
    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		if (strcasecmp
		    (column, (const char *) sqlite3_column_text (stmt, 1)) == 0)
		  {
		      if (sqlite3_column_int (stmt, 2) != 0)
			  is_nullable = 0;
		  }
	    }
      }
    sqlite3_finalize (stmt);

    p_table = gaiaDoubleQuotedSql (table);
    p_column = gaiaDoubleQuotedSql (column);
    sql_statement =
	sqlite3_mprintf ("SELECT \"%s\" FROM \"%s\"", p_column, p_table);
    free (p_table);
    free (p_column);
/* compiling SQL prepared statement */
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("recoverGeomColumn: error %d \"%s\"\n",
			sqlite3_errcode (sqlite), sqlite3_errmsg (sqlite));
	  return 0;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* checking Geometry features */
		geom = NULL;
		for (i_col = 0; i_col < sqlite3_column_count (stmt); i_col++)
		  {
		      if (sqlite3_column_type (stmt, i_col) == SQLITE_NULL)
			{
			    /* found a NULL geometry */
			    if (!is_nullable)
				ok = 0;
			}
		      else if (sqlite3_column_type (stmt, i_col) != SQLITE_BLOB)
			  ok = 0;
		      else
			{
			    blob_value = sqlite3_column_blob (stmt, i_col);
			    len = sqlite3_column_bytes (stmt, i_col);
			    geom = gaiaFromSpatiaLiteBlobWkb (blob_value, len);
			    if (!geom)
				ok = 0;
			    else
			      {
				  if (geom->DimensionModel != dims)
				      ok = 0;
				  if (geom->Srid != srid)
				      ok = 0;
				  type = gaiaGeometryType (geom);
				  if (xtype == -1)
				      ;	/* GEOMETRY */
				  else
				    {
					if (xtype == type)
					    ;
					else
					    ok = 0;
				    }
				  gaiaFreeGeomColl (geom);
			      }
			}
		  }
	    }
	  if (!ok)
	      break;
      }
    ret = sqlite3_finalize (stmt);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("recoverGeomColumn: error %d \"%s\"\n",
			sqlite3_errcode (sqlite), sqlite3_errmsg (sqlite));
	  return 0;
      }
    return ok;
}

static void
fnct_AddGeometryColumn (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ AddGeometryColumn(table, column, srid, type [ , dimension  [  , not-null ] ] )
/
/ creates a new COLUMN of given TYPE into TABLE
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    const unsigned char *type;
    const unsigned char *txt_dims;
    int xtype;
    int srid = -1;
    int dimension = 2;
    int dims = -1;
    int auto_dims = -1;
    char sql[1024];
    int ret;
    int notNull = 0;
    int metadata_version;
    sqlite3_stmt *stmt;
    char *p_table = NULL;
    const char *name;
    int len;
    char *quoted_table;
    char *quoted_column;
    const char *p_type;
    const char *p_dims;
    int n_type;
    int n_dims;
    char *sql_statement;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 3 [SRID] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    srid = sqlite3_value_int (argv[2]);
    if (sqlite3_value_type (argv[3]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 4 [geometry_type] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    type = sqlite3_value_text (argv[3]);
    if (argc > 4)
      {
	  if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	    {
		dimension = sqlite3_value_int (argv[4]);
		if (dimension == 2)
		    dims = GAIA_XY;
		if (dimension == 3)
		    dims = GAIA_XY_Z;
		if (dimension == 4)
		    dims = GAIA_XY_Z_M;
	    }
	  else if (sqlite3_value_type (argv[4]) == SQLITE_TEXT)
	    {
		txt_dims = sqlite3_value_text (argv[4]);
		if (strcasecmp ((char *) txt_dims, "XY") == 0)
		    dims = GAIA_XY;
		if (strcasecmp ((char *) txt_dims, "XYZ") == 0)
		    dims = GAIA_XY_Z;
		if (strcasecmp ((char *) txt_dims, "XYM") == 0)
		    dims = GAIA_XY_M;
		if (strcasecmp ((char *) txt_dims, "XYZM") == 0)
		    dims = GAIA_XY_Z_M;
	    }
	  else
	    {
		spatialite_e
		    ("AddGeometryColumn() error: argument 5 [dimension] is not of the Integer or Text type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
      }
    if (argc == 6)
      {
	  /* optional NOT NULL arg */
	  if (sqlite3_value_type (argv[5]) != SQLITE_INTEGER)
	    {
		spatialite_e
		    ("AddGeometryColumn() error: argument 6 [not null] is not of the Integer type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  notNull = sqlite3_value_int (argv[5]);
      }
    xtype = GAIA_UNKNOWN;
    if (strcasecmp ((char *) type, "POINT") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRING") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGON") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINT") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRING") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGON") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTION") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRY") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = -1;
      }
    if (strcasecmp ((char *) type, "POINTZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRINGZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGONZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINTZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRINGZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGONZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTIONZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRYZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = -1;
      }
    if (strcasecmp ((char *) type, "POINTM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRINGM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGONM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINTM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRINGM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGONM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTIONM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRYM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = -1;
      }
    if (strcasecmp ((char *) type, "POINTZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRINGZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGONZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINTZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRINGZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGONZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTIONZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRYZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = -1;
      }
    if (xtype == GAIA_UNKNOWN)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 4 [geometry_type] has an illegal value\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (dims < 0)
	dims = auto_dims;
    if (dims == GAIA_XY || dims == GAIA_XY_Z || dims == GAIA_XY_M
	|| dims == GAIA_XY_Z_M)
	;
    else
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 5 [dimension] ILLEGAL VALUE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (auto_dims != GAIA_XY && dims != auto_dims)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: argument 5 [dimension] ILLEGAL VALUE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
/* checking if the table exists */
    strcpy (sql,
	    "SELECT name FROM sqlite_master WHERE type = 'table' AND Lower(name) = Lower(?)");
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("AddGeometryColumn: \"%s\"\n", sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		name = (const char *) sqlite3_column_text (stmt, 0);
		len = sqlite3_column_bytes (stmt, 0);
		if (p_table)
		    free (p_table);
		p_table = malloc (len + 1);
		strcpy (p_table, name);
	    }
      }
    sqlite3_finalize (stmt);
    if (!p_table)
      {
	  spatialite_e
	      ("AddGeometryColumn() error: table '%s' does not exist\n", table);
	  sqlite3_result_int (context, 0);
	  return;
      }
    metadata_version = checkSpatialMetaData (sqlite);
    if (metadata_version == 1 || metadata_version == 3)
	;
    else
      {
	  spatialite_e
	      ("AddGeometryColumn() error: unexpected metadata layout\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
/* trying to add the column */
    switch (xtype)
      {
      case GAIA_POINT:
	  p_type = "POINT";
	  break;
      case GAIA_LINESTRING:
	  p_type = "LINESTRING";
	  break;
      case GAIA_POLYGON:
	  p_type = "POLYGON";
	  break;
      case GAIA_MULTIPOINT:
	  p_type = "MULTIPOINT";
	  break;
      case GAIA_MULTILINESTRING:
	  p_type = "MULTILINESTRING";
	  break;
      case GAIA_MULTIPOLYGON:
	  p_type = "MULTIPOLYGON";
	  break;
      case GAIA_GEOMETRYCOLLECTION:
	  p_type = "GEOMETRYCOLLECTION";
	  break;
      case -1:
	  p_type = "GEOMETRY";
	  break;
      };
    quoted_table = gaiaDoubleQuotedSql (p_table);
    quoted_column = gaiaDoubleQuotedSql (column);
    if (notNull)
      {
	  /* adding a NOT NULL clause */
	  sql_statement =
	      sqlite3_mprintf ("ALTER TABLE \"%s\" ADD COLUMN \"%s\" "
			       "%s NOT NULL DEFAULT ''", quoted_table,
			       quoted_column, p_type);
      }
    else
	sql_statement =
	    sqlite3_mprintf ("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s ",
			     quoted_table, quoted_column, p_type);
    free (quoted_table);
    free (quoted_column);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("AddGeometryColumn: \"%s\"\n", sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  free (p_table);
	  return;
      }
/*ok, inserting into geometry_columns [Spatial Metadata] */
    if (metadata_version == 1)
      {
	  /* legacy metadata style <= v.3.1.0 */
	  switch (xtype)
	    {
	    case GAIA_POINT:
		p_type = "POINT";
		break;
	    case GAIA_LINESTRING:
		p_type = "LINESTRING";
		break;
	    case GAIA_POLYGON:
		p_type = "POLYGON";
		break;
	    case GAIA_MULTIPOINT:
		p_type = "MULTIPOINT";
		break;
	    case GAIA_MULTILINESTRING:
		p_type = "MULTILINESTRING";
		break;
	    case GAIA_MULTIPOLYGON:
		p_type = "MULTIPOLYGON";
		break;
	    case GAIA_GEOMETRYCOLLECTION:
		p_type = "GEOMETRYCOLLECTION";
		break;
	    case -1:
		p_type = "GEOMETRY";
		break;
	    };
	  switch (dims)
	    {
	    case GAIA_XY:
		p_dims = "XY";
		break;
	    case GAIA_XY_Z:
		p_dims = "XYZ";
		break;
	    case GAIA_XY_M:
		p_dims = "XYM";
		break;
	    case GAIA_XY_Z_M:
		p_dims = "XYZM";
		break;
	    };
	  sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
					   "(f_table_name, f_geometry_column, type, coord_dimension, srid, "
					   "spatial_index_enabled) VALUES (?, ?, %Q, %Q, ?, 0)",
					   p_type, p_dims);
      }
    else
      {
	  /* current metadata style >= v.4.0.0 */
	  switch (xtype)
	    {
	    case GAIA_POINT:
		if (dims == GAIA_XY_Z)
		    n_type = 1001;
		else if (dims == GAIA_XY_M)
		    n_type = 2001;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3001;
		else
		    n_type = 1;
		break;
	    case GAIA_LINESTRING:
		if (dims == GAIA_XY_Z)
		    n_type = 1002;
		else if (dims == GAIA_XY_M)
		    n_type = 2002;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3002;
		else
		    n_type = 2;
		break;
	    case GAIA_POLYGON:
		if (dims == GAIA_XY_Z)
		    n_type = 1003;
		else if (dims == GAIA_XY_M)
		    n_type = 2003;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3003;
		else
		    n_type = 3;
		break;
	    case GAIA_MULTIPOINT:
		if (dims == GAIA_XY_Z)
		    n_type = 1004;
		else if (dims == GAIA_XY_M)
		    n_type = 2004;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3004;
		else
		    n_type = 4;
		break;
	    case GAIA_MULTILINESTRING:
		if (dims == GAIA_XY_Z)
		    n_type = 1005;
		else if (dims == GAIA_XY_M)
		    n_type = 2005;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3005;
		else
		    n_type = 5;
		break;
	    case GAIA_MULTIPOLYGON:
		if (dims == GAIA_XY_Z)
		    n_type = 1006;
		else if (dims == GAIA_XY_M)
		    n_type = 2006;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3006;
		else
		    n_type = 6;
		break;
	    case GAIA_GEOMETRYCOLLECTION:
		if (dims == GAIA_XY_Z)
		    n_type = 1007;
		else if (dims == GAIA_XY_M)
		    n_type = 2007;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3007;
		else
		    n_type = 7;
		break;
	    case -1:
		if (dims == GAIA_XY_Z)
		    n_type = 1000;
		else if (dims == GAIA_XY_M)
		    n_type = 2000;
		else if (dims == GAIA_XY_Z_M)
		    n_type = 3000;
		else
		    n_type = 0;
		break;
	    };
	  switch (dims)
	    {
	    case GAIA_XY:
		n_dims = 2;
		break;
	    case GAIA_XY_Z:
	    case GAIA_XY_M:
		n_dims = 3;
		break;
	    case GAIA_XY_Z_M:
		n_dims = 4;
		break;
	    };
	  sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
					   "(f_table_name, f_geometry_column, geometry_type, coord_dimension, "
					   "srid, spatial_index_enabled) VALUES (Lower(?), Lower(?), %d, %d, ?, 0)",
					   n_type, n_dims);
      }
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("AddGeometryColumn: \"%s\"\n", sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  free (p_table);
	  return;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, p_table, strlen (p_table), SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
    if (srid < 0)
	sqlite3_bind_int (stmt, 3, -1);
    else
	sqlite3_bind_int (stmt, 3, srid);
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  spatialite_e ("AddGeometryColumn() error: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_finalize (stmt);
	  goto error;
      }
    sqlite3_finalize (stmt);
    if (metadata_version == 3)
      {
	  /* current metadata style >= v.4.0.0 */

	  /* inserting a row into GEOMETRY_COLUMNS_AUTH */
	  strcpy (sql,
		  "INSERT OR REPLACE INTO geometry_columns_auth (f_table_name, f_geometry_column, ");
	  strcat (sql, "read_only, hidden) VALUES (Lower(?), Lower(?), 0, 0)");
	  ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("AddGeometryColumn: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_result_int (context, 0);
		free (p_table);
		return;
	    }
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("AddGeometryColumn() error: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  sqlite3_finalize (stmt);
	  /* inserting a row into GEOMETRY_COLUMNS_STATISTICS */
	  strcpy (sql,
		  "INSERT OR REPLACE INTO geometry_columns_statistics (f_table_name, f_geometry_column) ");
	  strcat (sql, "VALUES (Lower(?), Lower(?))");
	  ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("AddGeometryColumn: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_result_int (context, 0);
		free (p_table);
		return;
	    }
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("AddGeometryColumn() error: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  sqlite3_finalize (stmt);
	  /* inserting a row into GEOMETRY_COLUMNS_TIME */
	  strcpy (sql,
		  "INSERT OR REPLACE INTO geometry_columns_time (f_table_name, f_geometry_column) ");
	  strcat (sql, "VALUES (Lower(?), Lower(?))");
	  ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("AddGeometryColumn: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_result_int (context, 0);
		free (p_table);
		return;
	    }
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("AddGeometryColumn() error: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  sqlite3_finalize (stmt);
      }
    updateGeometryTriggers (sqlite, table, column);
    sqlite3_result_int (context, 1);
    switch (xtype)
      {
      case GAIA_POINT:
	  p_type = "POINT";
	  break;
      case GAIA_LINESTRING:
	  p_type = "LINESTRING";
	  break;
      case GAIA_POLYGON:
	  p_type = "POLYGON";
	  break;
      case GAIA_MULTIPOINT:
	  p_type = "MULTIPOINT";
	  break;
      case GAIA_MULTILINESTRING:
	  p_type = "MULTILINESTRING";
	  break;
      case GAIA_MULTIPOLYGON:
	  p_type = "MULTIPOLYGON";
	  break;
      case GAIA_GEOMETRYCOLLECTION:
	  p_type = "GEOMETRYCOLLECTION";
	  break;
      case -1:
	  p_type = "GEOMETRY";
	  break;
      };
    switch (dims)
      {
      case GAIA_XY:
	  p_dims = "XY";
	  break;
      case GAIA_XY_Z:
	  p_dims = "XYZ";
	  break;
      case GAIA_XY_M:
	  p_dims = "XYM";
	  break;
      case GAIA_XY_Z_M:
	  p_dims = "XYZM";
	  break;
      };
    sql_statement =
	sqlite3_mprintf ("Geometry [%s,%s,SRID=%d] successfully created",
			 p_type, p_dims, (srid <= 0) ? -1 : srid);
    updateSpatiaLiteHistory (sqlite, table, column, sql_statement);
    sqlite3_free (sql_statement);
    free (p_table);
    return;
  error:
    sqlite3_result_int (context, 0);
    free (p_table);
    return;
}

static void
fnct_RecoverGeometryColumn (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ RecoverGeometryColumn(table, column, srid, type , dimension )
/
/ checks if an existing TABLE.COLUMN satisfies the required geometric features
/ if yes adds it to SpatialMetaData and enabling triggers
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    const unsigned char *type;
    int xtype;
    int xxtype;
    int srid = -1;
    const unsigned char *txt_dims;
    int dimension = 2;
    int dims = -1;
    int auto_dims = -1;
    char sql[1024];
    int ret;
    int metadata_version;
    sqlite3_stmt *stmt;
    int exists = 0;
    const char *p_type;
    const char *p_dims;
    int n_type;
    int n_dims;
    char *sql_statement;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 3 [SRID] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    srid = sqlite3_value_int (argv[2]);
    if (sqlite3_value_type (argv[3]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 4 [geometry_type] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    type = sqlite3_value_text (argv[3]);
    if (argc == 5)
      {
	  if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	    {
		dimension = sqlite3_value_int (argv[4]);
		if (dimension == 2)
		    dims = GAIA_XY;
		if (dimension == 3)
		    dims = GAIA_XY_Z;
		if (dimension == 4)
		    dims = GAIA_XY_Z_M;
	    }
	  else if (sqlite3_value_type (argv[4]) == SQLITE_TEXT)
	    {
		txt_dims = sqlite3_value_text (argv[4]);
		if (strcasecmp ((char *) txt_dims, "XY") == 0)
		    dims = GAIA_XY;
		if (strcasecmp ((char *) txt_dims, "XYZ") == 0)
		    dims = GAIA_XY_Z;
		if (strcasecmp ((char *) txt_dims, "XYM") == 0)
		    dims = GAIA_XY_M;
		if (strcasecmp ((char *) txt_dims, "XYZM") == 0)
		    dims = GAIA_XY_Z_M;
	    }
	  else
	    {
		spatialite_e
		    ("RecoverGeometryColumn() error: argument 5 [dimension] is not of the Integer or Text type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
      }
    xtype = GAIA_UNKNOWN;
    if (strcasecmp ((char *) type, "POINT") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRING") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGON") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINT") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRING") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGON") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTION") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRY") == 0)
      {
	  auto_dims = GAIA_XY;
	  xtype = -1;
      }
    if (strcasecmp ((char *) type, "POINTZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRINGZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGONZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINTZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRINGZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGONZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTIONZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRYZ") == 0)
      {
	  auto_dims = GAIA_XY_Z;
	  xtype = -1;
      }
    if (strcasecmp ((char *) type, "POINTM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRINGM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGONM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINTM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRINGM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGONM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTIONM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRYM") == 0)
      {
	  auto_dims = GAIA_XY_M;
	  xtype = -1;
      }
    if (strcasecmp ((char *) type, "POINTZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_POINT;
      }
    if (strcasecmp ((char *) type, "LINESTRINGZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_LINESTRING;
      }
    if (strcasecmp ((char *) type, "POLYGONZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_POLYGON;
      }
    if (strcasecmp ((char *) type, "MULTIPOINTZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_MULTIPOINT;
      }
    if (strcasecmp ((char *) type, "MULTILINESTRINGZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_MULTILINESTRING;
      }
    if (strcasecmp ((char *) type, "MULTIPOLYGONZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_MULTIPOLYGON;
      }
    if (strcasecmp ((char *) type, "GEOMETRYCOLLECTIONZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = GAIA_GEOMETRYCOLLECTION;
      }
    if (strcasecmp ((char *) type, "GEOMETRYZM") == 0)
      {
	  auto_dims = GAIA_XY_Z_M;
	  xtype = -1;
      }
    if (dims < 0)
	dims = auto_dims;
    if (xtype == GAIA_UNKNOWN)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 4 [geometry_type] has an illegal value\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (dims == GAIA_XY || dims == GAIA_XY_Z || dims == GAIA_XY_M
	|| dims == GAIA_XY_Z_M)
	;
    else
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 5 [dimension] ILLEGAL VALUE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (auto_dims != GAIA_XY && dims != auto_dims)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: argument 5 [dimension] ILLEGAL VALUE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    metadata_version = checkSpatialMetaData (sqlite);
    if (metadata_version == 1 || metadata_version == 3)
	;
    else
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: unexpected metadata layout\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
/* checking if the table exists */
    strcpy (sql,
	    "SELECT name FROM sqlite_master WHERE type = 'table' AND Lower(name) = Lower(?)");
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RecoverGeometryColumn: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		if (sqlite3_column_type (stmt, 0) == SQLITE_TEXT)
		    exists = 1;
	    }
      }
    sqlite3_finalize (stmt);
    if (!exists)
      {
	  spatialite_e
	      ("RecoverGeometryColumn() error: table '%s' does not exist\n",
	       table);
	  sqlite3_result_int (context, 0);
	  return;
      }
/* adjusting the actual GeometryType */
    xxtype = xtype;
    xtype = GAIA_UNKNOWN;
    if (xxtype == GAIA_POINT)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_POINTZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_POINTM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_POINTZM;
		break;
	    default:
		xtype = GAIA_POINT;
		break;
	    };
      }
    if (xxtype == GAIA_LINESTRING)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_LINESTRINGZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_LINESTRINGM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_LINESTRINGZM;
		break;
	    default:
		xtype = GAIA_LINESTRING;
		break;
	    };
      }
    if (xxtype == GAIA_POLYGON)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_POLYGONZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_POLYGONM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_POLYGONZM;
		break;
	    default:
		xtype = GAIA_POLYGON;
		break;
	    };
      }
    if (xxtype == GAIA_MULTIPOINT)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_MULTIPOINTZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_MULTIPOINTM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_MULTIPOINTZM;
		break;
	    default:
		xtype = GAIA_MULTIPOINT;
		break;
	    };
      }
    if (xxtype == GAIA_MULTILINESTRING)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_MULTILINESTRINGZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_MULTILINESTRINGM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_MULTILINESTRINGZM;
		break;
	    default:
		xtype = GAIA_MULTILINESTRING;
		break;
	    };
      }
    if (xxtype == GAIA_MULTIPOLYGON)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_MULTIPOLYGONZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_MULTIPOLYGONM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_MULTIPOLYGONZM;
		break;
	    default:
		xtype = GAIA_MULTIPOLYGON;
		break;
	    };
      }
    if (xxtype == GAIA_GEOMETRYCOLLECTION)
      {
	  switch (dims)
	    {
	    case GAIA_XY_Z:
		xtype = GAIA_GEOMETRYCOLLECTIONZ;
		break;
	    case GAIA_XY_M:
		xtype = GAIA_GEOMETRYCOLLECTIONM;
		break;
	    case GAIA_XY_Z_M:
		xtype = GAIA_GEOMETRYCOLLECTIONZM;
		break;
	    default:
		xtype = GAIA_GEOMETRYCOLLECTION;
		break;
	    };
      }
    if (xxtype == -1)
	xtype = -1;		/* GEOMETRY */
    if (!recoverGeomColumn (sqlite, table, column, xtype, dims, srid))
      {
	  spatialite_e ("RecoverGeometryColumn(): validation failed\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
/* deleting anyway any previous definition */
    sql_statement = sqlite3_mprintf ("DELETE FROM geometry_columns "
				     "WHERE Lower(f_table_name) = Lower(?) AND "
				     "Lower(f_geometry_column) = Lower(?)");
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RecoverGeometryColumn: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  spatialite_e ("RecoverGeometryColumn() error: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_finalize (stmt);
	  goto error;
      }
    sqlite3_finalize (stmt);

    if (metadata_version == 1)
      {
	  /* legacy metadata style <= v.3.1.0 */
	  switch (xtype)
	    {
	    case GAIA_POINT:
	    case GAIA_POINTZ:
	    case GAIA_POINTM:
	    case GAIA_POINTZM:
		p_type = "POINT";
		break;
	    case GAIA_LINESTRING:
	    case GAIA_LINESTRINGZ:
	    case GAIA_LINESTRINGM:
	    case GAIA_LINESTRINGZM:
		p_type = "LINESTRING";
		break;
	    case GAIA_POLYGON:
	    case GAIA_POLYGONZ:
	    case GAIA_POLYGONM:
	    case GAIA_POLYGONZM:
		p_type = "POLYGON";
		break;
	    case GAIA_MULTIPOINT:
	    case GAIA_MULTIPOINTZ:
	    case GAIA_MULTIPOINTM:
	    case GAIA_MULTIPOINTZM:
		p_type = "MULTIPOINT";
		break;
	    case GAIA_MULTILINESTRING:
	    case GAIA_MULTILINESTRINGZ:
	    case GAIA_MULTILINESTRINGM:
	    case GAIA_MULTILINESTRINGZM:
		p_type = "MULTILINESTRING";
		break;
	    case GAIA_MULTIPOLYGON:
	    case GAIA_MULTIPOLYGONZ:
	    case GAIA_MULTIPOLYGONM:
	    case GAIA_MULTIPOLYGONZM:
		p_type = "MULTIPOLYGON";
		break;
	    case GAIA_GEOMETRYCOLLECTION:
	    case GAIA_GEOMETRYCOLLECTIONZ:
	    case GAIA_GEOMETRYCOLLECTIONM:
	    case GAIA_GEOMETRYCOLLECTIONZM:
		p_type = "GEOMETRYCOLLECTION";
		break;
	    case -1:
		p_type = "GEOMETRY";
		break;
	    };
	  strcat (sql, "', '");
	  switch (dims)
	    {
	    case GAIA_XY:
		p_dims = "XY";
		break;
	    case GAIA_XY_Z:
		p_dims = "XYZ";
		break;
	    case GAIA_XY_M:
		p_dims = "XYM";
		break;
	    case GAIA_XY_Z_M:
		p_dims = "XYZM";
		break;
	    };
/* Sandro 2013-01-07
/ fixing an issue reported by Peter Aronson [ESRI] <paronson@esri.com>
	  sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
					   "(f_table_name, f_geometry_column, type, coord_dimension, srid, "
					   "spatial_index_enabled) VALUES (Lower(?), Lower(?), %Q, %Q, ?, 0)",
					   p_type, p_dims);
*/
	  sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
					   "(f_table_name, f_geometry_column, type, coord_dimension, srid, "
					   "spatial_index_enabled) VALUES (?, ?, %Q, %Q, ?, 0)",
					   p_type, p_dims);
      }
    else
      {
	  /* current metadata style >= v.4.0.0 */
	  switch (xtype)
	    {
	    case GAIA_POINT:
		n_type = 1;
		n_dims = 2;
		break;
	    case GAIA_POINTZ:
		n_type = 1001;
		n_dims = 3;
		break;
	    case GAIA_POINTM:
		n_type = 2001;
		n_dims = 3;
		break;
	    case GAIA_POINTZM:
		n_type = 3001;
		n_dims = 4;
		break;
	    case GAIA_LINESTRING:
		n_type = 2;
		n_dims = 2;
		break;
	    case GAIA_LINESTRINGZ:
		n_type = 1002;
		n_dims = 3;
		break;
	    case GAIA_LINESTRINGM:
		n_type = 2002;
		n_dims = 3;
		break;
	    case GAIA_LINESTRINGZM:
		n_type = 3002;
		n_dims = 4;
		break;
	    case GAIA_POLYGON:
		n_type = 3;
		n_dims = 3;
		break;
	    case GAIA_POLYGONZ:
		n_type = 1003;
		n_dims = 3;
		break;
	    case GAIA_POLYGONM:
		n_type = 2003;
		n_dims = 3;
		break;
	    case GAIA_POLYGONZM:
		n_type = 3003;
		n_dims = 4;
		break;
	    case GAIA_MULTIPOINT:
		n_type = 4;
		n_dims = 2;
		break;
	    case GAIA_MULTIPOINTZ:
		n_type = 1004;
		n_dims = 3;
		break;
	    case GAIA_MULTIPOINTM:
		n_type = 2004;
		n_dims = 3;
		break;
	    case GAIA_MULTIPOINTZM:
		n_type = 3004;
		n_dims = 4;
		break;
	    case GAIA_MULTILINESTRING:
		n_type = 5;
		n_dims = 2;
		break;
	    case GAIA_MULTILINESTRINGZ:
		n_type = 1005;
		n_dims = 3;
		break;
	    case GAIA_MULTILINESTRINGM:
		n_type = 2005;
		n_dims = 3;
		break;
	    case GAIA_MULTILINESTRINGZM:
		n_type = 3005;
		n_dims = 4;
		break;
	    case GAIA_MULTIPOLYGON:
		n_type = 6;
		n_dims = 2;
		break;
	    case GAIA_MULTIPOLYGONZ:
		n_type = 1006;
		n_dims = 3;
		break;
	    case GAIA_MULTIPOLYGONM:
		n_type = 2006;
		n_dims = 3;
		break;
	    case GAIA_MULTIPOLYGONZM:
		n_type = 3006;
		n_dims = 4;
		break;
	    case GAIA_GEOMETRYCOLLECTION:
		n_type = 7;
		n_dims = 2;
		break;
	    case GAIA_GEOMETRYCOLLECTIONZ:
		n_type = 1007;
		n_dims = 3;
		break;
	    case GAIA_GEOMETRYCOLLECTIONM:
		n_type = 2007;
		n_dims = 3;
		break;
	    case GAIA_GEOMETRYCOLLECTIONZM:
		n_type = 3007;
		n_dims = 4;
		break;
	    case -1:
		switch (dims)
		  {
		  case GAIA_XY:
		      n_type = 0;
		      n_dims = 2;
		      break;
		  case GAIA_XY_Z:
		      n_type = 1000;
		      n_dims = 3;
		      break;
		  case GAIA_XY_M:
		      n_type = 2000;
		      n_dims = 3;
		      break;
		  case GAIA_XY_Z_M:
		      n_type = 3000;
		      n_dims = 4;
		      break;
		  };
		break;
	    };
	  sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
					   "(f_table_name, f_geometry_column, geometry_type, coord_dimension, "
					   "srid, spatial_index_enabled) VALUES (Lower(?), Lower(?), %d, %d, ?, 0)",
					   n_type, n_dims);
      }
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RecoverGeometryColumn: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
    if (srid < 0)
	sqlite3_bind_int (stmt, 3, -1);
    else
	sqlite3_bind_int (stmt, 3, srid);
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  spatialite_e ("RecoverGeometryColumn() error: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_finalize (stmt);
	  goto error;
      }
    sqlite3_finalize (stmt);
    if (metadata_version == 3)
      {
	  /* current metadata style >= v.4.0.0 */

	  /* inserting a row into GEOMETRY_COLUMNS_AUTH */
	  strcpy (sql,
		  "INSERT OR REPLACE INTO geometry_columns_auth (f_table_name, f_geometry_column, ");
	  strcat (sql, "read_only, hidden) VALUES (Lower(?), Lower(?), 0, 0)");
	  ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("RecoverGeometryColumn: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_result_int (context, 0);
		return;
	    }
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("RecoverGeometryColumn() error: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  sqlite3_finalize (stmt);
	  /* inserting a row into GEOMETRY_COLUMNS_STATISTICS */
	  strcpy (sql,
		  "INSERT OR REPLACE INTO geometry_columns_statistics (f_table_name, f_geometry_column) ");
	  strcat (sql, "VALUES (Lower(?), Lower(?))");
	  ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("RecoverGeometryColumn: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_result_int (context, 0);
		return;
	    }
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("RecoverGeometryColumn() error: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  sqlite3_finalize (stmt);
	  /* inserting a row into GEOMETRY_COLUMNS_TIME */
	  strcpy (sql,
		  "INSERT OR REPLACE INTO geometry_columns_time (f_table_name, f_geometry_column) ");
	  strcat (sql, "VALUES (Lower(?), Lower(?))");
	  ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("RecoverGeometryColumn: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_result_int (context, 0);
		return;
	    }
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, table, strlen (table), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 2, column, strlen (column), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("RecoverGeometryColumn() error: \"%s\"\n",
			      sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  sqlite3_finalize (stmt);
      }
    updateGeometryTriggers (sqlite, table, column);
    sqlite3_result_int (context, 1);
    switch (xtype)
      {
      case GAIA_POINT:
      case GAIA_POINTZ:
      case GAIA_POINTM:
      case GAIA_POINTZM:
	  p_type = "POINT";
	  break;
      case GAIA_LINESTRING:
      case GAIA_LINESTRINGZ:
      case GAIA_LINESTRINGM:
      case GAIA_LINESTRINGZM:
	  p_type = "LINESTRING";
	  break;
      case GAIA_POLYGON:
      case GAIA_POLYGONZ:
      case GAIA_POLYGONM:
      case GAIA_POLYGONZM:
	  p_type = "POLYGON";
	  break;
      case GAIA_MULTIPOINT:
      case GAIA_MULTIPOINTZ:
      case GAIA_MULTIPOINTM:
      case GAIA_MULTIPOINTZM:
	  p_type = "MULTIPOINT";
	  break;
      case GAIA_MULTILINESTRING:
      case GAIA_MULTILINESTRINGZ:
      case GAIA_MULTILINESTRINGM:
      case GAIA_MULTILINESTRINGZM:
	  p_type = "MULTILINESTRING";
	  break;
      case GAIA_MULTIPOLYGON:
      case GAIA_MULTIPOLYGONZ:
      case GAIA_MULTIPOLYGONM:
      case GAIA_MULTIPOLYGONZM:
	  p_type = "MULTIPOLYGON";
	  break;
      case GAIA_GEOMETRYCOLLECTION:
      case GAIA_GEOMETRYCOLLECTIONZ:
      case GAIA_GEOMETRYCOLLECTIONM:
      case GAIA_GEOMETRYCOLLECTIONZM:
	  p_type = "GEOMETRYCOLLECTION";
	  break;
      case -1:
	  p_type = "GEOMETRY";
	  break;
      };
    switch (dims)
      {
      case GAIA_XY:
	  p_dims = "XY";
	  break;
      case GAIA_XY_Z:
	  p_dims = "XYZ";
	  break;
      case GAIA_XY_M:
	  p_dims = "XYM";
	  break;
      case GAIA_XY_Z_M:
	  p_dims = "XYZM";
	  break;
      };
    sql_statement =
	sqlite3_mprintf ("Geometry [%s,%s,SRID=%d] successfully recovered",
			 p_type, p_dims, (srid <= 0) ? -1 : srid);
    updateSpatiaLiteHistory (sqlite, table, column, sql_statement);
    sqlite3_free (sql_statement);
    return;
  error:
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_DiscardGeometryColumn (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ DiscardGeometryColumn(table, column)
/
/ removes TABLE.COLUMN from the Spatial MetaData [thus disabling triggers too]
/ returns 1 on success
/ 0 on failure
*/
    const unsigned char *table;
    const unsigned char *column;
    char *p_table = NULL;
    char *p_column = NULL;
    sqlite3_stmt *stmt;
    char *sql_statement;
    char *raw;
    char *quoted;
    char *errMsg = NULL;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DiscardGeometryColumn() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DiscardGeometryColumn() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = sqlite3_value_text (argv[1]);

    sql_statement = sqlite3_mprintf ("DELETE FROM geometry_columns "
				     "WHERE Lower(f_table_name) = Lower(?) "
				     "AND Lower(f_geometry_column) = Lower(?)");
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("DiscardGeometryColumn: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, (const char *) table,
		       strlen ((const char *) table), SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, (const char *) column,
		       strlen ((const char *) column), SQLITE_STATIC);
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  spatialite_e ("DiscardGeometryColumn() error: \"%s\"\n",
			sqlite3_errmsg (sqlite));
	  sqlite3_finalize (stmt);
	  goto error;
      }
    sqlite3_finalize (stmt);
/* removing triggers too */
    if (!getRealSQLnames
	(sqlite, (const char *) table, (const char *) column, &p_table,
	 &p_column))
      {
	  spatialite_e
	      ("DiscardGeometryColumn() error: not existing Table or Column\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    raw = sqlite3_mprintf ("ggi_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("ggu_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gii_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("giu_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gid_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gci_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gcu_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gcd_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("tmi_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("tmu_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("tmd_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;

    /* trying to delete old versions [v2.0, v2.2] triggers[if any] */
    raw = sqlite3_mprintf ("gti_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gtu_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gsi_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    raw = sqlite3_mprintf ("gsu_%s_%s", p_table, p_column);
    quoted = gaiaDoubleQuotedSql (raw);
    sqlite3_free (raw);
    sql_statement = sqlite3_mprintf ("DROP TRIGGER IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    /* end deletion old versions [v2.0, v2.2] triggers[if any] */

    sqlite3_result_int (context, 1);
    updateSpatiaLiteHistory (sqlite, p_table,
			     p_column, "Geometry successfully discarded");
    free (p_table);
    free (p_column);
    return;
  error:
    if (p_table)
	free (p_table);
    if (p_column)
	free (p_column);
    spatialite_e ("DiscardGeometryColumn() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static int
registerVirtual (sqlite3 * sqlite, const char *table)
{
/* attempting to register a VirtualGeometry */
    char gtype[64];
    int xtype = -1;
    int srid;
    char **results;
    int ret;
    int rows;
    int columns;
    int i;
    char *errMsg = NULL;
    int ok_virt_name = 0;
    int ok_virt_geometry = 0;
    int ok_srid = 0;
    int ok_geometry_type = 0;
    int ok_type = 0;
    int ok_coord_dimension = 0;
    int xdims;
    char *quoted;
    char *sql_statement;

/* testing the layout of virts_geometry_columns table */
    ret = sqlite3_get_table (sqlite,
			     "PRAGMA table_info(virts_geometry_columns)",
			     &results, &rows, &columns, &errMsg);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RegisterVirtualGeometry() error: \"%s\"\n", errMsg);
	  sqlite3_free (errMsg);
	  return 0;
      }
    for (i = 1; i <= rows; i++)
      {
	  if (strcasecmp ("virt_name", results[(i * columns) + 1]) == 0)
	      ok_virt_name = 1;
	  if (strcasecmp ("virt_geometry", results[(i * columns) + 1]) == 0)
	      ok_virt_geometry = 1;
	  if (strcasecmp ("srid", results[(i * columns) + 1]) == 0)
	      ok_srid = 1;
	  if (strcasecmp ("geometry_type", results[(i * columns) + 1]) == 0)
	      ok_geometry_type = 1;
	  if (strcasecmp ("type", results[(i * columns) + 1]) == 0)
	      ok_type = 1;
	  if (strcasecmp ("coord_dimension", results[(i * columns) + 1]) == 0)
	      ok_coord_dimension = 1;
      }
    sqlite3_free_table (results);

    if (ok_virt_name && ok_virt_geometry && ok_srid && ok_geometry_type
	&& ok_coord_dimension)
	;
    else if (ok_virt_name && ok_virt_geometry && ok_srid && ok_type)
	;
    else
	return 0;

/* determining Geometry Type and dims */
    quoted = gaiaDoubleQuotedSql (table);
    sql_statement =
	sqlite3_mprintf ("SELECT DISTINCT "
			 "ST_GeometryType(Geometry), ST_Srid(Geometry) FROM \"%s\"",
			 quoted);
    free (quoted);
    ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows, &columns,
			     &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RegisterVirtualGeometry() error: \"%s\"\n", errMsg);
	  sqlite3_free (errMsg);
	  return 0;
      }
    for (i = 1; i <= rows; i++)
      {
	  if (results[(i * columns)] == NULL)
	      *gtype = '\0';
	  else
	      strcpy (gtype, results[(i * columns)]);
	  if (results[(i * columns) + 1] == NULL)
	      srid = 0;
	  else
	      srid = atoi (results[(i * columns) + 1]);
      }
    sqlite3_free_table (results);

/* normalized Geometry type */
    if (strcmp (gtype, "POINT") == 0)
	xtype = 1;
    if (strcmp (gtype, "POINT Z") == 0)
	xtype = 1001;
    if (strcmp (gtype, "POINT M") == 0)
	xtype = 2001;
    if (strcmp (gtype, "POINT ZM") == 0)
	xtype = 3001;
    if (strcmp (gtype, "LINESTRING") == 0)
	xtype = 2;
    if (strcmp (gtype, "LINESTRING Z") == 0)
	xtype = 1002;
    if (strcmp (gtype, "LINESTRING M") == 0)
	xtype = 2002;
    if (strcmp (gtype, "LINESTRING ZM") == 0)
	xtype = 3002;
    if (strcmp (gtype, "POLYGON") == 0)
	xtype = 3;
    if (strcmp (gtype, "POLYGON Z") == 0)
	xtype = 1003;
    if (strcmp (gtype, "POLYGON M") == 0)
	xtype = 2003;
    if (strcmp (gtype, "POLYGON ZM") == 0)
	xtype = 3003;
    if (strcmp (gtype, "MULTIPOINT") == 0)
	xtype = 4;
    if (strcmp (gtype, "MULTIPOINT Z") == 0)
	xtype = 1004;
    if (strcmp (gtype, "MULTIPOINT M") == 0)
	xtype = 2004;
    if (strcmp (gtype, "MULTIPOINT ZM") == 0)
	xtype = 3004;
    if (strcmp (gtype, "MULTILINESTRING") == 0)
	xtype = 5;
    if (strcmp (gtype, "MULTILINESTRING Z") == 0)
	xtype = 1005;
    if (strcmp (gtype, "MULTILINESTRING M") == 0)
	xtype = 2005;
    if (strcmp (gtype, "MULTILINESTRING ZM") == 0)
	xtype = 3005;
    if (strcmp (gtype, "MULTIPOLYGON") == 0)
	xtype = 6;
    if (strcmp (gtype, "MULTIPOLYGON Z") == 0)
	xtype = 1006;
    if (strcmp (gtype, "MULTIPOLYGON M") == 0)
	xtype = 2006;
    if (strcmp (gtype, "MULTIPOLYGON ZM") == 0)
	xtype = 3006;

/* updating metadata tables */
    xdims = -1;
    switch (xtype)
      {
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
	  xdims = 2;
	  break;
      case 1001:
      case 1002:
      case 1003:
      case 1004:
      case 1005:
      case 1006:
      case 2001:
      case 2002:
      case 2003:
      case 2004:
      case 2005:
      case 2006:
	  xdims = 3;
	  break;
      case 3001:
      case 3002:
      case 3003:
      case 3004:
      case 3005:
      case 3006:
	  xdims = 4;
	  break;
      };
    if (ok_geometry_type)
      {
	  /* has the "geometry_type" column */
	  sql_statement =
	      sqlite3_mprintf
	      ("INSERT OR REPLACE INTO virts_geometry_columns "
	       "(virt_name, virt_geometry, geometry_type, coord_dimension, srid) "
	       "VALUES (Lower(%Q), 'geometry', %d, %d, %d)", table, xtype,
	       xdims, srid);
      }
    else
      {
	  /* has the "type" column */
	  const char *xgtype = "UNKNOWN";
	  switch (xtype)
	    {
	    case 1:
	    case 1001:
	    case 2001:
	    case 3001:
		xgtype = "POINT";
		break;
	    case 2:
	    case 1002:
	    case 2002:
	    case 3002:
		xgtype = "LINESTRING";
		break;
	    case 3:
	    case 1003:
	    case 2003:
	    case 3003:
		xgtype = "POLYGON";
		break;
	    case 4:
	    case 1004:
	    case 2004:
	    case 3004:
		xgtype = "MULTIPOINT";
		break;
	    case 5:
	    case 1005:
	    case 2005:
	    case 3005:
		xgtype = "MULTILINESTRING";
		break;
	    case 6:
	    case 1006:
	    case 2006:
	    case 3006:
		xgtype = "MULTIPOLYGON";
		break;
	    };
	  sql_statement =
	      sqlite3_mprintf
	      ("INSERT OR REPLACE INTO virts_geometry_columns "
	       "(virt_name, virt_geometry, type, srid) "
	       "VALUES (Lower(%Q), 'geometry', %Q, %d)", table, xgtype, srid);
      }
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RegisterVirtualGeometry() error: \"%s\"\n", errMsg);
	  sqlite3_free (errMsg);
	  return 0;
      }
    if (checkSpatialMetaData (sqlite) == 3)
      {
	  /* current metadata style >= v.4.0.0 */

	  /* inserting a row into VIRTS_GEOMETRY_COLUMNS_AUTH */
	  sql_statement = sqlite3_mprintf ("INSERT OR REPLACE INTO "
					   "virts_geometry_columns_auth (virt_name, virt_geometry, hidden) "
					   "VALUES (Lower(%Q), 'geometry', 0)",
					   table);
	  ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
	  sqlite3_free (sql_statement);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("RegisterVirtualGeometry() error: \"%s\"\n",
			      errMsg);
		sqlite3_free (errMsg);
		return 0;
	    }
	  /* inserting a row into GEOMETRY_COLUMNS_STATISTICS */
	  sql_statement = sqlite3_mprintf ("INSERT OR REPLACE INTO "
					   "virts_geometry_columns_statistics (virt_name, virt_geometry) "
					   "VALUES (Lower(%Q), 'geometry')",
					   table);
	  ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
	  sqlite3_free (sql_statement);
	  if (ret != SQLITE_OK)
	    {
		spatialite_e ("RegisterVirtualGeometry() error: \"%s\"\n",
			      errMsg);
		sqlite3_free (errMsg);
		return 0;
	    }
      }
    return 1;
}

static void
fnct_RegisterVirtualGeometry (sqlite3_context * context, int argc,
			      sqlite3_value ** argv)
{
/* SQL function:
/ RegisterVirtualGeometry(table)
/
/ insert/updates TABLE.COLUMN into the Spatial MetaData [Virtual Table]
/ returns 1 on success
/ 0 on failure
*/
    const unsigned char *table;
    char sql[1024];
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RegisterVirtualGeometry() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = sqlite3_value_text (argv[0]);
    if (!registerVirtual (sqlite, (char *) table))
	goto error;
    sqlite3_result_int (context, 1);
    strcpy (sql, "Virtual Geometry successfully registered");
    updateSpatiaLiteHistory (sqlite, (const char *) table, "Geometry", sql);
    return;
  error:
    spatialite_e ("RegisterVirtualGeometry() error\n");
    sqlite3_result_int (context, 0);
    return;
}


static void
fnct_DropVirtualGeometry (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ DropVirtualGeometry(table)
/
/ removes TABLE.COLUMN from the Spatial MetaData and DROPs the Virtual Table
/ returns 1 on success
/ 0 on failure
*/
    const unsigned char *table;
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    char *quoted;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DropVirtualGeometry() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = sqlite3_value_text (argv[0]);
    sql_statement = sqlite3_mprintf ("DELETE FROM virts_geometry_columns "
				     "WHERE Lower(virt_name) = Lower(%Q)",
				     table);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    quoted = gaiaDoubleQuotedSql ((const char *) table);
    sql_statement = sqlite3_mprintf ("DROP TABLE IF EXISTS \"%s\"", quoted);
    free (quoted);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    sqlite3_result_int (context, 1);
    updateSpatiaLiteHistory (sqlite, (const char *) table, "Geometry",
			     "Virtual Geometry successfully dropped");
    return;
  error:
    spatialite_e ("DropVirtualGeometry() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_InitFDOSpatialMetaData (sqlite3_context * context, int argc,
			     sqlite3_value ** argv)
{
/* SQL function:
/ InitFDOSpatialMetaData(void)
/
/ creates the FDO-styled SPATIAL_REF_SYS and GEOMETRY_COLUMNS tables
/ returns 1 on success
/ 0 on failure
*/
    char sql[1024];
    char *errMsg = NULL;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
/* creating the SPATIAL_REF_SYS tables */
    strcpy (sql, "CREATE TABLE spatial_ref_sys (\n");
    strcat (sql, "srid INTEGER PRIMARY KEY,\n");
    strcat (sql, "auth_name TEXT,\n");
    strcat (sql, "auth_srid INTEGER,\n");
    strcat (sql, "srtext TEXT)");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &errMsg);
    if (ret != SQLITE_OK)
	goto error;
/* creating the GEOMETRY_COLUMN tables */
    strcpy (sql, "CREATE TABLE geometry_columns (\n");
    strcat (sql, "f_table_name TEXT,\n");
    strcat (sql, "f_geometry_column TEXT,\n");
    strcat (sql, "geometry_type INTEGER,\n");
    strcat (sql, "coord_dimension INTEGER,\n");
    strcat (sql, "srid INTEGER,\n");
    strcat (sql, "geometry_format TEXT)");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &errMsg);
    if (ret != SQLITE_OK)
	goto error;
    sqlite3_result_int (context, 1);
    return;
  error:
    spatialite_e ("InitFDOSpatiaMetaData() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static int
recoverFDOGeomColumn (sqlite3 * sqlite, const unsigned char *table,
		      const unsigned char *column, int xtype, int srid)
{
/* checks if TABLE.COLUMN exists and has the required features */
    int ok = 1;
    char *sql_statement;
    int type;
    sqlite3_stmt *stmt;
    gaiaGeomCollPtr geom;
    const void *blob_value;
    int len;
    int ret;
    int i_col;
    char *xcolumn;
    char *xtable;
    xcolumn = gaiaDoubleQuotedSql ((char *) column);
    xtable = gaiaDoubleQuotedSql ((char *) table);
    sql_statement =
	sqlite3_mprintf ("SELECT \"%s\" FROM \"%s\"", xcolumn, xtable);
    free (xcolumn);
    free (xtable);
/* compiling SQL prepared statement */
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("recoverFDOGeomColumn: error %d \"%s\"\n",
			sqlite3_errcode (sqlite), sqlite3_errmsg (sqlite));
	  return 0;
      }
    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* checking Geometry features */
		geom = NULL;
		for (i_col = 0; i_col < sqlite3_column_count (stmt); i_col++)
		  {
		      if (sqlite3_column_type (stmt, i_col) != SQLITE_BLOB)
			  ok = 0;
		      else
			{
			    blob_value = sqlite3_column_blob (stmt, i_col);
			    len = sqlite3_column_bytes (stmt, i_col);
			    geom = gaiaFromSpatiaLiteBlobWkb (blob_value, len);
			    if (!geom)
				ok = 0;
			    else
			      {
				  if (geom->Srid != srid)
				      ok = 0;
				  /* normalizing Geometry Type */
				  switch (gaiaGeometryType (geom))
				    {
				    case GAIA_POINT:
				    case GAIA_POINTZ:
				    case GAIA_POINTM:
				    case GAIA_POINTZM:
					type = GAIA_POINT;
					break;
				    case GAIA_LINESTRING:
				    case GAIA_LINESTRINGZ:
				    case GAIA_LINESTRINGM:
				    case GAIA_LINESTRINGZM:
					type = GAIA_LINESTRING;
					break;
				    case GAIA_POLYGON:
				    case GAIA_POLYGONZ:
				    case GAIA_POLYGONM:
				    case GAIA_POLYGONZM:
					type = GAIA_POLYGON;
					break;
				    case GAIA_MULTIPOINT:
				    case GAIA_MULTIPOINTZ:
				    case GAIA_MULTIPOINTM:
				    case GAIA_MULTIPOINTZM:
					type = GAIA_MULTIPOINT;
					break;
				    case GAIA_MULTILINESTRING:
				    case GAIA_MULTILINESTRINGZ:
				    case GAIA_MULTILINESTRINGM:
				    case GAIA_MULTILINESTRINGZM:
					type = GAIA_MULTILINESTRING;
					break;
				    case GAIA_MULTIPOLYGON:
				    case GAIA_MULTIPOLYGONZ:
				    case GAIA_MULTIPOLYGONM:
				    case GAIA_MULTIPOLYGONZM:
					type = GAIA_MULTIPOLYGON;
					break;
				    case GAIA_GEOMETRYCOLLECTION:
				    case GAIA_GEOMETRYCOLLECTIONZ:
				    case GAIA_GEOMETRYCOLLECTIONM:
				    case GAIA_GEOMETRYCOLLECTIONZM:
					type = GAIA_GEOMETRYCOLLECTION;
					break;
				    default:
					type = -1;
					break;
				    };
				  if (xtype == type)
				      ;
				  else
				      ok = 0;
				  gaiaFreeGeomColl (geom);
			      }
			}
		  }
	    }
	  if (!ok)
	      break;
      }
    ret = sqlite3_finalize (stmt);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("recoverFDOGeomColumn: error %d \"%s\"\n",
			sqlite3_errcode (sqlite), sqlite3_errmsg (sqlite));
	  return 0;
      }
    return ok;
}

static void
fnct_AddFDOGeometryColumn (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ AddFDOGeometryColumn(table, column, srid, geometry_type , dimension, geometry_format )
/
/ creates a new COLUMN of given TYPE into TABLE
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    const char *format;
    char xformat[64];
    int type;
    int srid = -1;
    int dimension = 2;
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    char **results;
    int rows;
    int columns;
    int i;
    int oktbl;
    char *xtable;
    char *xcolumn;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 3 [SRID] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    srid = sqlite3_value_int (argv[2]);
    if (sqlite3_value_type (argv[3]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 4 [geometry_type] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    type = sqlite3_value_int (argv[3]);
    if (sqlite3_value_type (argv[4]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 5 [dimension] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    dimension = sqlite3_value_int (argv[4]);
    if (sqlite3_value_type (argv[5]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 6 [geometry_format] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    format = (const char *) sqlite3_value_text (argv[5]);
    if (type ==
	GAIA_POINT
	|| type ==
	GAIA_LINESTRING
	|| type ==
	GAIA_POLYGON
	|| type ==
	GAIA_MULTIPOINT
	|| type ==
	GAIA_MULTILINESTRING
	|| type == GAIA_MULTIPOLYGON || type == GAIA_GEOMETRYCOLLECTION)
	;
    else
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 4 [geometry_type] has an illegal value\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (dimension < 2 || dimension > 4)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 5 [dimension] current version only accepts dimension=2,3,4\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (strcasecmp (format, "WKT") == 0)
	strcpy (xformat, "WKT");
    else if (strcasecmp (format, "WKB") == 0)
	strcpy (xformat, "WKB");
    else if (strcasecmp (format, "FGF") == 0)
	strcpy (xformat, "FGF");
    else if (strcasecmp (format, "SPATIALITE") == 0)
	strcpy (xformat, "SPATIALITE");
    else
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: argument 6 [geometry_format] has to be one of: WKT,WKB,FGF,SPATIALITE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
/* checking if the table exists */
    xtable = gaiaDoubleQuotedSql (table);
    xcolumn = gaiaDoubleQuotedSql (column);
    sql_statement = sqlite3_mprintf ("SELECT name FROM sqlite_master "
				     "WHERE type = 'table' AND Upper(name) = Upper(%Q)",
				     table);
    free (xtable);
    free (xcolumn);
    ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows, &columns,
			     &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("AddFDOGeometryColumn: \"%s\"\n", errMsg);
	  sqlite3_free (errMsg);
	  return;
      }
    oktbl = 0;
    for (i = 1; i <= rows; i++)
	oktbl = 1;
    sqlite3_free_table (results);
    if (!oktbl)
      {
	  spatialite_e
	      ("AddFDOGeometryColumn() error: table '%s' does not exist\n",
	       table);
	  sqlite3_result_int (context, 0);
	  return;
      }
/* trying to add the column */
    xtable = gaiaDoubleQuotedSql (table);
    xcolumn = gaiaDoubleQuotedSql (column);
    sql_statement = sqlite3_mprintf ("ALTER TABLE \"%s\" "
				     "ADD COLUMN \"%s\" BLOB", xtable, xcolumn);
    free (xtable);
    free (xcolumn);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
/*ok, inserting into geometry_columns [FDO Spatial Metadata] */
    sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
				     "(f_table_name, f_geometry_column, geometry_type, "
				     "coord_dimension, srid, geometry_format) VALUES (%Q, %Q, %d, %d, %d, %Q)",
				     table, column, type, dimension,
				     (srid <= 0) ? -1 : srid, xformat);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    sqlite3_result_int (context, 1);
    return;
  error:
    spatialite_e ("AddFDOGeometryColumn() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_RecoverFDOGeometryColumn (sqlite3_context * context, int argc,
			       sqlite3_value ** argv)
{
/* SQL function:
/ RecoverFDOGeometryColumn(table, column, srid, geometry_type , dimension, geometry_format )
/
/ checks if an existing TABLE.COLUMN satisfies the required geometric features
/ if yes adds it to FDO-styled SpatialMetaData 
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    const char *format;
    char xformat[64];
    int type;
    int srid = -1;
    int dimension = 2;
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    char **results;
    int rows;
    int columns;
    int i;
    int ok_tbl;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 3 [SRID] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    srid = sqlite3_value_int (argv[2]);
    if (sqlite3_value_type (argv[3]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 4 [geometry_type] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    type = sqlite3_value_int (argv[3]);
    if (sqlite3_value_type (argv[4]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 5 [dimension] is not of the Integer type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    dimension = sqlite3_value_int (argv[4]);
    if (sqlite3_value_type (argv[5]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 6 [geometry_format] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    format = (const char *) sqlite3_value_text (argv[5]);
    if (type ==
	GAIA_POINT
	|| type ==
	GAIA_LINESTRING
	|| type ==
	GAIA_POLYGON
	|| type ==
	GAIA_MULTIPOINT
	|| type ==
	GAIA_MULTILINESTRING
	|| type == GAIA_MULTIPOLYGON || type == GAIA_GEOMETRYCOLLECTION)
	;
    else
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 4 [geometry_type] has an illegal value\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (dimension < 2 || dimension > 4)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 5 [dimension] current version only accepts dimension=2,3,4\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (strcasecmp (format, "WKT") == 0)
	strcpy (xformat, "WKT");
    else if (strcasecmp (format, "WKB") == 0)
	strcpy (xformat, "WKB");
    else if (strcasecmp (format, "FGF") == 0)
	strcpy (xformat, "FGF");
    else if (strcasecmp (format, "SPATIALITE") == 0)
	strcpy (xformat, "SPATIALITE");
    else
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: argument 6 [geometry_format] has to be one of: WKT,WKB,FGF\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
/* checking if the table exists */
    sql_statement = sqlite3_mprintf ("SELECT name FROM sqlite_master "
				     "WHERE type = 'table' AND Upper(name) = Upper(%Q)",
				     table);
    ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows, &columns,
			     &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RecoverFDOGeometryColumn: \"%s\"\n", errMsg);
	  sqlite3_free (errMsg);
	  return;
      }
    ok_tbl = 0;
    for (i = 1; i <= rows; i++)
	ok_tbl = 1;
    sqlite3_free_table (results);
    if (!ok_tbl)
      {
	  spatialite_e
	      ("RecoverFDOGeometryColumn() error: table '%s' does not exist\n",
	       table);
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (!recoverFDOGeomColumn
	(sqlite, (const unsigned char *) table,
	 (const unsigned char *) column, type, srid))
      {
	  spatialite_e ("RecoverFDOGeometryColumn(): validation failed\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    sql_statement = sqlite3_mprintf ("INSERT INTO geometry_columns "
				     "(f_table_name, f_geometry_column, geometry_type, "
				     "coord_dimension, srid, geometry_format) VALUES (%Q, %Q, %d, %d, %d, %Q)",
				     table, column, type, dimension,
				     (srid <= 0) ? -1 : srid, xformat);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    sqlite3_result_int (context, 1);
    return;
  error:
    spatialite_e ("RecoverFDOGeometryColumn() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_DiscardFDOGeometryColumn (sqlite3_context * context, int argc,
			       sqlite3_value ** argv)
{
/* SQL function:
/ DiscardFDOGeometryColumn(table, column)
/
/ removes TABLE.COLUMN from the Spatial MetaData
/ returns 1 on success
/ 0 on failure
*/
    const unsigned char *table;
    const unsigned char *column;
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DiscardFDOGeometryColumn() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DiscardFDOGeometryColumn() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = sqlite3_value_text (argv[1]);
    sql_statement =
	sqlite3_mprintf
	("DELETE FROM geometry_columns WHERE Upper(f_table_name) = "
	 "Upper(%Q) AND Upper(f_geometry_column) = Upper(%Q)", table, column);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    sqlite3_result_int (context, 1);
    return;
  error:
    spatialite_e ("DiscardFDOGeometryColumn() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static int
eval_rtree_entry (int ok_geom, double geom_value, int ok_rtree,
		  double rtree_value)
{
/* evaluating geom-coord and rtree-coord */
    if (!ok_geom && !ok_rtree)
	return 1;
    if (ok_geom && ok_rtree)
      {
	  float g = (float) geom_value;
	  float r = (float) rtree_value;
	  double tic = fabs (geom_value - r) * 2.0;
	  float diff = g - r;
	  if (diff > tic)
	      return 0;
	  return 1;
      }
    return 0;
}

static int
check_spatial_index (sqlite3 * sqlite, const unsigned char *table,
		     const unsigned char *geom)
{
/* attempting to check an R*Tree for consistency */
    char *xtable = NULL;
    char *xgeom = NULL;
    char *idx_name;
    char *xidx_name = NULL;
    char sql[1024];
    char *sql_statement;
    int ret;
    int is_defined = 0;
    sqlite3_stmt *stmt;
    sqlite3_int64 count_geom;
    sqlite3_int64 count_rtree;
    double g_xmin;
    double g_ymin;
    double g_xmax;
    double g_ymax;
    int ok_g_xmin;
    int ok_g_ymin;
    int ok_g_xmax;
    int ok_g_ymax;
    double i_xmin;
    double i_ymin;
    double i_xmax;
    double i_ymax;
    int ok_i_xmin;
    int ok_i_ymin;
    int ok_i_xmax;
    int ok_i_ymax;

/* checking if the R*Tree Spatial Index is defined */
    sql_statement = sqlite3_mprintf ("SELECT Count(*) FROM geometry_columns "
				     "WHERE Upper(f_table_name) = Upper(%Q) "
				     "AND Upper(f_geometry_column) = Upper(%Q) AND spatial_index_enabled = 1",
				     table, geom);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CheckSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  goto err_label;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	      is_defined = sqlite3_column_int (stmt, 0);
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto err_label;
	    }
      }
    sqlite3_finalize (stmt);
    if (!is_defined)
	goto err_label;

    xgeom = gaiaDoubleQuotedSql ((char *) geom);
    xtable = gaiaDoubleQuotedSql ((char *) table);
    idx_name = sqlite3_mprintf ("idx_%s_%s", table, geom);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);

/* counting how many Geometries are set into the main-table */
    sql_statement = sqlite3_mprintf ("SELECT Count(*) FROM \"%s\" "
				     "WHERE ST_GeometryType(\"%s\") IS NOT NULL",
				     xtable, xgeom);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CheckSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  goto err_label;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	      count_geom = sqlite3_column_int (stmt, 0);
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto err_label;
	    }
      }
    sqlite3_finalize (stmt);

/* counting how many R*Tree entries are defined */
    sql_statement = sqlite3_mprintf ("SELECT Count(*) FROM \"%s\"", xidx_name);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CheckSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  goto err_label;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	      count_rtree = sqlite3_column_int (stmt, 0);
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto err_label;
	    }
      }
    sqlite3_finalize (stmt);
    if (count_geom != count_rtree)
      {
	  /* unexpected count difference */
	  goto mismatching_zero;
      }

/* checking the geometry-table against the corresponding R*Tree */
    sql_statement =
	sqlite3_mprintf ("SELECT MbrMinX(g.\"%s\"), MbrMinY(g.\"%s\"), "
			 "MbrMaxX(g.\"%s\"), MbrMaxY(g.\"%s\"), i.xmin, i.ymin, i.xmax, i.ymax\n"
			 "FROM \"%s\" AS g\nLEFT JOIN \"%s\" AS i ON (g.ROWID = i.pkid)",
			 xgeom, xgeom, xgeom, xgeom, xtable, xidx_name);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CheckSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  goto err_label;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		/* checking a row */
		ok_g_xmin = 1;
		ok_g_ymin = 1;
		ok_g_xmax = 1;
		ok_g_ymax = 1;
		ok_i_xmin = 1;
		ok_i_ymin = 1;
		ok_i_xmax = 1;
		ok_i_ymax = 1;
		if (sqlite3_column_type (stmt, 0) == SQLITE_NULL)
		    ok_g_xmin = 0;
		else
		    g_xmin = sqlite3_column_double (stmt, 0);
		if (sqlite3_column_type (stmt, 1) == SQLITE_NULL)
		    ok_g_ymin = 0;
		else
		    g_ymin = sqlite3_column_double (stmt, 1);
		if (sqlite3_column_type (stmt, 2) == SQLITE_NULL)
		    ok_g_xmax = 0;
		else
		    g_xmax = sqlite3_column_double (stmt, 2);
		if (sqlite3_column_type (stmt, 3) == SQLITE_NULL)
		    ok_g_ymax = 0;
		else
		    g_ymax = sqlite3_column_double (stmt, 3);
		if (sqlite3_column_type (stmt, 4) == SQLITE_NULL)
		    ok_i_xmin = 0;
		else
		    i_xmin = sqlite3_column_double (stmt, 4);
		if (sqlite3_column_type (stmt, 5) == SQLITE_NULL)
		    ok_i_ymin = 0;
		else
		    i_ymin = sqlite3_column_double (stmt, 5);
		if (sqlite3_column_type (stmt, 6) == SQLITE_NULL)
		    ok_i_xmax = 0;
		else
		    i_xmax = sqlite3_column_double (stmt, 6);
		if (sqlite3_column_type (stmt, 7) == SQLITE_NULL)
		    ok_i_ymax = 0;
		else
		    i_ymax = sqlite3_column_double (stmt, 7);
		if (eval_rtree_entry (ok_g_xmin, g_xmin, ok_i_xmin, i_xmin)
		    == 0)
		    goto mismatching;
		if (eval_rtree_entry (ok_g_ymin, g_ymin, ok_i_ymin, i_ymin)
		    == 0)
		    goto mismatching;
		if (eval_rtree_entry (ok_g_xmax, g_xmax, ok_i_xmax, i_xmax)
		    == 0)
		    goto mismatching;
		if (eval_rtree_entry (ok_g_ymax, g_ymax, ok_i_ymax, i_ymax)
		    == 0)
		    goto mismatching;
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto err_label;
	    }
      }
/* we have now to finalize the query [memory cleanup] */
    sqlite3_finalize (stmt);


/* now we'll check the R*Tree against the corresponding geometry-table */
    sql_statement =
	sqlite3_mprintf ("SELECT MbrMinX(g.\"%s\"), MbrMinY(g.\"%s\"), "
			 "MbrMaxX(g.\"%s\"), MbrMaxY(g.\"%s\"), i.xmin, i.ymin, i.xmax, i.ymax\n"
			 "FROM \"%s\" AS i\nLEFT JOIN \"%s\" AS g ON (g.ROWID = i.pkid)",
			 xgeom, xgeom, xgeom, xgeom, xidx_name, xtable);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CheckSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  goto err_label;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		/* checking a row */
		ok_g_xmin = 1;
		ok_g_ymin = 1;
		ok_g_xmax = 1;
		ok_g_ymax = 1;
		ok_i_xmin = 1;
		ok_i_ymin = 1;
		ok_i_xmax = 1;
		ok_i_ymax = 1;
		if (sqlite3_column_type (stmt, 0) == SQLITE_NULL)
		    ok_g_xmin = 0;
		else
		    g_xmin = sqlite3_column_double (stmt, 0);
		if (sqlite3_column_type (stmt, 1) == SQLITE_NULL)
		    ok_g_ymin = 0;
		else
		    g_ymin = sqlite3_column_double (stmt, 1);
		if (sqlite3_column_type (stmt, 2) == SQLITE_NULL)
		    ok_g_xmax = 0;
		else
		    g_xmax = sqlite3_column_double (stmt, 2);
		if (sqlite3_column_type (stmt, 3) == SQLITE_NULL)
		    ok_g_ymax = 0;
		else
		    g_ymax = sqlite3_column_double (stmt, 3);
		if (sqlite3_column_type (stmt, 4) == SQLITE_NULL)
		    ok_i_xmin = 0;
		else
		    i_xmin = sqlite3_column_double (stmt, 4);
		if (sqlite3_column_type (stmt, 5) == SQLITE_NULL)
		    ok_i_ymin = 0;
		else
		    i_ymin = sqlite3_column_double (stmt, 5);
		if (sqlite3_column_type (stmt, 6) == SQLITE_NULL)
		    ok_i_xmax = 0;
		else
		    i_xmax = sqlite3_column_double (stmt, 6);
		if (sqlite3_column_type (stmt, 7) == SQLITE_NULL)
		    ok_i_ymax = 0;
		else
		    i_ymax = sqlite3_column_double (stmt, 7);
		if (eval_rtree_entry (ok_g_xmin, g_xmin, ok_i_xmin, i_xmin)
		    == 0)
		    goto mismatching;
		if (eval_rtree_entry (ok_g_ymin, g_ymin, ok_i_ymin, i_ymin)
		    == 0)
		    goto mismatching;
		if (eval_rtree_entry (ok_g_xmax, g_xmax, ok_i_xmax, i_xmax)
		    == 0)
		    goto mismatching;
		if (eval_rtree_entry (ok_g_ymax, g_ymax, ok_i_ymax, i_ymax)
		    == 0)
		    goto mismatching;
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		goto err_label;
	    }
      }
    sqlite3_finalize (stmt);
    strcpy (sql, "Check SpatialIndex: is valid");
    updateSpatiaLiteHistory (sqlite, (const char *) table,
			     (const char *) geom, sql);
    free (xgeom);
    free (xtable);
    free (xidx_name);
    return 1;
  mismatching:
    sqlite3_finalize (stmt);
    strcpy (sql, "Check SpatialIndex: INCONSISTENCIES detected");
    updateSpatiaLiteHistory (sqlite, (const char *) table,
			     (const char *) geom, sql);
  mismatching_zero:
    if (xgeom)
	free (xgeom);
    if (xtable)
	free (xtable);
    if (xidx_name)
	free (xidx_name);
    return 0;
  err_label:
    if (xgeom)
	free (xgeom);
    if (xtable)
	free (xtable);
    if (xidx_name)
	free (xidx_name);
    return -1;
}

static int
check_any_spatial_index (sqlite3 * sqlite)
{
/* attempting to check any defined R*Tree for consistency */
    const unsigned char *table;
    const unsigned char *column;
    int status;
    char sql[1024];
    int ret;
    int invalid_rtree = 0;
    sqlite3_stmt *stmt;

/* retrieving any defined R*Tree */
    strcpy (sql,
	    "SELECT f_table_name, f_geometry_column FROM geometry_columns ");
    strcat (sql, "WHERE spatial_index_enabled = 1");
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CheckSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  return -1;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		/* checking a single R*Tree */
		table = sqlite3_column_text (stmt, 0);
		column = sqlite3_column_text (stmt, 1);
		status = check_spatial_index (sqlite, table, column);
		if (status < 0)
		  {
		      sqlite3_finalize (stmt);
		      return -1;
		  }
		if (status == 0)
		    invalid_rtree = 1;
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		return -1;
	    }
      }
    sqlite3_finalize (stmt);
    if (invalid_rtree)
	return 0;
    return 1;
}

static void
fnct_CheckSpatialIndex (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ CheckSpatialIndex()
/ CheckSpatialIndex(table, column)
/
/ checks a SpatialIndex for consistency, returning:
/ 1 - the R*Tree is fully consistent
/ 0 - the R*Tree is inconsistent
/ NULL on failure
*/
    const unsigned char *table;
    const unsigned char *column;
    int status;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 0)
      {
	  /* no arguments: we must check any defined R*Tree */
	  status = check_any_spatial_index (sqlite);
	  if (status < 0)
	      sqlite3_result_null (context);
	  else if (status > 0)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }

    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("CheckSpatialIndex() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_null (context);
	  return;
      }
    table = sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("CheckSpatialIndex() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_null (context);
	  return;
      }
    column = sqlite3_value_text (argv[1]);
    status = check_spatial_index (sqlite, table, column);
    if (status < 0)
	sqlite3_result_null (context);
    else if (status > 0)
	sqlite3_result_int (context, 1);
    else
	sqlite3_result_int (context, 0);
}

static int
recover_spatial_index (sqlite3 * sqlite, const unsigned char *table,
		       const unsigned char *geom)
{
/* attempting to rebuild an R*Tree */
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    char *idx_name;
    char *xidx_name;
    char sql[1024];
    int is_defined = 0;
    sqlite3_stmt *stmt;

/* checking if the R*Tree Spatial Index is defined */
    sql_statement = sqlite3_mprintf ("SELECT Count(*) FROM geometry_columns "
				     "WHERE Upper(f_table_name) = Upper(%Q) "
				     "AND Upper(f_geometry_column) = Upper(%Q) AND spatial_index_enabled = 1",
				     table, geom);
    ret = sqlite3_prepare_v2 (sqlite, sql_statement, strlen (sql_statement),
			      &stmt, NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RecoverSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  return -1;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	      is_defined = sqlite3_column_int (stmt, 0);
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		return -1;
	    }
      }
    sqlite3_finalize (stmt);
    if (!is_defined)
	return -1;

/* erasing the R*Tree table */
    idx_name = sqlite3_mprintf ("idx_%s_%s", table, geom);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement = sqlite3_mprintf ("DELETE FROM \"%s\"", xidx_name);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
/* populating the R*Tree table from scratch */
    buildSpatialIndex (sqlite, table, (const char *) geom);
    strcpy (sql, "SpatialIndex: successfully recovered");
    updateSpatiaLiteHistory (sqlite, (const char *) table,
			     (const char *) geom, sql);
    return 1;
  error:
    spatialite_e ("RecoverSpatialIndex() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    return 0;
}

static int
recover_any_spatial_index (sqlite3 * sqlite, int no_check)
{
/* attempting to rebuild any defined R*Tree */
    const unsigned char *table;
    const unsigned char *column;
    int status;
    char sql[1024];
    int ret;
    int to_be_fixed;
    sqlite3_stmt *stmt;

/* retrieving any defined R*Tree */
    strcpy (sql,
	    "SELECT f_table_name, f_geometry_column FROM geometry_columns ");
    strcat (sql, "WHERE spatial_index_enabled = 1");
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("RecoverSpatialIndex SQL error: %s\n",
			sqlite3_errmsg (sqlite));
	  return -1;
      }
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		/* checking a single R*Tree */
		table = sqlite3_column_text (stmt, 0);
		column = sqlite3_column_text (stmt, 1);
		to_be_fixed = 1;
		if (!no_check)
		  {
		      status = check_spatial_index (sqlite, table, column);
		      if (status < 0)
			{
			    /* some unexpected error occurred */
			    goto fatal_error;
			}
		      else if (status > 0)
			{
			    /* the Spatial Index is already valid */
			    to_be_fixed = 0;
			}
		  }
		if (to_be_fixed)
		  {
		      /* rebuilding the Spatial Index */
		      status = recover_spatial_index (sqlite, table, column);
		      if (status < 0)
			{
			    /* some unexpected error occurred */
			    goto fatal_error;
			}
		      else if (status == 0)
			  goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (sqlite));
		sqlite3_finalize (stmt);
		return -1;
	    }
      }
    sqlite3_finalize (stmt);
    return 1;
  error:
    sqlite3_finalize (stmt);
    return 0;
  fatal_error:
    sqlite3_finalize (stmt);
    return -1;
}

static void
fnct_RecoverSpatialIndex (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ RecoverSpatialIndex()
/ RecoverSpatialIndex(no_check)
/ RecoverSpatialIndex(table, column)
/ RecoverSpatialIndex(table, column, no_check)
/
/ attempts to rebuild a SpatialIndex, returning:
/ 1 - on success
/ 0 - on failure
/ NULL if any syntax error is detected
*/
    const unsigned char *table;
    const unsigned char *column;
    int no_check = 0;
    int status;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc <= 1)
      {
	  /* no arguments: we must rebuild any defined R*Tree */
	  if (argc == 1)
	    {
		if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
		    no_check = sqlite3_value_int (argv[0]);
		else
		  {
		      spatialite_e
			  ("RecoverSpatialIndex() error: argument 1 [no_check] is not of the Integer type\n");
		      sqlite3_result_null (context);
		      return;
		  }
	    }
	  status = recover_any_spatial_index (sqlite, no_check);
	  if (status < 0)
	      sqlite3_result_null (context);
	  else if (status > 0)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }

    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverSpatialIndex() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_null (context);
	  return;
      }
    table = sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RecoverSpatialIndex() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_null (context);
	  return;
      }
    column = sqlite3_value_text (argv[1]);
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      no_check = sqlite3_value_int (argv[2]);
	  else
	    {
		spatialite_e
		    ("RecoverSpatialIndex() error: argument 2 [no_check] is not of the Integer type\n");
		sqlite3_result_null (context);
		return;
	    }
      }
    if (!no_check)
      {
	  /* checking the current SpatialIndex validity */
	  status = check_spatial_index (sqlite, table, column);
	  if (status < 0)
	    {
		/* some unexpected error occurred */
		sqlite3_result_null (context);
		return;
	    }
	  else if (status > 0)
	    {
		/* the Spatial Index is already valid */
		sqlite3_result_int (context, 1);
		return;
	    }
      }
/* rebuilding the Spatial Index */
    status = recover_spatial_index (sqlite, table, column);
    if (status < 0)
	sqlite3_result_null (context);
    else if (status > 0)
	sqlite3_result_int (context, 1);
    else
	sqlite3_result_int (context, 0);
}

static void
fnct_CreateSpatialIndex (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ CreateSpatialIndex(table, column )
/
/ creates a SpatialIndex based on Column and Table
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    char *sql_statement;
    char sql[1024];
    char *errMsg = NULL;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("CreateSpatialIndex() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("CreateSpatialIndex() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    sql_statement =
	sqlite3_mprintf
	("UPDATE geometry_columns SET spatial_index_enabled = 1 "
	 "WHERE Upper(f_table_name) = Upper(%Q) AND "
	 "Upper(f_geometry_column) = Upper(%Q) AND spatial_index_enabled = 0",
	 table, column);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    if (sqlite3_changes (sqlite) == 0)
      {
	  spatialite_e
	      ("CreateSpatialIndex() error: either \"%s\".\"%s\" isn't a Geometry column or a SpatialIndex is already defined\n",
	       table, column);
	  sqlite3_result_int (context, 0);
	  return;
      }
    updateGeometryTriggers (sqlite, table, column);
    sqlite3_result_int (context, 1);
    strcpy (sql, "R*Tree Spatial Index successfully created");
    updateSpatiaLiteHistory (sqlite, table, column, sql);
    return;
  error:
    spatialite_e ("CreateSpatialIndex() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_CreateMbrCache (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CreateMbrCache(table, column )
/
/ creates an MBR Cache based on Column and Table
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    char *sql_statement;
    char sql[1024];
    char *errMsg = NULL;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("CreateMbrCache() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("CreateMbrCache() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    sql_statement =
	sqlite3_mprintf
	("UPDATE geometry_columns SET spatial_index_enabled = 2 "
	 "WHERE Upper(f_table_name) = Upper(%Q) "
	 "AND Upper(f_geometry_column) = Upper(%Q) AND spatial_index_enabled = 0",
	 table, column);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    if (sqlite3_changes (sqlite) == 0)
      {
	  spatialite_e
	      ("CreateMbrCache() error: either \"%s\".\"%s\" isn't a Geometry column or a SpatialIndex is already defined\n",
	       table, column);
	  sqlite3_result_int (context, 0);
	  return;
      }
    updateGeometryTriggers (sqlite, table, column);
    sqlite3_result_int (context, 1);
    strcpy (sql, "MbrCache successfully created");
    updateSpatiaLiteHistory (sqlite, table, column, sql);
    return;
  error:
    spatialite_e ("CreateMbrCache() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_DisableSpatialIndex (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ DisableSpatialIndex(table, column )
/
/ disables a SpatialIndex based on Column and Table
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    char sql[1024];
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DisableSpatialIndex() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("DisableSpatialIndex() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);

    sql_statement =
	sqlite3_mprintf
	("UPDATE geometry_columns SET spatial_index_enabled = 0 "
	 "WHERE Upper(f_table_name) = Upper(%Q) AND "
	 "Upper(f_geometry_column) = Upper(%Q) AND spatial_index_enabled <> 0",
	 table, column);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    if (sqlite3_changes (sqlite) == 0)
      {
	  spatialite_e
	      ("DisableSpatialIndex() error: either \"%s\".\"%s\" isn't a Geometry column or no SpatialIndex is defined\n",
	       table, column);
	  sqlite3_result_int (context, 0);
	  return;
      }
    updateGeometryTriggers (sqlite, table, column);
    sqlite3_result_int (context, 1);
    strcpy (sql, "SpatialIndex successfully disabled");
    updateSpatiaLiteHistory (sqlite, table, column, sql);
    return;
  error:
    spatialite_e ("DisableSpatialIndex() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_RebuildGeometryTriggers (sqlite3_context * context, int argc,
			      sqlite3_value ** argv)
{
/* SQL function:
/ RebuildGeometryTriggers(table, column )
/
/ rebuilds Geometry Triggers (constraints)  based on Column and Table
/ returns 1 on success
/ 0 on failure
*/
    const char *table;
    const char *column;
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    char **results;
    int rows;
    int columns;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RebuildGeometryTriggers() error: argument 1 [table_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    table = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  spatialite_e
	      ("RebuildGeometryTriggers() error: argument 2 [column_name] is not of the String type\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    column = (const char *) sqlite3_value_text (argv[1]);
    sql_statement =
	sqlite3_mprintf ("SELECT f_table_name FROM geometry_columns "
			 "WHERE Upper(f_table_name) = Upper(%Q) AND Upper(f_geometry_column) = Upper (%Q)",
			 table, column);
    ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows, &columns,
			     NULL);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
	goto error;
    sqlite3_free_table (results);
    if (rows <= 0)
      {
	  spatialite_e
	      ("RebuildGeometryTriggers() error: \"%s\".\"%s\" isn't a Geometry column\n",
	       table, column);
	  sqlite3_result_int (context, 0);
	  return;
      }
    updateGeometryTriggers (sqlite, table, column);
    sqlite3_result_int (context, 1);
    updateSpatiaLiteHistory (sqlite, table, column,
			     "Geometry Triggers successfully rebuilt");
    return;
  error:
    spatialite_e ("RebuildGeometryTriggers() error: \"%s\"\n", errMsg);
    sqlite3_free (errMsg);
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_UpdateLayerStatistics (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ UpdateLayerStatistics(table, column )
/
/ Updates LAYER_STATISTICS [based on Column and Table]
/ returns 1 on success
/ 0 on failure
*/
    const char *sql;
    const char *table = NULL;
    const char *column = NULL;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc >= 1)
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
	    {
		spatialite_e
		    ("UpdateLayerStatistics() error: argument 1 [table_name] is not of the String type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  table = (const char *) sqlite3_value_text (argv[0]);
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
	    {
		spatialite_e
		    ("UpdateLayerStatistics() error: argument 2 [column_name] is not of the String type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  column = (const char *) sqlite3_value_text (argv[1]);
      }
    if (!update_layer_statistics (sqlite, table, column))
	goto error;
    sqlite3_result_int (context, 1);
    sql = "UpdateLayerStatistics";
    if (table == NULL)
	table = "ALL-TABLES";
    if (column == NULL)
	column = "ALL-GEOMETRY-COLUMNS";
    updateSpatiaLiteHistory (sqlite, (const char *) table,
			     (const char *) column, sql);
    return;
  error:
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_GetLayerExtent (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GetLayerExtent(table)
/ GetLayerExtent(table, column )
/ GetLayerExtent(table, column, pessimistic )
/
/ Return a Geometry (Envelope) corresponding to the full layer
/ extent [eventually updating the supporting statistics
/ NULL on failure
*/
    const char *table = NULL;
    const char *column = NULL;
    int pessimistic = 0;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc >= 1)
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
	    {
		spatialite_e
		    ("GetLayerExtent() error: argument 1 [table_name] is not of the String type\n");
		sqlite3_result_null (context);
		return;
	    }
	  table = (const char *) sqlite3_value_text (argv[0]);
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
	    {
		spatialite_e
		    ("GetLayerExtent() error: argument 2 [column_name] is not of the String type\n");
		sqlite3_result_null (context);
		return;
	    }
	  column = (const char *) sqlite3_value_text (argv[1]);
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		spatialite_e
		    ("GetLayerExtent() error: argument 3 [OPTIMISTIC/PESSIMISTIC] is not of the Integer type\n");
		sqlite3_result_null (context);
		return;
	    }
	  pessimistic = sqlite3_value_int (argv[2]);
      }
    geom = gaiaGetLayerExtent (sqlite, table, column, pessimistic);
    if (!geom)
	goto error;
/* builds the BLOB geometry to be returned */
    gaiaToSpatiaLiteBlobWkb (geom, &p_result, &len);
    sqlite3_result_blob (context, p_result, len, free);
    gaiaFreeGeomColl (geom);
    return;
  error:
    sqlite3_result_null (context);
    return;
}

static void
fnct_CreateRasterCoveragesTable (sqlite3_context * context, int argc,
				 sqlite3_value ** argv)
{
/* SQL function:
/ CreateRasterCoveragesTable()
/
/ creates the main RasterCoverages table 
/ returns 1 on success
/ 0 on failure
*/
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */

    if (!createRasterCoveragesTable (sqlite))
	goto error;
    updateSpatiaLiteHistory (sqlite, "*** Raster Coveraverages ***", NULL,
			     "Main table successfully created");
    sqlite3_result_int (context, 1);
    return;

  error:
    sqlite3_result_int (context, 0);
    return;
}

static gaiaPointPtr
simplePoint (gaiaGeomCollPtr geo)
{
/* helper function
/ if this GEOMETRY contains only one POINT, and no other elementary geometry
/ the POINT address will be returned
/ otherwise NULL will be returned
*/
    int cnt = 0;
    gaiaPointPtr point;
    gaiaPointPtr this_point = NULL;
    if (!geo)
	return NULL;
    if (geo->FirstLinestring || geo->FirstPolygon)
	return NULL;
    point = geo->FirstPoint;
    while (point)
      {
	  /* counting how many POINTs are there */
	  cnt++;
	  this_point = point;
	  point = point->Next;
      }
    if (cnt == 1 && this_point)
	return this_point;
    return NULL;
}

static gaiaLinestringPtr
simpleLinestring (gaiaGeomCollPtr geo)
{
/* helper function
/ if this GEOMETRY contains only one LINESTRING, and no other elementary geometry
/ the LINESTRING address will be returned
/ otherwise NULL will be returned
*/
    int cnt = 0;
    gaiaLinestringPtr line;
    gaiaLinestringPtr this_line = NULL;
    if (!geo)
	return NULL;
    if (geo->FirstPoint || geo->FirstPolygon)
	return NULL;
    line = geo->FirstLinestring;
    while (line)
      {
	  /* counting how many LINESTRINGs are there */
	  cnt++;
	  this_line = line;
	  line = line->Next;
      }
    if (cnt == 1 && this_line)
	return this_line;
    return NULL;
}

static gaiaPolygonPtr
simplePolygon (gaiaGeomCollPtr geo)
{
/* helper function
/ if this GEOMETRY contains only one POLYGON, and no other elementary geometry
/ the POLYGON address will be returned
/ otherwise NULL will be returned
*/
    int cnt = 0;
    gaiaPolygonPtr polyg;
    gaiaPolygonPtr this_polyg = NULL;
    if (!geo)
	return NULL;
    if (geo->FirstPoint || geo->FirstLinestring)
	return NULL;
    polyg = geo->FirstPolygon;
    while (polyg)
      {
	  /* counting how many POLYGONs are there */
	  cnt++;
	  this_polyg = polyg;
	  polyg = polyg->Next;
      }
    if (cnt == 1 && this_polyg)
	return this_polyg;
    return NULL;
}

static void
fnct_AsText (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsText(BLOB encoded geometry)
/
/ returns the corresponding WKT encoded value
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    gaiaOutBufferInitialize (&out_buf);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaOutWkt (&out_buf, geo);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_AsWkt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsWkt(BLOB encoded geometry [, Integer precision])
/
/ returns the corresponding WKT encoded value
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    int precision = 15;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[1]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    gaiaOutBufferInitialize (&out_buf);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaOutWktStrict (&out_buf, geo, precision);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

/*
/
/ AsSvg(geometry,[relative], [precision]) implementation
/
////////////////////////////////////////////////////////////
/
/ Author: Klaus Foerster klaus.foerster@svg.cc
/ version 0.9. 2008 September 21
 /
 */

static void
fnct_AsSvg (sqlite3_context * context, int argc, sqlite3_value ** argv,
	    int relative, int precision)
{
/* SQL function:
   AsSvg(BLOB encoded geometry, [int relative], [int precision])
   returns the corresponding SVG encoded value or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
      {
	  sqlite3_result_null (context);
	  return;
      }
    else
      {
	  /* make sure relative is 0 or 1 */
	  if (relative > 0)
	      relative = 1;
	  else
	      relative = 0;
	  /* make sure precision is between 0 and 15 - default to 6 if absent */
	  if (precision > GAIA_SVG_DEFAULT_MAX_PRECISION)
	      precision = GAIA_SVG_DEFAULT_MAX_PRECISION;
	  if (precision < 0)
	      precision = 0;
	  /* produce SVG-notation - actual work is done in gaiageo/gg_wkt.c */
	  gaiaOutBufferInitialize (&out_buf);
	  gaiaOutSvg (&out_buf, geo, relative, precision);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_AsSvg1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* called without additional arguments */
    fnct_AsSvg (context, argc, argv, GAIA_SVG_DEFAULT_RELATIVE,
		GAIA_SVG_DEFAULT_PRECISION);
}

static void
fnct_AsSvg2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* called with relative-switch */
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	fnct_AsSvg (context, argc, argv, sqlite3_value_int (argv[1]),
		    GAIA_SVG_DEFAULT_PRECISION);
    else
	sqlite3_result_null (context);
}

static void
fnct_AsSvg3 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* called with relative-switch and precision-argument */
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER
	&& sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	fnct_AsSvg (context, argc, argv, sqlite3_value_int (argv[1]),
		    sqlite3_value_int (argv[2]));
    else
	sqlite3_result_null (context);
}

/* END of Klaus Foerster AsSvg() implementation */

SPATIALITE_PRIVATE void
getProjParams (void *p_sqlite, int srid, char **proj_params)
{
/* retrives the PROJ params from SPATIAL_SYS_REF table, if possible */
    sqlite3 *sqlite = (sqlite3 *) p_sqlite;
    char *sql;
    char **results;
    int rows;
    int columns;
    int i;
    int ret;
    int len;
    const char *proj4text;
    char *errMsg = NULL;
    *proj_params = NULL;
    sql = sqlite3_mprintf
	("SELECT proj4text FROM spatial_ref_sys WHERE srid = %d", srid);
    ret = sqlite3_get_table (sqlite, sql, &results, &rows, &columns, &errMsg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("unknown SRID: %d\t<%s>\n", srid, errMsg);
	  sqlite3_free (errMsg);
	  return;
      }
    for (i = 1; i <= rows; i++)
      {
	  proj4text = results[(i * columns)];
	  if (proj4text != NULL)
	    {
		len = strlen (proj4text);
		*proj_params = malloc (len + 1);
		strcpy (*proj_params, proj4text);
	    }
      }
    if (*proj_params == NULL)
	spatialite_e ("unknown SRID: %d\n", srid);
    sqlite3_free_table (results);
}

#ifndef OMIT_PROJ		/* PROJ.4 is strictly required to support KML */
static void
fnct_AsKml1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsKml(BLOB encoded geometry [, Integer precision])
/
/ returns the corresponding 'bare geom' KML representation 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geo_wgs84;
    char *proj_from;
    char *proj_to;
    int precision = 15;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[1]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    gaiaOutBufferInitialize (&out_buf);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->Srid == 4326)
	      ;			/* already WGS84 */
	  else if (geo->Srid <= 0)
	    {
		/* unknown SRID: giving up */
		sqlite3_result_null (context);
		goto stop;
	    }
	  else
	    {
		/* attempting to reproject into WGS84 */
		getProjParams (sqlite, geo->Srid, &proj_from);
		getProjParams (sqlite, 4326, &proj_to);
		if (proj_to == NULL || proj_from == NULL)
		  {
		      if (proj_from)
			  free (proj_from);
		      if (proj_to)
			  free (proj_to);
		      sqlite3_result_null (context);
		      goto stop;
		  }
		geo_wgs84 = gaiaTransform (geo, proj_from, proj_to);
		free (proj_from);
		free (proj_to);
		if (!geo_wgs84)
		  {
		      sqlite3_result_null (context);
		      goto stop;
		  }
		/* ok, reprojection was successful */
		gaiaFreeGeomColl (geo);
		geo = geo_wgs84;
	    }
	  /* produce KML-notation - actual work is done in gaiageo/gg_wkt.c */
	  gaiaOutBareKml (&out_buf, geo, precision);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
  stop:
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_AsKml3 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsKml(Anything name, Anything description, BLOB encoded geometry [, Integer precision])
/
/ returns the corresponding 'full' KML representation 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geo_wgs84;
    sqlite3_int64 int_value;
    double dbl_value;
    const char *name;
    const char *desc;
    char *name_malloc = NULL;
    char *desc_malloc = NULL;
    char dummy[128];
    char *xdummy;
    char *proj_from;
    char *proj_to;
    int precision = 15;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    switch (sqlite3_value_type (argv[0]))
      {
      case SQLITE_TEXT:
	  name = (const char *) sqlite3_value_text (argv[0]);
	  len = strlen (name);
	  name_malloc = malloc (len + 1);
	  strcpy (name_malloc, name);
	  name = name_malloc;
	  break;
      case SQLITE_INTEGER:
	  int_value = sqlite3_value_int64 (argv[0]);
	  sprintf (dummy, FRMT64, int_value);
	  len = strlen (dummy);
	  name_malloc = malloc (len + 1);
	  strcpy (name_malloc, dummy);
	  name = name_malloc;
	  break;
      case SQLITE_FLOAT:
	  dbl_value = sqlite3_value_double (argv[0]);
	  xdummy = sqlite3_mprintf ("%1.6f", dbl_value);
	  len = strlen (xdummy);
	  name_malloc = malloc (len + 1);
	  strcpy (name_malloc, xdummy);
	  sqlite3_free (xdummy);
	  name = name_malloc;
	  break;
      case SQLITE_BLOB:
	  name = "BLOB";
	  break;
      default:
	  name = "NULL";
	  break;
      };
    switch (sqlite3_value_type (argv[1]))
      {
      case SQLITE_TEXT:
	  desc = (const char *) sqlite3_value_text (argv[1]);
	  len = strlen (desc);
	  desc_malloc = malloc (len + 1);
	  strcpy (desc_malloc, desc);
	  desc = desc_malloc;
	  break;
      case SQLITE_INTEGER:
	  int_value = sqlite3_value_int64 (argv[1]);
	  sprintf (dummy, FRMT64, int_value);
	  len = strlen (dummy);
	  desc_malloc = malloc (len + 1);
	  strcpy (desc_malloc, dummy);
	  desc = desc_malloc;
	  break;
      case SQLITE_FLOAT:
	  dbl_value = sqlite3_value_double (argv[1]);
	  xdummy = sqlite3_mprintf ("%1.6f", dbl_value);
	  len = strlen (xdummy);
	  desc_malloc = malloc (len + 1);
	  strcpy (desc_malloc, xdummy);
	  sqlite3_free (xdummy);
	  desc = desc_malloc;
	  break;
      case SQLITE_BLOB:
	  desc = "BLOB";
	  break;
      default:
	  desc = "NULL";
	  break;
      };
    gaiaOutBufferInitialize (&out_buf);
    if (sqlite3_value_type (argv[2]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[2]);
    n_bytes = sqlite3_value_bytes (argv[2]);
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[3]);
	  else
	    {
		sqlite3_result_null (context);
		goto stop;
	    }
      }
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->Srid == 4326)
	      ;			/* already WGS84 */
	  else if (geo->Srid == 0)
	    {
		/* unknown SRID: giving up */
		sqlite3_result_null (context);
		goto stop;
	    }
	  else
	    {
		/* attempting to reproject into WGS84 */
		getProjParams (sqlite, geo->Srid, &proj_from);
		getProjParams (sqlite, 4326, &proj_to);
		if (proj_to == NULL || proj_from == NULL)
		  {
		      if (proj_from != NULL)
			  free (proj_from);
		      if (proj_to != NULL)
			  free (proj_to);
		      sqlite3_result_null (context);
		      goto stop;
		  }
		geo_wgs84 = gaiaTransform (geo, proj_from, proj_to);
		free (proj_from);
		free (proj_to);
		if (!geo_wgs84)
		  {
		      sqlite3_result_null (context);
		      goto stop;
		  }
		/* ok, reprojection was successful */
		gaiaFreeGeomColl (geo);
		geo = geo_wgs84;
	    }
	  /* produce KML-notation - actual work is done in gaiageo/gg_wkt.c */
	  gaiaOutFullKml (&out_buf, name, desc, geo, precision);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
  stop:
    gaiaFreeGeomColl (geo);
    if (name_malloc)
	free (name_malloc);
    if (desc_malloc)
	free (desc_malloc);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_AsKml (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsKml(Anything name, Anything description, BLOB encoded geometry)
/     or
/ AsKml(BLOB encoded geometry)
/
/ returns the corresponding KML representation 
/ or NULL if any error is encountered
*/
    if (argc == 3 || argc == 4)
	fnct_AsKml3 (context, argc, argv);
    else
	fnct_AsKml1 (context, argc, argv);
}
#endif /* end including PROJ.4 */

static void
fnct_AsGml (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsGml(BLOB encoded geometry)
/    or
/ AsGml(integer version, BLOB encoded geometry)
/    or
/ AsGml(integer version, BLOB encoded geometry, integer precision)
/
/ *version* may be 2 (GML 2.1.2) or 3 (GML 3.1.1)
/ default *version*: 2
/
/ *precision* is the number of output decimal digits
/ default *precision*: 15
/
/ returns the corresponding GML representation 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    int version = 2;
    int precision = 15;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
	      version = sqlite3_value_int (argv[0]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
	  n_bytes = sqlite3_value_bytes (argv[1]);
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[2]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    else if (argc == 2)
      {
	  if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER
	      && sqlite3_value_type (argv[1]) == SQLITE_BLOB)
	    {
		version = sqlite3_value_int (argv[0]);
		p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
		n_bytes = sqlite3_value_bytes (argv[1]);
	    }
	  else if (sqlite3_value_type (argv[0]) == SQLITE_BLOB
		   && sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
		n_bytes = sqlite3_value_bytes (argv[0]);
		precision = sqlite3_value_int (argv[1]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    else
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
      }
    gaiaOutBufferInitialize (&out_buf);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  /* produce GML-notation - actual work is done in gaiageo/gg_wkt.c */
	  gaiaOutGml (&out_buf, version, precision, geo);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_AsGeoJSON (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsGeoJSON(BLOB encoded geometry)
/    or
/ AsGeoJSON(BLOB encoded geometry, integer precision)
/    or
/ AsGeoJSON(BLOB encoded geometry, integer precision, integer options)
/
/ *precision* is the number of output decimal digits
/ default *precision*: 15
/
/ *options* may be one of the followings:
/   0 = no options [default]
/   1 = GeoJSON MBR
/   2 = GeoJSON Short CRS (e.g EPSG:4326) 
/   3 = 1 + 2 (Mbr + shortCrs)
/   4 = GeoJSON Long CRS (e.g urn:ogc:def:crs:EPSG::4326)
/   5 = 1 + 4 (Mbr + longCrs)
/
/ returns the corresponding GML representation 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    int precision = 15;
    int options = 0;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[0]) == SQLITE_BLOB
	      && sqlite3_value_type (argv[1]) == SQLITE_INTEGER
	      && sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
		n_bytes = sqlite3_value_bytes (argv[0]);
		precision = sqlite3_value_int (argv[1]);
		options = sqlite3_value_int (argv[2]);
		if (options >= 1 && options <= 5)
		    ;
		else
		    options = 0;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    else if (argc == 2)
      {
	  if (sqlite3_value_type (argv[0]) == SQLITE_BLOB
	      && sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
		n_bytes = sqlite3_value_bytes (argv[0]);
		precision = sqlite3_value_int (argv[1]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    else
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
      }
    gaiaOutBufferInitialize (&out_buf);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  /* produce GeoJSON-notation - actual work is done in gaiageo/gg_wkt.c */
	  gaiaOutGeoJSON (&out_buf, geo, precision, options);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_AsBinary (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsBinary(BLOB encoded geometry)
/
/ returns the corresponding WKB encoded value
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaToWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_AsFGF (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsFGF(BLOB encoded geometry)
/
/ returns the corresponding FGF encoded value
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    int coord_dims;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  spatialite_e
	      ("AsFGF() error: argument 2 [geom_coords] is not of the Integer type\n");
	  sqlite3_result_null (context);
	  return;
      }
    coord_dims = sqlite3_value_int (argv[1]);
    if (coord_dims
	== 0 || coord_dims == 1 || coord_dims == 2 || coord_dims == 3)
	;
    else
      {
	  spatialite_e
	      ("AsFGF() error: argument 2 [geom_coords] out of range [0,1,2,3]\n");
	  sqlite3_result_null (context);
	  return;
      }
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaToFgf (geo, &p_result, &len, coord_dims);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_MakePoint1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePoint(double X, double Y)
/    alias
/ ST_Point(double X, double Y)
/
/ builds a POINT 
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePoint (x, y, 0, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePoint2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePoint(double X, double Y, int SRID)
/
/ builds a POINT 
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    int srid;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePoint (x, y, srid, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePointZ1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePointZ(double X, double Y, double Z)
/
/ builds a POINT Z 
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double z;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	z = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  z = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePointZ (x, y, z, 0, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePointZ2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePointZ(double X, double Y, double Z, int SRID)
/
/ builds a POINT Z
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double z;
    int srid;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	z = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  z = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePointZ (x, y, z, srid, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePointM1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePointM(double X, double Y, double M)
/
/ builds a POINT M
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double m;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	m = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  m = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePointM (x, y, m, 0, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePointM2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePointM(double X, double Y, double M, int SRID)
/
/ builds a POINT M
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double m;
    int srid;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	m = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  m = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePointM (x, y, m, srid, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePointZM1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePointZM(double X, double Y, double Z, double M)
/
/ builds a POINT ZM 
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double z;
    double m;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	z = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  z = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	m = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  m = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePointZM (x, y, z, m, 0, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_MakePointZM2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakePointZM(double X, double Y, double Z, double M, int SRID)
/
/ builds a POINT 
/ or NULL if any error is encountered
*/
    int len;
    int int_value;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double z;
    double m;
    int srid;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	z = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  z = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	m = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  m = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaMakePointZM (x, y, z, m, srid, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
addGeomPointToDynamicLine (gaiaDynamicLinePtr dyn, gaiaGeomCollPtr geom)
{
/* appending a simple-Point Geometry to a Dynamic Line */
    int pts;
    int lns;
    int pgs;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;

    if (dyn == NULL)
	return;
    if (dyn->Error)
	return;
/* checking if GEOM simply is a POINT */
    if (geom == NULL)
      {
	  dyn->Error = 1;
	  return;
      }
    pts = 0;
    lns = 0;
    pgs = 0;
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts == 1 && lns == 0 && pgs == 0)
	;
    else
      {
	  /* failure: not a simple POINT */
	  dyn->Error = 1;
	  return;
      }

    if (dyn->Srid != geom->Srid)
      {
	  /* failure: SRID mismatch */
	  dyn->Error = 1;
	  return;
      }

    switch (geom->FirstPoint->DimensionModel)
      {
      case GAIA_XY_Z_M:
	  gaiaAppendPointZMToDynamicLine (dyn, geom->FirstPoint->X,
					  geom->FirstPoint->Y,
					  geom->FirstPoint->Z,
					  geom->FirstPoint->M);
	  break;
      case GAIA_XY_Z:
	  gaiaAppendPointZToDynamicLine (dyn, geom->FirstPoint->X,
					 geom->FirstPoint->Y,
					 geom->FirstPoint->Z);
	  break;
      case GAIA_XY_M:
	  gaiaAppendPointMToDynamicLine (dyn, geom->FirstPoint->X,
					 geom->FirstPoint->Y,
					 geom->FirstPoint->M);
	  break;
      default:
	  gaiaAppendPointToDynamicLine (dyn, geom->FirstPoint->X,
					geom->FirstPoint->Y);
	  break;
      }
}

static void
fnct_MakeLine_step (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakeLine(BLOBencoded geom)
/
/ aggregate function - STEP
/
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    gaiaDynamicLinePtr *p;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geom)
	return;
    p = sqlite3_aggregate_context (context, sizeof (gaiaDynamicLinePtr));
    if (!(*p))
      {
	  /* this is the first row */
	  *p = gaiaAllocDynamicLine ();
	  (*p)->Srid = geom->Srid;
	  addGeomPointToDynamicLine (*p, geom);
	  gaiaFreeGeomColl (geom);
      }
    else
      {
	  /* subsequent rows */
	  addGeomPointToDynamicLine (*p, geom);
	  gaiaFreeGeomColl (geom);
      }
}

static gaiaGeomCollPtr
geomFromDynamicLine (gaiaDynamicLinePtr dyn)
{
/* attempting to build a Geometry from a Dynamic Line */
    gaiaGeomCollPtr geom = NULL;
    gaiaLinestringPtr ln = NULL;
    gaiaPointPtr pt;
    int iv;
    int count = 0;
    int dims = GAIA_XY;

    if (dyn == NULL)
	return NULL;
    if (dyn->Error)
	return NULL;

    pt = dyn->First;
    while (pt)
      {
	  /* counting points and checking dims */
	  count++;
	  if (dims == GAIA_XY && pt->DimensionModel != GAIA_XY)
	      dims = pt->DimensionModel;
	  if (dims == GAIA_XY_Z
	      && (pt->DimensionModel == GAIA_XY_M
		  || pt->DimensionModel == GAIA_XY_Z_M))
	      dims = GAIA_XY_Z_M;
	  if (dims == GAIA_XY_M
	      && (pt->DimensionModel == GAIA_XY_Z
		  || pt->DimensionModel == GAIA_XY_Z_M))
	      dims = GAIA_XY_Z_M;
	  pt = pt->Next;
      }
    if (count < 2)
	return NULL;

    switch (dims)
      {
      case GAIA_XY_Z_M:
	  geom = gaiaAllocGeomCollXYZM ();
	  ln = gaiaAllocLinestringXYZM (count);
	  break;
      case GAIA_XY_Z:
	  geom = gaiaAllocGeomCollXYZ ();
	  ln = gaiaAllocLinestringXYZ (count);
	  break;
      case GAIA_XY_M:
	  geom = gaiaAllocGeomCollXYM ();
	  ln = gaiaAllocLinestringXYM (count);
	  break;
      default:
	  geom = gaiaAllocGeomColl ();
	  ln = gaiaAllocLinestring (count);
	  break;
      };

    if (geom != NULL && ln != NULL)
      {
	  gaiaInsertLinestringInGeomColl (geom, ln);
	  geom->Srid = dyn->Srid;
      }
    else
      {
	  if (geom)
	      gaiaFreeGeomColl (geom);
	  if (ln)
	      gaiaFreeLinestring (ln);
	  return NULL;
      }

    iv = 0;
    pt = dyn->First;
    while (pt)
      {
	  /* setting linestring points */
	  if (dims == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (ln->Coords, iv, pt->X, pt->Y, pt->Z, pt->M);
	    }
	  else if (dims == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (ln->Coords, iv, pt->X, pt->Y, pt->Z);
	    }
	  else if (dims == GAIA_XY_M)
	    {
		gaiaSetPointXYM (ln->Coords, iv, pt->X, pt->Y, pt->M);
	    }
	  else
	    {
		gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
	    }
	  iv++;
	  pt = pt->Next;
      }
    return geom;
}

static void
fnct_MakeLine_final (sqlite3_context * context)
{
/* SQL function:
/ MakeLine(BLOBencoded geom)
/
/ aggregate function - FINAL
/
*/
    gaiaGeomCollPtr result;
    gaiaDynamicLinePtr *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    result = geomFromDynamicLine (*p);
    gaiaFreeDynamicLine (*p);
    if (!result)
	sqlite3_result_null (context);
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (result);
      }
}

static void
buildLineFromMultiPoint (sqlite3_context * context, gaiaGeomCollPtr geom,
			 int direction)
{
/* internal: building a Linestring from a MultiPolygon */
    gaiaGeomCollPtr result;
    gaiaDynamicLinePtr dyn;
    int n_pts = 0;
    int n_lns = 0;
    int n_pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    if (geom)
      {
	  pt = geom->FirstPoint;
	  while (pt)
	    {
		n_pts++;
		pt = pt->Next;
	    }
	  ln = geom->FirstLinestring;
	  while (ln)
	    {
		n_lns++;
		ln = ln->Next;
	    }
	  pg = geom->FirstPolygon;
	  while (pg)
	    {
		n_pgs++;
		pg = pg->Next;
	    }
      }
    /* checking if really is a MultiPoint */
    if (n_pts >= 2 && n_lns == 0 && n_pgs == 0)
	;
    else
      {
	  sqlite3_result_null (context);
	  goto end;
      }
    dyn = gaiaAllocDynamicLine ();
    dyn->Srid = geom->Srid;
    pt = geom->FirstPoint;
    while (pt)
      {
	  /* inserting all Points accordingly to required direction */
	  if (direction)
	    {
		/* conformant direction */
		switch (pt->DimensionModel)
		  {
		  case GAIA_XY_Z_M:
		      gaiaAppendPointZMToDynamicLine (dyn, pt->X, pt->Y,
						      pt->Z, pt->M);
		      break;
		  case GAIA_XY_Z:
		      gaiaAppendPointZToDynamicLine (dyn, pt->X, pt->Y, pt->Z);
		      break;
		  case GAIA_XY_M:
		      gaiaAppendPointMToDynamicLine (dyn, pt->X, pt->Y, pt->M);
		      break;
		  default:
		      gaiaAppendPointToDynamicLine (dyn, pt->X, pt->Y);
		      break;
		  }
	    }
	  else
	    {
		/* reverse direction */
		switch (pt->DimensionModel)
		  {
		  case GAIA_XY_Z_M:
		      gaiaPrependPointZMToDynamicLine (dyn, pt->X, pt->Y,
						       pt->Z, pt->M);
		      break;
		  case GAIA_XY_Z:
		      gaiaPrependPointZToDynamicLine (dyn, pt->X, pt->Y, pt->Z);
		      break;
		  case GAIA_XY_M:
		      gaiaPrependPointMToDynamicLine (dyn, pt->X, pt->Y, pt->M);
		      break;
		  default:
		      gaiaPrependPointToDynamicLine (dyn, pt->X, pt->Y);
		      break;
		  }
	    }
	  pt = pt->Next;
      }
    result = geomFromDynamicLine (dyn);
    gaiaFreeDynamicLine (dyn);
    if (!result)
	sqlite3_result_null (context);
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (result);
      }
  end:
    gaiaFreeGeomColl (geom);
}

static void
fnct_MakeLine (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakeLine(point-geometry geom1, point-geometry geom2)
/     or
/ MakeLine(multipoint geom, boolean direction)
/
/ - builds a SEGMENT joining two POINTs 
/ - the MultiPoint version works exactely as the corresponding aggregate
/   function, but not requiring aggregation; direction=TRUE direct order,
/   direction=FALSE reverse order
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_blob;
    int n_bytes;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  /* expecting a single MultiPoint input */
	  int direction = sqlite3_value_int (argv[1]);
	  buildLineFromMultiPoint (context, geo1, direction);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo2)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    gaiaMakeLine (geo1, geo2, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
  stop:
    if (geo1)
	gaiaFreeGeomColl (geo1);
    if (geo2)
	gaiaFreeGeomColl (geo2);
}

static void
fnct_MakeCircle (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakeCircle(double cx, double cy, double radius)
/     or
/ MakeCircle(double cx, double cy, double radius, int srid)
/     or
/ MakeCircle(double cx, double cy, double radius, int srid, double step)
/
/ - builds a Linestring approximating a Circle
/ - step is the angular distance (in degrees) between points on 
/   the circurmference (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom = NULL;
    int ival;
    double cx;
    double cy;
    double r;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  r = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	r = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 4)
      {
	  if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[3]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 5)
      {
	  if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[4]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[4]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    geom = gaiaMakeCircle (cx, cy, r, step);
    if (!geom)
	sqlite3_result_null (context);
    else
      {
	  if (srid != 0)
	      geom->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (geom, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (geom)
	gaiaFreeGeomColl (geom);
}

static void
fnct_MakeArc (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakeArc(double cx, double cy, double radius, double start, double stop)
/     or
/ MakeArc(double cx, double cy, double radius, double start, double stop, 
/         int srid)
/     or
/ MakeArc(double cx, double cy, double radius, double start, double stop, 
/         int srid, double step)
/
/ - builds a Linestring approximating a Circular Arc
/ - start and stop are the initial and final angles (in degrees)
/ - step is the angular distance (in degrees) between points on 
/   the circurmference (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom = NULL;
    int ival;
    double cx;
    double cy;
    double r;
    double start;
    double stop;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  r = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	r = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[3]);
	  start = ival;
      }
    else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	start = sqlite3_value_double (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[4]);
	  stop = ival;
      }
    else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	stop = sqlite3_value_double (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 6)
      {
	  if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[5]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 7)
      {
	  if (sqlite3_value_type (argv[6]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[6]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[6]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[6]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    geom = gaiaMakeArc (cx, cy, r, start, stop, step);
    if (!geom)
	sqlite3_result_null (context);
    else
      {
	  if (srid != 0)
	      geom->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (geom, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (geom)
	gaiaFreeGeomColl (geom);
}

static void
fnct_MakeEllipse (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakeEllipse(double cx, double cy, double x_axis, double y_axis)
/     or
/ MakeEllipse(double cx, double cy, double x_axis, double y_axis,
/            int srid)
/     or
/ MakeEllipse(double cx, double cy, double x_axis, double y_axis,
/             int srid, double step)
/
/ - builds a Linestring approximating an Ellipse
/ - step is the angular distance (in degrees) between points on 
/   the ellipse (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom = NULL;
    int ival;
    double cx;
    double cy;
    double x_axis;
    double y_axis;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  x_axis = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	x_axis = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[3]);
	  y_axis = ival;
      }
    else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	y_axis = sqlite3_value_double (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 5)
      {
	  if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[4]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 6)
      {
	  if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[5]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[5]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[5]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    geom = gaiaMakeEllipse (cx, cy, x_axis, y_axis, step);
    if (!geom)
	sqlite3_result_null (context);
    else
      {
	  if (srid != 0)
	      geom->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (geom, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (geom)
	gaiaFreeGeomColl (geom);
}

static void
fnct_MakeEllipticArc (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ MakeEllipticArc(double cx, double cy, double x_axis, double y_axis, 
/                 double start, double stop)
/     or
/ MakeEllipticArc(double cx, double cy, double x_axis, double y_axis,
/                 double start, double stop, int srid)
/     or
/ MakeEllipticArc(double cx, double cy, double x_axis, double y_axis,
/                 double start, double stop, int srid, double step)
/
/ - builds a Linestring approximating an Elliptic Arc
/ - start and stop are the initial and final angles (in degrees)
/ - step is the angular distance (in degrees) between points on 
/   the ellipse (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom = NULL;
    int ival;
    double cx;
    double cy;
    double x_axis;
    double y_axis;
    double start;
    double stop;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  x_axis = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	x_axis = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[3]);
	  y_axis = ival;
      }
    else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	y_axis = sqlite3_value_double (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[4]);
	  start = ival;
      }
    else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	start = sqlite3_value_double (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[5]);
	  stop = ival;
      }
    else if (sqlite3_value_type (argv[5]) == SQLITE_FLOAT)
	stop = sqlite3_value_double (argv[5]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 7)
      {
	  if (sqlite3_value_type (argv[6]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[6]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 8)
      {
	  if (sqlite3_value_type (argv[7]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[7]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[7]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[7]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    geom = gaiaMakeEllipticArc (cx, cy, x_axis, y_axis, start, stop, step);
    if (!geom)
	sqlite3_result_null (context);
    else
      {
	  if (srid != 0)
	      geom->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (geom, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (geom)
	gaiaFreeGeomColl (geom);
}

static void
fnct_MakeCircularSector (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ MakeCircularSector(double cx, double cy, double radius, double start, double stop)
/     or
/ MakeCircularSector(double cx, double cy, double radius, double start, double stop, 
/         int srid)
/     or
/ MakeCircularSector(double cx, double cy, double radius, double start, double stop, 
/         int srid, double step)
/
/ - builds a Polygon approximating a Circular Sector
/ - start and stop are the initial and final angles (in degrees)
/ - step is the angular distance (in degrees) between points on 
/   the circurmference (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom = NULL;
    gaiaGeomCollPtr sector = NULL;
    int ival;
    double cx;
    double cy;
    double r;
    double start;
    double stop;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  r = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	r = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[3]);
	  start = ival;
      }
    else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	start = sqlite3_value_double (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[4]);
	  stop = ival;
      }
    else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	stop = sqlite3_value_double (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 6)
      {
	  if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[5]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 7)
      {
	  if (sqlite3_value_type (argv[6]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[6]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[6]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[6]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    geom = gaiaMakeArc (cx, cy, r, start, stop, step);
    if (!geom)
	sqlite3_result_null (context);
    else
      {
	  int ii;
	  int io = 0;
	  double x;
	  double y;
	  gaiaLinestringPtr in = geom->FirstLinestring;
	  gaiaPolygonPtr pg;
	  gaiaRingPtr out;
	  sector = gaiaAllocGeomColl ();
	  pg = gaiaAddPolygonToGeomColl (sector, in->Points + 2, 0);
	  out = pg->Exterior;
	  /* inserting the Centre - first point */
	  gaiaSetPoint (out->Coords, io, cx, cy);
	  io++;
	  for (ii = 0; ii < in->Points; ii++)
	    {
		/* copying the Arc's points */
		gaiaGetPoint (in->Coords, ii, &x, &y);
		gaiaSetPoint (out->Coords, io, x, y);
		io++;
	    }
	  /* inserting the Centre - last point */
	  gaiaSetPoint (out->Coords, io, cx, cy);
	  if (srid != 0)
	      sector->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (sector, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (geom)
	gaiaFreeGeomColl (geom);
    if (sector)
	gaiaFreeGeomColl (sector);
}

static void
fnct_MakeCircularStripe (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ MakeCircularStripe(double cx, double cy, double radius_1, double radius_2,
/                    double start, double stop)
/     or
/ MakeCircularStripe(double cx, double cy, double radius_1, double radius_2,
/                    double start, double stop, int srid)
/     or
/ MakeCircularStripe(double cx, double cy, double radius_1, double radius_2, 
/                    double start, double stop, int srid, double step)
/
/ - builds a Polygon approximating a Circular Stripe delimited by two
/   arcs sharing the same Centre-Point but having different radii
/ - start and stop are the initial and final angles (in degrees)
/ - step is the angular distance (in degrees) between points on 
/   the circurmference (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr arc1 = NULL;
    gaiaGeomCollPtr arc2 = NULL;
    gaiaGeomCollPtr stripe = NULL;
    int ival;
    double cx;
    double cy;
    double r1;
    double r2;
    double start;
    double stop;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  r1 = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	r1 = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[3]);
	  r2 = ival;
      }
    else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	r2 = sqlite3_value_double (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[4]);
	  start = ival;
      }
    else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	start = sqlite3_value_double (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[5]);
	  stop = ival;
      }
    else if (sqlite3_value_type (argv[5]) == SQLITE_FLOAT)
	stop = sqlite3_value_double (argv[5]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 7)
      {
	  if (sqlite3_value_type (argv[6]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[6]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 8)
      {
	  if (sqlite3_value_type (argv[7]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[7]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[7]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[7]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    arc1 = gaiaMakeArc (cx, cy, r1, start, stop, step);
    arc2 = gaiaMakeArc (cx, cy, r2, start, stop, step);
    if (arc1 == NULL || arc2 == NULL)
	sqlite3_result_null (context);
    else
      {
	  int ii;
	  int io = 0;
	  double x;
	  double y;
	  gaiaLinestringPtr in1 = arc1->FirstLinestring;
	  gaiaLinestringPtr in2 = arc2->FirstLinestring;
	  gaiaPolygonPtr pg;
	  gaiaRingPtr out;
	  stripe = gaiaAllocGeomColl ();
	  pg = gaiaAddPolygonToGeomColl (stripe,
					 in1->Points + in2->Points + 1, 0);
	  out = pg->Exterior;
	  for (ii = 0; ii < in1->Points; ii++)
	    {
		/* copying the first Arc's points - direct order */
		gaiaGetPoint (in1->Coords, ii, &x, &y);
		gaiaSetPoint (out->Coords, io, x, y);
		io++;
	    }
	  for (ii = in2->Points - 1; ii >= 0; ii--)
	    {
		/* copying the second Arc's points - reverse order */
		gaiaGetPoint (in2->Coords, ii, &x, &y);
		gaiaSetPoint (out->Coords, io, x, y);
		io++;
	    }
	  /* closing the Polygon Ring */
	  gaiaGetPoint (out->Coords, 0, &x, &y);
	  gaiaSetPoint (out->Coords, io, x, y);
	  if (srid != 0)
	      stripe->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (stripe, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (arc1)
	gaiaFreeGeomColl (arc1);
    if (arc2)
	gaiaFreeGeomColl (arc2);
    if (stripe)
	gaiaFreeGeomColl (stripe);
}

static void
fnct_MakeEllipticSector (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ MakeEllipticSector(double cx, double cy, double x_axis, double y_axis, 
/                 double start, double stop)
/     or
/ MakeEllipticSector(double cx, double cy, double x_axis, double y_axis,
/                 double start, double stop, int srid)
/     or
/ MakeEllipticSector(double cx, double cy, double x_axis, double y_axis,
/                 double start, double stop, int srid, double step)
/
/ - builds a Polygon approximating an Elliptic Sector
/ - start and stop are the initial and final angles (in degrees)
/ - step is the angular distance (in degrees) between points on 
/   the ellipse (by default: every 10 degs) 
/ - or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geom = NULL;
    gaiaGeomCollPtr sector = NULL;
    int ival;
    double cx;
    double cy;
    double x_axis;
    double y_axis;
    double start;
    double stop;
    int srid = 0;
    double step = 10.0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[0]);
	  cx = ival;
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	cx = sqlite3_value_double (argv[0]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  cy = ival;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	cy = sqlite3_value_double (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  x_axis = ival;
      }
    else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	x_axis = sqlite3_value_double (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[3]);
	  y_axis = ival;
      }
    else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	y_axis = sqlite3_value_double (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[4]);
	  start = ival;
      }
    else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	start = sqlite3_value_double (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[5]);
	  stop = ival;
      }
    else if (sqlite3_value_type (argv[5]) == SQLITE_FLOAT)
	stop = sqlite3_value_double (argv[5]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 7)
      {
	  if (sqlite3_value_type (argv[6]) == SQLITE_INTEGER)
	      srid = sqlite3_value_int (argv[6]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 8)
      {
	  if (sqlite3_value_type (argv[7]) == SQLITE_INTEGER)
	    {
		ival = sqlite3_value_int (argv[7]);
		step = ival;
	    }
	  else if (sqlite3_value_type (argv[7]) == SQLITE_FLOAT)
	      step = sqlite3_value_double (argv[7]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    geom = gaiaMakeEllipticArc (cx, cy, x_axis, y_axis, start, stop, step);
    if (!geom)
	sqlite3_result_null (context);
    else
      {
	  int ii;
	  int io = 0;
	  double x;
	  double y;
	  gaiaLinestringPtr in = geom->FirstLinestring;
	  gaiaPolygonPtr pg;
	  gaiaRingPtr out;
	  sector = gaiaAllocGeomColl ();
	  pg = gaiaAddPolygonToGeomColl (sector, in->Points + 2, 0);
	  out = pg->Exterior;
	  /* inserting the Centre - first point */
	  gaiaSetPoint (out->Coords, io, cx, cy);
	  io++;
	  for (ii = 0; ii < in->Points; ii++)
	    {
		/* copying the Arc's points */
		gaiaGetPoint (in->Coords, ii, &x, &y);
		gaiaSetPoint (out->Coords, io, x, y);
		io++;
	    }
	  /* inserting the Centre - last point */
	  gaiaSetPoint (out->Coords, io, cx, cy);
	  if (srid != 0)
	      sector->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (sector, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    if (geom)
	gaiaFreeGeomColl (geom);
    if (sector)
	gaiaFreeGeomColl (sector);
}

static void
fnct_Collect_step (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Collect(BLOBencoded geom)
/
/ aggregate function - STEP
/
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr *p;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geom)
	return;
    p = sqlite3_aggregate_context (context, sizeof (gaiaGeomCollPtr));
    if (!(*p))
      {
	  /* this is the first row */
	  *p = geom;
      }
    else
      {
	  /* subsequent rows */
	  result = gaiaMergeGeometries (*p, geom);
	  gaiaFreeGeomColl (*p);
	  *p = result;
	  gaiaFreeGeomColl (geom);
      }
}

static void
fnct_Collect_final (sqlite3_context * context)
{
/* SQL function:
/ Collect(BLOBencoded geom)
/
/ aggregate function - FINAL
/
*/
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    result = *p;
    if (!result)
	sqlite3_result_null (context);
    else if (gaiaIsEmpty (result))
      {
	  gaiaFreeGeomColl (result);
	  sqlite3_result_null (context);
      }
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (result);
      }
}

static void
fnct_Collect (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Collect(geometry geom1, geometry geom2)
/
/ merges two generic GEOMETRIES into a single one 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaMergeGeometries (geo1, geo2);
	  if (!result)
	      sqlite3_result_null (context);
	  else if (gaiaIsEmpty (result))
	    {
		gaiaFreeGeomColl (result);
		sqlite3_result_null (context);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
geom_from_text1 (sqlite3_context * context, int argc, sqlite3_value ** argv,
		 short type)
{
/* SQL function:
/ GeomFromText(WKT encoded geometry)
/
/ returns the current geometry by parsing WKT encoded string 
/ or NULL if any error is encountered
/
/ if *type* is a negative value can accept any GEOMETRY CLASS
/ otherwise only requests conforming with required CLASS are valid
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, type);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
geom_from_text2 (sqlite3_context * context, int argc, sqlite3_value ** argv,
		 short type)
{
/* SQL function:
/ GeomFromText(WKT encoded geometry, SRID)
/
/ returns the current geometry by parsing WKT encoded string 
/ or NULL if any error is encountered
/
/ if *type* is a negative value can accept any GEOMETRY CLASS
/ otherwise only requests conforming with required CLASS are valid
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, type);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static int
check_wkb (const unsigned char *wkb, int size, short type)
{
/* checking type coherency for WKB encoded GEOMETRY */
    int little_endian;
    int wkb_type;
    int endian_arch = gaiaEndianArch ();
    if (size < 5)
	return 0;		/* too short to be a WKB */
    if (*(wkb + 0) == 0x01)
	little_endian = GAIA_LITTLE_ENDIAN;
    else if (*(wkb + 0) == 0x00)
	little_endian = GAIA_BIG_ENDIAN;
    else
	return 0;		/* illegal byte ordering; neither BIG-ENDIAN nor LITTLE-ENDIAN */
    wkb_type = gaiaImport32 (wkb + 1, little_endian, endian_arch);
    if (wkb_type == GAIA_POINT || wkb_type == GAIA_LINESTRING
	|| wkb_type == GAIA_POLYGON || wkb_type == GAIA_MULTIPOINT
	|| wkb_type == GAIA_MULTILINESTRING || wkb_type == GAIA_MULTIPOLYGON
	|| wkb_type == GAIA_GEOMETRYCOLLECTION || wkb_type == GAIA_POINTZ
	|| wkb_type == GAIA_LINESTRINGZ || wkb_type == GAIA_POLYGONZ
	|| wkb_type == GAIA_MULTIPOINTZ || wkb_type == GAIA_MULTILINESTRINGZ
	|| wkb_type == GAIA_MULTIPOLYGONZ
	|| wkb_type == GAIA_GEOMETRYCOLLECTIONZ || wkb_type == GAIA_POINTM
	|| wkb_type == GAIA_LINESTRINGM || wkb_type == GAIA_POLYGONM
	|| wkb_type == GAIA_MULTIPOINTM || wkb_type == GAIA_MULTILINESTRINGM
	|| wkb_type == GAIA_MULTIPOLYGONM
	|| wkb_type == GAIA_GEOMETRYCOLLECTIONM || wkb_type == GAIA_POINTZM
	|| wkb_type == GAIA_LINESTRINGZM || wkb_type == GAIA_POLYGONZM
	|| wkb_type == GAIA_MULTIPOINTZM || wkb_type == GAIA_MULTILINESTRINGZM
	|| wkb_type == GAIA_MULTIPOLYGONZM
	|| wkb_type == GAIA_GEOMETRYCOLLECTIONZM)
	;
    else
	return 0;		/* illegal GEOMETRY CLASS */
    if (type < 0)
	;			/* no restrinction about GEOMETRY CLASS TYPE */
    else
      {
	  if (wkb_type != type)
	      return 0;		/* invalid CLASS TYPE for request */
      }
    return 1;
}

static void
geom_from_wkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv,
		short type)
{
/* SQL function:
/ GeomFromWKB(WKB encoded geometry)
/
/ returns the current geometry by parsing a WKB encoded blob 
/ or NULL if any error is encountered
/
/ if *type* is a negative value can accept any GEOMETRY CLASS
/ otherwise only requests conforming with required CLASS are valid
*/
    int len;
    int n_bytes;
    unsigned char *p_result = NULL;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, type))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
geom_from_wkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv,
		short type)
{
/* SQL function:
/ GeomFromWKB(WKB encoded geometry, SRID)
/
/ returns the current geometry by parsing a WKB encoded blob
/ or NULL if any error is encountered
/
/ if *type* is a negative value can accept any GEOMETRY CLASS
/ otherwise only requests conforming with required CLASS are valid
*/
    int len;
    int n_bytes;
    unsigned char *p_result = NULL;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, type))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_GeometryFromFGF1 (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromFGF(FGF encoded geometry)
/
/ returns the current geometry by parsing an FGF encoded blob 
/ or NULL if any error is encountered
/
/ if *type* is a negative value can accept any GEOMETRY CLASS
/ otherwise only requests conforming with required CLASS are valid
*/
    int len;
    int n_bytes;
    unsigned char *p_result = NULL;
    const unsigned char *fgf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    fgf = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromFgf (fgf, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_GeometryFromFGF2 (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromFGF(FGF encoded geometry, SRID)
/
/ returns the current geometry by parsing an FGF encoded string 
/ or NULL if any error is encountered
/
/ if *type* is a negative value can accept any GEOMETRY CLASS
/ otherwise only requests conforming with required CLASS are valid
*/
    int len;
    int n_bytes;
    unsigned char *p_result = NULL;
    const unsigned char *fgf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    fgf = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromFgf (fgf, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

/*
/ the following functions simply readdress the request to geom_from_text?()
/ setting the appropriate GEOMETRY CLASS TYPE
*/

static void
fnct_GeomFromText1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) -1);
}

static void
fnct_GeomFromText2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) -1);
}

static void
fnct_GeomCollFromText1 (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_GEOMETRYCOLLECTION);
}

static void
fnct_GeomCollFromText2 (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_GEOMETRYCOLLECTION);
}

static void
fnct_LineFromText1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_LINESTRING);
}

static void
fnct_LineFromText2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_LINESTRING);
}

static void
fnct_PointFromText1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_POINT);
}

static void
fnct_PointFromText2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_POINT);
}

static void
fnct_PolyFromText1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_POLYGON);
}

static void
fnct_PolyFromText2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_POLYGON);
}

static void
fnct_MLineFromText1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_MULTILINESTRING);
}

static void
fnct_MLineFromText2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_MULTILINESTRING);
}

static void
fnct_MPointFromText1 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_MULTIPOINT);
}

static void
fnct_MPointFromText2 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_MULTIPOINT);
}

static void
fnct_MPolyFromText1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text1 (context, argc, argv, (short) GAIA_MULTIPOLYGON);
}

static void
fnct_MPolyFromText2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_text2 (context, argc, argv, (short) GAIA_MULTIPOLYGON);
}

static void
fnct_WktToSql (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_WKTToSQL(WKT encoded geometry)
/
/ returns the current geometry by parsing WKT encoded string 
/ or NULL if any error is encountered
/
/ the SRID is always 0 [SQL/MM function]
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, -1);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = 0;
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

/*
/ the following functions simply readdress the request to geom_from_wkb?()
/ setting the appropriate GEOMETRY CLASS TYPE
*/

static void
fnct_GeomFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) -1);
}

static void
fnct_GeomFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) -1);
}

static void
fnct_GeomCollFromWkb1 (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_GEOMETRYCOLLECTION);
}

static void
fnct_GeomCollFromWkb2 (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_GEOMETRYCOLLECTION);
}

static void
fnct_LineFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_LINESTRING);
}

static void
fnct_LineFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_LINESTRING);
}

static void
fnct_PointFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_POINT);
}

static void
fnct_PointFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_POINT);
}

static void
fnct_PolyFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_POLYGON);
}

static void
fnct_PolyFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_POLYGON);
}

static void
fnct_MLineFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_MULTILINESTRING);
}

static void
fnct_MLineFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_MULTILINESTRING);
}

static void
fnct_MPointFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_MULTIPOINT);
}

static void
fnct_MPointFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_MULTIPOINT);
}

static void
fnct_MPolyFromWkb1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb1 (context, argc, argv, (short) GAIA_MULTIPOLYGON);
}

static void
fnct_MPolyFromWkb2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    geom_from_wkb2 (context, argc, argv, (short) GAIA_MULTIPOLYGON);
}

static void
fnct_WkbToSql (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_WKBToSQL(WKB encoded geometry)
/
/ returns the current geometry by parsing a WKB encoded blob 
/ or NULL if any error is encountered
/
/ the SRID is always 0 [SQL/MM function]
*/
    int len;
    int n_bytes;
    unsigned char *p_result = NULL;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, -1))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = 0;
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_CompressGeometry (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ CompressGeometry(BLOB encoded geometry)
/
/ returns a COMPRESSED geometry [if a valid Geometry was supplied]
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaToCompressedBlobWkb (geo, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_UncompressGeometry (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ UncompressGeometry(BLOB encoded geometry)
/
/ returns an UNCOMPRESSED geometry [if a valid Geometry was supplied] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_SanitizeGeometry (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ SanitizeGeometry(BLOB encoded geometry)
/
/ returns a SANITIZED geometry [if a valid Geometry was supplied]
/ or NULL in any other case
/
/ Sanitizing includes:
/ - repeated vertices suppression
/ - enforcing ring closure
/
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr sanitized = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  sanitized = gaiaSanitize (geo);
	  gaiaToSpatiaLiteBlobWkb (sanitized, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
    gaiaFreeGeomColl (sanitized);
}

static void
cast_count (gaiaGeomCollPtr geom, int *pts, int *lns, int *pgs)
{
/* counting elementary geometries */
    int n_pts = 0;
    int n_lns = 0;
    int n_pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    if (geom)
      {
	  pt = geom->FirstPoint;
	  while (pt)
	    {
		n_pts++;
		pt = pt->Next;
	    }
	  ln = geom->FirstLinestring;
	  while (ln)
	    {
		n_lns++;
		ln = ln->Next;
	    }
	  pg = geom->FirstPolygon;
	  while (pg)
	    {
		n_pgs++;
		pg = pg->Next;
	    }
      }
    *pts = n_pts;
    *lns = n_lns;
    *pgs = n_pgs;
}

static void
fnct_CastToPoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToPoint(BLOB encoded geometry)
/
/ returns a POINT-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts == 1 && lns == 0 && pgs == 0)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_POINT;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToLinestring (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ CastToLinestring(BLOB encoded geometry)
/
/ returns a LINESTRING-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts == 0 && lns == 1 && pgs == 0)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_LINESTRING;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToPolygon (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToPolygon(BLOB encoded geometry)
/
/ returns a POLYGON-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts == 0 && lns == 0 && pgs == 1)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_POLYGON;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToMultiPoint (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ CastToMultiPoint(BLOB encoded geometry)
/
/ returns a MULTIPOINT-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts >= 1 && lns == 0 && pgs == 0)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_MULTIPOINT;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToMultiLinestring (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ CastToMultiLinestring(BLOB encoded geometry)
/
/ returns a MULTILINESTRING-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts == 0 && lns >= 1 && pgs == 0)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_MULTILINESTRING;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToMultiPolygon (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ CastToMultiPolygon(BLOB encoded geometry)
/
/ returns a MULTIPOLYGON-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts == 0 && lns == 0 && pgs >= 1)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_MULTIPOLYGON;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToGeometryCollection (sqlite3_context * context, int argc,
			       sqlite3_value ** argv)
{
/* SQL function:
/ CastToGeometryCollection(BLOB encoded geometry)
/
/ returns a GEOMETRYCOLLECTION-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts >= 1 || lns >= 1 || pgs >= 1)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_GEOMETRYCOLLECTION;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToMulti (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToMulti(BLOB encoded geometry)
/
/ returns a MULTIPOINT, MULTILINESTRING, MULTIPOLYGON or
/ GEOMETRYCOLLECTION-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts >= 1 || lns >= 1 || pgs >= 1)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		if (pts >= 1 && lns == 0 && pgs == 0)
		    geom2->DeclaredType = GAIA_MULTIPOINT;
		else if (pts == 0 && lns >= 1 && pgs == 0)
		    geom2->DeclaredType = GAIA_MULTILINESTRING;
		else if (pts == 0 && lns == 0 && pgs >= 1)
		    geom2->DeclaredType = GAIA_MULTIPOLYGON;
		else
		    geom2->DeclaredType = GAIA_GEOMETRYCOLLECTION;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToSingle (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToSingle(BLOB encoded geometry)
/
/ returns a POINT, LINESTRING or POLYGON-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    int ok;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  ok = 0;
	  if (pts == 1 && lns == 0 && pgs == 0)
	      ok = 1;
	  if (pts == 0 && lns == 1 && pgs == 0)
	      ok = 1;
	  if (pts == 0 && lns == 0 && pgs == 1)
	      ok = 1;
	  if (ok)
	    {
		geom2 = gaiaCloneGeomColl (geo);
		geom2->Srid = geo->Srid;
		if (pts == 1)
		    geom2->DeclaredType = GAIA_POINT;
		else if (lns == 1)
		    geom2->DeclaredType = GAIA_LINESTRING;
		else
		    geom2->DeclaredType = GAIA_POLYGON;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToXY (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToXY(BLOB encoded geometry)
/
/ returns an XY-dimension Geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geom2 = gaiaCastGeomCollToXY (geo);
	  if (geom2)
	    {
		geom2->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToXYZ (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToXY(BLOB encoded geometry)
/
/ returns an XY-dimension Geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geom2 = gaiaCastGeomCollToXYZ (geo);
	  if (geom2)
	    {
		geom2->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToXYM (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToXY(BLOB encoded geometry)
/
/ returns an XYM-dimension Geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geom2 = gaiaCastGeomCollToXYM (geo);
	  if (geom2)
	    {
		geom2->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CastToXYZM (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToXY(BLOB encoded geometry)
/
/ returns an XYZM-dimension Geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geom2 = gaiaCastGeomCollToXYZM (geo);
	  if (geom2)
	    {
		geom2->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ExtractMultiPoint (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ ExtractMultiPoint(BLOB encoded geometry)
/
/ returns a MULTIPOINT-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pts >= 1)
	    {
		geom2 = gaiaCloneGeomCollPoints (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_MULTIPOINT;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ExtractMultiLinestring (sqlite3_context * context, int argc,
			     sqlite3_value ** argv)
{
/* SQL function:
/ ExtractMultiLinestring(BLOB encoded geometry)
/
/ returns a MULTILINESTRING-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (lns >= 1)
	    {
		geom2 = gaiaCloneGeomCollLinestrings (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_MULTILINESTRING;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ExtractMultiPolygon (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ ExtractMultiPolygon(BLOB encoded geometry)
/
/ returns a MULTIPOLYGON-type geometry [if conversion is possible] 
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    int pts;
    int lns;
    int pgs;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  cast_count (geo, &pts, &lns, &pgs);
	  if (pgs >= 1)
	    {
		geom2 = gaiaCloneGeomCollPolygons (geo);
		geom2->Srid = geo->Srid;
		geom2->DeclaredType = GAIA_MULTIPOLYGON;
		gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
		gaiaFreeGeomColl (geom2);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Reverse (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_Reverse(BLOB encoded geometry)
/
/ returns a new Geometry: any Linestring or Ring will be in reverse order
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geom2 = gaiaCloneGeomCollSpecial (geo, GAIA_REVERSE_ORDER);
	  geom2->Srid = geo->Srid;
	  gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
	  gaiaFreeGeomColl (geom2);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_ForceLHR (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_ForceLHR(BLOB encoded geometry)
/
/ returns a new Geometry: any Exterior Ring will be in clockwise orientation
/         and any Interior Ring will be in counter-clockwise orientation
/ or NULL in any other case
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geom2 = gaiaCloneGeomCollSpecial (geo, GAIA_LHR_ORDER);
	  geom2->Srid = geo->Srid;
	  gaiaToSpatiaLiteBlobWkb (geom2, &p_result, &len);
	  gaiaFreeGeomColl (geom2);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_Dimension (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Dimension(BLOB encoded geometry)
/
/ returns:
/ 0 if geometry is a POINT or MULTIPOINT
/ 1 if geometry is a LINESTRING or MULTILINESTRING
/ 2 if geometry is a POLYGON or MULTIPOLYGON
/ 0, 1, 2, for GEOMETRYCOLLECTIONS according to geometries contained inside
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int dim;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  dim = gaiaDimension (geo);
	  sqlite3_result_int (context, dim);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CoordDimension (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CoordDimension(BLOB encoded geometry)
/
/ returns:
/ 'XY', 'XYM', 'XYZ', 'XYZM'
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    char *p_dim = NULL;
    char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->DimensionModel == GAIA_XY)
	      p_dim = "XY";
	  else if (geo->DimensionModel == GAIA_XY_Z)
	      p_dim = "XYZ";
	  else if (geo->DimensionModel == GAIA_XY_M)
	      p_dim = "XYM";
	  else if (geo->DimensionModel == GAIA_XY_Z_M)
	      p_dim = "XYZM";
	  if (p_dim)
	    {
		len = strlen (p_dim);
		p_result = malloc (len + 1);
		strcpy (p_result, p_dim);
	    }
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	    {
		len = strlen (p_result);
		sqlite3_result_text (context, p_result, len, free);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NDims (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_NDims(BLOB encoded geometry)
/
/ returns:
/ 2, 3 or 4
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int result = 0;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->DimensionModel == GAIA_XY)
	      result = 2;
	  else if (geo->DimensionModel == GAIA_XY_Z)
	      result = 3;
	  else if (geo->DimensionModel == GAIA_XY_M)
	      result = 3;
	  else if (geo->DimensionModel == GAIA_XY_Z_M)
	      result = 4;
	  sqlite3_result_int (context, result);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_GeometryType (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeometryType(BLOB encoded geometry)
/
/ returns the class for current geometry:
/ 'POINT' or 'MULTIPOINT' [Z, M, ZM]
/ 'LINESTRING' or 'MULTILINESTRING' [Z, M, ZM]
/ 'POLYGON' or 'MULTIPOLYGON' [Z, M, ZM]
/ 'GEOMETRYCOLLECTION'  [Z, M, ZM]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    int type;
    char *p_type = NULL;
    char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  type = gaiaGeometryType (geo);
	  switch (type)
	    {
	    case GAIA_POINT:
		p_type = "POINT";
		break;
	    case GAIA_POINTZ:
		p_type = "POINT Z";
		break;
	    case GAIA_POINTM:
		p_type = "POINT M";
		break;
	    case GAIA_POINTZM:
		p_type = "POINT ZM";
		break;
	    case GAIA_MULTIPOINT:
		p_type = "MULTIPOINT";
		break;
	    case GAIA_MULTIPOINTZ:
		p_type = "MULTIPOINT Z";
		break;
	    case GAIA_MULTIPOINTM:
		p_type = "MULTIPOINT M";
		break;
	    case GAIA_MULTIPOINTZM:
		p_type = "MULTIPOINT ZM";
		break;
	    case GAIA_LINESTRING:
	    case GAIA_COMPRESSED_LINESTRING:
		p_type = "LINESTRING";
		break;
	    case GAIA_LINESTRINGZ:
	    case GAIA_COMPRESSED_LINESTRINGZ:
		p_type = "LINESTRING Z";
		break;
	    case GAIA_LINESTRINGM:
	    case GAIA_COMPRESSED_LINESTRINGM:
		p_type = "LINESTRING M";
		break;
	    case GAIA_LINESTRINGZM:
	    case GAIA_COMPRESSED_LINESTRINGZM:
		p_type = "LINESTRING ZM";
		break;
	    case GAIA_MULTILINESTRING:
		p_type = "MULTILINESTRING";
		break;
	    case GAIA_MULTILINESTRINGZ:
		p_type = "MULTILINESTRING Z";
		break;
	    case GAIA_MULTILINESTRINGM:
		p_type = "MULTILINESTRING M";
		break;
	    case GAIA_MULTILINESTRINGZM:
		p_type = "MULTILINESTRING ZM";
		break;
	    case GAIA_POLYGON:
	    case GAIA_COMPRESSED_POLYGON:
		p_type = "POLYGON";
		break;
	    case GAIA_POLYGONZ:
	    case GAIA_COMPRESSED_POLYGONZ:
		p_type = "POLYGON Z";
		break;
	    case GAIA_POLYGONM:
	    case GAIA_COMPRESSED_POLYGONM:
		p_type = "POLYGON M";
		break;
	    case GAIA_POLYGONZM:
	    case GAIA_COMPRESSED_POLYGONZM:
		p_type = "POLYGON ZM";
		break;
	    case GAIA_MULTIPOLYGON:
		p_type = "MULTIPOLYGON";
		break;
	    case GAIA_MULTIPOLYGONZ:
		p_type = "MULTIPOLYGON Z";
		break;
	    case GAIA_MULTIPOLYGONM:
		p_type = "MULTIPOLYGON M";
		break;
	    case GAIA_MULTIPOLYGONZM:
		p_type = "MULTIPOLYGON ZM";
		break;
	    case GAIA_GEOMETRYCOLLECTION:
		p_type = "GEOMETRYCOLLECTION";
		break;
	    case GAIA_GEOMETRYCOLLECTIONZ:
		p_type = "GEOMETRYCOLLECTION Z";
		break;
	    case GAIA_GEOMETRYCOLLECTIONM:
		p_type = "GEOMETRYCOLLECTION M";
		break;
	    case GAIA_GEOMETRYCOLLECTIONZM:
		p_type = "GEOMETRYCOLLECTION ZM";
		break;
	    };
	  if (p_type)
	    {
		len = strlen (p_type);
		p_result = malloc (len + 1);
		strcpy (p_result, p_type);
	    }
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	    {
		len = strlen (p_result);
		sqlite3_result_text (context, p_result, len, free);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_GeometryAliasType (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ GeometryAliasType(BLOB encoded geometry)
/
/ returns the alias-class for current geometry:
/ 'POINT'
/ 'LINESTRING'
/ 'POLYGON'
/ 'MULTIPOINT'
/ 'MULTILINESTRING'
/ 'MULTIPOLYGON'
/ 'GEOMETRYCOLLECTION' 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    int type;
    char *p_type = NULL;
    char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  type = gaiaGeometryAliasType (geo);
	  switch (type)
	    {
	    case GAIA_POINT:
		p_type = "POINT";
		break;
	    case GAIA_MULTIPOINT:
		p_type = "MULTIPOINT";
		break;
	    case GAIA_LINESTRING:
		p_type = "LINESTRING";
		break;
	    case GAIA_MULTILINESTRING:
		p_type = "MULTILINESTRING";
		break;
	    case GAIA_POLYGON:
		p_type = "POLYGON";
		break;
	    case GAIA_MULTIPOLYGON:
		p_type = "MULTIPOLYGON";
		break;
	    case GAIA_GEOMETRYCOLLECTION:
		p_type = "GEOMETRYCOLLECTION";
		break;
	    };
	  if (p_type)
	    {
		len = strlen (p_type);
		p_result = malloc (len + 1);
		strcpy (p_result, p_type);
	    }
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	    {
		len = strlen (p_result);
		sqlite3_result_text (context, p_result, len, free);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_SridFromAuthCRS (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ SridFromAuthCRS(auth_name, auth_srid)
/
/ returns the SRID
/ or NULL if any error is encountered
*/
    const unsigned char *auth_name;
    int auth_srid;
    int srid = -1;
    char *sql;
    char **results;
    int n_rows;
    int n_columns;
    char *err_msg = NULL;
    int ret;
    int i;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    auth_name = sqlite3_value_text (argv[0]);
    auth_srid = sqlite3_value_int (argv[1]);

    sql = sqlite3_mprintf ("SELECT srid FROM spatial_ref_sys "
			   "WHERE Upper(auth_name) = Upper(%Q) AND auth_srid = %d",
			   auth_name, auth_srid);
    ret = sqlite3_get_table (sqlite, sql, &results, &n_rows, &n_columns,
			     &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
	goto done;
    if (n_rows >= 1)
      {
	  for (i = 1; i <= n_rows; i++)
	      srid = atoi (results[(i * n_columns) + 0]);
      }
    sqlite3_free_table (results);
  done:
    sqlite3_result_int (context, srid);
}

static void
fnct_SRID (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Srid(BLOB encoded geometry)
/
/ returns the SRID
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
	sqlite3_result_int (context, geo->Srid);
    gaiaFreeGeomColl (geo);
}

static void
fnct_SetSRID (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ SetSrid(BLOBencoded geometry, srid)
/
/ returns a new geometry that is the original one received, but with the new SRID [no coordinates translation is applied]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    int srid;
    unsigned char *p_result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  geo->Srid = srid;
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &n_bytes);
	  sqlite3_result_blob (context, p_result, n_bytes, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_IsEmpty (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ IsEmpty(BLOB encoded geometry)
/
/ returns:
/ 1 if this geometry contains no elementary geometries
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
	sqlite3_result_int (context, gaiaIsEmpty (geo));
    gaiaFreeGeomColl (geo);
}

static void
fnct_Is3D (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Is3D(BLOB encoded geometry)
/
/ returns:
/ 1 if this geometry has Z coords
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
      {
	  if (geo->DimensionModel == GAIA_XY_Z
	      || geo->DimensionModel == GAIA_XY_Z_M)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_IsMeasured (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ IsMeasured(BLOB encoded geometry)
/
/ returns:
/ 1 if this geometry has M coords
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
      {
	  if (geo->DimensionModel == GAIA_XY_M
	      || geo->DimensionModel == GAIA_XY_Z_M)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_MinZ (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_MinZ(BLOB encoded GEMETRY)
/
/ returns the MinZ coordinate for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double min;
    double max;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->DimensionModel == GAIA_XY_Z
	      || geo->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaZRangeGeometry (geo, &min, &max);
		sqlite3_result_double (context, min);
	    }
	  else
	      sqlite3_result_null (context);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_MaxZ (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_MaxZ(BLOB encoded GEMETRY)
/
/ returns the MaxZ coordinate for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double min;
    double max;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->DimensionModel == GAIA_XY_Z
	      || geo->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaZRangeGeometry (geo, &min, &max);
		sqlite3_result_double (context, max);
	    }
	  else
	      sqlite3_result_null (context);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_MinM (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_MinM(BLOB encoded GEMETRY)
/
/ returns the MinM coordinate for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double min;
    double max;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->DimensionModel == GAIA_XY_M
	      || geo->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaMRangeGeometry (geo, &min, &max);
		sqlite3_result_double (context, min);
	    }
	  else
	      sqlite3_result_null (context);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_MaxM (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_MaxM(BLOB encoded GEMETRY)
/
/ returns the MaxM coordinate for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double min;
    double max;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->DimensionModel == GAIA_XY_M
	      || geo->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaMRangeGeometry (geo, &min, &max);
		sqlite3_result_double (context, max);
	    }
	  else
	      sqlite3_result_null (context);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_Envelope (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Envelope(BLOB encoded geometry)
/
/ returns the MBR for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr bbox;
    gaiaPolygonPtr polyg;
    gaiaRingPtr rect;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaMbrGeometry (geo);
	  bbox = gaiaAllocGeomColl ();
	  bbox->Srid = geo->Srid;
	  polyg = gaiaAddPolygonToGeomColl (bbox, 5, 0);
	  rect = polyg->Exterior;
	  gaiaSetPoint (rect->Coords, 0, geo->MinX, geo->MinY);	/* vertex # 1 */
	  gaiaSetPoint (rect->Coords, 1, geo->MaxX, geo->MinY);	/* vertex # 2 */
	  gaiaSetPoint (rect->Coords, 2, geo->MaxX, geo->MaxY);	/* vertex # 3 */
	  gaiaSetPoint (rect->Coords, 3, geo->MinX, geo->MaxY);	/* vertex # 4 */
	  gaiaSetPoint (rect->Coords, 4, geo->MinX, geo->MinY);	/* vertex # 5 [same as vertex # 1 to close the polygon] */
	  gaiaToSpatiaLiteBlobWkb (bbox, &p_result, &len);
	  gaiaFreeGeomColl (bbox);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Expand (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_Expand(BLOB encoded geometry, double amount)
/
/ returns the MBR for current geometry expanded by "amount" in each direction
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr bbox;
    gaiaPolygonPtr polyg;
    gaiaRingPtr rect;
    double tic;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	tic = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  tic = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaMbrGeometry (geo);
	  bbox = gaiaAllocGeomColl ();
	  bbox->Srid = geo->Srid;
	  polyg = gaiaAddPolygonToGeomColl (bbox, 5, 0);
	  rect = polyg->Exterior;
	  gaiaSetPoint (rect->Coords, 0, geo->MinX - tic, geo->MinY - tic);	/* vertex # 1 */
	  gaiaSetPoint (rect->Coords, 1, geo->MaxX + tic, geo->MinY - tic);	/* vertex # 2 */
	  gaiaSetPoint (rect->Coords, 2, geo->MaxX + tic, geo->MaxY + tic);	/* vertex # 3 */
	  gaiaSetPoint (rect->Coords, 3, geo->MinX - tic, geo->MaxY + tic);	/* vertex # 4 */
	  gaiaSetPoint (rect->Coords, 4, geo->MinX - tic, geo->MinY - tic);	/* vertex # 5 [same as vertex # 1 to close the polygon] */
	  gaiaToSpatiaLiteBlobWkb (bbox, &p_result, &len);
	  gaiaFreeGeomColl (bbox);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
build_filter_mbr (sqlite3_context * context, int argc,
		  sqlite3_value ** argv, int mode)
{
/* SQL functions:
/ BuildMbrFilter(double X1, double Y1, double X2, double Y2)
/ FilterMBRWithin(double X1, double Y1, double X2, double Y2)
/ FilterMBRContain(double X1, double Y1, double X2, double Y2)
/ FilterMBRIntersects(double X1, double Y1, double X2, double Y2)
/
/ builds a generic filter for MBR from two points (identifying a rectangle's diagonal) 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    double x1;
    double y1;
    double x2;
    double y2;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x1 = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y1 = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	x2 = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  x2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	y2 = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  y2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaBuildFilterMbr (x1, y1, x2, y2, mode, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

/*
/ the following functions simply readdress the request to build_filter_mbr()
/ setting the appropriate MODe
*/

static void
fnct_BuildMbrFilter (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    build_filter_mbr (context, argc, argv, GAIA_FILTER_MBR_DECLARE);
}

static void
fnct_FilterMbrWithin (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
    build_filter_mbr (context, argc, argv, GAIA_FILTER_MBR_WITHIN);
}

static void
fnct_FilterMbrContains (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
    build_filter_mbr (context, argc, argv, GAIA_FILTER_MBR_CONTAINS);
}

static void
fnct_FilterMbrIntersects (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
    build_filter_mbr (context, argc, argv, GAIA_FILTER_MBR_INTERSECTS);
}

static void
fnct_BuildMbr1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BuildMBR(double X1, double Y1, double X2, double Y2)
/
/ builds an MBR from two points (identifying a rectangle's diagonal) 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    double x1;
    double y1;
    double x2;
    double y2;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x1 = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y1 = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	x2 = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  x2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	y2 = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  y2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaBuildMbr (x1, y1, x2, y2, -1, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_BuildMbr2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BuildMBR(double X1, double Y1, double X2, double Y2, int SRID)
/
/ builds an MBR from two points (identifying a rectangle's diagonal) 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    double x1;
    double y1;
    double x2;
    double y2;
    int int_value;
    int srid;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x1 = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y1 = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	x2 = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  x2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	y2 = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  y2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[4]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaBuildMbr (x1, y1, x2, y2, srid, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_BuildCircleMbr1 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ BuildCircleMBR(double X, double Y, double radius)
/
/ builds an MBR from two points (identifying a rectangle's diagonal) 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double radius;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	radius = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  radius = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaBuildCircleMbr (x, y, radius, -1, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_BuildCircleMbr2 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ BuildCircleMBR(double X, double Y, double radius, int SRID)
/
/ builds an MBR from two points (identifying a rectangle's diagonal) 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    double x;
    double y;
    double radius;
    int int_value;
    int srid;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	radius = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  radius = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	srid = sqlite3_value_int (argv[3]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaBuildCircleMbr (x, y, radius, srid, &p_result, &len);
    if (!p_result)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_Extent_step (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Extent(BLOBencoded geom)
/
/ aggregate function - STEP
/
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    double **p;
    double *max_min;
    int *srid_check;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geom)
	return;
    gaiaMbrGeometry (geom);
    p = sqlite3_aggregate_context (context, sizeof (double **));
    if (!(*p))
      {
	  /* this is the first row */
	  max_min = malloc ((sizeof (double) * 5));
	  *(max_min + 0) = geom->MinX;
	  *(max_min + 1) = geom->MinY;
	  *(max_min + 2) = geom->MaxX;
	  *(max_min + 3) = geom->MaxY;
	  srid_check = (int *) (max_min + 4);
	  *(srid_check + 0) = geom->Srid;
	  *(srid_check + 1) = geom->Srid;
	  *p = max_min;
      }
    else
      {
	  /* subsequent rows */
	  max_min = *p;
	  if (geom->MinX < *(max_min + 0))
	      *(max_min + 0) = geom->MinX;
	  if (geom->MinY < *(max_min + 1))
	      *(max_min + 1) = geom->MinY;
	  if (geom->MaxX > *(max_min + 2))
	      *(max_min + 2) = geom->MaxX;
	  if (geom->MaxY > *(max_min + 3))
	      *(max_min + 3) = geom->MaxY;
	  srid_check = (int *) (max_min + 4);
	  if (*(srid_check + 1) != geom->Srid)
	      *(srid_check + 1) = geom->Srid;
      }
    gaiaFreeGeomColl (geom);
}

static void
fnct_Extent_final (sqlite3_context * context)
{
/* SQL function:
/ Extent(BLOBencoded geom)
/
/ aggregate function - FINAL
/
*/
    gaiaGeomCollPtr result;
    gaiaPolygonPtr polyg;
    gaiaRingPtr rect;
    double *max_min;
    double minx;
    double miny;
    double maxx;
    double maxy;
    int *srid_check;
    double **p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    max_min = *p;
    if (!max_min)
      {
	  sqlite3_result_null (context);
	  return;
      }
    srid_check = (int *) (max_min + 4);
    if (*(srid_check + 0) != *(srid_check + 1))
      {
	  sqlite3_result_null (context);
	  return;
      }
    result = gaiaAllocGeomColl ();
    if (!result)
	sqlite3_result_null (context);
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  result->Srid = *(srid_check + 0);
	  polyg = gaiaAddPolygonToGeomColl (result, 5, 0);
	  rect = polyg->Exterior;
	  minx = *(max_min + 0);
	  miny = *(max_min + 1);
	  maxx = *(max_min + 2);
	  maxy = *(max_min + 3);
	  gaiaSetPoint (rect->Coords, 0, minx, miny);	/* vertex # 1 */
	  gaiaSetPoint (rect->Coords, 1, maxx, miny);	/* vertex # 2 */
	  gaiaSetPoint (rect->Coords, 2, maxx, maxy);	/* vertex # 3 */
	  gaiaSetPoint (rect->Coords, 3, minx, maxy);	/* vertex # 4 */
	  gaiaSetPoint (rect->Coords, 4, minx, miny);	/* vertex # 5 [same as vertex # 1 to close the polygon] */
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (result);
      }
    free (max_min);
}

static void
fnct_MbrMinX (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MbrMinX(BLOB encoded GEMETRY)
/
/ returns the MinX coordinate for current geometry's MBR 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double coord;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!gaiaGetMbrMinX (p_blob, n_bytes, &coord))
	sqlite3_result_null (context);
    else
	sqlite3_result_double (context, coord);
}

static void
fnct_MbrMaxX (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MbrMaxX(BLOB encoded GEMETRY)
/
/ returns the MaxX coordinate for current geometry's MBR 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double coord;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!gaiaGetMbrMaxX (p_blob, n_bytes, &coord))
	sqlite3_result_null (context);
    else
	sqlite3_result_double (context, coord);
}

static void
fnct_MbrMinY (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MbrMinY(BLOB encoded GEMETRY)
/
/ returns the MinY coordinate for current geometry's MBR 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double coord;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!gaiaGetMbrMinY (p_blob, n_bytes, &coord))
	sqlite3_result_null (context);
    else
	sqlite3_result_double (context, coord);
}

static void
fnct_MbrMaxY (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MbrMaxY(BLOB encoded GEMETRY)
/
/ returns the MaxY coordinate for current geometry's MBR 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double coord;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!gaiaGetMbrMaxY (p_blob, n_bytes, &coord))
	sqlite3_result_null (context);
    else
	sqlite3_result_double (context, coord);
}

#ifndef OMIT_GEOCALLBACKS	/* supporting RTree geometry callbacks */
static void
gaia_mbr_del (void *p)
{
/* freeing data used by R*Tree Geometry Callback */
    sqlite3_free (p);
}

static int
fnct_RTreeIntersects (sqlite3_rtree_geometry * p, int nCoord, double *aCoord,
		      int *pRes)
{
/* R*Tree Geometry callback function:
/ ... MATCH RTreeIntersects(double x1, double y1, double x2, double y2)
*/
    struct gaia_rtree_mbr *mbr;
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    float fminx;
    float fminy;
    float fmaxx;
    float fmaxy;
    double tic;
    double tic2;

    if (p->pUser == 0)
      {
	  /* first call: we must check args and then initialize the MBR struct */
	  if (nCoord != 4)
	      return SQLITE_ERROR;
	  if (p->nParam != 4)
	      return SQLITE_ERROR;
	  mbr = (struct gaia_rtree_mbr *) (p->pUser =
					   sqlite3_malloc (sizeof
							   (struct
							    gaia_rtree_mbr)));
	  if (!mbr)
	      return SQLITE_NOMEM;
	  p->xDelUser = gaia_mbr_del;
	  xmin = p->aParam[0];
	  ymin = p->aParam[1];
	  xmax = p->aParam[2];
	  ymax = p->aParam[3];
	  if (xmin > xmax)
	    {
		xmin = p->aParam[2];
		xmax = p->aParam[0];
	    }
	  if (ymin > ymax)
	    {
		ymin = p->aParam[3];
		ymax = p->aParam[1];
	    }

	  /* adjusting the MBR so to compensate for DOUBLE/FLOAT truncations */
	  fminx = (float) xmin;
	  fminy = (float) ymin;
	  fmaxx = (float) xmax;
	  fmaxy = (float) ymax;
	  tic = fabs (xmin - fminx);
	  tic2 = fabs (ymin - fminy);
	  if (tic2 > tic)
	      tic = tic2;
	  tic2 = fabs (xmax - fmaxx);
	  if (tic2 > tic)
	      tic = tic2;
	  tic2 = fabs (ymax - fmaxy);
	  if (tic2 > tic)
	      tic = tic2;
	  tic *= 2.0;

	  mbr->minx = xmin - tic;
	  mbr->miny = ymin - tic;
	  mbr->maxx = xmax + tic;
	  mbr->maxy = ymax + tic;
      }

    mbr = (struct gaia_rtree_mbr *) (p->pUser);
    xmin = aCoord[0];
    xmax = aCoord[1];
    ymin = aCoord[2];
    ymax = aCoord[3];
    *pRes = 1;
/* evaluating Intersects relationship */
    if (xmin > mbr->maxx)
	*pRes = 0;
    if (xmax < mbr->minx)
	*pRes = 0;
    if (ymin > mbr->maxy)
	*pRes = 0;
    if (ymax < mbr->miny)
	*pRes = 0;
    return SQLITE_OK;
}

static int
fnct_RTreeDistWithin (sqlite3_rtree_geometry * p, int nCoord, double *aCoord,
		      int *pRes)
{
/* R*Tree Geometry callback function:
/ ... MATCH RTreeDistWithin(double x, double y, double radius)
*/
    struct gaia_rtree_mbr *mbr;
    double xmin;
    double xmax;
    double ymin;
    double ymax;

    if (p->pUser == 0)
      {
	  /* first call: we must check args and then initialize the MBR struct */
	  if (nCoord != 4)
	      return SQLITE_ERROR;
	  if (p->nParam != 3)
	      return SQLITE_ERROR;
	  mbr = (struct gaia_rtree_mbr *) (p->pUser =
					   sqlite3_malloc (sizeof
							   (struct
							    gaia_rtree_mbr)));
	  if (!mbr)
	      return SQLITE_NOMEM;
	  p->xDelUser = gaia_mbr_del;
	  mbr->minx = p->aParam[0] - p->aParam[2];
	  mbr->miny = p->aParam[1] - p->aParam[2];
	  mbr->maxx = p->aParam[0] + p->aParam[2];
	  mbr->maxy = p->aParam[1] + p->aParam[2];
      }

    mbr = (struct gaia_rtree_mbr *) (p->pUser);
    xmin = aCoord[0];
    xmax = aCoord[1];
    ymin = aCoord[2];
    ymax = aCoord[3];
    *pRes = 1;
/* evaluating Intersects relationship */
    if (xmin > mbr->maxx)
	*pRes = 0;
    if (xmax < mbr->minx)
	*pRes = 0;
    if (ymin > mbr->maxy)
	*pRes = 0;
    if (ymax < mbr->miny)
	*pRes = 0;
    return SQLITE_OK;
}
#endif /* end RTree geometry callbacks */

static void
fnct_X (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ X(BLOB encoded POINT)
/
/ returns the X coordinate for current POINT geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaPointPtr point;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = simplePoint (geo);
	  if (!point)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, point->X);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Y (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Y(BLOB encoded POINT)
/
/ returns the Y coordinate for current POINT geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaPointPtr point;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = simplePoint (geo);
	  if (!point)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, point->Y);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Z (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Z(BLOB encoded POINT)
/
/ returns the Z coordinate for current POINT geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaPointPtr point;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = simplePoint (geo);
	  if (!point)
	      sqlite3_result_null (context);
	  else
	    {
		if (point->DimensionModel == GAIA_XY_Z
		    || point->DimensionModel == GAIA_XY_Z_M)
		    sqlite3_result_double (context, point->Z);
		else
		    sqlite3_result_null (context);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_M (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ M(BLOB encoded POINT)
/
/ returns the M coordinate for current POINT geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaPointPtr point;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = simplePoint (geo);
	  if (!point)
	      sqlite3_result_null (context);
	  else
	    {
		if (point->DimensionModel == GAIA_XY_M
		    || point->DimensionModel == GAIA_XY_Z_M)
		    sqlite3_result_double (context, point->M);
		else
		    sqlite3_result_null (context);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NumPoints (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ NumPoints(BLOB encoded LINESTRING)
/
/ returns the number of vertices for current LINESTRING geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaLinestringPtr line;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  line = simpleLinestring (geo);
	  if (!line)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_int (context, line->Points);
      }
    gaiaFreeGeomColl (geo);
}

static void
point_n (sqlite3_context * context, int argc, sqlite3_value ** argv,
	 int request)
{
/* SQL functions:
/ StartPoint(BLOB encoded LINESTRING geometry)
/ EndPoint(BLOB encoded LINESTRING geometry)
/ PointN(BLOB encoded LINESTRING geometry, integer point_no)
/
/ returns the Nth POINT for current LINESTRING geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int vertex;
    int len;
    double x;
    double y;
    double z;
    double m;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    gaiaLinestringPtr line;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (request == GAIA_POINTN)
      {
	  /* PointN() requires point index to be defined as an SQL function argument */
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  vertex = sqlite3_value_int (argv[1]);
      }
    else if (request == GAIA_END_POINT)
	vertex = -1;		/* EndPoint() specifies a negative point index */
    else
	vertex = 1;		/* StartPoint() */
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  line = simpleLinestring (geo);
	  if (!line)
	      sqlite3_result_null (context);
	  else
	    {
		if (vertex < 0)
		    vertex = line->Points - 1;
		else
		    vertex -= 1;	/* decreasing the point index by 1, because PointN counts starting at index 1 */
		if (vertex >= 0 && vertex < line->Points)
		  {
		      if (line->DimensionModel == GAIA_XY_Z)
			{
			    gaiaGetPointXYZ (line->Coords, vertex, &x, &y, &z);
			    result = gaiaAllocGeomCollXYZ ();
			    result->Srid = geo->Srid;
			    gaiaAddPointToGeomCollXYZ (result, x, y, z);
			}
		      else if (line->DimensionModel == GAIA_XY_M)
			{
			    gaiaGetPointXYM (line->Coords, vertex, &x, &y, &m);
			    result = gaiaAllocGeomCollXYM ();
			    result->Srid = geo->Srid;
			    gaiaAddPointToGeomCollXYM (result, x, y, m);
			}
		      else if (line->DimensionModel == GAIA_XY_Z_M)
			{
			    gaiaGetPointXYZM (line->Coords, vertex, &x, &y,
					      &z, &m);
			    result = gaiaAllocGeomCollXYZM ();
			    result->Srid = geo->Srid;
			    gaiaAddPointToGeomCollXYZM (result, x, y, z, m);
			}
		      else
			{
			    gaiaGetPoint (line->Coords, vertex, &x, &y);
			    result = gaiaAllocGeomColl ();
			    result->Srid = geo->Srid;
			    gaiaAddPointToGeomColl (result, x, y);
			}
		  }
		else
		    result = NULL;
		if (!result)
		    sqlite3_result_null (context);
		else
		  {
		      gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		      gaiaFreeGeomColl (result);
		      sqlite3_result_blob (context, p_result, len, free);
		  }
	    }
      }
    gaiaFreeGeomColl (geo);
}

/*
/ the following functions simply readdress the request to point_n()
/ setting the appropriate request mode
*/

static void
fnct_StartPoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    point_n (context, argc, argv, GAIA_START_POINT);
}

static void
fnct_EndPoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    point_n (context, argc, argv, GAIA_END_POINT);
}

static void
fnct_PointN (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    point_n (context, argc, argv, GAIA_POINTN);
}

static void
fnct_ExteriorRing (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL functions:
/ ExteriorRing(BLOB encoded POLYGON geometry)
/
/ returns the EXTERIOR RING for current POLYGON geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int iv;
    double x;
    double y;
    double z;
    double m;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    gaiaPolygonPtr polyg;
    gaiaRingPtr ring;
    gaiaLinestringPtr line;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  polyg = simplePolygon (geo);
	  if (!polyg)
	      sqlite3_result_null (context);
	  else
	    {
		ring = polyg->Exterior;
		if (ring->DimensionModel == GAIA_XY_Z)
		    result = gaiaAllocGeomCollXYZ ();
		else if (ring->DimensionModel == GAIA_XY_M)
		    result = gaiaAllocGeomCollXYM ();
		else if (ring->DimensionModel == GAIA_XY_Z_M)
		    result = gaiaAllocGeomCollXYZM ();
		else
		    result = gaiaAllocGeomColl ();
		result->Srid = geo->Srid;
		line = gaiaAddLinestringToGeomColl (result, ring->Points);
		for (iv = 0; iv < line->Points; iv++)
		  {
		      if (ring->DimensionModel == GAIA_XY_Z)
			{
			    gaiaGetPointXYZ (ring->Coords, iv, &x, &y, &z);
			    gaiaSetPointXYZ (line->Coords, iv, x, y, z);
			}
		      else if (ring->DimensionModel == GAIA_XY_M)
			{
			    gaiaGetPointXYM (ring->Coords, iv, &x, &y, &m);
			    gaiaSetPointXYM (line->Coords, iv, x, y, m);
			}
		      else if (ring->DimensionModel == GAIA_XY_Z_M)
			{
			    gaiaGetPointXYZM (ring->Coords, iv, &x, &y, &z, &m);
			    gaiaSetPointXYZM (line->Coords, iv, x, y, z, m);
			}
		      else
			{
			    gaiaGetPoint (ring->Coords, iv, &x, &y);
			    gaiaSetPoint (line->Coords, iv, x, y);
			}
		  }
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		gaiaFreeGeomColl (result);
		sqlite3_result_blob (context, p_result, len, free);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NumInteriorRings (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ NumInteriorRings(BLOB encoded POLYGON)
/
/ returns the number of INTERIOR RINGS for current POLYGON geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaPolygonPtr polyg;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  polyg = simplePolygon (geo);
	  if (!polyg)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_int (context, polyg->NumInteriors);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_InteriorRingN (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL functions:
/ InteriorRingN(BLOB encoded POLYGON geometry)
/
/ returns the Nth INTERIOR RING for current POLYGON geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int border;
    int iv;
    double x;
    double y;
    double z;
    double m;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    gaiaPolygonPtr polyg;
    gaiaRingPtr ring;
    gaiaLinestringPtr line;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    border = sqlite3_value_int (argv[1]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  polyg = simplePolygon (geo);
	  if (!polyg)
	      sqlite3_result_null (context);
	  else
	    {
		if (border >= 1 && border <= polyg->NumInteriors)
		  {
		      ring = polyg->Interiors + (border - 1);
		      if (ring->DimensionModel == GAIA_XY_Z)
			  result = gaiaAllocGeomCollXYZ ();
		      else if (ring->DimensionModel == GAIA_XY_M)
			  result = gaiaAllocGeomCollXYM ();
		      else if (ring->DimensionModel == GAIA_XY_Z_M)
			  result = gaiaAllocGeomCollXYZM ();
		      else
			  result = gaiaAllocGeomColl ();
		      result->Srid = geo->Srid;
		      line = gaiaAddLinestringToGeomColl (result, ring->Points);
		      for (iv = 0; iv < line->Points; iv++)
			{
			    if (ring->DimensionModel == GAIA_XY_Z)
			      {
				  gaiaGetPointXYZ (ring->Coords, iv, &x,
						   &y, &z);
				  gaiaSetPointXYZ (line->Coords, iv, x, y, z);
			      }
			    else if (ring->DimensionModel == GAIA_XY_M)
			      {
				  gaiaGetPointXYM (ring->Coords, iv, &x,
						   &y, &m);
				  gaiaSetPointXYM (line->Coords, iv, x, y, m);
			      }
			    else if (ring->DimensionModel == GAIA_XY_Z_M)
			      {
				  gaiaGetPointXYZM (ring->Coords, iv, &x,
						    &y, &z, &m);
				  gaiaSetPointXYZM (line->Coords, iv, x,
						    y, z, m);
			      }
			    else
			      {
				  gaiaGetPoint (ring->Coords, iv, &x, &y);
				  gaiaSetPoint (line->Coords, iv, x, y);
			      }
			}
		      gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		      gaiaFreeGeomColl (result);
		      sqlite3_result_blob (context, p_result, len, free);
		  }
		else
		    sqlite3_result_null (context);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NumGeometries (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ NumGeometries(BLOB encoded GEOMETRYCOLLECTION)
/
/ returns the number of elementary geometries for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int cnt = 0;
    gaiaPointPtr point;
    gaiaLinestringPtr line;
    gaiaPolygonPtr polyg;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = geo->FirstPoint;
	  while (point)
	    {
		/* counts how many points are there */
		cnt++;
		point = point->Next;
	    }
	  line = geo->FirstLinestring;
	  while (line)
	    {
		/* counts how many linestrings are there */
		cnt++;
		line = line->Next;
	    }
	  polyg = geo->FirstPolygon;
	  while (polyg)
	    {
		/* counts how many polygons are there */
		cnt++;
		polyg = polyg->Next;
	    }
	  sqlite3_result_int (context, cnt);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NPoints (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_NPoints(BLOB encoded GEOMETRYCOLLECTION)
/
/ returns the total number of points/vertices for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int cnt = 0;
    int ib;
    gaiaPointPtr point;
    gaiaLinestringPtr line;
    gaiaPolygonPtr polyg;
    gaiaRingPtr rng;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = geo->FirstPoint;
	  while (point)
	    {
		/* counts how many points are there */
		cnt++;
		point = point->Next;
	    }
	  line = geo->FirstLinestring;
	  while (line)
	    {
		/* counts how many points are there */
		cnt += line->Points;
		line = line->Next;
	    }
	  polyg = geo->FirstPolygon;
	  while (polyg)
	    {
		/* counts how many points are in the exterior ring */
		rng = polyg->Exterior;
		cnt += rng->Points;
		for (ib = 0; ib < polyg->NumInteriors; ib++)
		  {
		      /* processing any interior ring */
		      rng = polyg->Interiors + ib;
		      cnt += rng->Points;
		  }
		polyg = polyg->Next;
	    }
	  sqlite3_result_int (context, cnt);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NRings (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_NRings(BLOB encoded GEOMETRYCOLLECTION)
/
/ returns the total number of rings for current geometry 
/ (this including both interior and exterior rings)
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int cnt = 0;
    gaiaPolygonPtr polyg;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  polyg = geo->FirstPolygon;
	  while (polyg)
	    {
		/* counts how many rings are there */
		cnt += polyg->NumInteriors + 1;
		polyg = polyg->Next;
	    }
	  sqlite3_result_int (context, cnt);
      }
    gaiaFreeGeomColl (geo);
}

static int
is_single_point (gaiaGeomCollPtr geom)
{
/* check if this geometry is a simple Point */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts == 1 && lns == 0 && pgs == 0)
	return 1;
    return 0;
}

static int
is_single_linestring (gaiaGeomCollPtr geom)
{
/* check if this geometry is a simple Linestring */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts == 0 && lns == 1 && pgs == 0)
	return 1;
    return 0;
}

static void
fnct_AddPoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL functions:
/ ST_AddPoint(BLOB encoded LINESTRING line, BLOB encoded POINT point)
/ ST_AddPoint(BLOB encoded LINESTRING line, BLOB encoded POINT point, INTEGER position)
/
/ returns a new Linestring by adding a new Point before "position" (zero-based index)
/ a negative "position" (default) means appending to the end 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaLinestringPtr ln;
    gaiaLinestringPtr out_ln;
    gaiaPointPtr pt;
    int position = -1;
    gaiaGeomCollPtr line = NULL;
    gaiaGeomCollPtr point = NULL;
    gaiaGeomCollPtr out;
    int len;
    unsigned char *p_result = NULL;
    int iv;
    int out_iv;
    double x;
    double y;
    double m;
    double z;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    line = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!line)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  gaiaFreeGeomColl (line);
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    point = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!point)
      {
	  gaiaFreeGeomColl (line);
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		goto stop;
	    }
	  position = sqlite3_value_int (argv[2]);
      }
    if (is_single_linestring (line) && is_single_point (point))
	;
    else
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    ln = line->FirstLinestring;
    pt = point->FirstPoint;
    if (position >= 0 && position >= ln->Points)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
/* creating the output Geometry */
    if (line->DimensionModel == GAIA_XY_Z)
	out = gaiaAllocGeomCollXYZ ();
    else if (line->DimensionModel == GAIA_XY_M)
	out = gaiaAllocGeomCollXYM ();
    else if (line->DimensionModel == GAIA_XY_Z_M)
	out = gaiaAllocGeomCollXYZM ();
    else
	out = gaiaAllocGeomColl ();
    out->Srid = line->Srid;
    out->DeclaredType = line->DeclaredType;
    out_ln = gaiaAddLinestringToGeomColl (out, ln->Points + 1);
    if (position < 0)
      {
	  /* appending the new Point */
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		if (line->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      gaiaSetPointXYZ (out_ln->Coords, iv, x, y, z);
		  }
		else if (line->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      gaiaSetPointXYM (out_ln->Coords, iv, x, y, m);
		  }
		else if (line->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      gaiaSetPointXYZM (out_ln->Coords, iv, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      gaiaSetPoint (out_ln->Coords, iv, x, y);
		  }
	    }
	  /* appending the new Point */
	  x = pt->X;
	  y = pt->Y;
	  z = pt->Z;
	  m = pt->M;
	  out_iv = ln->Points;
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (out_ln->Coords, out_iv, x, y, z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (out_ln->Coords, out_iv, x, y, m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (out_ln->Coords, out_iv, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (out_ln->Coords, out_iv, x, y);
	    }
      }
    else
      {
	  /* inserting the new Point before "position" */
	  out_iv = 0;
	  for (iv = 0; iv < position; iv++)
	    {
		/* copying all Points before "position" */
		if (line->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      gaiaSetPointXYZ (out_ln->Coords, out_iv, x, y, z);
		  }
		else if (line->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      gaiaSetPointXYM (out_ln->Coords, out_iv, x, y, m);
		  }
		else if (line->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      gaiaSetPointXYZM (out_ln->Coords, out_iv, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      gaiaSetPoint (out_ln->Coords, out_iv, x, y);
		  }
		out_iv++;
	    }
	  /* inserting the new Point */
	  x = pt->X;
	  y = pt->Y;
	  z = pt->Z;
	  m = pt->M;
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (out_ln->Coords, out_iv, x, y, z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (out_ln->Coords, out_iv, x, y, m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (out_ln->Coords, out_iv, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (out_ln->Coords, out_iv, x, y);
	    }
	  out_iv++;
	  for (iv = position; iv < ln->Points; iv++)
	    {
		/* copying all Points after "position" */
		if (line->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      gaiaSetPointXYZ (out_ln->Coords, out_iv, x, y, z);
		  }
		else if (line->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      gaiaSetPointXYM (out_ln->Coords, out_iv, x, y, m);
		  }
		else if (line->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      gaiaSetPointXYZM (out_ln->Coords, out_iv, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      gaiaSetPoint (out_ln->Coords, out_iv, x, y);
		  }
		out_iv++;
	    }
      }
    gaiaToSpatiaLiteBlobWkb (out, &p_result, &len);
    gaiaFreeGeomColl (out);
    sqlite3_result_blob (context, p_result, len, free);
  stop:
    gaiaFreeGeomColl (line);
    gaiaFreeGeomColl (point);
}

static void
fnct_SetPoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL functions:
/ ST_SetPoint(BLOB encoded LINESTRING line, INTEGER position, BLOB encoded POINT point)
/
/ returns a new Linestring by replacing the Point at "position" (zero-based index)
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaLinestringPtr ln;
    gaiaLinestringPtr out_ln;
    gaiaPointPtr pt;
    int position;
    gaiaGeomCollPtr line = NULL;
    gaiaGeomCollPtr point = NULL;
    gaiaGeomCollPtr out;
    int len;
    unsigned char *p_result = NULL;
    int iv;
    double x;
    double y;
    double m;
    double z;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    line = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!line)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  gaiaFreeGeomColl (line);
	  sqlite3_result_null (context);
	  return;
      }
    position = sqlite3_value_int (argv[1]);
    if (sqlite3_value_type (argv[2]) != SQLITE_BLOB)
      {
	  gaiaFreeGeomColl (line);
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[2]);
    n_bytes = sqlite3_value_bytes (argv[2]);
    point = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!point)
      {
	  gaiaFreeGeomColl (line);
	  sqlite3_result_null (context);
	  return;
      }
    if (is_single_linestring (line) && is_single_point (point))
	;
    else
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    ln = line->FirstLinestring;
    pt = point->FirstPoint;
    if (position < 0 || position >= ln->Points)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
/* creating the output Geometry */
    if (line->DimensionModel == GAIA_XY_Z)
	out = gaiaAllocGeomCollXYZ ();
    else if (line->DimensionModel == GAIA_XY_M)
	out = gaiaAllocGeomCollXYM ();
    else if (line->DimensionModel == GAIA_XY_Z_M)
	out = gaiaAllocGeomCollXYZM ();
    else
	out = gaiaAllocGeomColl ();
    out->Srid = line->Srid;
    out->DeclaredType = line->DeclaredType;
    out_ln = gaiaAddLinestringToGeomColl (out, ln->Points);
    for (iv = 0; iv < ln->Points; iv++)
      {
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
	    }
	  if (iv == position)
	    {
		/* replacing the new Point */
		x = pt->X;
		y = pt->Y;
		z = pt->Z;
		m = pt->M;
	    }
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (out_ln->Coords, iv, x, y, z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (out_ln->Coords, iv, x, y, m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (out_ln->Coords, iv, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (out_ln->Coords, iv, x, y);
	    }
      }
    gaiaToSpatiaLiteBlobWkb (out, &p_result, &len);
    gaiaFreeGeomColl (out);
    sqlite3_result_blob (context, p_result, len, free);
  stop:
    gaiaFreeGeomColl (line);
    gaiaFreeGeomColl (point);
}

static void
fnct_RemovePoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL functions:
/ ST_RemovePoint(BLOB encoded LINESTRING line, INTEGER position)
/
/ returns a new Linestring by removing the Point at "position" (zero-based index)
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaLinestringPtr ln;
    gaiaLinestringPtr out_ln;
    int position;
    gaiaGeomCollPtr line = NULL;
    gaiaGeomCollPtr out;
    int len;
    unsigned char *p_result = NULL;
    int iv;
    int out_iv;
    double x;
    double y;
    double m;
    double z;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    line = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!line)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    position = sqlite3_value_int (argv[1]);
    if (!is_single_linestring (line))
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
    ln = line->FirstLinestring;
    if (position < 0 || position >= ln->Points)
      {
	  sqlite3_result_null (context);
	  goto stop;
      }
/* creating the output Geometry */
    if (line->DimensionModel == GAIA_XY_Z)
	out = gaiaAllocGeomCollXYZ ();
    else if (line->DimensionModel == GAIA_XY_M)
	out = gaiaAllocGeomCollXYM ();
    else if (line->DimensionModel == GAIA_XY_Z_M)
	out = gaiaAllocGeomCollXYZM ();
    else
	out = gaiaAllocGeomColl ();
    out->Srid = line->Srid;
    out->DeclaredType = line->DeclaredType;
    out_ln = gaiaAddLinestringToGeomColl (out, ln->Points - 1);
    out_iv = 0;
    for (iv = 0; iv < position; iv++)
      {
	  /* copying all Points before "position" */
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		gaiaSetPointXYZ (out_ln->Coords, out_iv, x, y, z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		gaiaSetPointXYM (out_ln->Coords, out_iv, x, y, m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		gaiaSetPointXYZM (out_ln->Coords, out_iv, x, y, z, m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
		gaiaSetPoint (out_ln->Coords, out_iv, x, y);
	    }
	  out_iv++;
      }
    for (iv = position + 1; iv < ln->Points; iv++)
      {
	  /* copying all Points after "position" */
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		gaiaSetPointXYZ (out_ln->Coords, out_iv, x, y, z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		gaiaSetPointXYM (out_ln->Coords, out_iv, x, y, m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		gaiaSetPointXYZM (out_ln->Coords, out_iv, x, y, z, m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
		gaiaSetPoint (out_ln->Coords, out_iv, x, y);
	    }
	  out_iv++;
      }
    gaiaToSpatiaLiteBlobWkb (out, &p_result, &len);
    gaiaFreeGeomColl (out);
    sqlite3_result_blob (context, p_result, len, free);
  stop:
    gaiaFreeGeomColl (line);
}

static int
getXYZMSinglePoint (gaiaGeomCollPtr geom, double *x, double *y, double *z,
		    double *m)
{
/* check if this geometry is a simple Point (returning full coords) */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts == 1 && lns == 0 && pgs == 0)
	;
    else
	return 0;
    *x = geom->FirstPoint->X;
    *y = geom->FirstPoint->Y;
    if (geom->DimensionModel == GAIA_XY_Z
	|| geom->DimensionModel == GAIA_XY_Z_M)
	*z = geom->FirstPoint->Z;
    else
	*z = 0.0;
    if (geom->DimensionModel == GAIA_XY_M
	|| geom->DimensionModel == GAIA_XY_Z_M)
	*m = geom->FirstPoint->M;
    else
	*m = 0.0;
    return 1;
}

static void
fnct_SnapToGrid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_SnapToGrid(BLOBencoded geom, double size)
/ ST_SnapToGrid(BLOBencoded geom, double sizeX, double sizeY)
/ ST_SnapToGrid(BLOBencoded geom, double originX, double originY, 
/               double sizeX, double sizeY)
/
/ Snap all points of the input geometry to the grid defined by its 
/ origin and cell size. Remove consecutive points falling on the same
/ cell. Collapsed geometries in a collection are stripped from it.
/
/
/ ST_SnapToGrid(BLOBencoded geom, BLOBencoded point, double sizeX,
/               double sizeY, double sizeZ, double sizeM)
/
/ Snap all points of the input geometry to the grid defined by its 
/ origin (the second argument, must be a point) and cell sizes.
/ 
/ Specify 0 as size for any dimension you don't want to snap to
/ a grid.
/ return NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double origin_z = 0.0;
    double origin_m = 0.0;
    double size_x;
    double size_y;
    double size_z = 0.0;
    double size_m = 0.0;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr point = NULL;
    gaiaGeomCollPtr result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  /* ST_SnapToGrid(BLOBencoded geom, double size) */
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[1]);
		size_x = int_value;
		size_y = size_x;
	    }
	  else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	    {
		size_x = sqlite3_value_double (argv[1]);
		size_y = size_x;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 3)
      {
	  /* ST_SnapToGrid(BLOBencoded geom, double sizeX, double sizeY) */
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[1]);
		size_x = int_value;
	    }
	  else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	    {
		size_x = sqlite3_value_double (argv[1]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[2]);
		size_y = int_value;
	    }
	  else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	    {
		size_y = sqlite3_value_double (argv[2]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 5)
      {
	  /*
	     / ST_SnapToGrid(BLOBencoded geom, double originX, double originY, 
	     /               double sizeX, double sizeY)
	   */
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[1]);
		origin_x = int_value;
	    }
	  else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	    {
		origin_x = sqlite3_value_double (argv[1]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[2]);
		origin_y = int_value;
	    }
	  else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	    {
		origin_y = sqlite3_value_double (argv[2]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[3]);
		size_x = int_value;
	    }
	  else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	    {
		size_x = sqlite3_value_double (argv[3]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[4]);
		size_y = int_value;
	    }
	  else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	    {
		size_y = sqlite3_value_double (argv[4]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 6)
      {
	  /*
	     / ST_SnapToGrid(BLOBencoded geom, BLOBencoded point, double sizeX,
	     /               double sizeY, double sizeZ, double sizeM)
	   */
	  if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
	  n_bytes = sqlite3_value_bytes (argv[1]);
	  point = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
	  if (!point)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (!getXYZMSinglePoint
	      (point, &origin_x, &origin_y, &origin_z, &origin_m))
	    {
		gaiaFreeGeomColl (point);
		sqlite3_result_null (context);
		return;
	    }
	  gaiaFreeGeomColl (point);
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[2]);
		size_x = int_value;
	    }
	  else if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	    {
		size_x = sqlite3_value_double (argv[2]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[3]);
		size_y = int_value;
	    }
	  else if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	    {
		size_y = sqlite3_value_double (argv[3]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[4]);
		size_z = int_value;
	    }
	  else if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	    {
		size_z = sqlite3_value_double (argv[4]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (sqlite3_value_type (argv[5]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[5]);
		size_m = int_value;
	    }
	  else if (sqlite3_value_type (argv[5]) == SQLITE_FLOAT)
	    {
		size_m = sqlite3_value_double (argv[5]);
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result =
	      gaiaSnapToGrid (geo, origin_x, origin_y, origin_z, origin_m,
			      size_x, size_y, size_z, size_m);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static char garsMapping[24] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J',
    'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

static char
garsLetterCode (int value)
{
    return garsMapping[value];
}

static int
garsMappingIndex (const char letter)
{
    int i = 0;
    for (i = 0; i < 24; ++i)
      {
	  if (letter == garsMapping[i])
	      return i;
      }
    return -1;
}

static double
garsLetterToDegreesLat (char msd, char lsd)
{
    double high = garsMappingIndex (msd) * 24.0;
    double low = garsMappingIndex (lsd);
    if ((high < 0) || (low < 0))
      {
	  return -100.0;
      }
    return (((high + low) * 0.5) - 90.0);
}


static void
fnct_GARSMbr (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GARSMbr(Text)
/
/ converts the Text (which should be a valid GARS area) to the corresponding
/ MBR geometry.
/ This function will return NULL if an error occurs
*/
    const char *text = NULL;
    int len = 0;
    unsigned char *p_result = NULL;
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;

    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = (const char *) sqlite3_value_text (argv[0]);
    if ((strlen (text) < 5) || (strlen (text) > 7))
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (strlen (text) == 5)
      {
	  int numMatch = 0;
	  unsigned int digit100 = 0;
	  char letterMSD = '\0';
	  char letterLSD = '\0';
	  numMatch = sscanf (text, "%u%c%c", &digit100, &letterMSD, &letterLSD);
	  if (numMatch != 3)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  x1 = ((digit100 - 1) * 0.5) - 180.0;
	  y1 = garsLetterToDegreesLat (letterMSD, letterLSD);
	  if ((x1 < -180.0) || (x1 > 179.5) || (y1 < -90.0) || (y1 > 89.5))
	    {
		sqlite3_result_null (context);
		return;
	    }
	  x2 = x1 + 0.5;
	  y2 = y1 + 0.5;
      }
    if (strlen (text) == 6)
      {
	  unsigned int numMatch = 0;
	  unsigned int digit100 = 0;
	  char letterMSD = '\0';
	  char letterLSD = '\0';
	  unsigned int digitSegment = 0;
	  numMatch =
	      sscanf (text, "%u%c%c%u", &digit100, &letterMSD, &letterLSD,
		      &digitSegment);
	  if (numMatch != 4)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if ((digitSegment < 1) || (digitSegment > 4))
	    {
		sqlite3_result_null (context);
		return;
	    }
	  x1 = ((digit100 - 1) * 0.5) - 180.0;
	  if ((digitSegment == 2) || (digitSegment == 4))
	    {
		x1 += 0.25;
	    }
	  y1 = garsLetterToDegreesLat (letterMSD, letterLSD);
	  if ((digitSegment == 1) || (digitSegment == 2))
	    {
		y1 += 0.25;
	    }
	  if ((x1 < -180.0) || (x1 > 179.75) || (y1 < -90.0) || (y1 > 89.75))
	    {
		sqlite3_result_null (context);
		return;
	    }
	  x2 = x1 + 0.25;
	  y2 = y1 + 0.25;
      }
    if (strlen (text) == 7)
      {
	  unsigned int numMatch = 0;
	  unsigned int digit100 = 0;
	  char letterMSD = '\0';
	  char letterLSD = '\0';
	  unsigned int digitAndKeypad = 0;
	  unsigned int digitSegment = 0;
	  unsigned int keypadNumber = 0;
	  numMatch =
	      sscanf (text, "%u%c%c%u", &digit100, &letterMSD, &letterLSD,
		      &digitAndKeypad);
	  if (numMatch != 4)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  digitSegment = digitAndKeypad / 10;
	  keypadNumber = digitAndKeypad % 10;
	  if ((digitSegment < 1) || (digitSegment > 4))
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (keypadNumber < 1)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  x1 = ((digit100 - 1) * 0.5) - 180.0;
	  if ((digitSegment == 2) || (digitSegment == 4))
	    {
		x1 += 0.25;
	    }
	  y1 = garsLetterToDegreesLat (letterMSD, letterLSD);
	  if ((digitSegment == 1) || (digitSegment == 2))
	    {
		y1 += 0.25;
	    }
	  switch (keypadNumber)
	    {
	    case 1:
		x1 += 0 * 0.25 / 3;
		y1 += 2 * 0.25 / 3;
		break;
	    case 2:
		x1 += 1 * 0.25 / 3;
		y1 += 2 * 0.25 / 3;
		break;
	    case 3:
		x1 += 2 * 0.25 / 3;
		y1 += 2 * 0.25 / 3;
		break;
	    case 4:
		x1 += 0 * 0.25 / 3;
		y1 += 1 * 0.25 / 3;
		break;
	    case 5:
		x1 += 1 * 0.25 / 3;
		y1 += 1 * 0.25 / 3;
		break;
	    case 6:
		x1 += 2 * 0.25 / 3;
		y1 += 1 * 0.25 / 3;
		break;
	    case 7:
		x1 += 0 * 0.25 / 3;
		y1 += 0 * 0.25 / 3;
		break;
	    case 8:
		x1 += 1 * 0.25 / 3;
		y1 += 0 * 0.25 / 3;
		break;
	    case 9:
		x1 += 2 * 0.25 / 3;
		y1 += 0 * 0.25 / 3;
		break;
	    }
	  if ((x1 < -180.0) || (x1 >= 180.0) || (y1 < -90.0) || (y1 >= 90.0))
	    {
		sqlite3_result_null (context);
		return;
	    }
	  x2 = x1 + (0.25 / 3);
	  y2 = y1 + (0.25 / 3);
      }
    gaiaBuildMbr (x1, y1, x2, y2, 4326, &p_result, &len);
    if (!p_result)
      {
	  sqlite3_result_null (context);
	  printf ("bad p_result\n");
      }
    else
	sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_ToGARS (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ToGARS(BLOB encoded POINT)
/
/ returns the Global Area Reference System coordinate area for a given point,
/ or NULL if an error occurs
*/
    unsigned char *p_blob;
    int n_bytes;
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaPointPtr point;
    gaiaLinestringPtr line;
    gaiaPolygonPtr polyg;
    gaiaGeomCollPtr geo = NULL;
    char p_result[8];
    int lon_band = 0;
    double lon_minutes = 0;
    int segmentNumber = 0;
    int lat_band = 0;
    double lat_minutes = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaNormalizeLonLat (geo);
    point = geo->FirstPoint;
    while (point != NULL)
      {
	  pts++;
	  point = point->Next;
      }
    line = geo->FirstLinestring;
    while (line != NULL)
      {
	  lns++;
	  line = line->Next;
      }
    polyg = geo->FirstPolygon;
    while (polyg != NULL)
      {
	  pgs++;
	  polyg = polyg->Next;
      }
    if (pts == 1 && lns == 0 && pgs == 0)
	point = geo->FirstPoint;
    else
      {
	  /* not a single Point */
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    /* longitude band */
    lon_band = 1 + (int) ((point->X + 180.0) * 2);
    sprintf (p_result, "%03i", lon_band);
    /* latitude band */
    lat_band = (int) ((point->Y + 90.0) * 2);
    p_result[3] = garsLetterCode (lat_band / 24);
    p_result[4] = garsLetterCode (lat_band % 24);
    /* quadrant */
    lon_minutes = fmod ((point->X + 180.0), 0.5) * 60.0;
    if (lon_minutes < 15.0)
      {
	  segmentNumber = 1;
      }
    else
      {
	  segmentNumber = 2;
	  lon_minutes -= 15.0;
      }
    lat_minutes = fmod ((point->Y + 90.0), 0.5) * 60.0;
    if (lat_minutes < 15.0)
      {
	  segmentNumber += 2;
      }
    else
      {
	  /* we already have the right segment */
	  lat_minutes -= 15.0;
      }
    sprintf (&(p_result[5]), "%i", segmentNumber);
    /* area */
    segmentNumber = 0;
    if (lon_minutes >= 10.0)
      {
	  segmentNumber = 3;
      }
    else if (lon_minutes >= 5.0)
      {
	  segmentNumber = 2;
      }
    else
      {
	  segmentNumber = 1;
      }
    if (lat_minutes >= 10.0)
      {
	  /* nothing to add */
      }
    else if (lat_minutes >= 5.0)
      {
	  segmentNumber += 3;
      }
    else
      {
	  segmentNumber += 6;
      }
    sprintf (&(p_result[6]), "%i", segmentNumber);
    sqlite3_result_text (context, p_result, 7, SQLITE_TRANSIENT);
    gaiaFreeGeomColl (geo);
}

static void
fnct_GeometryN (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeometryN(BLOB encoded GEOMETRYCOLLECTION geometry)
/
/ returns the Nth geometry for current GEOMETRYCOLLECTION or MULTIxxxx geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int entity;
    int len;
    int cnt = 0;
    int iv;
    int ib;
    double x;
    double y;
    double z;
    double m;
    gaiaPointPtr point;
    gaiaLinestringPtr line;
    gaiaLinestringPtr line2;
    gaiaPolygonPtr polyg;
    gaiaPolygonPtr polyg2;
    gaiaRingPtr ring_in;
    gaiaRingPtr ring_out;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    entity = sqlite3_value_int (argv[1]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  point = geo->FirstPoint;
	  while (point)
	    {
		/* counts how many points are there */
		cnt++;
		if (cnt == entity)
		  {
		      /* ok, required elementary geometry is this POINT */
		      if (point->DimensionModel == GAIA_XY_Z)
			  result = gaiaAllocGeomCollXYZ ();
		      else if (point->DimensionModel == GAIA_XY_M)
			  result = gaiaAllocGeomCollXYM ();
		      else if (point->DimensionModel == GAIA_XY_Z_M)
			  result = gaiaAllocGeomCollXYZM ();
		      else
			  result = gaiaAllocGeomColl ();
		      result->Srid = geo->Srid;
		      if (point->DimensionModel == GAIA_XY_Z)
			  gaiaAddPointToGeomCollXYZ (result, point->X,
						     point->Y, point->Z);
		      else if (point->DimensionModel == GAIA_XY_M)
			  gaiaAddPointToGeomCollXYM (result, point->X,
						     point->Y, point->M);
		      else if (point->DimensionModel == GAIA_XY_Z_M)
			  gaiaAddPointToGeomCollXYZM (result, point->X,
						      point->Y, point->Z,
						      point->M);
		      else
			  gaiaAddPointToGeomColl (result, point->X, point->Y);
		      goto skip;
		  }
		point = point->Next;
	    }
	  line = geo->FirstLinestring;
	  while (line)
	    {
		/* counts how many linestrings are there */
		cnt++;
		if (cnt == entity)
		  {
		      /* ok, required elementary geometry is this LINESTRING */
		      if (line->DimensionModel == GAIA_XY_Z)
			  result = gaiaAllocGeomCollXYZ ();
		      else if (line->DimensionModel == GAIA_XY_M)
			  result = gaiaAllocGeomCollXYM ();
		      else if (line->DimensionModel == GAIA_XY_Z_M)
			  result = gaiaAllocGeomCollXYZM ();
		      else
			  result = gaiaAllocGeomColl ();
		      result->Srid = geo->Srid;
		      line2 =
			  gaiaAddLinestringToGeomColl (result, line->Points);
		      for (iv = 0; iv < line2->Points; iv++)
			{
			    if (line->DimensionModel == GAIA_XY_Z)
			      {
				  gaiaGetPointXYZ (line->Coords, iv, &x,
						   &y, &z);
				  gaiaSetPointXYZ (line2->Coords, iv, x, y, z);
			      }
			    else if (line->DimensionModel == GAIA_XY_M)
			      {
				  gaiaGetPointXYM (line->Coords, iv, &x,
						   &y, &m);
				  gaiaSetPointXYM (line2->Coords, iv, x, y, m);
			      }
			    else if (line->DimensionModel == GAIA_XY_Z_M)
			      {
				  gaiaGetPointXYZM (line->Coords, iv, &x,
						    &y, &z, &m);
				  gaiaSetPointXYZM (line2->Coords, iv, x,
						    y, z, m);
			      }
			    else
			      {
				  gaiaGetPoint (line->Coords, iv, &x, &y);
				  gaiaSetPoint (line2->Coords, iv, x, y);
			      }
			}
		      goto skip;
		  }
		line = line->Next;
	    }
	  polyg = geo->FirstPolygon;
	  while (polyg)
	    {
		/* counts how many polygons are there */
		cnt++;
		if (cnt == entity)
		  {
		      /* ok, required elementary geometry is this POLYGON */
		      if (polyg->DimensionModel == GAIA_XY_Z)
			  result = gaiaAllocGeomCollXYZ ();
		      else if (polyg->DimensionModel == GAIA_XY_M)
			  result = gaiaAllocGeomCollXYM ();
		      else if (polyg->DimensionModel == GAIA_XY_Z_M)
			  result = gaiaAllocGeomCollXYZM ();
		      else
			  result = gaiaAllocGeomColl ();
		      result->Srid = geo->Srid;
		      ring_in = polyg->Exterior;
		      polyg2 =
			  gaiaAddPolygonToGeomColl (result,
						    ring_in->Points,
						    polyg->NumInteriors);
		      ring_out = polyg2->Exterior;
		      for (iv = 0; iv < ring_out->Points; iv++)
			{
			    /* copying the exterior ring POINTs */
			    if (ring_in->DimensionModel == GAIA_XY_Z)
			      {
				  gaiaGetPointXYZ (ring_in->Coords, iv,
						   &x, &y, &z);
				  gaiaSetPointXYZ (ring_out->Coords, iv,
						   x, y, z);
			      }
			    else if (ring_in->DimensionModel == GAIA_XY_M)
			      {
				  gaiaGetPointXYM (ring_in->Coords, iv,
						   &x, &y, &m);
				  gaiaSetPointXYM (ring_out->Coords, iv,
						   x, y, m);
			      }
			    else if (ring_in->DimensionModel == GAIA_XY_Z_M)
			      {
				  gaiaGetPointXYZM (ring_in->Coords, iv,
						    &x, &y, &z, &m);
				  gaiaSetPointXYZM (ring_out->Coords, iv,
						    x, y, z, m);
			      }
			    else
			      {
				  gaiaGetPoint (ring_in->Coords, iv, &x, &y);
				  gaiaSetPoint (ring_out->Coords, iv, x, y);
			      }
			}
		      for (ib = 0; ib < polyg2->NumInteriors; ib++)
			{
			    /* processing the interior rings */
			    ring_in = polyg->Interiors + ib;
			    ring_out =
				gaiaAddInteriorRing (polyg2, ib,
						     ring_in->Points);
			    for (iv = 0; iv < ring_out->Points; iv++)
			      {
				  if (ring_in->DimensionModel == GAIA_XY_Z)
				    {
					gaiaGetPointXYZ (ring_in->Coords,
							 iv, &x, &y, &z);
					gaiaSetPointXYZ (ring_out->Coords, iv,
							 x, y, z);
				    }
				  else if (ring_in->DimensionModel == GAIA_XY_M)
				    {
					gaiaGetPointXYM (ring_in->Coords,
							 iv, &x, &y, &m);
					gaiaSetPointXYM (ring_out->Coords, iv,
							 x, y, m);
				    }
				  else if (ring_in->DimensionModel ==
					   GAIA_XY_Z_M)
				    {
					gaiaGetPointXYZM (ring_in->Coords, iv,
							  &x, &y, &z, &m);
					gaiaSetPointXYZM (ring_out->Coords, iv,
							  x, y, z, m);
				    }
				  else
				    {
					gaiaGetPoint (ring_in->Coords,
						      iv, &x, &y);
					gaiaSetPoint (ring_out->Coords,
						      iv, x, y);
				    }
			      }
			}
		      goto skip;
		  }
		polyg = polyg->Next;
	    }
	skip:
	  if (result)
	    {
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		gaiaFreeGeomColl (result);
		sqlite3_result_blob (context, p_result, len, free);
	    }
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo);
}

static void
mbrs_eval (sqlite3_context * context, int argc, sqlite3_value ** argv,
	   int request)
{
/* SQL function:
/ MBRsomething(BLOB encoded GEOMETRY-1, BLOB encoded GEOMETRY-2)
/
/ returns:
/ 1 if the required spatial relationship between the two MBRs is TRUE
/ 0 otherwise
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobMbr (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobMbr (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  ret = 0;
	  gaiaMbrGeometry (geo1);
	  gaiaMbrGeometry (geo2);
	  switch (request)
	    {
	    case GAIA_MBR_CONTAINS:
		ret = gaiaMbrsContains (geo1, geo2);
		break;
	    case GAIA_MBR_DISJOINT:
		ret = gaiaMbrsDisjoint (geo1, geo2);
		break;
	    case GAIA_MBR_EQUAL:
		ret = gaiaMbrsEqual (geo1, geo2);
		break;
	    case GAIA_MBR_INTERSECTS:
		ret = gaiaMbrsIntersects (geo1, geo2);
		break;
	    case GAIA_MBR_OVERLAPS:
		ret = gaiaMbrsOverlaps (geo1, geo2);
		break;
	    case GAIA_MBR_TOUCHES:
		ret = gaiaMbrsTouches (geo1, geo2);
		break;
	    case GAIA_MBR_WITHIN:
		ret = gaiaMbrsWithin (geo1, geo2);
		break;
	    }
	  if (ret < 0)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

/*
/ the following functions simply readdress the mbr_eval()
/ setting the appropriate request mode
*/

static void
fnct_MbrContains (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_CONTAINS);
}

static void
fnct_MbrDisjoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_DISJOINT);
}

static void
fnct_MbrEqual (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_EQUAL);
}

static void
fnct_MbrIntersects (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_INTERSECTS);
}

static void
fnct_EnvIntersects (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_EnvIntersects(Geometry geom, double X1, double Y1, double X2, double Y2)
/ ST_EnvelopesIntersects(Geometry geom, double X1, double Y1, double X2, double Y2)
/
/ the second MBR is defined by two points (identifying a rectangle's diagonal) 
/ or NULL if any error is encountered
*/
    double x1;
    double y1;
    double x2;
    double y2;
    int int_value;
    unsigned char *p_blob;
    int n_bytes;
    int ret = 0;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaLinestringPtr ln;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	x1 = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  x1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	y1 = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  y1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	x2 = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  x2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_FLOAT)
	y2 = sqlite3_value_double (argv[4]);
    else if (sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[4]);
	  y2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1)
	sqlite3_result_null (context);
    else
      {
	  gaiaMbrGeometry (geo1);
	  geo2 = gaiaAllocGeomColl ();
	  ln = gaiaAddLinestringToGeomColl (geo2, 2);
	  gaiaSetPoint (ln->Coords, 0, x1, y1);
	  gaiaSetPoint (ln->Coords, 1, x2, y2);
	  gaiaMbrGeometry (geo2);
	  ret = gaiaMbrsIntersects (geo1, geo2);
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}


static void
fnct_MbrOverlaps (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_OVERLAPS);
}

static void
fnct_MbrTouches (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_TOUCHES);
}

static void
fnct_MbrWithin (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    mbrs_eval (context, argc, argv, GAIA_MBR_WITHIN);
}

static void
fnct_ShiftCoords (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ShiftCoords(BLOBencoded geometry, shiftX, shiftY)
/
/ returns a new geometry that is the original one received, but with shifted coordinates
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    double shift_x;
    double shift_y;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	shift_x = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  shift_x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	shift_y = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  shift_y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaShiftCoords (geo, shift_x, shift_y);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Translate (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Translate(BLOBencoded geometry, shiftX, shiftY, shiftZ)
/
/ returns a new geometry that is the original one received, but with shifted coordinates
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    double shift_x;
    double shift_y;
    double shift_z;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	shift_x = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  shift_x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	shift_y = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  shift_y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	shift_z = sqlite3_value_double (argv[3]);
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[3]);
	  shift_z = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaShiftCoords3D (geo, shift_x, shift_y, shift_z);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}


static void
fnct_ShiftLongitude (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ShiftLongitude(BLOBencoded geometry)
/
/ returns a new geometry that is the original one received, but with negative
/ longitudes shifted by 360
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaShiftLongitude (geo);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_NormalizeLonLat (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ NormalizeLonLat (BLOBencoded geometry)
/
/ returns a new geometry that is the original one received, but with longitude
/ and latitude values shifted into the range [-180 - 180, -90 - 90]. 
/ NULL is returned if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaNormalizeLonLat (geo);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ScaleCoords (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ScaleCoords(BLOBencoded geometry, scale_factor_x [, scale_factor_y])
/
/ returns a new geometry that is the original one received, but with scaled coordinates
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    double scale_x;
    double scale_y;
    int int_value;
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	scale_x = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  scale_x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
	scale_y = scale_x;	/* this one is an isotropic scaling request */
    else
      {
	  /* an anisotropic scaling is requested */
	  if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	      scale_y = sqlite3_value_double (argv[2]);
	  else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[2]);
		scale_y = int_value;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaScaleCoords (geo, scale_x, scale_y);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_RotateCoords (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ RotateCoords(BLOBencoded geometry, angle)
/
/ returns a new geometry that is the original one received, but with rotated coordinates
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    double angle;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	angle = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  angle = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaRotateCoords (geo, angle);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ReflectCoords (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ReflectCoords(BLOBencoded geometry, x_axis,  y_axis)
/
/ returns a new geometry that is the original one received, but with mirrored coordinates
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    int x_axis;
    int y_axis;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	x_axis = sqlite3_value_int (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	y_axis = sqlite3_value_int (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaReflectCoords (geo, x_axis, y_axis);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_SwapCoords (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ SwapCoords(BLOBencoded geometry)
/
/ returns a new geometry that is the original one received, but with swapped x- and y-coordinate
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaSwapCoords (geo);
	  gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
	  if (!p_result)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (geo);
}

SPATIALITE_PRIVATE int
getEllipsoidParams (void *p_sqlite, int srid, double *a, double *b, double *rf)
{
/* 
/ retrieves the PROJ +ellps=xx [+a=xx +b=xx] params 
/from SPATIAL_SYS_REF table, if possible 
*/
    sqlite3 *sqlite = (sqlite3 *) p_sqlite;
    char *proj4text;
    char *p_proj;
    char *p_ellps;
    char *p_datum;
    char *p_a;
    char *p_b;
    char *p_end;

    if (srid == 0)
      {
	  /* 
	     / SRID=0 is formally defined as "Undefined Geographic"
	     / so will default to SRID=4326 (WGS84 Long/Lat)
	   */
	  srid = 4326;
      }
    getProjParams (sqlite, srid, &proj4text);
    if (proj4text == NULL)
	return 0;
/* parsing the proj4text geodesic string */
    p_proj = strstr (proj4text, "+proj=");
    p_datum = strstr (proj4text, "+datum=");
    p_ellps = strstr (proj4text, "+ellps=");
    p_a = strstr (proj4text, "+a=");
    p_b = strstr (proj4text, "+b=");
/* checking if +proj=longlat is true */
    if (!p_proj)
	goto invalid;
    p_end = strchr (p_proj, ' ');
    if (p_end)
	*p_end = '\0';
    if (strcmp (p_proj + 6, "longlat") != 0)
	goto invalid;
    if (p_ellps)
      {
	  /* trying to retrieve the ellipsoid params by name */
	  p_end = strchr (p_ellps, ' ');
	  if (p_end)
	      *p_end = '\0';
	  if (gaiaEllipseParams (p_ellps + 7, a, b, rf))
	      goto valid;
      }
    else if (p_datum)
      {
	  /*
	     / starting since GDAL 1.9.0 the WGS84 [4326] PROJ.4 def doesn't 
	     / declares any longer the "+ellps=" param
	     / in this case we'll attempt to recover using "+datum=".
	   */
	  p_end = strchr (p_datum, ' ');
	  if (p_end)
	      *p_end = '\0';
	  if (gaiaEllipseParams (p_datum + 7, a, b, rf))
	      goto valid;
      }
    if (p_a && p_b)
      {
	  /* trying to retrieve the +a=xx and +b=xx args */
	  p_end = strchr (p_a, ' ');
	  if (p_end)
	      *p_end = '\0';
	  p_end = strchr (p_b, ' ');
	  if (p_end)
	      *p_end = '\0';
	  *a = atof (p_a + 3);
	  *b = atof (p_b + 3);
	  *rf = 1.0 / ((*a - *b) / *a);
	  goto valid;
      }

  valid:
    free (proj4text);
    return 1;

  invalid:
    free (proj4text);
    return 0;
}

static void
fnct_FromEWKB (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromEWKB(EWKB encoded geometry)
/
/ returns the current geometry by parsing Geos/PostGis EWKB encoded string 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaFromEWKB (text);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_ToEWKB (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsEWKB(BLOB encoded geometry)
/
/ returns a text string corresponding to Geos/PostGIS EWKB notation 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
      {
	  sqlite3_result_null (context);
	  return;
      }
    else
      {
	  gaiaOutBufferInitialize (&out_buf);
	  gaiaToEWKB (&out_buf, geo);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_ToEWKT (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsEWKT(BLOB encoded geometry)
/
/ returns the corresponding PostGIS EWKT encoded value
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    gaiaOutBuffer out_buf;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    gaiaOutBufferInitialize (&out_buf);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  gaiaToEWKT (&out_buf, geo);
	  if (out_buf.Error || out_buf.Buffer == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		len = out_buf.WriteOffset;
		sqlite3_result_text (context, out_buf.Buffer, len, free);
		out_buf.Buffer = NULL;
	    }
      }
    gaiaFreeGeomColl (geo);
    gaiaOutBufferReset (&out_buf);
}

static void
fnct_FromEWKT (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromEWKT(EWKT encoded geometry)
/
/ returns the current geometry by parsing EWKT  (PostGIS) encoded string 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseEWKT (text);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_FromGeoJSON (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromGeoJSON(GeoJSON encoded geometry)
/
/ returns the current geometry by parsing GeoJSON encoded string 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseGeoJSON (text);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_FromKml (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromKml(KML encoded geometry)
/
/ returns the current geometry by parsing KML encoded string 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseKml (text);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_FromGml (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromGml(GML encoded geometry)
/
/ returns the current geometry by parsing GML encoded string 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseGml (text, sqlite);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geo, &p_result, &len);
    gaiaFreeGeomColl (geo);
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_LinesFromRings (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ LinesFromRings(BLOBencoded geometry, BOOL multi_linestring)
/
/ returns a new geometry [LINESTRING or MULTILINESTRING] representing 
/ the linearization for current (MULTI)POLYGON geometry
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr geom_new = NULL;
    int len;
    int multi_linestring = 0;
    unsigned char *p_result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      multi_linestring = sqlite3_value_int (argv[1]);
      }
    geom_new = gaiaLinearize (geo, multi_linestring);
    if (!geom_new)
	goto invalid;
    gaiaFreeGeomColl (geo);
    gaiaToSpatiaLiteBlobWkb (geom_new, &p_result, &len);
    gaiaFreeGeomColl (geom_new);
    sqlite3_result_blob (context, p_result, len, free);
    return;
  invalid:
    if (geo)
	gaiaFreeGeomColl (geo);
    sqlite3_result_null (context);
}

#ifndef OMIT_GEOS		/* including GEOS */

static void
fnct_BuildArea (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BuildArea(BLOBencoded geometry)
/
/ Assuming that Geometry represents a set of sparse Linestrings,
/ this function will attempt to reassemble a single Polygon
/ (or a set of Polygons)
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaPolygonize (geo, 0);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}


static void
fnct_Polygonize_step (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ Polygonize(BLOBencoded geom)
/
/ aggregate function - STEP
/
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr *p;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geom)
	return;
    p = sqlite3_aggregate_context (context, sizeof (gaiaGeomCollPtr));
    if (!(*p))
      {
	  /* this is the first row */
	  *p = geom;
      }
    else
      {
	  /* subsequent rows */
	  result = gaiaMergeGeometries (*p, geom);
	  gaiaFreeGeomColl (*p);
	  *p = result;
	  gaiaFreeGeomColl (geom);
      }
}

static void
fnct_Polygonize_final (sqlite3_context * context)
{
/* SQL function:
/ Polygonize(BLOBencoded geom)
/
/ aggregate function - FINAL
/
*/
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr geom;
    gaiaGeomCollPtr *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    result = *p;
    if (!result)
	sqlite3_result_null (context);
    else
      {
	  geom = gaiaPolygonize (result, 0);
	  if (geom == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		geom->Srid = result->Srid;
		gaiaToSpatiaLiteBlobWkb (geom, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (geom);
	    }
	  gaiaFreeGeomColl (result);
      }
}

#endif /* end including GEOS */

static void
fnct_DissolveSegments (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ DissolveSegments(BLOBencoded geometry)
/
/ Dissolves any LINESTRING or RING into elementary segments
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaDissolveSegments (geo);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_DissolvePoints (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ DissolvePoints(BLOBencoded geometry)
/
/ Dissolves any LINESTRING or RING into elementary Vertices
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaDissolvePoints (geo);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_CollectionExtract (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ CollectionExtract(BLOBencoded geometry, Integer type)
/
/ Extracts from a GEOMETRYCOLLECTION any item of the required TYPE
/ 1=Point - 2=Linestring - 3=Polygon
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    int type;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	type = sqlite3_value_int (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (type == 1 || type == 2 || type == 3)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  switch (type)
	    {
	    case 1:
		result = gaiaExtractPointsFromGeomColl (geo);
		break;
	    case 2:
		result = gaiaExtractLinestringsFromGeomColl (geo);
		break;
	    case 3:
		result = gaiaExtractPolygonsFromGeomColl (geo);
		break;
	    };
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_LocateBetweenMeasures (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL functions:
/ ST_Locate_Along_Measure(BLOBencoded geometry, Double m_value)
/ ST_Locate_Between_Measures(BLOBencoded geometry, Double m_start, Double m_end)
/
/ Extracts from a GEOMETRY (supporting M) any Point/Linestring
/ matching the range of measures
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    double m_start;
    double m_end;
    int intval;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	m_start = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  intval = sqlite3_value_int (argv[1]);
	  m_start = intval;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc > 2)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	      m_end = sqlite3_value_double (argv[2]);
	  else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		intval = sqlite3_value_int (argv[2]);
		m_end = intval;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    else
	m_end = m_start;
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaLocateBetweenMeasures (geo, m_start, m_end);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

#ifndef OMIT_PROJ		/* including PROJ.4 */

static void
fnct_Transform (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Transform(BLOBencoded geometry, srid)
/
/ returns a new geometry that is the original one received, but with the new SRID [no coordinates translation is applied]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    int srid_from;
    int srid_to;
    char *proj_from;
    char *proj_to;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	srid_to = sqlite3_value_int (argv[1]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  srid_from = geo->Srid;
	  getProjParams (sqlite, srid_from, &proj_from);
	  getProjParams (sqlite, srid_to, &proj_to);
	  if (proj_to == NULL || proj_from == NULL)
	    {
		if (proj_from)
		    free (proj_from);
		if (proj_to)
		    free (proj_to);
		gaiaFreeGeomColl (geo);
		sqlite3_result_null (context);
		return;
	    }
	  result = gaiaTransform (geo, proj_from, proj_to);
	  free (proj_from);
	  free (proj_to);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = srid_to;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

#endif /* end including PROJ.4 */

#ifndef OMIT_GEOS		/* including GEOS */

static void
fnct_Boundary (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Boundary(BLOB encoded geometry)
/
/ returns the combinatorial boundary for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr boundary;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (gaiaIsEmpty (geo))
	      sqlite3_result_null (context);
	  else
	    {
		boundary = gaiaBoundary (geo);
		if (!boundary)
		    sqlite3_result_null (context);
		else
		  {
		      gaiaToSpatiaLiteBlobWkb (boundary, &p_result, &len);
		      gaiaFreeGeomColl (boundary);
		      sqlite3_result_blob (context, p_result, len, free);
		  }
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_IsClosed (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ IsClosed(BLOB encoded LINESTRING or MULTILINESTRING geometry)
/
/ returns:
/ 1 if this LINESTRING is closed [or if this is a MULTILINESTRING and every LINESTRINGs are closed] 
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
      {
	  sqlite3_result_int (context, gaiaIsClosedGeom (geo));
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_IsSimple (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ IsSimple(BLOB encoded GEOMETRY)
/
/ returns:
/ 1 if this GEOMETRY is simple
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = gaiaIsSimple (geo);
	  if (ret < 0)
	      sqlite3_result_int (context, -1);
	  else
	      sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_IsRing (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ IsRing(BLOB encoded LINESTRING geometry)
/
/ returns:
/ 1 if this LINESTRING is a valid RING
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    gaiaGeomCollPtr geo = NULL;
    gaiaLinestringPtr line;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
      {
	  line = simpleLinestring (geo);
	  if (!line < 0)
	      sqlite3_result_int (context, -1);
	  else
	    {
		ret = gaiaIsRing (line);
		sqlite3_result_int (context, ret);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_IsValid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ IsValid(BLOB encoded GEOMETRY)
/
/ returns:
/ 1 if this GEOMETRY is a valid one
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = gaiaIsValid (geo);
	  if (ret < 0)
	      sqlite3_result_int (context, -1);
	  else
	      sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo);
}

static void
length_common (sqlite3_context * context, int argc, sqlite3_value ** argv,
	       int is_perimeter)
{
/* common implementation supporting both ST_Length and ST_Perimeter */
    unsigned char *p_blob;
    int n_bytes;
    double length = 0.0;
    int ret;
    int use_ellipsoid = -1;
    double a;
    double b;
    double rf;
    gaiaGeomCollPtr geo = NULL;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  use_ellipsoid = sqlite3_value_int (argv[1]);
	  if (use_ellipsoid != 0)
	      use_ellipsoid = 1;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (use_ellipsoid >= 0)
	    {
		/* attempting to identify the corresponding ellipsoid */
		if (getEllipsoidParams (sqlite, geo->Srid, &a, &b, &rf))
		  {
		      double l;
		      int ib;
		      gaiaLinestringPtr line;
		      gaiaPolygonPtr polyg;
		      gaiaRingPtr ring;
		      if (use_ellipsoid)
			{
			    /* measuring on the Ellipsoid */
			    if (!is_perimeter)
			      {
				  line = geo->FirstLinestring;
				  while (line)
				    {
					/* Linestrings */
					l = gaiaGeodesicTotalLength (a,
								     b,
								     rf,
								     line->DimensionModel,
								     line->
								     Coords,
								     line->
								     Points);
					if (l < 0.0)
					  {
					      length = -1.0;
					      break;
					  }
					length += l;
					line = line->Next;
				    }
			      }
			    if (length >= 0)
			      {
				  if (is_perimeter)
				    {
					/* Polygons */
					polyg = geo->FirstPolygon;
					while (polyg)
					  {
					      /* exterior Ring */
					      ring = polyg->Exterior;
					      l = gaiaGeodesicTotalLength (a, b,
									   rf,
									   ring->
									   DimensionModel,
									   ring->
									   Coords,
									   ring->
									   Points);
					      if (l < 0.0)
						{
						    length = -1.0;
						    break;
						}
					      length += l;
					      for (ib = 0;
						   ib <
						   polyg->NumInteriors; ib++)
						{
						    /* interior Rings */
						    ring =
							polyg->Interiors + ib;
						    l = gaiaGeodesicTotalLength
							(a, b, rf,
							 ring->DimensionModel,
							 ring->Coords,
							 ring->Points);
						    if (l < 0.0)
						      {
							  length = -1.0;
							  break;
						      }
						    length += l;
						}
					      if (length < 0.0)
						  break;
					      polyg = polyg->Next;
					  }
				    }
			      }
			}
		      else
			{
			    /* measuring on the Great Circle */
			    if (!is_perimeter)
			      {
				  line = geo->FirstLinestring;
				  while (line)
				    {
					/* Linestrings */
					length +=
					    gaiaGreatCircleTotalLength
					    (a, b, line->DimensionModel,
					     line->Coords, line->Points);
					line = line->Next;
				    }
			      }
			    if (length >= 0)
			      {
				  if (is_perimeter)
				    {
					/* Polygons */
					polyg = geo->FirstPolygon;
					while (polyg)
					  {
					      /* exterior Ring */
					      ring = polyg->Exterior;
					      length +=
						  gaiaGreatCircleTotalLength
						  (a, b,
						   ring->DimensionModel,
						   ring->Coords, ring->Points);
					      for (ib = 0;
						   ib <
						   polyg->NumInteriors; ib++)
						{
						    /* interior Rings */
						    ring =
							polyg->Interiors + ib;
						    length +=
							gaiaGreatCircleTotalLength
							(a, b,
							 ring->DimensionModel,
							 ring->Coords,
							 ring->Points);
						}
					      polyg = polyg->Next;
					  }
				    }
			      }
			}
		      if (length < 0.0)
			{
			    /* invalid distance */
			    sqlite3_result_null (context);
			}
		      else
			  sqlite3_result_double (context, length);
		  }
		else
		    sqlite3_result_null (context);
		goto stop;
	    }
	  ret = gaiaGeomCollLengthOrPerimeter (geo, is_perimeter, &length);
	  if (!ret)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, length);
      }
  stop:
    gaiaFreeGeomColl (geo);
}

static void
fnct_Length (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_Length(BLOB encoded GEOMETRYCOLLECTION)
/ ST_Length(BLOB encoded GEOMETRYCOLLECTION, Boolean use_ellipsoid)
/
/ returns  the total length for current geometry 
/ or NULL if any error is encountered
/
/ Please note: starting since 4.0.0 this function will ignore
/ any Polygon (only Linestrings will be considered)
/
*/
    length_common (context, argc, argv, 0);
}

static void
fnct_Perimeter (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_Perimeter(BLOB encoded GEOMETRYCOLLECTION)
/ ST_Perimeter(BLOB encoded GEOMETRYCOLLECTION, Boolean use_ellipsoid)
/
/ returns  the total perimeter length for current geometry 
/ or NULL if any error is encountered
/
/ Please note: starting since 4.0.0 this function will ignore
/ any Linestring (only Polygons will be considered)
/
*/
    length_common (context, argc, argv, 1);
}

static void
fnct_Area (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Area(BLOB encoded GEOMETRYCOLLECTION)
/
/ returns the total area for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double area = 0.0;
    int ret;
    int use_ellipsoid = -1;
#ifdef ENABLE_LWGEOM		/* only if LWGEOM is enabled */
    double a;
    double b;
    double rf;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
#endif /* end LWGEOM conditional */
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  use_ellipsoid = sqlite3_value_int (argv[1]);
	  if (use_ellipsoid != 0)
	      use_ellipsoid = 1;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (use_ellipsoid >= 0)
	    {
#ifdef ENABLE_LWGEOM		/* only if LWGEOM is enabled */
		/* attempting to identify the corresponding ellipsoid */
		if (getEllipsoidParams (sqlite, geo->Srid, &a, &b, &rf))
		    ret = gaiaGeodesicArea (geo, a, b, use_ellipsoid, &area);
		else
		    ret = 0;
#else
		ret = 0;
#endif /* end LWGEOM conditional */
	    }
	  else
	      ret = gaiaGeomCollArea (geo, &area);
	  if (!ret)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, area);
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Centroid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Centroid(BLOBencoded POLYGON or MULTIPOLYGON geometry)
/
/ returns a POINT representing the centroid for current POLYGON / MULTIPOLYGON geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    int ret;
    double x;
    double y;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (gaiaIsEmpty (geo))
	      sqlite3_result_null (context);
	  else
	    {
		ret = gaiaGeomCollCentroid (geo, &x, &y);
		if (!ret)
		    sqlite3_result_null (context);
		else
		  {
		      result = gaiaAllocGeomColl ();
		      result->Srid = geo->Srid;
		      gaiaAddPointToGeomColl (result, x, y);
		      gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		      gaiaFreeGeomColl (result);
		      sqlite3_result_blob (context, p_result, len, free);
		  }
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_PointOnSurface (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ PointOnSurface(BLOBencoded POLYGON or MULTIPOLYGON geometry)
/
/ returns a POINT guaranteed to lie on the Surface
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    double x;
    double y;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (!gaiaGetPointOnSurface (geo, &x, &y))
	      sqlite3_result_null (context);
	  else
	    {
		result = gaiaAllocGeomColl ();
		gaiaAddPointToGeomColl (result, x, y);
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		gaiaFreeGeomColl (result);
		sqlite3_result_blob (context, p_result, len, free);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Simplify (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Simplify(BLOBencoded geometry, tolerance)
/
/ returns a new geometry that is a caricature of the original one received, but simplified using the Douglas-Peuker algorihtm
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    int int_value;
    double tolerance;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	tolerance = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  tolerance = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeomCollSimplify (geo, tolerance);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_SimplifyPreserveTopology (sqlite3_context * context, int argc,
			       sqlite3_value ** argv)
{
/* SQL function:
/ SimplifyPreserveTopology(BLOBencoded geometry, tolerance)
/
/ returns a new geometry that is a caricature of the original one received, but simplified using the Douglas-Peuker algorihtm [preserving topology]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    int int_value;
    double tolerance;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	tolerance = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  tolerance = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeomCollSimplifyPreserveTopology (geo, tolerance);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ConvexHull (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ConvexHull(BLOBencoded geometry)
/
/ returns a new geometry representing the CONVEX HULL for current geometry
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int len;
    unsigned char *p_result = NULL;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaConvexHull (geo);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Buffer (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Buffer(BLOBencoded geometry, radius)
/
/ returns a new geometry representing the BUFFER for current geometry
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    double radius;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	radius = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  radius = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeomCollBuffer (geo, radius, 30);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Intersection (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Intersection(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns a new geometry representing the INTERSECTION of both geometries
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeometryIntersection (geo1, geo2);
	  if (!result)
	      sqlite3_result_null (context);
	  else if (gaiaIsEmpty (result))
	    {
		gaiaFreeGeomColl (result);
		sqlite3_result_null (context);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static int
gaia_union_polygs (gaiaGeomCollPtr geom)
{
/* testing if this geometry simply contains Polygons */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts || lns)
	return 0;
    if (!pgs)
	return 0;
    return 1;
}

static void
fnct_Union_step (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Union(BLOBencoded geom)
/
/ aggregate function - STEP
/
*/
    struct gaia_geom_chain *chain;
    struct gaia_geom_chain_item *item;
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    struct gaia_geom_chain **p;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geom)
	return;
    p = sqlite3_aggregate_context (context, sizeof (struct gaia_geom_chain **));
    if (!(*p))
      {
	  /* this is the first row */
	  chain = malloc (sizeof (struct gaia_geom_chain));
	  *p = chain;
	  item = malloc (sizeof (struct gaia_geom_chain_item));
	  item->geom = geom;
	  item->next = NULL;
	  chain->all_polygs = gaia_union_polygs (geom);
	  chain->first = item;
	  chain->last = item;
      }
    else
      {
	  /* subsequent rows */
	  chain = *p;
	  item = malloc (sizeof (struct gaia_geom_chain_item));
	  item->geom = geom;
	  item->next = NULL;
	  if (!gaia_union_polygs (geom))
	      chain->all_polygs = 0;
	  chain->last->next = item;
	  chain->last = item;
      }
}

static void
gaia_free_geom_chain (struct gaia_geom_chain *chain)
{
    struct gaia_geom_chain_item *p = chain->first;
    struct gaia_geom_chain_item *pn;
    while (p)
      {
	  pn = p->next;
	  gaiaFreeGeomColl (p->geom);
	  free (p);
	  p = pn;
      }
    free (chain);
}

static void
fnct_Union_final (sqlite3_context * context)
{
/* SQL function:
/ Union(BLOBencoded geom)
/
/ aggregate function - FINAL
/
*/
    gaiaGeomCollPtr tmp;
    struct gaia_geom_chain *chain;
    struct gaia_geom_chain_item *item;
    gaiaGeomCollPtr aggregate;
    gaiaGeomCollPtr result;
    struct gaia_geom_chain **p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    chain = *p;

#ifdef GEOS_ADVANCED
/* we can apply UnaryUnion */
    item = chain->first;
    while (item)
      {
	  gaiaGeomCollPtr geom = item->geom;
	  if (item == chain->first)
	    {
		/* initializing the aggregate geometry */
		aggregate = geom;
		item->geom = NULL;
		item = item->next;
		continue;
	    }
	  tmp = gaiaMergeGeometries (aggregate, geom);
	  gaiaFreeGeomColl (aggregate);
	  gaiaFreeGeomColl (geom);
	  item->geom = NULL;
	  aggregate = tmp;
	  item = item->next;
      }
    result = gaiaUnaryUnion (aggregate);
    gaiaFreeGeomColl (aggregate);
/* end UnaryUnion */
#else
/* old GEOS; no UnaryUnion available */
    if (chain->all_polygs)
      {
	  /* all Polygons: we can apply UnionCascaded */
	  item = chain->first;
	  while (item)
	    {
		gaiaGeomCollPtr geom = item->geom;
		if (item == chain->first)
		  {
		      /* initializing the aggregate geometry */
		      aggregate = geom;
		      item->geom = NULL;
		      item = item->next;
		      continue;
		  }
		tmp = gaiaMergeGeometries (aggregate, geom);
		gaiaFreeGeomColl (aggregate);
		gaiaFreeGeomColl (geom);
		item->geom = NULL;
		aggregate = tmp;
		item = item->next;
	    }
	  result = gaiaUnionCascaded (aggregate);
	  gaiaFreeGeomColl (aggregate);
      }
    else
      {
	  /* mixed types: the hardest/slowest way */
	  item = chain->first;
	  while (item)
	    {
		gaiaGeomCollPtr geom = item->geom;
		if (item == chain->first)
		  {
		      result = geom;
		      item->geom = NULL;
		      item = item->next;
		      continue;
		  }
		tmp = gaiaGeometryUnion (result, geom);
		gaiaFreeGeomColl (result);
		gaiaFreeGeomColl (geom);
		item->geom = NULL;
		result = tmp;
		item = item->next;
	    }
      }
/* end old GEOS: no UnaryUnion available */
#endif
    gaia_free_geom_chain (chain);

    if (result == NULL)
	sqlite3_result_null (context);
    else if (gaiaIsEmpty (result))
	sqlite3_result_null (context);
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
      }
    gaiaFreeGeomColl (result);
}

static void
fnct_Union (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Union(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns a new geometry representing the UNION of both geometries
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeometryUnion (geo1, geo2);
	  if (!result)
	      sqlite3_result_null (context);
	  else if (gaiaIsEmpty (result))
	    {
		gaiaFreeGeomColl (result);
		sqlite3_result_null (context);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Difference (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Difference(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns a new geometry representing the DIFFERENCE of both geometries
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeometryDifference (geo1, geo2);
	  if (!result)
	      sqlite3_result_null (context);
	  else if (gaiaIsEmpty (result))
	    {
		gaiaFreeGeomColl (result);
		sqlite3_result_null (context);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_SymDifference (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ SymDifference(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns a new geometry representing the SYMMETRIC DIFFERENCE of both geometries
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaGeometrySymDifference (geo1, geo2);
	  if (!result)
	      sqlite3_result_null (context);
	  else if (gaiaIsEmpty (result))
	    {
		gaiaFreeGeomColl (result);
		sqlite3_result_null (context);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Equals (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Equals(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if the two geometries are "spatially equal"
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = gaiaGeomCollEquals (geo1, geo2);
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Intersects (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Intersects(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if the two geometries do "spatially intersects"
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedIntersects (sqlite3_user_data (context),
						geo1, blob1, bytes1, geo2,
						blob2, bytes2);
#else
	  ret = gaiaGeomCollIntersects (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Disjoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Disjoint(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if the two geometries are "spatially disjoint"
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedDisjoint (sqlite3_user_data (context),
					      geo1, blob1, bytes1, geo2,
					      blob2, bytes2);
#else
	  ret = gaiaGeomCollDisjoint (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Overlaps (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Overlaps(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if the two geometries do "spatially overlaps"
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedOverlaps (sqlite3_user_data (context),
					      geo1, blob1, bytes1, geo2,
					      blob2, bytes2);
#else
	  ret = gaiaGeomCollOverlaps (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Crosses (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Crosses(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if the two geometries do "spatially crosses"
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedCrosses (sqlite3_user_data (context),
					     geo1, blob1, bytes1, geo2, blob2,
					     bytes2);
#else
	  ret = gaiaGeomCollCrosses (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Touches (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Touches(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if the two geometries do "spatially touches"
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedTouches (sqlite3_user_data (context),
					     geo1, blob1, bytes1, geo2, blob2,
					     bytes2);
#else
	  ret = gaiaGeomCollTouches (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Within (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Within(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if GEOM-1 is completely contained within GEOM-2
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedWithin (sqlite3_user_data (context), geo1,
					    blob1, bytes1, geo2, blob2, bytes2);
#else
	  ret = gaiaGeomCollWithin (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Contains (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Contains(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if GEOM-1 completely contains GEOM-2
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
	  ret = gaiaGeomCollPreparedContains (sqlite3_user_data (context),
					      geo1, blob1, bytes1, geo2,
					      blob2, bytes2);
#else
	  ret = gaiaGeomCollContains (geo1, geo2);
#endif /* end GEOS_ADVANCED */
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Relate (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Relate(BLOBencoded geom1, BLOBencoded geom2, string pattern)
/
/ returns:
/ 1 if GEOM-1 and GEOM-2 have a spatial relationship as specified by the patternMatrix 
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    const unsigned char *pattern;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[2]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    pattern = sqlite3_value_text (argv[2]);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = gaiaGeomCollRelate (geo1, geo2, (char *) pattern);
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Distance (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Distance(BLOBencoded geom1, BLOBencoded geom2)
/ Distance(BLOBencoded geom1, BLOBencoded geom2, Boolen use_ellipsoid)
/
/ returns the distance between GEOM-1 and GEOM-2
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    double dist;
    int use_ellipsoid = -1;
    double a;
    double b;
    double rf;
    int ret;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  use_ellipsoid = sqlite3_value_int (argv[2]);
	  if (use_ellipsoid != 0)
	      use_ellipsoid = 1;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  if (use_ellipsoid >= 0)
	    {
		/* attempting to identify the corresponding ellipsoid */
		if (getEllipsoidParams (sqlite, geo1->Srid, &a, &b, &rf))
		  {
#ifdef GEOS_ADVANCED
		      /* GEOS advanced features support is strictly required */
		      gaiaGeomCollPtr shortest = gaiaShortestLine (geo1, geo2);
		      if (shortest == NULL)
			  sqlite3_result_null (context);
		      else if (shortest->FirstLinestring == NULL)
			{
			    gaiaFreeGeomColl (shortest);
			    sqlite3_result_null (context);
			}
		      else
			{
			    /* computes the metric distance */
			    double x0;
			    double y0;
			    double x1;
			    double y1;
			    double z;
			    double m;
			    gaiaLinestringPtr ln = shortest->FirstLinestring;
			    dist = -1.0;
			    if (ln->Points == 2)
			      {
				  if (ln->DimensionModel == GAIA_XY_Z)
				    {
					gaiaGetPointXYZ (ln->Coords, 0,
							 &x0, &y0, &z);
				    }
				  else if (ln->DimensionModel == GAIA_XY_M)
				    {
					gaiaGetPointXYM (ln->Coords, 0,
							 &x0, &y0, &m);
				    }
				  else if (ln->DimensionModel == GAIA_XY_Z_M)
				    {
					gaiaGetPointXYZM (ln->Coords, 0,
							  &x0, &y0, &z, &m);
				    }
				  else
				    {
					gaiaGetPoint (ln->Coords, 0, &x0, &y0);
				    }
				  if (ln->DimensionModel == GAIA_XY_Z)
				    {
					gaiaGetPointXYZ (ln->Coords, 1,
							 &x1, &y1, &z);
				    }
				  else if (ln->DimensionModel == GAIA_XY_M)
				    {
					gaiaGetPointXYM (ln->Coords, 1,
							 &x1, &y1, &m);
				    }
				  else if (ln->DimensionModel == GAIA_XY_Z_M)
				    {
					gaiaGetPointXYZM (ln->Coords, 1,
							  &x1, &y1, &z, &m);
				    }
				  else
				    {
					gaiaGetPoint (ln->Coords, 1, &x1, &y1);
				    }
				  if (use_ellipsoid)
				      dist =
					  gaiaGeodesicDistance (a, b,
								rf, y0,
								x0, y1, x1);
				  else
				    {
					a = 6378137.0;
					rf = 298.257223563;
					b = (a * (1.0 - (1.0 / rf)));
					dist =
					    gaiaGreatCircleDistance (a,
								     b,
								     y0,
								     x0,
								     y1, x1);
				    }
				  if (dist < 0.0)
				    {
					/* invalid distance */
					sqlite3_result_null (context);
				    }
				  else
				      sqlite3_result_double (context, dist);
			      }
			    else
				sqlite3_result_null (context);
			    gaiaFreeGeomColl (shortest);
			}
#else
		      /* GEOS advanced features support unavailable */
		      sqlite3_result_null (context);
#endif
		  }
		else
		    sqlite3_result_null (context);
		goto stop;
	    }
	  ret = gaiaGeomCollDistance (geo1, geo2, &dist);
	  if (!ret)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, dist);
      }
  stop:
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_PtDistWithin (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ PtDistWithin(BLOBencoded geom1, BLOBencoded geom2, double dist 
/ [, boolen use_spheroid])
/
/ returns TRUE if the distance between GEOM-1 and GEOM-2
/ is less or equal to dist
/
/ - if both geom1 and geom2 are in the 4326 (WGS84) SRID,
/   (and does actually contains a single POINT each one)
/   dist is assumed to be measured in Meters
/ - in this case the optional arg use_spheroid is
/   checked to determine if geodesic distance has to be
/   computed on the sphere (quickest) or on the spheroid 
/   default: use_spheroid = FALSE
/ 
/ in any other case the "plain" distance is evaluated
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    double ref_dist;
    int use_spheroid = 0;
    double x0;
    double y0;
    double x1;
    double y1;
    int pt0 = 0;
    int ln0 = 0;
    int pg0 = 0;
    int pt1 = 0;
    int ln1 = 0;
    int pg1 = 0;
    double dist;
    double a;
    double b;
    double rf;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER
	|| sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 4)
      {
	  /* optional use_spheroid arg */
	  if (sqlite3_value_type (argv[3]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int dst = sqlite3_value_int (argv[2]);
	  ref_dist = dst;
      }
    else
	ref_dist = sqlite3_value_double (argv[2]);
    if (argc == 4)
	use_spheroid = sqlite3_value_int (argv[3]);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  if (geo1->Srid == 4326 && geo2->Srid == 4326)
	    {
		/* checking for single points */
		pt = geo1->FirstPoint;
		while (pt)
		  {
		      x0 = pt->X;
		      y0 = pt->Y;
		      pt0++;
		      pt = pt->Next;
		  }
		ln = geo1->FirstLinestring;
		while (ln)
		  {
		      ln0++;
		      ln = ln->Next;
		  }
		pg = geo1->FirstPolygon;
		while (pg)
		  {
		      pg0++;
		      pg = pg->Next;
		  }
		pt = geo2->FirstPoint;
		while (pt)
		  {
		      x1 = pt->X;
		      y1 = pt->Y;
		      pt1++;
		      pt = pt->Next;
		  }
		ln = geo2->FirstLinestring;
		while (ln)
		  {
		      ln1++;
		      ln = ln->Next;
		  }
		pg = geo2->FirstPolygon;
		while (pg)
		  {
		      pg1++;
		      pg = pg->Next;
		  }
		if (pt0 == 1 && pt1 == 1 && ln0 == 0 && ln1 == 0 && pg0 == 0
		    && pg1 == 0)
		  {
		      /* using geodesic distance */
		      a = 6378137.0;
		      rf = 298.257223563;
		      b = (a * (1.0 - (1.0 / rf)));
		      if (use_spheroid)
			{
			    dist =
				gaiaGeodesicDistance (a, b, rf, y0, x0, y1, x1);
			    if (dist <= ref_dist)
				sqlite3_result_int (context, 1);
			    else
				sqlite3_result_int (context, 0);
			}
		      else
			{
			    dist =
				gaiaGreatCircleDistance (a, b, y0, x0, y1, x1);
			    if (dist <= ref_dist)
				sqlite3_result_int (context, 1);
			    else
				sqlite3_result_int (context, 0);
			}
		      goto stop;
		  }
	    }
/* defaulting to flat distance */
	  ret = gaiaGeomCollDistance (geo1, geo2, &dist);
	  if (!ret)
	      sqlite3_result_null (context);
	  if (dist <= ref_dist)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
      }
  stop:
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

SPATIALITE_PRIVATE void
geos_error (const char *fmt, ...)
{
/* reporting some GEOS error */
    va_list ap;
    char *msg;
    va_start (ap, fmt);
    msg = sqlite3_vmprintf (fmt, ap);
    va_end (ap);
    if (msg)
      {
	  spatialite_e ("GEOS error: %s\n", msg);
	  gaiaSetGeosErrorMsg (msg);
	  sqlite3_free (msg);
      }
    else
	gaiaSetGeosErrorMsg (NULL);
}


SPATIALITE_PRIVATE void
geos_warning (const char *fmt, ...)
{
/* reporting some GEOS warning */
    va_list ap;
    char *msg;
    va_start (ap, fmt);
    msg = sqlite3_vmprintf (fmt, ap);
    va_end (ap);
    if (msg)
      {
	  spatialite_e ("GEOS warning: %s\n", msg);
	  gaiaSetGeosWarningMsg (msg);
	  sqlite3_free (msg);
      }
    else
	gaiaSetGeosWarningMsg (NULL);
}

static void
fnct_aux_polygonize (sqlite3_context * context, gaiaGeomCollPtr geom_org,
		     int force_multipolygon, int allow_multipolygon)
{
/* a  common function performing any kind of polygonization op */
    gaiaGeomCollPtr geom_new = NULL;
    int len;
    unsigned char *p_result = NULL;
    gaiaPolygonPtr pg;
    int pgs = 0;
    if (!geom_org)
	goto invalid;
    geom_new = gaiaPolygonize (geom_org, force_multipolygon);
    if (!geom_new)
	goto invalid;
    gaiaFreeGeomColl (geom_org);
    pg = geom_new->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pgs > 1 && allow_multipolygon == 0)
      {
	  /* invalid: a POLYGON is expected !!! */
	  gaiaFreeGeomColl (geom_new);
	  sqlite3_result_null (context);
	  return;
      }
    gaiaToSpatiaLiteBlobWkb (geom_new, &p_result, &len);
    gaiaFreeGeomColl (geom_new);
    sqlite3_result_blob (context, p_result, len, free);
    return;
  invalid:
    if (geom_org)
	gaiaFreeGeomColl (geom_org);
    sqlite3_result_null (context);
}

/*
/ the following functions performs initial argument checking, 
/ and then readdressing the request to fnct_aux_polygonize()
/ for actual processing
*/

static void
fnct_BdPolyFromText1 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ BdPolyFromText(WKT encoded MULTILINESTRING)
/
/ returns the current geometry [POLYGON] by parsing a WKT encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, -1);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = 0;
    fnct_aux_polygonize (context, geo, 0, 0);
    return;
}

static void
fnct_BdPolyFromText2 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ BdPolyFromText(WKT encoded MULTILINESTRING, SRID)
/
/ returns the current geometry [POLYGON] by parsing a WKT encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, -1);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    fnct_aux_polygonize (context, geo, 0, 0);
    return;
}

static void
fnct_BdMPolyFromText1 (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ BdMPolyFromText(WKT encoded MULTILINESTRING)
/
/ returns the current geometry [MULTIPOLYGON] by parsing a WKT encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, -1);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = 0;
    fnct_aux_polygonize (context, geo, 1, 1);
    return;
}

static void
fnct_BdMPolyFromText2 (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ BdMPolyFromText(WKT encoded MULTILINESTRING, SRID)
/
/ returns the current geometry [MULTIPOLYGON] by parsing a WKT encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    const unsigned char *text;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    text = sqlite3_value_text (argv[0]);
    geo = gaiaParseWkt (text, -1);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    fnct_aux_polygonize (context, geo, 1, 1);
    return;
}

static void
fnct_BdPolyFromWKB1 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BdPolyFromWKB(WKB encoded MULTILINESTRING)
/
/ returns the current geometry [POLYGON] by parsing a WKB encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    int n_bytes;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, -1))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = 0;
    fnct_aux_polygonize (context, geo, 0, 0);
    return;
}

static void
fnct_BdPolyFromWKB2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BdPolyFromWKB(WKB encoded MULTILINESTRING)
/
/ returns the current geometry [POLYGON] by parsing a WKB encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    int n_bytes;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, -1))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    fnct_aux_polygonize (context, geo, 0, 0);
    return;
}

static void
fnct_BdMPolyFromWKB1 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ BdMPolyFromWKB(WKB encoded MULTILINESTRING)
/
/ returns the current geometry [MULTIPOLYGON] by parsing a WKB encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    int n_bytes;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, -1))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = 0;
    fnct_aux_polygonize (context, geo, 1, 1);
    return;
}

static void
fnct_BdMPolyFromWKB2 (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ BdMPolyFromWKB(WKB encoded MULTILINESTRING)
/
/ returns the current geometry [MULTIPOLYGON] by parsing a WKB encoded MULTILINESTRING 
/ or NULL if any error is encountered
/
*/
    int n_bytes;
    const unsigned char *wkb;
    gaiaGeomCollPtr geo = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
      {
	  sqlite3_result_null (context);
	  return;
      }
    wkb = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (!check_wkb (wkb, n_bytes, -1))
	return;
    geo = gaiaFromWkb (wkb, n_bytes);
    if (geo == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geo->DeclaredType != GAIA_MULTILINESTRING)
      {
	  gaiaFreeGeomColl (geo);
	  sqlite3_result_null (context);
	  return;
      }
    geo->Srid = sqlite3_value_int (argv[1]);
    fnct_aux_polygonize (context, geo, 1, 1);
    return;
}

#ifdef GEOS_ADVANCED		/* GEOS advanced features */

static int
check_topo_table (sqlite3 * sqlite, const char *table, int is_view)
{
/* checking if some Topology-related table/view already exists */
    int exists = 0;
    char *sql_statement;
    char *errMsg = NULL;
    int ret;
    char **results;
    int rows;
    int columns;
    int i;
    sql_statement =
	sqlite3_mprintf ("SELECT name FROM sqlite_master WHERE type = '%s'"
			 "AND Upper(name) = Upper(%Q)",
			 (!is_view) ? "table" : "view", table);
    ret = sqlite3_get_table (sqlite, sql_statement, &results, &rows, &columns,
			     &errMsg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  sqlite3_free (errMsg);
	  return 0;
      }
    for (i = 1; i <= rows; i++)
	exists = 1;
    sqlite3_free_table (results);
    return exists;
}

static int
create_topo_nodes (sqlite3 * sqlite, const char *table, int srid, int dims)
{
/* creating the topo_nodes table */
    char *sql_statement;
    char *sqltable;
    char *idx_name;
    char *xidx_name;
    int ret;
    char *err_msg = NULL;
    sqltable = gaiaDoubleQuotedSql (table);
    sql_statement = sqlite3_mprintf ("CREATE TABLE \"%s\" (\n"
				     "node_id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
				     "node_code TEXT)", sqltable);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sql_statement =
	sqlite3_mprintf
	("SELECT AddGeometryColumn(%Q, 'Geometry', %d, 'POINT', '%s', 1)",
	 table, srid, (dims == GAIA_XY_Z) ? "XYZ" : "XY");
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("AddGeometryColumn '%s'.'Geometry' error: %s\n",
			table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sql_statement =
	sqlite3_mprintf ("SELECT CreateSpatialIndex(%Q, 'Geometry')", table);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CreateSpatialIndex '%s'.'Geometry' error: %s\n",
			table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_code", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (node_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('node_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_topo_edges (sqlite3 * sqlite, const char *table, int srid, int dims)
{
/* creating the topo_edges table */
    char *sql_statement;
    char *sqltable;
    char *idx_name;
    char *xidx_name;
    int ret;
    char *err_msg = NULL;
    sqltable = gaiaDoubleQuotedSql (table);
    sql_statement = sqlite3_mprintf ("CREATE TABLE \"%s\" (\n"
				     "edge_id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
				     "node_from_code TEXT,\n"
				     "node_to_code TEXT,\n"
				     "edge_code TEXT)", sqltable);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sql_statement =
	sqlite3_mprintf
	("SELECT AddGeometryColumn(%Q, 'Geometry', %d, 'LINESTRING', '%s', 1)",
	 table, srid, (dims == GAIA_XY_Z) ? "XYZ" : "XY");
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("AddGeometryColumn '%s'.'Geometry' error: %s\n",
			table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sql_statement =
	sqlite3_mprintf ("SELECT CreateSpatialIndex(%Q, 'Geometry')", table);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CreateSpatialIndex '%s'.'Geometry' error: %s\n",
			table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_code", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (edge_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('edge_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_from", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (node_from_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('node_from_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_to", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (node_to_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('node_to_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_topo_faces (sqlite3 * sqlite, const char *table)
{
/* creating the topo_faces table */
    char *sql_statement;
    char *sqltable;
    char *idx_name;
    char *xidx_name;
    int ret;
    char *err_msg = NULL;
    sqltable = gaiaDoubleQuotedSql (table);
    sql_statement = sqlite3_mprintf ("CREATE TABLE \"%s\" (\n"
				     "face_id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
				     "face_code TEXT)", sqltable);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_code", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (face_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('face_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_topo_faces_edges (sqlite3 * sqlite, const char *table,
			 const char *table2)
{
/* creating the topo_faces_edges table */
    char *sql_statement;
    char *sqltable;
    char *sqltable2;
    char *idx_name;
    char *xidx_name;
    int ret;
    char *err_msg = NULL;
    sqltable = gaiaDoubleQuotedSql (table);
    sqltable2 = gaiaDoubleQuotedSql (table2);
    sql_statement = sqlite3_mprintf ("CREATE TABLE \"%s\" (\n"
				     "face_id INTEGER NOT NULL,\n"
				     "edge_code TEXT NOT NULL,\n"
				     "orientation TEXT,\n"
				     "CONSTRAINT pk_faces_edges PRIMARY KEY "
				     "(face_id, edge_code),\n"
				     "CONSTRAINT fk_faces_edges FOREIGN KEY "
				     "(face_id) REFERENCES \"%s\" (face_id))\n",
				     sqltable, sqltable2);
    free (sqltable);
    free (sqltable2);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_edge", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (edge_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('edge_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_topo_curves (sqlite3 * sqlite, const char *table)
{
/* creating the topo_curves table */
    char *sql_statement;
    char *sqltable;
    char *idx_name;
    char *xidx_name;
    int ret;
    char *err_msg = NULL;
    sqltable = gaiaDoubleQuotedSql (table);
    sql_statement = sqlite3_mprintf ("CREATE TABLE \"%s\" (\n"
				     "curve_id INTEGER NOT NULL,\n"
				     "edge_code TEXT NOT NULL,\n"
				     "orientation TEXT,\n"
				     "CONSTRAINT pk_curves PRIMARY KEY "
				     "(curve_id, edge_code))\n", sqltable);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_edge", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (edge_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('edge_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_topo_surfaces (sqlite3 * sqlite, const char *table)
{
/* creating the topo_surfaces table */
    char *sql_statement;
    char *sqltable;
    char *idx_name;
    char *xidx_name;
    int ret;
    char *err_msg = NULL;
    sqltable = gaiaDoubleQuotedSql (table);
    sql_statement = sqlite3_mprintf ("CREATE TABLE \"%s\" (\n"
				     "surface_id INTEGER NOT NULL,\n"
				     "face_code TEXT NOT NULL,\n"
				     "orientation TEXT,\n"
				     "CONSTRAINT pk_surfaces PRIMARY KEY "
				     "(surface_id, face_code))", sqltable);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    sqltable = gaiaDoubleQuotedSql (table);
    idx_name = sqlite3_mprintf ("idx_%s_face", table);
    xidx_name = gaiaDoubleQuotedSql (idx_name);
    sqlite3_free (idx_name);
    sql_statement =
	sqlite3_mprintf ("CREATE INDEX \"%s\" ON \"%s\" (face_code)",
			 xidx_name, sqltable);
    free (sqltable);
    free (xidx_name);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("Create Index '%s'('face_code') error: %s\n",
			sqltable, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_node_codes (sqlite3 * sqlite, const char *view,
			 const char *table_nodes)
{
/* creating the check node codes VIEW */
    char *sql_statement;
    char *sqltable;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sqltable = gaiaDoubleQuotedSql (table_nodes);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT node_code AS node_code, Count(node_id) AS count\n"
				     "FROM \"%s\"\nGROUP BY node_code\nHAVING count > 1\n",
				     sqlview, sqltable);
    free (sqlview);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_node_geoms (sqlite3 * sqlite, const char *view,
			 const char *table_nodes)
{
/* creating the check node geoms VIEW */
    char *sql_statement;
    char *sqltable;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sqltable = gaiaDoubleQuotedSql (table_nodes);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT n1.node_id AS node1_id, n1.node_code AS node1_code, "
				     "n2.node_id AS node2_id, n2.node_code AS node2_code\n"
				     "FROM \"%s\" AS n1\nJOIN \"%s\" AS n2 ON (\n"
				     "  n1.node_id <> n2.node_id AND\n"
				     "  ST_Equals(n1.Geometry, n2.Geometry) = 1 AND\n"
				     "  n2.node_id IN (\n	SELECT ROWID FROM SpatialIndex\n"
				     "  WHERE f_table_name = %Q AND\n  search_frame = n1.Geometry))\n",
				     sqlview, sqltable, sqltable, table_nodes);
    free (sqlview);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_edge_codes (sqlite3 * sqlite, const char *view,
			 const char *table_edges)
{
/* creating the check edge codes VIEW */
    char *sql_statement;
    char *sqltable;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sqltable = gaiaDoubleQuotedSql (table_edges);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT edge_code AS edge_code, Count(edge_id) AS count\n"
				     "FROM \"%s\"\nGROUP BY edge_code\nHAVING count > 1\n",
				     sqlview, sqltable);
    free (sqlview);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_edge_geoms (sqlite3 * sqlite, const char *view,
			 const char *table_edges)
{
/* creating the check edge geoms VIEW */
    char *sql_statement;
    char *sqltable;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sqltable = gaiaDoubleQuotedSql (table_edges);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT e1.edge_id AS edge1_id, e1.edge_code AS edge1_code, "
				     "e2.edge_id AS edge2_id, e2.edge_code AS edge2_code\n"
				     "FROM \"%s\" AS e1\nJOIN \"%s\" AS e2 ON (\n  e1.edge_id <> e2.edge_id AND\n"
				     "NOT (e1.node_from_code = e2.node_from_code "
				     "AND e1.node_to_code = e2.node_to_code) AND\n"
				     "  ST_Crosses(e1.Geometry, e2.Geometry) = 1 AND\n"
				     "  e2.edge_id IN (\n"
				     "    SELECT ROWID FROM SpatialIndex\n"
				     "	   WHERE f_table_name = %Q AND\n        search_frame = e1.Geometry))\n",
				     sqlview, sqltable, sqltable, table_edges);
    free (sqlview);
    free (sqltable);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_edge_node_geoms (sqlite3 * sqlite, const char *view,
			      const char *table_edges, const char *table_nodes)
{
/* creating the check edge/node geoms VIEW */
    char *sql_statement;
    char *sql_edges;
    char *sql_nodes;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_edges = gaiaDoubleQuotedSql (table_edges);
    sql_nodes = gaiaDoubleQuotedSql (table_nodes);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT e.edge_id AS edge_id, n.node_id AS node_id\n"
				     "FROM \"%s\" AS e,\n\"%s\" AS n\n"
				     "WHERE ST_Intersects(e.Geometry, n.Geometry)\n"
				     "  AND ST_Equals(ST_StartPoint(e.Geometry), n.Geometry) = 0\n"
				     "  AND ST_Equals(ST_EndPoint(e.Geometry), n.Geometry) = 0\n"
				     "  AND n.ROWID IN (\n    SELECT ROWID FROM SpatialIndex\n"
				     "  WHERE f_table_name = %Q\n      AND search_frame = e.Geometry);",
				     sqlview, sql_edges, sql_nodes,
				     table_nodes);
    free (sqlview);
    free (sql_nodes);
    free (sql_edges);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_face_codes (sqlite3 * sqlite, const char *view,
			 const char *table_faces)
{
/* creating the check face codes VIEW */
    char *sql_statement;
    char *sqltable;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sqltable = gaiaDoubleQuotedSql (table_faces);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT face_code AS face_code, Count(face_id) AS count\n"
				     "FROM \"%s\"\nGROUP BY face_code\nHAVING count > 1\n",
				     sqlview, sqltable);
    free (sqltable);
    free (sqlview);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_faces_resolved (sqlite3 * sqlite, const char *view, const char *faces,
		       const char *faces_edges, const char *edges)
{
/* creating the Faces Resolved VIEW */
    char *sql_statement;
    char *sql_faces;
    char *sql_faces_edges;
    char *sql_edges;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_faces = gaiaDoubleQuotedSql (faces);
    sql_faces_edges = gaiaDoubleQuotedSql (faces_edges);
    sql_edges = gaiaDoubleQuotedSql (edges);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT f.face_id AS face_id, f.face_code AS face_code, "
				     "ST_Polygonize(e.Geometry) AS Geometry\n"
				     "FROM \"%s\" AS f\nLEFT JOIN \"%s\" AS fe ON (fe.face_id = f.face_id)\n"
				     "LEFT JOIN \"%s\" AS e ON (e.edge_code = fe.edge_code)\n"
				     "GROUP BY f.face_id\n", sqlview,
				     sql_faces, sql_faces_edges, sql_edges);
    free (sqlview);
    free (sql_faces);
    free (sql_faces_edges);
    free (sql_edges);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_curves_resolved (sqlite3 * sqlite, const char *view,
			const char *curves, char *edges)
{
/* creating the Curves Resolved VIEW */
    char *sql_statement;
    char *sql_curves;
    char *sql_edges;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_curves = gaiaDoubleQuotedSql (curves);
    sql_edges = gaiaDoubleQuotedSql (edges);
    sql_statement =
	sqlite3_mprintf
	("CREATE VIEW \"%s\" AS\nSELECT c.curve_id AS curve_id, "
	 "CastToMultiLinestring(ST_Collect(e.Geometry)) AS Geometry\n"
	 "FROM \"%s\" AS c\nLEFT JOIN \"%s\" AS e ON (e.edge_code = c.edge_code)\n"
	 "GROUP BY c.curve_id\n", sqlview, sql_curves, sql_edges);
    free (sqlview);
    free (sql_edges);
    free (sql_curves);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_surfaces_resolved (sqlite3 * sqlite, const char *view,
			  const char *surfaces, const char *faces)
{
/* creating the Surfaces Resolved VIEW */
    char *sql_statement;
    char *sql_surfaces;
    char *sql_faces;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_surfaces = gaiaDoubleQuotedSql (surfaces);
    sql_faces = gaiaDoubleQuotedSql (faces);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT s.surface_id AS surface_id,\n"
				     "  CastToMultipolygon(ST_UnaryUnion(ST_Collect(f.Geometry))) AS Geometry\n"
				     "FROM \"%s\" AS s\n"
				     "LEFT JOIN \"%s\" AS f ON (f.face_code = s.face_code)\n"
				     "GROUP BY s.surface_id\n", sqlview,
				     sql_surfaces, sql_faces);
    free (sqlview);
    free (sql_surfaces);
    free (sql_faces);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_dangling_nodes (sqlite3 * sqlite, const char *view,
		       const char *nodes, const char *edges)
{
/* creating the Dangling Nodes VIEW */
    char *sql_statement;
    char *sql_nodes;
    char *sql_edges;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_nodes = gaiaDoubleQuotedSql (nodes);
    sql_edges = gaiaDoubleQuotedSql (edges);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT n.node_id AS node_id\nFROM \"%s\" AS n\n"
				     "LEFT JOIN \"%s\" AS e ON (n.node_code = e.node_from_code)\n"
				     "WHERE e.edge_id IS NULL\nINTERSECT\nSELECT n.node_id AS node_id\n"
				     "FROM \"%s\" AS n\nLEFT JOIN \"%s\" AS e ON (n.node_code = e.node_to_code)\n"
				     "WHERE e.edge_id IS NULL\n", sqlview,
				     sql_nodes, sql_edges, sql_nodes,
				     sql_edges);
    free (sqlview);
    free (sql_nodes);
    free (sql_edges);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_dangling_edges (sqlite3 * sqlite, const char *view,
		       const char *edges, const char *faces_edges,
		       const char *curves)
{
/* creating the Dangling Edges VIEW */
    char *sql_statement;
    char *sql_edges;
    char *sql_faces_edges;
    char *sql_curves;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_edges = gaiaDoubleQuotedSql (edges);
    sql_faces_edges = gaiaDoubleQuotedSql (faces_edges);
    sql_curves = gaiaDoubleQuotedSql (curves);
    sql_statement = sqlite3_mprintf ("CREATE VIEW \"%s\" AS\n"
				     "SELECT e.edge_id AS edge_id\nFROM \"%s\" AS e\n"
				     "LEFT JOIN \"%s\" AS f ON (e.edge_code = f.edge_code)\n"
				     "WHERE f.edge_code IS NULL\nINTERSECT\nSELECT e.edge_id AS edge_id\n"
				     "FROM \"%s\" AS e\nLEFT JOIN \"%s\" AS c ON (e.edge_code = c.edge_code)\n"
				     "WHERE c.edge_code IS NULL\n", sqlview,
				     sql_edges, sql_faces_edges, sql_edges,
				     sql_curves);
    free (sqlview);
    free (sql_edges);
    free (sql_faces_edges);
    free (sql_curves);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_check_edges_from_to (sqlite3 * sqlite, const char *view,
			    const char *edges, const char *nodes)
{
/* creating the Edges/Nodes [from/to] VIEW */
    char skeleton[2048];
    char *sql_statement;
    char *sql_edges;
    char *sql_nodes;
    char *sqlview;
    int ret;
    char *err_msg = NULL;
    sqlview = gaiaDoubleQuotedSql (view);
    sql_edges = gaiaDoubleQuotedSql (edges);
    sql_nodes = gaiaDoubleQuotedSql (nodes);
    strcpy (skeleton, "CREATE VIEW \"%s\" AS\n");
    strcat (skeleton, "SELECT e.edge_id AS edge_id, n.node_id AS node_id,\n");
    strcat (skeleton, "  n.node_code AS node_code,\n");
    strcat (skeleton, "'Mismatching coords' AS error_cause\n");
    strcat (skeleton, "FROM \"%s\" AS e\n");
    strcat (skeleton, "JOIN \"%s\" AS n ON ");
    strcat (skeleton, "(e.node_from_code = n.node_code)\n");
    strcat (skeleton,
	    "WHERE ST_Equals(ST_StartPoint(e.Geometry), n.Geometry) = 0\n");
    strcat (skeleton, "UNION\n");
    strcat (skeleton, "SELECT e.edge_id AS edge_id, n.node_id AS node_id,\n");
    strcat (skeleton, "  n.node_code AS node_code,\n");
    strcat (skeleton, " 'Mismatching coords' AS error_cause\n");
    strcat (skeleton, "FROM \"%s\" AS e\n");
    strcat (skeleton, "JOIN \"%s\" AS n ON ");
    strcat (skeleton, "(e.node_to_code = n.node_code)\n");
    strcat (skeleton,
	    "WHERE ST_Equals(ST_EndPoint(e.Geometry), n.Geometry) = 0\n");
    strcat (skeleton, "UNION\n");
    strcat (skeleton, "SELECT e.edge_id AS edge_id, n.node_id AS node_id,\n");
    strcat (skeleton, "  n.node_code AS node_code,\n");
    strcat (skeleton, "  'Unresolved Node reference' AS error_cause\n");
    strcat (skeleton, "FROM \"%s\" AS e\n");
    strcat (skeleton, "LEFT JOIN \"%s\" AS n ON ");
    strcat (skeleton, "(e.node_from_code = n.node_code)\n");
    strcat (skeleton, "WHERE n.node_id IS NULL\n");
    strcat (skeleton, "UNION\n");
    strcat (skeleton, "SELECT e.edge_id AS edge_id, n.node_id AS node_id,\n");
    strcat (skeleton, "  n.node_code AS node_code,\n");
    strcat (skeleton, "  'Unresolved Node reference' AS error_cause\n");
    strcat (skeleton, "FROM \"%s\" AS e\n");
    strcat (skeleton, "LEFT JOIN \"%s\" AS n ON ");
    strcat (skeleton, "(e.node_to_code = n.node_code)\n");
    strcat (skeleton, "WHERE n.node_id IS NULL\n");
    sql_statement = sqlite3_mprintf (skeleton, sqlview,
				     sql_edges, sql_nodes, sql_edges,
				     sql_nodes, sql_edges, sql_nodes,
				     sql_edges, sql_nodes);
    free (sqlview);
    free (sql_edges);
    free (sql_nodes);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE VIEW '%s' error: %s\n", view, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_topo_master (sqlite3 * sqlite)
{
/* creating the topo_master table */
    char sql[2048];
    int ret;
    char *err_msg = NULL;

/* creating the table */
    strcpy (sql, "CREATE TABLE topology_master (\n");
    strcat (sql, "nodes TEXT NOT NULL,\n");
    strcat (sql, "edges TEXT NOT NULL,\n");
    strcat (sql, "faces TEXT NOT NULL,\n");
    strcat (sql, "faces_edges TEXT NOT NULL,\n");
    strcat (sql, "curves TEXT NOT NULL,\n");
    strcat (sql, "surfaces TEXT NOT NULL,\n");
    strcat (sql, "check_node_ids TEXT NOT NULL,\n");
    strcat (sql, "check_node_geoms TEXT NOT NULL,\n");
    strcat (sql, "check_edge_ids TEXT NOT NULL,\n");
    strcat (sql, "check_edge_geoms TEXT NOT NULL,\n");
    strcat (sql, "check_edge_node_geoms TEXT NOT NULL,\n");
    strcat (sql, "check_face_ids TEXT NOT NULL,\n");
    strcat (sql, "faces_resolved TEXT NOT NULL,\n");
    strcat (sql, "curves_resolved TEXT NOT NULL,\n");
    strcat (sql, "surfaces_resolved TEXT NOT NULL,\n");
    strcat (sql, "dangling_nodes TEXT NOT NULL,\n");
    strcat (sql, "dangling_edges TEXT NOT NULL,\n");
    strcat (sql, "check_edges_from_to TEXT NOT NULL,\n");
    strcat (sql, "coord_dimension TEXT NOT NULL,\n");
    strcat (sql, "srid INTEGER NOT NULL,\n");
    strcat (sql, "CONSTRAINT fk_topo_master FOREIGN KEY \n");
    strcat (sql, "(srid) REFERENCES spatial_ref_sys (srid))");
    ret = sqlite3_exec (sqlite, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("CREATE TABLE 'topology_master' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
update_topo_master (sqlite3 * sqlite, const char *nodes, const char *edges,
		    const char *faces, const char *faces_edges,
		    const char *curves, const char *surfaces,
		    const char *check_nodes, const char *check_node_geoms,
		    const char *check_edges, const char *check_edge_geoms,
		    const char *check_edge_node_geoms,
		    const char *check_faces, const char *faces_res,
		    const char *curves_res, const char *surfaces_res,
		    const char *dangling_nodes, const char *dangling_edges,
		    const char *check_edges_from_to, int srid, int dims)
{
/* updating the topo_master table */
    char *sql_statement;
    int ret;
    char *err_msg = NULL;

/* inserting Topology data into MASTER */
    sql_statement = sqlite3_mprintf ("INSERT INTO topology_master "
				     "(nodes, edges, faces, faces_edges, curves, surfaces, check_node_ids, "
				     "check_node_geoms, check_edge_ids, check_edge_geoms, check_edge_node_geoms, "
				     "check_face_ids, faces_resolved, curves_resolved, surfaces_resolved, "
				     "dangling_nodes, dangling_edges, check_edges_from_to, coord_dimension, srid) "
				     "VALUES (%Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %d)",
				     nodes, edges, faces, faces_edges, curves,
				     surfaces, check_nodes, check_node_geoms,
				     check_edges, check_edge_geoms,
				     check_edge_node_geoms, check_faces,
				     faces_res, curves_res, surfaces_res,
				     dangling_nodes, dangling_edges,
				     check_edges_from_to,
				     (dims == GAIA_XY_Z) ? "XYZ" : "XY", srid);
    ret = sqlite3_exec (sqlite, sql_statement, NULL, NULL, &err_msg);
    sqlite3_free (sql_statement);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("INSERT INTO 'topology_master' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static void
fnct_CreateTopologyTables (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ CreateTopologyTables(srid, coord_dims)
/  or
/ CreateTopologyTables(prefix, srid, coord_dims)
/
/ creates any Topology related table 
/ returns 1 on success
/ 0 on failure
*/
    const char *prefix = "topo_";
    const unsigned char *txt_dims;
    int srid = -1;
    int dimension;
    int dims = -1;
    char *table_curves;
    char *table_surfaces;
    char *table_nodes;
    char *table_edges;
    char *table_faces;
    char *table_faces_edges;
    char *view_check_node_codes;
    char *view_check_node_geoms;
    char *view_check_edge_codes;
    char *view_check_edge_geoms;
    char *view_check_edge_node_geoms;
    char *view_check_face_codes;
    char *view_faces_resolved;
    char *view_curves_resolved;
    char *view_surfaces_resolved;
    char *view_dangling_nodes;
    char *view_dangling_edges;
    char *view_edges_check_from_to;
    const char *tables[20];
    int views[20];
    int *p_view;
    const char **p_tbl;
    int ok_table;
    int create_master = 1;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
	    {
		spatialite_e
		    ("CreateTopologyTables() error: argument 1 [table_prefix] is not of the String type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  prefix = (char *) sqlite3_value_text (argv[0]);
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		spatialite_e
		    ("CreateTopologyTables() error: argument 2 [SRID] is not of the Integer type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  srid = sqlite3_value_int (argv[1]);
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		dimension = sqlite3_value_int (argv[2]);
		if (dimension == 2)
		    dims = GAIA_XY;
		if (dimension == 3)
		    dims = GAIA_XY_Z;
	    }
	  else if (sqlite3_value_type (argv[2]) == SQLITE_TEXT)
	    {
		txt_dims = sqlite3_value_text (argv[2]);
		if (strcasecmp ((char *) txt_dims, "XY") == 0)
		    dims = GAIA_XY;
		if (strcasecmp ((char *) txt_dims, "XYZ") == 0)
		    dims = GAIA_XY_Z;
	    }
	  else
	    {
		spatialite_e
		    ("CreateTopologyTables() error: argument 3 [dimension] is not of the Integer or Text type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
      }
    else
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_INTEGER)
	    {
		spatialite_e
		    ("CreateTopologyTables() error: argument 1 [SRID] is not of the Integer type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
	  srid = sqlite3_value_int (argv[0]);
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		dimension = sqlite3_value_int (argv[1]);
		if (dimension == 2)
		    dims = GAIA_XY;
		if (dimension == 3)
		    dims = GAIA_XY_Z;
	    }
	  else if (sqlite3_value_type (argv[1]) == SQLITE_TEXT)
	    {
		txt_dims = sqlite3_value_text (argv[1]);
		if (strcasecmp ((char *) txt_dims, "XY") == 0)
		    dims = GAIA_XY;
		if (strcasecmp ((char *) txt_dims, "XYZ") == 0)
		    dims = GAIA_XY_Z;
	    }
	  else
	    {
		spatialite_e
		    ("CreateTopologyTables() error: argument 2 [dimension] is not of the Integer or Text type\n");
		sqlite3_result_int (context, 0);
		return;
	    }
      }
    if (dims == GAIA_XY || dims == GAIA_XY_Z)
	;
    else
      {
	  spatialite_e
	      ("CreateTopologyTables() error: [dimension] ILLEGAL VALUE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (srid <= 0)
      {
	  spatialite_e ("CreateTopologyTables() error: [SRID] ILLEGAL VALUE\n");
	  sqlite3_result_int (context, 0);
	  return;
      }

/* checking Topology tables */
    tables[0] = "topology_master";
    views[0] = 0;
    table_curves = sqlite3_mprintf ("%scurves", prefix);
    tables[1] = table_curves;
    views[1] = 0;
    table_surfaces = sqlite3_mprintf ("%ssurfaces", prefix);
    tables[2] = table_surfaces;
    views[2] = 0;
    table_nodes = sqlite3_mprintf ("%snodes", prefix);
    tables[3] = table_nodes;
    views[3] = 0;
    table_edges = sqlite3_mprintf ("%sedges", prefix);
    tables[4] = table_edges;
    views[4] = 0;
    table_faces = sqlite3_mprintf ("%sfaces", prefix);
    tables[5] = table_faces;
    views[5] = 0;
    table_faces_edges = sqlite3_mprintf ("%sfaces_edges", prefix);
    tables[6] = table_faces_edges;
    views[6] = 0;
    view_check_node_codes =
	sqlite3_mprintf ("%snodes_check_dupl_codes", prefix);
    tables[7] = view_check_node_codes;
    views[7] = 1;
    view_check_node_geoms =
	sqlite3_mprintf ("%snodes_check_dupl_geoms", prefix);
    tables[8] = view_check_node_geoms;
    views[8] = 1;
    view_check_edge_codes =
	sqlite3_mprintf ("%sedges_check_dupl_codes", prefix);
    tables[9] = view_check_edge_codes;
    views[9] = 1;
    view_check_edge_geoms =
	sqlite3_mprintf ("%sedges_check_intersections", prefix);
    tables[10] = view_check_edge_geoms;
    views[10] = 1;
    view_check_edge_node_geoms =
	sqlite3_mprintf ("%sedges_check_nodes", prefix);
    tables[11] = view_check_edge_node_geoms;
    views[11] = 1;
    view_check_face_codes =
	sqlite3_mprintf ("%sfaces_check_dupl_codes", prefix);
    tables[12] = view_check_face_codes;
    views[12] = 1;
    view_faces_resolved = sqlite3_mprintf ("%sfaces_resolved", prefix);
    tables[13] = view_faces_resolved;
    views[13] = 1;
    view_curves_resolved = sqlite3_mprintf ("%scurves_resolved", prefix);
    tables[14] = view_curves_resolved;
    views[14] = 1;
    view_surfaces_resolved = sqlite3_mprintf ("%ssurfaces_resolved", prefix);
    tables[15] = view_surfaces_resolved;
    views[15] = 1;
    view_dangling_nodes = sqlite3_mprintf ("%sdangling_nodes", prefix);
    tables[16] = view_dangling_nodes;
    views[16] = 1;
    view_dangling_edges = sqlite3_mprintf ("%sdangling_edges", prefix);
    tables[17] = view_dangling_edges;
    views[17] = 1;
    view_edges_check_from_to =
	sqlite3_mprintf ("%sedges_check_from_to", prefix);
    tables[18] = view_edges_check_from_to;
    views[18] = 1;
    tables[19] = NULL;
    p_view = views;
    p_tbl = tables;
    while (*p_tbl != NULL)
      {
	  ok_table = check_topo_table (sqlite, *p_tbl, *p_view);
	  if (ok_table)
	    {
		if (strcmp (*p_tbl, "topology_master") == 0)
		    create_master = 0;
		else
		  {
		      spatialite_e
			  ("CreateTopologyTables() error: table '%s' already exists\n",
			   *p_tbl);
		      goto error;
		  }
	    }
	  p_tbl++;
	  p_view++;
      }

/* creating Topology tables */
    if (create_master)
      {
	  if (!create_topo_master (sqlite))
	      goto error;
      }
    if (!create_topo_nodes (sqlite, table_nodes, srid, dims))
	goto error;
    if (!create_topo_edges (sqlite, table_edges, srid, dims))
	goto error;
    if (!create_topo_faces (sqlite, table_faces))
	goto error;
    if (!create_topo_faces_edges (sqlite, table_faces_edges, table_faces))
	goto error;
    if (!create_topo_curves (sqlite, table_curves))
	goto error;
    if (!create_topo_surfaces (sqlite, table_surfaces))
	goto error;
    if (!create_check_node_codes (sqlite, view_check_node_codes, table_nodes))
	goto error;
    if (!create_check_node_geoms (sqlite, view_check_node_geoms, table_nodes))
	goto error;
    if (!create_check_edge_codes (sqlite, view_check_edge_codes, table_edges))
	goto error;
    if (!create_check_edge_geoms (sqlite, view_check_edge_geoms, table_edges))
	goto error;
    if (!create_check_edge_node_geoms
	(sqlite, view_check_edge_node_geoms, table_edges, table_nodes))
	goto error;
    if (!create_check_face_codes (sqlite, view_check_face_codes, table_faces))
	goto error;
    if (!create_faces_resolved
	(sqlite, view_faces_resolved, table_faces, table_faces_edges,
	 table_edges))
	goto error;
    if (!create_curves_resolved
	(sqlite, view_curves_resolved, table_curves, table_edges))
	goto error;
    if (!create_surfaces_resolved
	(sqlite, view_surfaces_resolved, table_surfaces, view_faces_resolved))
	goto error;
    if (!create_dangling_nodes
	(sqlite, view_dangling_nodes, table_nodes, table_edges))
	goto error;
    if (!create_dangling_edges
	(sqlite, view_dangling_edges, table_edges, table_faces_edges,
	 table_curves))
	goto error;
    if (!create_check_edges_from_to
	(sqlite, view_edges_check_from_to, table_edges, table_nodes))
	goto error;
    if (!update_topo_master
	(sqlite, table_nodes, table_edges, table_faces, table_faces_edges,
	 table_curves, table_surfaces, view_check_node_codes,
	 view_check_node_geoms, view_check_edge_codes, view_check_edge_geoms,
	 view_check_edge_node_geoms, view_check_face_codes,
	 view_faces_resolved, view_curves_resolved, view_surfaces_resolved,
	 view_dangling_nodes, view_dangling_edges, view_edges_check_from_to,
	 srid, dims))
	goto error;
    updateSpatiaLiteHistory (sqlite, "*** TOPOLOGY ***", NULL,
			     "Topology tables successfully created");
    sqlite3_result_int (context, 1);
    sqlite3_free (table_curves);
    sqlite3_free (table_surfaces);
    sqlite3_free (table_nodes);
    sqlite3_free (table_edges);
    sqlite3_free (table_faces);
    sqlite3_free (table_faces_edges);
    sqlite3_free (view_check_node_codes);
    sqlite3_free (view_check_node_geoms);
    sqlite3_free (view_check_edge_codes);
    sqlite3_free (view_check_edge_geoms);
    sqlite3_free (view_check_edge_node_geoms);
    sqlite3_free (view_check_face_codes);
    sqlite3_free (view_faces_resolved);
    sqlite3_free (view_curves_resolved);
    sqlite3_free (view_surfaces_resolved);
    sqlite3_free (view_dangling_nodes);
    sqlite3_free (view_dangling_edges);
    sqlite3_free (view_edges_check_from_to);
    return;

  error:
    sqlite3_result_int (context, 0);
    sqlite3_free (table_curves);
    sqlite3_free (table_surfaces);
    sqlite3_free (table_nodes);
    sqlite3_free (table_edges);
    sqlite3_free (table_faces);
    sqlite3_free (table_faces_edges);
    sqlite3_free (view_check_node_codes);
    sqlite3_free (view_check_node_geoms);
    sqlite3_free (view_check_edge_codes);
    sqlite3_free (view_check_edge_geoms);
    sqlite3_free (view_check_edge_node_geoms);
    sqlite3_free (view_check_face_codes);
    sqlite3_free (view_faces_resolved);
    sqlite3_free (view_curves_resolved);
    sqlite3_free (view_surfaces_resolved);
    sqlite3_free (view_dangling_nodes);
    sqlite3_free (view_dangling_edges);
    sqlite3_free (view_edges_check_from_to);
    return;
}

static void
fnct_OffsetCurve (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ OffsetCurve(BLOBencoded geometry, radius, left-or-right-side)
/
/ returns a new geometry representing the OFFSET-CURVE for current geometry
/ [a LINESTRING is expected]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    double radius;
    int int_value;
    int left_right;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	radius = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  radius = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	left_right = sqlite3_value_int (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaOffsetCurve (geo, radius, 16, left_right);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_SingleSidedBuffer (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ SingleSidedBuffer(BLOBencoded geometry, radius, left-or-right-side)
/
/ returns a new geometry representing the SingleSided BUFFER 
/ for current geometry [a LINESTRING is expected]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    double radius;
    int int_value;
    int left_right;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	radius = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  radius = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	left_right = sqlite3_value_int (argv[2]);
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaSingleSidedBuffer (geo, radius, 16, left_right);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_HausdorffDistance (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ HausdorffDistance(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns the discrete Hausdorff distance between GEOM-1 and GEOM-2
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    double dist;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  ret = gaiaHausdorffDistance (geo1, geo2, &dist);
	  if (!ret)
	      sqlite3_result_null (context);
	  sqlite3_result_double (context, dist);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_SharedPaths (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ SharedPaths(BLOBencoded geometry1, BLOBencoded geometry2)
/
/ returns a new geometry representing common (shared) Edges
/ [two LINESTRINGs/MULTILINESTRINGs are expected]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo1 == NULL || geo2 == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaSharedPaths (geo1, geo2);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo1->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Covers (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Covers(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if GEOM-1 "spatially covers" GEOM-2
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = gaiaGeomCollPreparedCovers (sqlite3_user_data (context), geo1,
					    blob1, bytes1, geo2, blob2, bytes2);
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_CoveredBy (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CoveredBy(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns:
/ 1 if GEOM-1 is "spatially covered by" GEOM-2
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *blob1;
    unsigned char *blob2;
    int bytes1;
    int bytes2;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    blob1 = (unsigned char *) sqlite3_value_blob (argv[0]);
    bytes1 = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (blob1, bytes1);
    blob2 = (unsigned char *) sqlite3_value_blob (argv[1]);
    bytes2 = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (blob2, bytes2);
    if (!geo1 || !geo2)
	sqlite3_result_int (context, -1);
    else
      {
	  ret = gaiaGeomCollPreparedCoveredBy (sqlite3_user_data (context),
					       geo1, blob1, bytes1, geo2,
					       blob2, bytes2);
	  sqlite3_result_int (context, ret);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_LineInterpolatePoint (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ LineInterpolatePoint(BLOBencoded geometry1, double fraction)
/
/ returns a new geometry representing a point interpolated along a line
/ [a LINESTRING is expected / fraction ranging from 0.0 to 1.0]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double fraction;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	fraction = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  fraction = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaLineInterpolatePoint (geo, fraction);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_LineInterpolateEquidistantPoints (sqlite3_context * context, int argc,
				       sqlite3_value ** argv)
{
/* SQL function:
/ LineInterpolateEquidistantPointS(BLOBencoded geometry1, double distance)
/
/ returns a new geometry representing a point interpolated along a line
/ [a LINESTRING is expected / fraction ranging from 0.0 to 1.0]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double distance;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	distance = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  distance = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaLineInterpolateEquidistantPoints (geo, distance);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_LineLocatePoint (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ LineLocatePoint(BLOBencoded geometry1, BLOBencoded geometry2)
/
/ return a number (between 0.0 and 1.0) representing the location 
/ of the closest point on LineString to the given Point, as a fraction 
/ of total 2d line length
/
/ - geom1 is expected to represent some LINESTRING
/ - geom2 is expected to represent some POINT
*/
    unsigned char *p_blob;
    int n_bytes;
    double fraction;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo1 == NULL || geo2 == NULL)
	sqlite3_result_null (context);
    else
      {
	  fraction = gaiaLineLocatePoint (geo1, geo2);
	  if (fraction >= 0.0 && fraction <= 1.0)
	      sqlite3_result_double (context, fraction);
	  else
	      sqlite3_result_null (context);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_LineSubstring (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ LineSubstring(BLOBencoded geometry1, double start_fraction, double end_fraction)
/
/ Return a Linestring being a substring of the input one starting and ending at 
/ the given fractions of total 2d length [fractions ranging from 0.0 to 1.0]
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double fraction1;
    double fraction2;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	fraction1 = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  fraction1 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	fraction2 = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  fraction2 = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaLineSubstring (geo, fraction1, fraction2);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ClosestPoint (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ClosestPoint(BLOBencoded geometry1, BLOBencoded geometry2)
/
/ Returns the Point on geom1 that is closest to geom2
/ NULL is returned for invalid arguments (or if distance is ZERO)
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo1 == NULL || geo2 == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaShortestLine (geo1, geo2);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else if (result->FirstLinestring == NULL)
	    {
		gaiaFreeGeomColl (result);
		sqlite3_result_null (context);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		double x;
		double y;
		double z;
		double m;
		int len;
		unsigned char *p_result = NULL;
		gaiaGeomCollPtr pt = NULL;
		gaiaLinestringPtr ln = result->FirstLinestring;
		if (ln->DimensionModel == GAIA_XY_Z)
		    pt = gaiaAllocGeomCollXYZ ();
		else if (ln->DimensionModel == GAIA_XY_M)
		    pt = gaiaAllocGeomCollXYM ();
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		    pt = gaiaAllocGeomCollXYZM ();
		else
		    pt = gaiaAllocGeomColl ();
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, 0, &x, &y, &z);
		      gaiaAddPointToGeomCollXYZ (pt, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, 0, &x, &y, &m);
		      gaiaAddPointToGeomCollXYM (pt, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, 0, &x, &y, &z, &m);
		      gaiaAddPointToGeomCollXYZM (pt, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, 0, &x, &y);
		      gaiaAddPointToGeomColl (pt, x, y);
		  }
		pt->Srid = geo1->Srid;
		gaiaToSpatiaLiteBlobWkb (pt, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
		gaiaFreeGeomColl (pt);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_ShortestLine (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ShortestLine(BLOBencoded geometry1, BLOBencoded geometry2)
/
/ Returns the shortest line between two geometries
/ NULL is returned for invalid arguments (or if distance is ZERO)
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo1 == NULL || geo2 == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaShortestLine (geo1, geo2);
	  sqlite3_result_null (context);
	  if (!result)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo1->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_Snap (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Snap(BLOBencoded geometry1, BLOBencoded geometry2, double tolerance)
/
/ Returns a new Geometry corresponding to geom1 snapped to geom2
/ and using the given tolerance
/ NULL is returned for invalid arguments (or if distance is ZERO)
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double tolerance;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	tolerance = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[2]);
	  tolerance = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo1 == NULL || geo2 == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaSnap (geo1, geo2, tolerance);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo1->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_LineMerge (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ LineMerge(BLOBencoded geometry)
/
/ Assuming that Geometry represents a set of sparse Linestrings,
/ this function will attempt to reassemble a single line
/ (or a set of lines)
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaLineMerge (geo);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_UnaryUnion (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ UnaryUnion(BLOBencoded geometry)
/
/ exactly like Union, but using a single Collection
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaUnaryUnion (geo);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_SquareGrid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_SquareGrid(BLOBencoded geom, double size)
/ ST_SquareGrid(BLOBencoded geom, double size, boolean edges_only)
/ ST_SquareGrid(BLOBencoded geom, double size, boolean edges_only, BLOBencoded origin)
/
/ Builds a regular grid (Square cells) covering the geom.
/ each cell has the edges's length as defined by the size argument
/ an arbitrary origin is supported (0,0 is assumed by default)
/ return NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double size;
    int edges_only = 0;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr point = NULL;
    gaiaGeomCollPtr result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  size = int_value;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
      {
	  size = sqlite3_value_double (argv[1]);
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (size <= 0.0)
      {
	  /* negative side size */
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      edges_only = sqlite3_value_int (argv[2]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[3]);
	  n_bytes = sqlite3_value_bytes (argv[3]);
	  point = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
	  if (!point)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (point->FirstLinestring != NULL)
	      goto no_point;
	  if (point->FirstPolygon != NULL)
	      goto no_point;
	  if (point->FirstPoint != NULL)
	    {
		if (point->FirstPoint == point->LastPoint)
		  {
		      origin_x = point->FirstPoint->X;
		      origin_y = point->FirstPoint->Y;
		      gaiaFreeGeomColl (point);
		  }
		else
		    goto no_point;
	    }
	  else
	      goto no_point;

      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->FirstPoint != NULL)
	      goto no_polygon;
	  if (geo->FirstLinestring != NULL)
	      goto no_polygon;
	  if (geo->FirstPolygon == NULL)
	      goto no_polygon;
	  result = gaiaSquareGrid (geo, origin_x, origin_y, size, edges_only);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
    return;

  no_point:
    gaiaFreeGeomColl (point);
    sqlite3_result_null (context);
    return;

  no_polygon:
    gaiaFreeGeomColl (geo);
    sqlite3_result_null (context);
    return;
}

static void
fnct_TriangularGrid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_TriangularGrid(BLOBencoded geom, double size)
/ ST_TriangularGrid(BLOBencoded geom, double size, boolean edges_only)
/ ST_TriangularGrid(BLOBencoded geom, double size, boolean edges_only, BLOBencoded origin)
/
/ Builds a regular grid (Triangular cells) covering the geom.
/ each cell has the edge's length as defined by the size argument
/ an arbitrary origin is supported (0,0 is assumed by default)
/ return NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double size;
    int edges_only = 0;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr point = NULL;
    gaiaGeomCollPtr result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  size = int_value;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
      {
	  size = sqlite3_value_double (argv[1]);
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (size <= 0.0)
      {
	  /* negative side size */
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      edges_only = sqlite3_value_int (argv[2]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[3]);
	  n_bytes = sqlite3_value_bytes (argv[3]);
	  point = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
	  if (!point)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (point->FirstLinestring != NULL)
	      goto no_point;
	  if (point->FirstPolygon != NULL)
	      goto no_point;
	  if (point->FirstPoint != NULL)
	    {
		if (point->FirstPoint == point->LastPoint)
		  {
		      origin_x = point->FirstPoint->X;
		      origin_y = point->FirstPoint->Y;
		      gaiaFreeGeomColl (point);
		  }
		else
		    goto no_point;
	    }
	  else
	      goto no_point;

      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->FirstPoint != NULL)
	      goto no_polygon;
	  if (geo->FirstLinestring != NULL)
	      goto no_polygon;
	  if (geo->FirstPolygon == NULL)
	      goto no_polygon;
	  result =
	      gaiaTriangularGrid (geo, origin_x, origin_y, size, edges_only);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
    return;

  no_point:
    gaiaFreeGeomColl (point);
    sqlite3_result_null (context);
    return;

  no_polygon:
    gaiaFreeGeomColl (geo);
    sqlite3_result_null (context);
    return;
}

static void
fnct_HexagonalGrid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ST_HexagonalGrid(BLOBencoded geom, double size)
/ ST_HexagonalGrid(BLOBencoded geom, double size, boolean edges_only)
/ ST_HexagonalGrid(BLOBencoded geom, double size, boolean edges_only, BLOBencoded origin)
/
/ Builds a regular grid (Hexagonal cells) covering the geom.
/ each cell has the edges's length as defined by the size argument
/ an arbitrary origin is supported (0,0 is assumed by default)
/ return NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double size;
    int edges_only = 0;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr point = NULL;
    gaiaGeomCollPtr result = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  size = int_value;
      }
    else if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
      {
	  size = sqlite3_value_double (argv[1]);
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (size <= 0.0)
      {
	  /* negative side size */
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      edges_only = sqlite3_value_int (argv[2]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) != SQLITE_BLOB)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[3]);
	  n_bytes = sqlite3_value_bytes (argv[3]);
	  point = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
	  if (!point)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  if (point->FirstLinestring != NULL)
	      goto no_point;
	  if (point->FirstPolygon != NULL)
	      goto no_point;
	  if (point->FirstPoint != NULL)
	    {
		if (point->FirstPoint == point->LastPoint)
		  {
		      origin_x = point->FirstPoint->X;
		      origin_y = point->FirstPoint->Y;
		      gaiaFreeGeomColl (point);
		  }
		else
		    goto no_point;
	    }
	  else
	      goto no_point;

      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (geo->FirstPoint != NULL)
	      goto no_polygon;
	  if (geo->FirstLinestring != NULL)
	      goto no_polygon;
	  if (geo->FirstPolygon == NULL)
	      goto no_polygon;
	  result =
	      gaiaHexagonalGrid (geo, origin_x, origin_y, size, edges_only);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
    return;

  no_point:
    gaiaFreeGeomColl (point);
    sqlite3_result_null (context);
    return;

  no_polygon:
    gaiaFreeGeomColl (geo);
    sqlite3_result_null (context);
    return;
}

static void
fnct_LinesCutAtNodes (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ LinesCutAtNodes(BLOBencoded geometry1, BLOBencoded geometry2)
/
/ Assuming that Geometry-1 represents a set of arbitray Linestrings,
/ and that Geometry-2 represents of arbitrary Points, this function 
/ will then attempt to cut lines accordingly to given nodes.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom1 = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geom2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom1 == NULL || geom2 == NULL)
      {
	  if (geom1)
	      gaiaFreeGeomColl (geom1);
	  if (geom2)
	      gaiaFreeGeomColl (geom2);
	  sqlite3_result_null (context);
	  return;
      }
    result = gaiaLinesCutAtNodes (geom1, geom2);
    if (result == NULL)
      {
	  sqlite3_result_null (context);
      }
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  result->Srid = geom1->Srid;
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (result);
      }
    gaiaFreeGeomColl (geom1);
    gaiaFreeGeomColl (geom2);
}

static int
cmp_pt_coords (const void *p1, const void *p2)
{
/* compares two nodes  by ID [for QSORT] */
    gaiaPointPtr pt1 = *((gaiaPointPtr *) p1);
    gaiaPointPtr pt2 = *((gaiaPointPtr *) p2);
    if (pt1->X == pt2->X && pt1->Y == pt2->Y && pt1->Z == pt2->Z)
	return 0;
    if (pt1->X > pt2->X)
	return 1;
    if (pt1->X == pt2->X && pt1->Y > pt2->Y)
	return 1;
    if (pt1->X == pt2->X && pt1->Y == pt2->Y && pt1->Z > pt2->Z)
	return 1;
    return -1;
}

static gaiaGeomCollPtr
auxPolygNodes (gaiaGeomCollPtr geom)
{
/* attempting to identify Ring-Nodes */
    gaiaGeomCollPtr result = NULL;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    gaiaPointPtr pt;
    gaiaPointPtr prev_pt;
    gaiaPointPtr *sorted = NULL;
    int count = 0;
    int iv;
    int ib;
    double x;
    double y;
    double z;
    double m;

/* inserting all Points into a Dynamic Line */
    gaiaDynamicLinePtr dyn = gaiaAllocDynamicLine ();
    pg = geom->FirstPolygon;
    while (pg)
      {
	  rng = pg->Exterior;
	  /* CAVEAT: first point needs to be skipped (closed ring) */
	  for (iv = 1; iv < rng->Points; iv++)
	    {
		/* exterior ring */
		if (geom->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
		      gaiaAppendPointZToDynamicLine (dyn, x, y, z);
		  }
		else if (geom->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
		      gaiaAppendPointMToDynamicLine (dyn, x, y, m);
		  }
		else if (geom->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
		      gaiaAppendPointZMToDynamicLine (dyn, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (rng->Coords, iv, &x, &y);
		      gaiaAppendPointToDynamicLine (dyn, x, y);
		  }
	    }

	  for (ib = 0; ib < pg->NumInteriors; ib++)
	    {
		rng = pg->Interiors + ib;
		/* CAVEAT: first point needs to be skipped (closed ring) */
		for (iv = 1; iv < rng->Points; iv++)
		  {
		      /* interior ring */
		      if (geom->DimensionModel == GAIA_XY_Z)
			{
			    gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
			    gaiaAppendPointZToDynamicLine (dyn, x, y, z);
			}
		      else if (geom->DimensionModel == GAIA_XY_M)
			{
			    gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
			    gaiaAppendPointMToDynamicLine (dyn, x, y, m);
			}
		      else if (geom->DimensionModel == GAIA_XY_Z_M)
			{
			    gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
			    gaiaAppendPointZMToDynamicLine (dyn, x, y, z, m);
			}
		      else
			{
			    gaiaGetPoint (rng->Coords, iv, &x, &y);
			    gaiaAppendPointToDynamicLine (dyn, x, y);
			}
		  }
	    }
	  pg = pg->Next;
      }

    pt = dyn->First;
    while (pt)
      {
	  /* counting how many points */
	  count++;
	  pt = pt->Next;
      }
    if (count == 0)
      {
	  gaiaFreeDynamicLine (dyn);
	  return NULL;
      }

/* allocating and initializing an array of pointers */
    sorted = malloc (sizeof (gaiaPointPtr) * count);
    iv = 0;
    pt = dyn->First;
    while (pt)
      {
	  *(sorted + iv++) = pt;
	  pt = pt->Next;
      }

/* sorting points by coords */
    qsort (sorted, count, sizeof (gaiaPointPtr), cmp_pt_coords);

    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaAllocGeomCollXYZ ();
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaAllocGeomCollXYM ();
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaAllocGeomCollXYZM ();
    else
	result = gaiaAllocGeomColl ();
    result->Srid = geom->Srid;

/* identifying nodes */
    prev_pt = NULL;
    for (iv = 0; iv < count; iv++)
      {
	  pt = *(sorted + iv);
	  if (prev_pt != NULL)
	    {
		if (prev_pt->X == pt->X && prev_pt->Y == pt->Y
		    && prev_pt->Z == pt->Z)
		  {
		      if (result->LastPoint != NULL)
			{
			    if (result->LastPoint->X == pt->X
				&& result->LastPoint->Y == pt->Y
				&& result->LastPoint->Z == pt->Z)
				continue;
			}
		      /* Node found */
		      if (result->DimensionModel == GAIA_XY_Z)
			  gaiaAddPointToGeomCollXYZ (result, pt->X, pt->Y,
						     pt->Z);
		      else if (result->DimensionModel == GAIA_XY_M)
			  gaiaAddPointToGeomCollXYM (result, pt->X, pt->Y,
						     pt->M);
		      else if (result->DimensionModel == GAIA_XY_Z_M)
			  gaiaAddPointToGeomCollXYZM (result, pt->X,
						      pt->Y, pt->Z, pt->M);
		      else
			  gaiaAddPointToGeomColl (result, pt->X, pt->Y);
		  }
	    }
	  prev_pt = pt;
      }

    if (result->FirstPoint == NULL)
      {
	  gaiaFreeGeomColl (result);
	  result = NULL;
      }
    free (sorted);
    gaiaFreeDynamicLine (dyn);
    return result;
}

static void
fnct_RingsCutAtNodes (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ RingsCutAtNodes(BLOBencoded geometry)
/
/ This function will attempt to return a collection of lines
/ representing Polygon/Rings: the input geometry is expected
/ to be a Polygon or MultiPolygon.
/ Each Ring will be cut accordingly to any identified "node"
/ i.e. self-intersections or intersections between two Rings.
/
/ NULL is returned for invalid arguments
*/
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    unsigned char *p_blob;
    int n_bytes;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaGeomCollPtr geom = NULL;
    gaiaGeomCollPtr geom1 = NULL;
    gaiaGeomCollPtr geom2 = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }

/* checking if Geometry is a Polygon or MultiPolyhon */
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts > 0 || lns > 0 || pgs == 0)
      {
	  /* not Polygon/MultiPolygon */
	  gaiaFreeGeomColl (geom);
	  sqlite3_result_null (context);
	  return;
      }

/* transforming Rings into Linestrings */
    geom1 = gaiaLinearize (geom, 1);
    if (geom1 == NULL)
      {
	  gaiaFreeGeomColl (geom);
	  sqlite3_result_null (context);
	  return;
      }

/* identifying the Nodes */
    geom2 = auxPolygNodes (geom);
    if (geom2 == NULL)
      {
	  /* there is no need to cut any Ring [no Nodes] */
	  int len;
	  unsigned char *p_result = NULL;
	  geom1->Srid = geom->Srid;
	  gaiaToSpatiaLiteBlobWkb (geom1, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (geom);
	  gaiaFreeGeomColl (geom1);
	  return;
      }

/* attempting to cut Rings */
    result = gaiaLinesCutAtNodes (geom1, geom2);
    if (result == NULL)
	sqlite3_result_null (context);
    else
      {
	  /* builds the BLOB geometry to be returned */
	  int len;
	  unsigned char *p_result = NULL;
	  result->Srid = geom->Srid;
	  gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
	  sqlite3_result_blob (context, p_result, len, free);
	  gaiaFreeGeomColl (result);
      }
    gaiaFreeGeomColl (geom);
    gaiaFreeGeomColl (geom1);
    gaiaFreeGeomColl (geom2);
}

#endif /* end GEOS advanced features */

#ifdef GEOS_TRUNK		/* GEOS experimental features */

static void
fnct_DelaunayTriangulation (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ DelaunayTriangulation(BLOBencoded geometry)
/ DelaunayTriangulation(BLOBencoded geometry, boolean onlyEdges)
/ DelaunayTriangulation(BLOBencoded geometry, boolean onlyEdges, double tolerance)
/
/ Attempts to build a Delaunay Triangulation using all points/vertices 
/ found in the input geometry.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    int int_value;
    double tolerance = 0.0;
    int only_edges = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      only_edges = sqlite3_value_int (argv[1]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	      tolerance = sqlite3_value_double (argv[2]);
	  else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[2]);
		tolerance = int_value;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaDelaunayTriangulation (geo, tolerance, only_edges);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_VoronojDiagram (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ VoronojDiagram(BLOBencoded geometry)
/ VoronojDiagram(BLOBencoded geometry, boolean onlyEdges)
/ VoronojDiagram(BLOBencoded geometry, boolean onlyEdges, 
/        double extra_frame_size)
/ VoronojDiagram(BLOBencoded geometry, boolean onlyEdges,
/        double extra_frame_size, double tolerance)
/
/ Attempts to build a Voronoj Diagram using all points/vertices 
/ found in the input geometry.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    int int_value;
    double tolerance = 0.0;
    double extra_frame_size = -1.0;
    int only_edges = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      only_edges = sqlite3_value_int (argv[1]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	      extra_frame_size = sqlite3_value_double (argv[2]);
	  else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[2]);
		extra_frame_size = int_value;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	      tolerance = sqlite3_value_double (argv[3]);
	  else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[3]);
		tolerance = int_value;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result =
	      gaiaVoronojDiagram (geo, extra_frame_size, tolerance, only_edges);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_ConcaveHull (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ConcaveHull(BLOBencoded geometry)
/ ConcaveHull(BLOBencoded geometry, double factor)
/ ConcaveHull(BLOBencoded geometry, double factor, boolean allow_holes)
/ ConcaveHull(BLOBencoded geometry, double factor,
/        boolean allow_holes, double tolerance)
/
/ Attempts to build a ConcaveHull using all points/vertices 
/ found in the input geometry.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    int int_value;
    double tolerance = 0.0;
    double factor = 3.0;
    int allow_holes = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	      factor = sqlite3_value_double (argv[1]);
	  else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[1]);
		factor = int_value;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      allow_holes = sqlite3_value_int (argv[2]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
	      tolerance = sqlite3_value_double (argv[3]);
	  else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	    {
		int_value = sqlite3_value_int (argv[3]);
		tolerance = int_value;
	    }
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaConcaveHull (geo, factor, tolerance, allow_holes);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

#endif /* end GEOS experimental features */

#ifdef ENABLE_LWGEOM		/* enabling LWGEOM support */

static void
fnct_MakeValid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MakeValid(BLOBencoded geometry)
/
/ Attempts to make an invalid geometry valid without loosing vertices.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaMakeValid (geo);
	  if (result == NULL)
	    {
		char *msg;
		const char *lw_err = gaiaGetLwGeomErrorMsg ();
		if (lw_err)
		    msg = sqlite3_mprintf
			("MakeValid error - LWGEOM reports: %s\n", lw_err);
		else
		    msg = sqlite3_mprintf
			("MakeValid error - LWGEOM reports: Unknown Reason\n");
		sqlite3_result_error (context, msg, strlen (msg));
		sqlite3_free (msg);
	    }
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_MakeValidDiscarded (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ MakeValidDiscarded(BLOBencoded geometry)
/
/ Strictly related to MakeValid(); useful to collect any offending item
/ discarded during the validation process.
/ NULL is returned for invalid arguments (or if no discarded items are found)
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaMakeValidDiscarded (geo);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Segmentize (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Segmentize(BLOBencoded geometry, double dist)
/
/ Ensure every segment is at most 'dist' long
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    int int_value;
    double dist;
    gaiaGeomCollPtr geo = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	dist = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  dist = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geo == NULL)
	sqlite3_result_null (context);
    else
      {
	  result = gaiaSegmentize (geo, dist);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = geo->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (geo);
}

static void
fnct_Split (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Split(BLOBencoded input, BLOBencoded blade)
/
/ Returns a collection of geometries resulting by splitting a geometry
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr input = NULL;
    gaiaGeomCollPtr blade = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    input = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (input == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    blade = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (blade == NULL)
      {
	  gaiaFreeGeomColl (input);
	  sqlite3_result_null (context);
	  return;
      }
    else
      {
	  result = gaiaSplit (input, blade);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = input->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (input);
    gaiaFreeGeomColl (blade);
}

static void
fnct_SplitLeft (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ SplitLeft(BLOBencoded input, BLOBencoded blade)
/
/ Returns a collection of geometries resulting by splitting a geometry [left half]
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr input = NULL;
    gaiaGeomCollPtr blade = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    input = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (input == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    blade = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (blade == NULL)
      {
	  gaiaFreeGeomColl (input);
	  sqlite3_result_null (context);
	  return;
      }
    else
      {
	  result = gaiaSplitLeft (input, blade);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = input->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (input);
    gaiaFreeGeomColl (blade);
}

static void
fnct_SplitRight (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ SplitRight(BLOBencoded input, BLOBencoded blade)
/
/ Returns a collection of geometries resulting by splitting a geometry [right half]
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr input = NULL;
    gaiaGeomCollPtr blade = NULL;
    gaiaGeomCollPtr result;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    input = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (input == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    blade = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (blade == NULL)
      {
	  gaiaFreeGeomColl (input);
	  sqlite3_result_null (context);
	  return;
      }
    else
      {
	  result = gaiaSplitRight (input, blade);
	  if (result == NULL)
	      sqlite3_result_null (context);
	  else
	    {
		/* builds the BLOB geometry to be returned */
		int len;
		unsigned char *p_result = NULL;
		result->Srid = input->Srid;
		gaiaToSpatiaLiteBlobWkb (result, &p_result, &len);
		sqlite3_result_blob (context, p_result, len, free);
		gaiaFreeGeomColl (result);
	    }
      }
    gaiaFreeGeomColl (input);
    gaiaFreeGeomColl (blade);
}

static int
getXYSinglePoint (gaiaGeomCollPtr geom, double *x, double *y)
{
/* check if this geometry is a simple Point (returning 2D coords) */
    double z;
    double m;
    return getXYZMSinglePoint (geom, x, y, &z, &m);
}

static void
fnct_Azimuth (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Azimuth(BLOBencoded pointA, BLOBencoded pointB)
/
/ Returns the angle in radians from the horizontal of the vector 
/ defined by pointA and pointB. 
/ Angle is computed clockwise from down-to-up: on the clock: 
/ 12=0; 3=PI/2; 6=PI; 9=3PI/2.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    double x1;
    double y1;
    double x2;
    double y2;
    double a;
    double b;
    double rf;
    double azimuth;
    int srid;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }

/* retrieving and validating the first point */
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (!getXYSinglePoint (geom, &x1, &y1))
      {
	  gaiaFreeGeomColl (geom);
	  sqlite3_result_null (context);
	  return;
      }
    srid = geom->Srid;
    gaiaFreeGeomColl (geom);

/* retrieving and validating the second point */
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (!getXYSinglePoint (geom, &x2, &y2))
      {
	  gaiaFreeGeomColl (geom);
	  sqlite3_result_null (context);
	  return;
      }
    gaiaFreeGeomColl (geom);

    if (getEllipsoidParams (sqlite, srid, &a, &b, &rf))
      {
	  if (gaiaEllipsoidAzimuth (x1, y1, x2, y2, a, b, &azimuth))
	      sqlite3_result_double (context, azimuth);
	  else
	      sqlite3_result_null (context);
	  return;
      }

    if (gaiaAzimuth (x1, y1, x2, y2, &azimuth))
	sqlite3_result_double (context, azimuth);
    else
	sqlite3_result_null (context);
}

static void
fnct_Project (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ Project(BLOBencoded point, distance Double, bearing Double)
/
/ Returns a new Point projected from a start point given a
/ distance and a bearing. 
/ - Point is expected to be Long/Lat
/ - Distance is in meters
/ - Bearing is in radians; on the clock: 
/   12=0; 3=PI/2; 6=PI; 9=3PI/2.
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    double x1;
    double y1;
    double x2;
    double y2;
    int ival;
    double distance;
    double azimuth;
    double a;
    double b;
    double rf;
    int srid;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	distance = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[1]);
	  distance = ival;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
	azimuth = sqlite3_value_double (argv[2]);
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
      {
	  ival = sqlite3_value_int (argv[2]);
	  azimuth = ival;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }

/* retrieving and validating the start point */
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (!getXYSinglePoint (geom, &x1, &y1))
      {
	  gaiaFreeGeomColl (geom);
	  sqlite3_result_null (context);
	  return;
      }
    srid = geom->Srid;
    gaiaFreeGeomColl (geom);
    if (!getEllipsoidParams (sqlite, srid, &a, &b, &rf))
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (distance == 0.0)
      {
	  /* returning the Start Point */
	  gaiaMakePoint (x1, y1, srid, &p_blob, &n_bytes);
	  if (!p_blob)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_blob, n_bytes, free);
	  return;
      }

    if (gaiaProjectedPoint (x1, y1, a, b, distance, azimuth, &x2, &y2))
      {
	  gaiaMakePoint (x2, y2, srid, &p_blob, &n_bytes);
	  if (!p_blob)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_blob (context, p_blob, n_bytes, free);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_GeoHash (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeoHash(BLOBencoded geom)
/ GeoHash(BLOBencoded geom, Integer precision)
/
/ Returns a GeoHash representation for input geometry
/ (expected to be in longitude/latitude coords)
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    int precision = 0;
    char *geo_hash;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[1]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    geo_hash = gaiaGeoHash (geom, precision);
    if (geo_hash != NULL)
      {
	  int len = strlen (geo_hash);
	  sqlite3_result_text (context, geo_hash, len, free);
      }
    else
	sqlite3_result_null (context);
    gaiaFreeGeomColl (geom);
}

static char *
get_srs_by_srid (sqlite3 * sqlite, int srid, int longsrs)
{
/* retrieves the short- or long- srs reference for the given srid */
    char sql[1024];
    int ret;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;
    int len;
    char *srs = NULL;

    if (longsrs)
	sprintf (sql,
		 "SELECT 'urn:ogc:def:crs:' || auth_name || '::' || auth_srid "
		 "FROM spatial_ref_sys WHERE srid = %d", srid);
    else
	sprintf (sql, "SELECT auth_name || ':' || auth_srid "
		 "FROM spatial_ref_sys WHERE srid = %d", srid);
    ret = sqlite3_get_table (sqlite, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	return NULL;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 0];
		len = strlen (name);
		srs = malloc (len + 1);
		strcpy (srs, name);
	    }
      }
    sqlite3_free_table (results);
    return srs;
}

static void
fnct_AsX3D (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ AsX3D(BLOBencoded geom)
/ AsX3D(BLOBencoded geom, Integer precision)
/ AsX3D(BLOBencoded geom, Integer precision, Integer options)
/ AsX3D(BLOBencoded geom, Integer precision, Integer options, Text refid)
/
/ Returns an X3D representation for input geometry
/ NULL is returned for invalid arguments
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    int precision = 15;
    int options = 0;
    const char *refid = "";
    char *srs = NULL;
    char *x3d;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[1]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc >= 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      options = sqlite3_value_int (argv[2]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 4)
      {
	  if (sqlite3_value_type (argv[3]) == SQLITE_TEXT)
	      refid = (const char *) sqlite3_value_text (argv[3]);
	  else
	    {
		sqlite3_result_null (context);
		return;
	    }
      }

    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (geom == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (geom->Srid > 0)
      {
	  int longshort = 0;
	  if (options & 1)
	      longshort = 1;
	  srs = get_srs_by_srid (sqlite, geom->Srid, longshort);
      }
    x3d = gaiaAsX3D (geom, srs, precision, options, refid);
    if (x3d != NULL)
      {
	  int len = strlen (x3d);
	  sqlite3_result_text (context, x3d, len, free);
      }
    else
	sqlite3_result_null (context);
    gaiaFreeGeomColl (geom);
    if (srs)
	free (srs);
}

static void
fnct_3DDistance (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ 3DDistance(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns the 3D distance between GEOM-1 and GEOM-2
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    double dist;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  ret = gaia3DDistance (geo1, geo2, &dist);
	  if (!ret)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, dist);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_MaxDistance (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MaxDistance(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns the max 2D distance between GEOM-1 and GEOM-2
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    double dist;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  ret = gaiaMaxDistance (geo1, geo2, &dist);
	  if (!ret)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, dist);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

static void
fnct_3DMaxDistance (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ 3DMaxDistance(BLOBencoded geom1, BLOBencoded geom2)
/
/ returns the max 3D distance between GEOM-1 and GEOM-2
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo1 = NULL;
    gaiaGeomCollPtr geo2 = NULL;
    double dist;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo1 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    p_blob = (unsigned char *) sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    geo2 = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo1 || !geo2)
	sqlite3_result_null (context);
    else
      {
	  ret = gaia3DMaxDistance (geo1, geo2, &dist);
	  if (!ret)
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, dist);
      }
    gaiaFreeGeomColl (geo1);
    gaiaFreeGeomColl (geo2);
}

#endif /* end LWGEOM support */

#endif /* end including GEOS */

static int
text2double (const unsigned char *str, double *val)
{
/* checks for a valid number, eventually returning a DOUBLE */
    int err = 0;
    int sign = 0;
    int decimal = 0;
    int exp = 0;
    int expsign = 0;
    const unsigned char *p = str;
    while (*p != '\0')
      {
	  switch (*p)
	    {
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		break;
	    case '-':
	    case '+':
		if (!exp)
		    sign++;
		else
		    expsign++;
	    case '.':
		decimal++;
		break;
	    case 'e':
	    case 'E':
		exp++;
		break;
	    default:
		err = 1;
		break;
	    };
	  p++;
      }
    if (sign > 1 || expsign > 1 || decimal > 1 || (exp == 0 && expsign > 0))
	err = 1;
    if (err)
	return 0;
    *val = atof ((const char *) str);
    return 1;
}

static void
fnct_CastToInteger (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToInteger(generic value)
/
/ returns an INTEGER value [if conversion is possible] 
/ or NULL in any other case
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  sqlite3_int64 val = sqlite3_value_int64 (argv[0]);
	  sqlite3_result_int64 (context, val);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  sqlite3_int64 val;
	  double dval = sqlite3_value_double (argv[0]);
	  double diff = dval - floor (dval);
	  val = (sqlite3_int64) sqlite3_value_double (argv[0]);
	  if (diff >= 0.5)
	      val++;
	  sqlite3_result_int64 (context, val);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
      {
	  const unsigned char *txt = sqlite3_value_text (argv[0]);
	  double dval;
	  if (text2double (txt, &dval))
	    {
		sqlite3_int64 val;
		double dval = sqlite3_value_double (argv[0]);
		double diff = dval - floor (dval);
		val = (sqlite3_int64) sqlite3_value_double (argv[0]);
		if (diff >= 0.5)
		    val++;
		sqlite3_result_int64 (context, val);
		return;
	    }
      }
    sqlite3_result_null (context);
}

static void
fnct_CastToDouble (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToDouble(generic value)
/
/ returns a DOUBLE value [if conversion is possible] 
/ or NULL in any other case
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  double val = (double) sqlite3_value_int64 (argv[0]);
	  sqlite3_result_double (context, val);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  double val = sqlite3_value_double (argv[0]);
	  sqlite3_result_double (context, val);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
      {
	  const unsigned char *txt = sqlite3_value_text (argv[0]);
	  double val;
	  if (text2double (txt, &val))
	    {
		sqlite3_result_double (context, val);
		return;
	    }
      }
    sqlite3_result_null (context);
}

static void
fnct_CastToText (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToText(generic value)
/ CastToText(generic value, Integer left-aligned-length)
/
/ returns a TEXT value [if conversion is possible] 
/ or NULL in any other case
*/
    char *txt;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  char format[32];
	  const char *fmt = FRMT64;
	  sqlite3_int64 val;
	  if (argc == 2)
	    {
		int length;
		if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
		  {
		      sqlite3_result_null (context);
		      return;
		  }
		length = sqlite3_value_int (argv[1]);
		if (length > 0)
		  {
		      sprintf (format, "%%0%d" FRMT64_WO_PCT, length);
		      fmt = format;
		  }
	    }
	  val = sqlite3_value_int64 (argv[0]);
	  txt = sqlite3_mprintf (fmt, val);
	  sqlite3_result_text (context, txt, strlen (txt), sqlite3_free);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  int i;
	  int len;
	  double val = sqlite3_value_double (argv[0]);
	  char format[32];
	  const char *fmt = "%1.18f";
	  if (argc == 2)
	    {
		int length;
		if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
		  {
		      sqlite3_result_null (context);
		      return;
		  }
		length = sqlite3_value_int (argv[1]);
		if (length > 0)
		  {
		      sprintf (format, "%%0%d.18f", length + 19);
		      fmt = format;
		  }
	    }
	  txt = sqlite3_mprintf (fmt, val);
	  len = strlen (txt);
	  for (i = len - 1; i > 0; i--)
	    {
		/* suppressing meaningless trailing zeroes */
		if (txt[i] >= '1' && txt[i] <= '9')
		    break;
		if (txt[i] == '.')
		  {
		      txt[i + 1] = '0';
		      break;
		  }
		if (txt[i] == '0')
		    txt[i] = '\0';
	    }
	  sqlite3_result_text (context, txt, strlen (txt), sqlite3_free);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
      {
	  int n_bytes;
	  txt = (char *) sqlite3_value_text (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
	  sqlite3_result_text (context, txt, n_bytes, SQLITE_TRANSIENT);
	  return;
      }
    sqlite3_result_null (context);
}

static int
parseHexByte (unsigned char hi, unsigned char lo, unsigned char *val)
{
/* converting a byte for its Hex representation */
    unsigned char x;
    switch (hi)
      {
      case '0':
	  x = 0;
	  break;
      case '1':
	  x = 16;
	  break;
      case '2':
	  x = 16 * 2;
	  break;
      case '3':
	  x = 16 * 3;
	  break;
      case '4':
	  x = 16 * 4;
	  break;
      case '5':
	  x = 16 * 5;
	  break;
      case '6':
	  x = 16 * 6;
	  break;
      case '7':
	  x = 16 * 7;
	  break;
      case '8':
	  x = 16 * 8;
	  break;
      case '9':
	  x = 16 * 9;
	  break;
      case 'a':
      case 'A':
	  x = 16 * 10;
	  break;
      case 'b':
      case 'B':
	  x = 16 * 11;
	  break;
      case 'c':
      case 'C':
	  x = 16 * 12;
	  break;
      case 'd':
      case 'D':
	  x = 16 * 13;
	  break;
      case 'e':
      case 'E':
	  x = 16 * 14;
	  break;
      case 'f':
      case 'F':
	  x = 16 * 15;
	  break;
      default:
	  return 0;
      };
    switch (lo)
      {
      case '0':
	  x += 0;
	  break;
      case '1':
	  x += 1;
	  break;
      case '2':
	  x += 2;
	  break;
      case '3':
	  x += 3;
	  break;
      case '4':
	  x += 4;
	  break;
      case '5':
	  x += 5;
	  break;
      case '6':
	  x += 6;
	  break;
      case '7':
	  x += 7;
	  break;
      case '8':
	  x += 8;
	  break;
      case '9':
	  x += 9;
	  break;
      case 'a':
      case 'A':
	  x += 10;
	  break;
      case 'b':
      case 'B':
	  x += 11;
	  break;
      case 'c':
      case 'C':
	  x += 12;
	  break;
      case 'd':
      case 'D':
	  x += 13;
	  break;
      case 'e':
      case 'E':
	  x += 14;
	  break;
      case 'f':
      case 'F':
	  x += 15;
	  break;
      default:
	  return 0;
      };
    *val = x;
    return 1;
}

static int
parseHexString (const unsigned char *in, int in_len, unsigned char **out,
		int *out_len)
{
/* parsing an Hexadecimal string */
    unsigned char *buf;
    unsigned char *p_out;
    unsigned char byteval;
    int i;
    int len;
    *out = NULL;
    *out_len = 0;
    if (in == NULL)
	return 0;
    len = in_len / 2;
    if (len * 2 != in_len)	/* # digits is an odd number */
	return 0;
    buf = malloc (len);
    p_out = buf;
    for (i = 0; i < in_len; i += 2)
      {
	  if (!parseHexByte (in[i], in[i + 1], &byteval))
	      goto error;
	  *p_out++ = byteval;
      }
    *out = buf;
    *out_len = len;
    return 1;
  error:
    free (buf);
    return 0;
}

static void
fnct_CastToBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CastToBlob(generic value)
/   or
/ CastToBlob(generic value, boolen hex_input)
/
/ returns a BLOB value [if conversion is possible] 
/ or NULL in any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    int is_hex = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
	  is_hex = sqlite3_value_int (argv[1]);
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_BLOB)
      {
	  p_blob = sqlite3_value_blob (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
	  if (is_hex)
	    {
		/* attempting to convert Hexadecimal input */
		unsigned char *blob;
		int bytes;
		if (!parseHexString (p_blob, n_bytes, &blob, &bytes))
		  {
		      sqlite3_result_null (context);
		      return;
		  }
		sqlite3_result_blob (context, blob, bytes, free);
		return;
	    }
	  sqlite3_result_blob (context, p_blob, n_bytes, SQLITE_TRANSIENT);
	  return;
      }
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
      {
	  p_blob = sqlite3_value_text (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
	  if (is_hex)
	    {
		/* attempting to convert Hexadecimal input */
		unsigned char *blob;
		int bytes;
		if (!parseHexString (p_blob, n_bytes, &blob, &bytes))
		  {
		      sqlite3_result_null (context);
		      return;
		  }
		sqlite3_result_blob (context, blob, bytes, free);
		return;
	    }
	  sqlite3_result_blob (context, p_blob, n_bytes, SQLITE_TRANSIENT);
	  return;
      }
    sqlite3_result_null (context);
}

static void
fnct_ForceAsNull (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ForceAsNull(generic val1, generic val2)
/
/ returns a NULL value if val1 and val2 are equal 
/ (and exactly of the same type)
/ return val1 in any other case
*/
    int type1;
    int type2;
    const unsigned char *p_blob;
    int n_bytes;
    const unsigned char *p_blob2;
    int n_bytes2;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    type1 = sqlite3_value_type (argv[0]);
    type2 = sqlite3_value_type (argv[1]);
    if (type1 == type2)
      {
	  switch (type1)
	    {
	    case SQLITE_INTEGER:
		if (sqlite3_value_int64 (argv[0]) ==
		    sqlite3_value_int64 (argv[1]))
		  {
		      sqlite3_result_null (context);
		      return;
		  }
		break;
	    case SQLITE_FLOAT:
		if (sqlite3_value_double (argv[0]) ==
		    sqlite3_value_double (argv[1]))
		  {
		      sqlite3_result_null (context);
		      return;
		  }
		break;
	    case SQLITE_TEXT:
		p_blob = sqlite3_value_text (argv[0]);
		n_bytes = sqlite3_value_bytes (argv[0]);
		p_blob2 = sqlite3_value_text (argv[1]);
		n_bytes2 = sqlite3_value_bytes (argv[1]);
		if (n_bytes == n_bytes2)
		  {
		      if (strcasecmp
			  ((const char *) p_blob, (const char *) p_blob2) == 0)
			{
			    sqlite3_result_null (context);
			    return;
			}
		  }
		break;
	    case SQLITE_BLOB:
		p_blob = sqlite3_value_blob (argv[0]);
		n_bytes = sqlite3_value_bytes (argv[0]);
		p_blob2 = sqlite3_value_blob (argv[1]);
		n_bytes2 = sqlite3_value_bytes (argv[1]);
		if (n_bytes == n_bytes2)
		  {
		      if (memcmp (p_blob, p_blob2, n_bytes) == 0)
			{
			    sqlite3_result_null (context);
			    return;
			}
		  }
		break;
	    case SQLITE_NULL:
		sqlite3_result_null (context);
		return;
	    };
      }
/* returning the first argument */
    switch (type1)
      {
      case SQLITE_INTEGER:
	  sqlite3_result_int64 (context, sqlite3_value_int64 (argv[0]));
	  break;
      case SQLITE_FLOAT:
	  sqlite3_result_double (context, sqlite3_value_double (argv[0]));
	  break;
      case SQLITE_TEXT:
	  p_blob = sqlite3_value_text (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
	  sqlite3_result_text (context, (const char *) p_blob, n_bytes,
			       SQLITE_TRANSIENT);
	  break;
      case SQLITE_BLOB:
	  p_blob = sqlite3_value_blob (argv[0]);
	  n_bytes = sqlite3_value_bytes (argv[0]);
	  sqlite3_result_blob (context, p_blob, n_bytes, SQLITE_TRANSIENT);
	  break;
      default:
	  sqlite3_result_null (context);
	  break;
      };
}

static void
fnct_CreateUUID (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ CreateUUID()
/
/ returns a TEXT value containing an UUID
/ [xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxxxxxx]
*/
    unsigned char rnd[16];
    char uuid[64];
    char *p = uuid;
    int i;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    sqlite3_randomness (16, rnd);
    for (i = 0; i < 16; i++)
      {
	  if (i == 4 || i == 6 || i == 8 || i == 10)
	      *p++ = '-';
	  sprintf (p, "%02x", rnd[i]);
	  p += 2;
      }
    *p = '\0';
    uuid[14] = '4';
    uuid[19] = '8';
    sqlite3_result_text (context, uuid, strlen (uuid), SQLITE_TRANSIENT);
}

static void
fnct_MD5Checksum (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ MD5Checksum(blob)
/
/ returns a TEXT value containing an hex MD5 checksum
/ [xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx]
/      or
/ NULL on invalid arguments
*/
    void *md5;
    char *checksum;
    const unsigned char *blob;
    int blob_len;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */

    if (sqlite3_value_type (argv[0]) == SQLITE_BLOB)
      {
	  blob = sqlite3_value_blob (argv[0]);
	  blob_len = sqlite3_value_bytes (argv[0]);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
      {
	  blob = sqlite3_value_text (argv[0]);
	  blob_len = sqlite3_value_bytes (argv[0]);
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
/* creating an MD5 object */
    md5 = gaiaCreateMD5Checksum ();
/* evaluating the BLOB */
    gaiaUpdateMD5Checksum (md5, blob, blob_len);
    checksum = gaiaFinalizeMD5Checksum (md5);
    gaiaFreeMD5Checksum (md5);
    if (checksum == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, checksum, strlen (checksum), free);
}

static void
fnct_MD5TotalChecksum_step (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ MD5TotalChecksum(BLOB)
/
/ aggregate function - STEP
/
*/
    void **p;
    void *md5;
    const unsigned char *blob;
    int blob_len;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */

    if (sqlite3_value_type (argv[0]) == SQLITE_BLOB)
      {
	  blob = sqlite3_value_blob (argv[0]);
	  blob_len = sqlite3_value_bytes (argv[0]);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
      {
	  blob = sqlite3_value_text (argv[0]);
	  blob_len = sqlite3_value_bytes (argv[0]);
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p = sqlite3_aggregate_context (context, sizeof (void *));
    if (!(*p))
      {
	  /* this is the first row - creating an MD5 object */
	  md5 = gaiaCreateMD5Checksum ();
	  gaiaUpdateMD5Checksum (md5, blob, blob_len);
	  *p = md5;
      }
    else
      {
	  /* subsequent rows */
	  md5 = *p;
	  gaiaUpdateMD5Checksum (md5, blob, blob_len);
      }
}

static void
fnct_MD5TotalChecksum_final (sqlite3_context * context)
{
/* SQL function:
/ MD5TotalChecksum(BLOB)
/
/ aggregate function - FINAL
/
*/
    void **p;
    void *md5;
    char *checksum;
    p = sqlite3_aggregate_context (context, 0);
    if (!(*p))
      {
	  sqlite3_result_null (context);
	  return;
      }
    md5 = *p;
    checksum = gaiaFinalizeMD5Checksum (md5);
    gaiaFreeMD5Checksum (md5);
    if (checksum == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, checksum, strlen (checksum), free);
}

#ifndef OMIT_MATHSQL		/* supporting SQL math functions */

static int
testInvalidFP (double x)
{
/* testing if this one is an invalid Floating Point */
#ifdef _WIN32
    if (_fpclass (x) == _FPCLASS_NN || _fpclass (x) == _FPCLASS_PN ||
	_fpclass (x) == _FPCLASS_NZ || _fpclass (x) == _FPCLASS_PZ)
	;
    else
	return 1;
#else
    if (fpclassify (x) == FP_NORMAL || fpclassify (x) == FP_ZERO)
	;
    else
	return 1;
#endif
    return 0;
}

static void
fnct_math_acos (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ acos(double X)
/
/ Returns the arc cosine of X, that is, the value whose cosine is X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = acos (sqlite3_value_double (argv[0]));
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = acos (x);
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_asin (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ asin(double X)
/
/ Returns the arc sine of X, that is, the value whose sine is X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = asin (sqlite3_value_double (argv[0]));
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = asin (x);
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_atan (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ atan(double X)
/
/ Returns the arc tangent of X, that is, the value whose tangent is X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = atan (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = atan (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_ceil (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ceil(double X)
/
/ Returns the smallest integer value not less than X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = ceil (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = ceil (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_cos (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ cos(double X)
/
/ Returns the cosine of X, where X is given in radians
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = cos (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = cos (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_cot (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ cot(double X)
/
/ Returns the cotangent of X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    double tang;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    tang = tan (x);
    if (tang == 0.0)
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = 1.0 / tang;
    sqlite3_result_double (context, x);
}

static void
fnct_math_degrees (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ degrees(double X)
/
/ Returns the argument X, converted from radians to degrees
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = x * 57.29577951308232;
    sqlite3_result_double (context, x);
}

static void
fnct_math_exp (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ exp(double X)
/
/ Returns the value of e (the base of natural logarithms) raised to the power of X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = exp (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = exp (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_floor (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ floor(double X)
/
/ Returns the largest integer value not greater than X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = floor (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = floor (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_logn (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ log(double X)
/
/ Returns the natural logarithm of X; that is, the base-e logarithm of X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = log (sqlite3_value_double (argv[0]));
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = log (x);
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_logn2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ log(double B, double X)
/
/ Returns the logarithm of X to the base B
/ or NULL if any error is encountered
*/
    int int_value;
    double x = 0.0;
    double b = 1.0;
    double log1;
    double log2;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	b = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  b = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (x <= 0.0 || b <= 1.0)
      {
	  sqlite3_result_null (context);
	  return;
      }
    log1 = log (x);
    if (testInvalidFP (log1))
      {
	  sqlite3_result_null (context);
	  return;
      }
    log2 = log (b);
    if (testInvalidFP (log2))
      {
	  sqlite3_result_null (context);
	  return;
      }
    sqlite3_result_double (context, log1 / log2);
}

static void
fnct_math_log_2 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ log2(double X)
/
/ Returns the base-2 logarithm of X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    double log1;
    double log2;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    log1 = log (x);
    if (testInvalidFP (log1))
      {
	  sqlite3_result_null (context);
	  return;
      }
    log2 = log (2.0);
    sqlite3_result_double (context, log1 / log2);
}

static void
fnct_math_log_10 (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ log10(double X)
/
/ Returns the base-10 logarithm of X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    double log1;
    double log2;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    log1 = log (x);
    if (testInvalidFP (log1))
      {
	  sqlite3_result_null (context);
	  return;
      }
    log2 = log (10.0);
    sqlite3_result_double (context, log1 / log2);
}

static void
fnct_math_pi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ pi(void)
/
/ Returns the value of (pi)
*/
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    sqlite3_result_double (context, 3.14159265358979323846);
}

static void
fnct_math_pow (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ pow(double X, double Y)
/
/ Returns the value of X raised to the power of Y.
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    double y;
    double p;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_FLOAT)
	y = sqlite3_value_double (argv[1]);
    else if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[1]);
	  y = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p = pow (x, y);
    if (testInvalidFP (p))
	sqlite3_result_null (context);
    else
	sqlite3_result_double (context, p);
}

static void
fnct_math_stddev_step (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ stddev_pop(double X)
/ stddev_samp(double X)
/ var_pop(double X)
/ var_samp(double X)
/
/ aggregate function - STEP
/
*/
    struct stddev_str *p;
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
	return;
    p = sqlite3_aggregate_context (context, sizeof (struct stddev_str));
    if (!(p->cleaned))
      {
	  p->cleaned = 1;
	  p->mean = x;
	  p->quot = 0.0;
	  p->count = 0.0;
      }
    p->count += 1.0;
    p->quot =
	p->quot +
	(((p->count - 1.0) * ((x - p->mean) * (x - p->mean))) / p->count);
    p->mean = p->mean + ((x - p->mean) / p->count);
}

static void
fnct_math_stddev_pop_final (sqlite3_context * context)
{
/* SQL function:
/ stddev_pop(double X)
/ aggregate function -  FINAL
/
*/
    double x;
    struct stddev_str *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = sqrt (p->quot / p->count);
    sqlite3_result_double (context, x);
}

static void
fnct_math_stddev_samp_final (sqlite3_context * context)
{
/* SQL function:
/ stddev_samp(double X)
/ aggregate function -  FINAL
/
*/
    double x;
    struct stddev_str *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = sqrt (p->quot / (p->count - 1.0));
    sqlite3_result_double (context, x);
}

static void
fnct_math_var_pop_final (sqlite3_context * context)
{
/* SQL function:
/ var_pop(double X)
/ aggregate function -  FINAL
/
*/
    double x;
    struct stddev_str *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = p->quot / p->count;
    sqlite3_result_double (context, x);
}

static void
fnct_math_var_samp_final (sqlite3_context * context)
{
/* SQL function:
/ var_samp(double X)
/ aggregate function -  FINAL
/
*/
    double x;
    struct stddev_str *p = sqlite3_aggregate_context (context, 0);
    if (!p)
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = p->quot / (p->count - 1.0);
    sqlite3_result_double (context, x);
}

static void
fnct_math_radians (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ radians(double X)
/
/ Returns the argument X, converted from degrees to radians
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    x = x * .0174532925199432958;
    sqlite3_result_double (context, x);
}


static void
fnct_math_round (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ round(double X)
/
/ Returns the nearest integer, but round halfway cases away from zero
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = math_round (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = math_round (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_sign (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ sign(double X)
/
/ Returns the sign of the argument as -1, 0, or 1, depending on whether X is negative, zero, or positive
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	x = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (x > 0.0)
	sqlite3_result_double (context, 1.0);
    else if (x < 0.0)
	sqlite3_result_double (context, -1.0);
    else
	sqlite3_result_double (context, 0.0);
}

static void
fnct_math_sin (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ sin(double X)
/
/ Returns the sine of X, where X is given in radians
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = sin (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = sin (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_sqrt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ sqrt(double X)
/
/ Returns the square root of a non-negative number X
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = sqrt (sqlite3_value_double (argv[0]));
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = sqrt (x);
	  if (testInvalidFP (x))
	      sqlite3_result_null (context);
	  else
	      sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

static void
fnct_math_tan (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ tan(double X)
/
/ Returns the tangent of X, where X is given in radians
/ or NULL if any error is encountered
*/
    int int_value;
    double x;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
      {
	  x = tan (sqlite3_value_double (argv[0]));
	  sqlite3_result_double (context, x);
      }
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  x = int_value;
	  x = tan (x);
	  sqlite3_result_double (context, x);
      }
    else
	sqlite3_result_null (context);
}

#endif /* end supporting SQL math functions */

static void
fnct_GeomFromExifGpsBlob (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ GeomFromExifGpsBlob(BLOB encoded image)
/
/ returns:
/ a POINT geometry
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geom;
    unsigned char *geoblob;
    int geosize;
    double longitude;
    double latitude;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (gaiaGetGpsCoords (p_blob, n_bytes, &longitude, &latitude))
      {
	  geom = gaiaAllocGeomColl ();
	  geom->Srid = 4326;
	  gaiaAddPointToGeomColl (geom, longitude, latitude);
	  gaiaToSpatiaLiteBlobWkb (geom, &geoblob, &geosize);
	  gaiaFreeGeomColl (geom);
	  sqlite3_result_blob (context, geoblob, geosize, free);
      }
    else
	sqlite3_result_null (context);
}

static char *
guess_mime_type (const unsigned char *p_blob, int n_bytes)
{
/* guessing the mime-type corresponding to some BLOB */
    int blob_type;
    const char *mime = NULL;
    int len;
    char *string = NULL;
    blob_type = gaiaGuessBlobType (p_blob, n_bytes);
    switch (blob_type)
      {
      case GAIA_ZIP_BLOB:
	  mime = "application/zip";
	  break;
      case GAIA_PDF_BLOB:
	  mime = "application/pdf";
	  break;
      case GAIA_TIFF_BLOB:
	  mime = "image/tiff";
	  break;
      case GAIA_GIF_BLOB:
	  mime = "image/gif";
	  break;
      case GAIA_PNG_BLOB:
	  mime = "image/png";
	  break;
      case GAIA_JPEG_BLOB:
      case GAIA_EXIF_BLOB:
      case GAIA_EXIF_GPS_BLOB:
	  mime = "image/jpeg";
	  break;
#ifdef ENABLE_LIBXML2		/* including LIBXML2 */
      case GAIA_XML_BLOB:
	  mime = "application/xml";
	  if (gaiaIsSvgXmlBlob (p_blob, n_bytes))
	      mime = "image/svg+xml";
	  break;
#endif /* end including LIBXML2 */
      };
    if (mime != NULL)
      {
	  len = strlen (mime);
	  string = malloc (len + 1);
	  strcpy (string, mime);
      }
    return string;
}

static void
fnct_GetMimeType (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GetMimeType(BLOB)
/
/ returns:
/ the Mime-Type corresponding to the BLOB
/ or NULL if any error is encountered or no valid mime is defined
*/
    unsigned char *p_blob;
    int n_bytes;
    char *mime = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    mime = guess_mime_type (p_blob, n_bytes);
    if (mime == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, mime, strlen (mime), free);
}

static void
blob_guess (sqlite3_context * context, int argc, sqlite3_value ** argv,
	    int request)
{
/* SQL function:
/ IsGifBlob(BLOB encoded image)
/ IsPngBlob, IsJpegBlob, IsExifBlob, IsExifGpsBlob, IsTiffBlob,
/ IsZipBlob, IsPdfBlob,IsGeometryBlob
/
/ returns:
/ 1 if the required BLOB_TYPE is TRUE
/ 0 otherwise
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int blob_type;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    blob_type = gaiaGuessBlobType (p_blob, n_bytes);
    if (request == GAIA_GEOMETRY_BLOB)
      {
	  if (blob_type == GAIA_GEOMETRY_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_ZIP_BLOB)
      {
	  if (blob_type == GAIA_ZIP_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_PDF_BLOB)
      {
	  if (blob_type == GAIA_PDF_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_TIFF_BLOB)
      {
	  if (blob_type == GAIA_TIFF_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_GIF_BLOB)
      {
	  if (blob_type == GAIA_GIF_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_PNG_BLOB)
      {
	  if (blob_type == GAIA_PNG_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_JPEG_BLOB)
      {
	  if (blob_type == GAIA_JPEG_BLOB || blob_type == GAIA_EXIF_BLOB
	      || blob_type == GAIA_EXIF_GPS_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_EXIF_BLOB)
      {
	  if (blob_type == GAIA_EXIF_BLOB || blob_type == GAIA_EXIF_GPS_BLOB)
	    {
		sqlite3_result_int (context, 1);
	    }
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_EXIF_GPS_BLOB)
      {
	  if (blob_type == GAIA_EXIF_GPS_BLOB)
	    {
		sqlite3_result_int (context, 1);
	    }
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    if (request == GAIA_WEBP_BLOB)
      {
	  if (blob_type == GAIA_WEBP_BLOB)
	      sqlite3_result_int (context, 1);
	  else
	      sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_result_int (context, -1);
}

/*
/ the following functions simply readdress the blob_guess()
/ setting the appropriate request mode
*/

static void
fnct_IsGeometryBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_GEOMETRY_BLOB);
}

static void
fnct_IsZipBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_ZIP_BLOB);
}

static void
fnct_IsPdfBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_PDF_BLOB);
}

static void
fnct_IsTiffBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_TIFF_BLOB);
}

static void
fnct_IsGifBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_GIF_BLOB);
}

static void
fnct_IsPngBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_PNG_BLOB);
}

static void
fnct_IsJpegBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_JPEG_BLOB);
}

static void
fnct_IsExifBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_EXIF_BLOB);
}

static void
fnct_IsExifGpsBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_EXIF_GPS_BLOB);
}

static void
fnct_IsWebPBlob (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    blob_guess (context, argc, argv, GAIA_WEBP_BLOB);
}

static void
fnct_BlobFromFile (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BlobFromFile(TEXT filepath)
/
/ returns:
/ some BLOB on success
/ or NULL on failure
*/
    unsigned char *p_blob;
    int n_bytes;
    int max_blob;
    int rd;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    const char *path = NULL;
    FILE *in = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
	path = (const char *) sqlite3_value_text (argv[0]);
    if (path == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    in = fopen (path, "rb");
    if (in == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    else
      {
	  /* querying the file length */
	  if (fseek (in, 0, SEEK_END) < 0)
	    {
		sqlite3_result_null (context);
		fclose (in);
		return;
	    }
	  n_bytes = ftell (in);
	  max_blob = sqlite3_limit (sqlite, SQLITE_LIMIT_LENGTH, -1);
	  if (n_bytes > max_blob)
	    {
		/* too big; cannot be stored into a BLOB */
		sqlite3_result_null (context);
		fclose (in);
		return;
	    }
	  rewind (in);
	  p_blob = malloc (n_bytes);
	  /* attempting to load the BLOB from the file */
	  rd = fread (p_blob, 1, n_bytes, in);
	  fclose (in);
	  if (rd != n_bytes)
	    {
		/* read error */
		free (p_blob);
		sqlite3_result_null (context);
		return;
	    }
	  sqlite3_result_blob (context, p_blob, n_bytes, free);
      }
}

static void
fnct_BlobToFile (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ BlobToFile(BLOB payload, TEXT filepath)
/
/ returns:
/ 1 on success
/ or 0 on failure
*/
    unsigned char *p_blob;
    int n_bytes;
    const char *path = NULL;
    FILE *out = NULL;
    int ret = 1;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, 0);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_TEXT)
	path = (const char *) sqlite3_value_text (argv[1]);
    if (path == NULL)
      {
	  sqlite3_result_int (context, 0);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    out = fopen (path, "wb");
    if (out == NULL)
	ret = 0;
    else
      {
	  /* exporting the BLOB into the file */
	  int wr = fwrite (p_blob, 1, n_bytes, out);
	  if (wr != n_bytes)
	      ret = 0;
	  fclose (out);
      }
    sqlite3_result_int (context, ret);
}

static void
fnct_ExportDXF (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ ExportDXF(TEXT out_dir, TEXT filename, TEXT sql_query, TEXT layer_col_name,
/           TEXT geom_col_name, TEXT label_col_name, TEXT text_height_col_name,
/           TEXT text_rotation_col_name, BLOB geom_filter)
/     or
/ ExportDXF(TEXT out_dir, TEXT filename, TEXT sql_query, TEXT layer_col_name,
/           TEXT geom_col_name, TEXT label_col_name, TEXT text_height_col_name,
/           TEXT text_rotation_col_name, BLOB geom_filter, INT precision)
/
/ returns:
/ 1 on success
/ or 0 on failure
*/
    unsigned char *p_blob;
    int n_bytes;
    char *path;
    const char *dir_path = NULL;
    const char *filename = NULL;
    FILE *out = NULL;
    const char *sql_query = NULL;
    const char *layer_col_name = NULL;
    const char *geom_col_name = NULL;
    const char *label_col_name = NULL;
    const char *text_height_col_name = NULL;
    const char *text_rotation_col_name = NULL;
    gaiaGeomCollPtr geom = NULL;
    int precision = 3;
    int ret = 1;
    sqlite3 *db_handle = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_TEXT)
	dir_path = (const char *) sqlite3_value_text (argv[0]);
    if (sqlite3_value_type (argv[1]) == SQLITE_TEXT)
	filename = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) == SQLITE_TEXT)
	sql_query = (const char *) sqlite3_value_text (argv[2]);
    if (sqlite3_value_type (argv[3]) == SQLITE_TEXT)
	layer_col_name = (const char *) sqlite3_value_text (argv[3]);
    if (sqlite3_value_type (argv[4]) == SQLITE_TEXT)
	geom_col_name = (const char *) sqlite3_value_text (argv[4]);
    if (sqlite3_value_type (argv[5]) == SQLITE_TEXT)
	label_col_name = (const char *) sqlite3_value_text (argv[5]);
    if (sqlite3_value_type (argv[6]) == SQLITE_TEXT)
	text_height_col_name = (const char *) sqlite3_value_text (argv[6]);
    if (sqlite3_value_type (argv[7]) == SQLITE_TEXT)
	text_rotation_col_name = (const char *) sqlite3_value_text (argv[7]);
    if (sqlite3_value_type (argv[8]) == SQLITE_BLOB)
      {
	  p_blob = (unsigned char *) sqlite3_value_blob (argv[8]);
	  n_bytes = sqlite3_value_bytes (argv[8]);
	  geom = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
      }
    if (argc == 10)
      {
	  if (sqlite3_value_type (argv[9]) == SQLITE_INTEGER)
	      precision = sqlite3_value_int (argv[9]);
      }
    if (dir_path == NULL || filename == NULL || sql_query == NULL
	|| layer_col_name == NULL || geom_col_name == NULL)
      {
	  sqlite3_result_int (context, 0);
	  if (geom != NULL)
	      gaiaFreeGeomColl (geom);
	  return;
      }

    path = sqlite3_mprintf ("%s/%s.dxf", dir_path, filename);
    out = fopen (path, "wb");
    if (out == NULL)
      {
	  ret = 0;
	  spatialite_e ("ExportDXF error - unable to create \"%s\"\n", path);
      }
    else
      {
	  /* exporting the DXF */
	  gaiaDxfWriter dxf;
	  gaiaDxfWriterInit (&dxf, out, precision, GAIA_DXF_V12);
	  ret = gaiaExportDxf (&dxf, db_handle, sql_query, layer_col_name,
			       geom_col_name, label_col_name,
			       text_height_col_name, text_rotation_col_name,
			       geom);
	  if (ret > 0)
	      ret = 1;
	  fclose (out);
      }
    sqlite3_result_int (context, ret);
    if (geom != NULL)
	gaiaFreeGeomColl (geom);
    sqlite3_free (path);
}

static void
fnct_CountUnsafeTriggers (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ CountUnsafeTriggers()
/
/ returns:
/ the total count of *unsafe* triggers found
/ 0 if no dubious trigger has been identifiedfailure
*/
    int ret;
    int i;
    char **results;
    int rows;
    int columns;
    const char *sql;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    int count = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */

/* checking all Triggers */
    sql = "SELECT Count(*) FROM sqlite_master WHERE "
	"type IN ('trigger', 'view') AND (sql LIKE '%BlobFromFile%' "
	"OR sql LIKE '%BlobToFile%' OR sql LIKE '%XB_LoadXML%' "
	"OR sql LIKE '%XB_StoreXML%')";
    ret = sqlite3_get_table (sqlite, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		count = atoi (results[(i * columns) + 0]);
	    }
      }
    sqlite3_free_table (results);
  unknown:
    sqlite3_result_int (context, count);
}

static void
fnct_GeodesicLength (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ GeodesicLength(BLOB encoded GEOMETRYCOLLECTION)
/
/ returns  the total Geodesic length for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double l;
    double length = 0.0;
    double a;
    double b;
    double rf;
    gaiaGeomCollPtr geo = NULL;
    gaiaLinestringPtr line;
    gaiaPolygonPtr polyg;
    gaiaRingPtr ring;
    int ib;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (getEllipsoidParams (sqlite, geo->Srid, &a, &b, &rf))
	    {
		line = geo->FirstLinestring;
		while (line)
		  {
		      /* Linestrings */
		      l = gaiaGeodesicTotalLength (a, b, rf,
						   line->DimensionModel,
						   line->Coords, line->Points);
		      if (l < 0.0)
			{
			    length = -1.0;
			    break;
			}
		      length += l;
		      line = line->Next;
		  }
		if (length >= 0)
		  {
		      /* Polygons */
		      polyg = geo->FirstPolygon;
		      while (polyg)
			{
			    /* exterior Ring */
			    ring = polyg->Exterior;
			    l = gaiaGeodesicTotalLength (a, b, rf,
							 ring->DimensionModel,
							 ring->Coords,
							 ring->Points);
			    if (l < 0.0)
			      {
				  length = -1.0;
				  break;
			      }
			    length += l;
			    for (ib = 0; ib < polyg->NumInteriors; ib++)
			      {
				  /* interior Rings */
				  ring = polyg->Interiors + ib;
				  l = gaiaGeodesicTotalLength (a, b, rf,
							       ring->
							       DimensionModel,
							       ring->Coords,
							       ring->Points);
				  if (l < 0.0)
				    {
					length = -1.0;
					break;
				    }
				  length += l;
			      }
			    if (length < 0.0)
				break;
			    polyg = polyg->Next;
			}
		  }
		if (length < 0.0)
		    sqlite3_result_null (context);
		else
		    sqlite3_result_double (context, length);
	    }
	  else
	      sqlite3_result_null (context);
	  gaiaFreeGeomColl (geo);
      }
}

static void
fnct_GreatCircleLength (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ GreatCircleLength(BLOB encoded GEOMETRYCOLLECTION)
/
/ returns  the total Great Circle length for current geometry 
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    double length = 0.0;
    double a;
    double b;
    double rf;
    gaiaGeomCollPtr geo = NULL;
    gaiaLinestringPtr line;
    gaiaPolygonPtr polyg;
    gaiaRingPtr ring;
    int ib;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkb (p_blob, n_bytes);
    if (!geo)
	sqlite3_result_null (context);
    else
      {
	  if (getEllipsoidParams (sqlite, geo->Srid, &a, &b, &rf))
	    {
		line = geo->FirstLinestring;
		while (line)
		  {
		      /* Linestrings */
		      length +=
			  gaiaGreatCircleTotalLength (a, b,
						      line->DimensionModel,
						      line->Coords,
						      line->Points);
		      line = line->Next;
		  }
		if (length >= 0)
		  {
		      /* Polygons */
		      polyg = geo->FirstPolygon;
		      while (polyg)
			{
			    /* exterior Ring */
			    ring = polyg->Exterior;
			    length +=
				gaiaGreatCircleTotalLength (a, b,
							    ring->
							    DimensionModel,
							    ring->Coords,
							    ring->Points);
			    for (ib = 0; ib < polyg->NumInteriors; ib++)
			      {
				  /* interior Rings */
				  ring = polyg->Interiors + ib;
				  length +=
				      gaiaGreatCircleTotalLength (a, b,
								  ring->
								  DimensionModel,
								  ring->Coords,
								  ring->Points);
			      }
			    polyg = polyg->Next;
			}
		  }
		sqlite3_result_double (context, length);
	    }
	  else
	      sqlite3_result_null (context);
	  gaiaFreeGeomColl (geo);
      }
}

static void
convertUnit (sqlite3_context * context, int argc, sqlite3_value ** argv,
	     int unit_from, int unit_to)
{
/* SQL functions:
/ CvtToKm(), CvtToDm(), CvtToCm(), CvtToMm(), CvtToKmi(), CvtToIn(), CvtToFt(),
/ CvtToYd(), CvtToMi(), CvtToFath(), CvtToCh(), CvtToLink(), CvtToUsIn(), 
/ CvtToUsFt(), CvtToUsYd(), CvtToUsCh(), CvtToUsMi(), CvtToIndFt(), 
/ CvtToIndYd(), CvtToIndCh(), 
/ CvtFromKm(), CvtFromDm(), CvtFromCm(), CvtFromMm(), CvtFromKmi(), 
/ CvtFromIn(), CvtFromFt(), CvtFromYd(), CvtFromMi(), CvtFromFath(), 
/ CvtFromCh(), CvtFromLink(), CvtFromUsIn(), CvtFromUsFt(), CvtFromUsYd(), 
/ CvtFromUsCh(), CvtFromUsMi(), CvtFromIndFt(), CvtFromIndYd(), 
/ CvtFromIndCh()
/
/ converts a Length from one unit to a different one
/ or NULL if any error is encountered
*/
    double cvt;
    double value;
    int int_value;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) == SQLITE_FLOAT)
	value = sqlite3_value_double (argv[0]);
    else if (sqlite3_value_type (argv[0]) == SQLITE_INTEGER)
      {
	  int_value = sqlite3_value_int (argv[0]);
	  value = int_value;
      }
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (!gaiaConvertLength (value, unit_from, unit_to, &cvt))
	sqlite3_result_null (context);
    else
	sqlite3_result_double (context, cvt);
}

static void
fnct_cvtToKm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_KM);
}

static void
fnct_cvtToDm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_DM);
}

static void
fnct_cvtToCm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_CM);
}

static void
fnct_cvtToMm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_MM);
}

static void
fnct_cvtToKmi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_KMI);
}

static void
fnct_cvtToIn (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_IN);
}

static void
fnct_cvtToFt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_FT);
}

static void
fnct_cvtToYd (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_YD);
}

static void
fnct_cvtToMi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_MI);
}

static void
fnct_cvtToFath (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_FATH);
}

static void
fnct_cvtToCh (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_CH);
}

static void
fnct_cvtToLink (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_LINK);
}

static void
fnct_cvtToUsIn (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_US_IN);
}

static void
fnct_cvtToUsFt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_US_FT);
}

static void
fnct_cvtToUsYd (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_US_YD);
}

static void
fnct_cvtToUsCh (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_US_CH);
}

static void
fnct_cvtToUsMi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_US_MI);
}

static void
fnct_cvtToIndFt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_IND_FT);
}

static void
fnct_cvtToIndYd (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_IND_YD);
}

static void
fnct_cvtToIndCh (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_M, GAIA_IND_CH);
}

static void
fnct_cvtFromKm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_KM, GAIA_M);
}

static void
fnct_cvtFromDm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_DM, GAIA_M);
}

static void
fnct_cvtFromCm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_CM, GAIA_M);
}

static void
fnct_cvtFromMm (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_MM, GAIA_M);
}

static void
fnct_cvtFromKmi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_KMI, GAIA_M);
}

static void
fnct_cvtFromIn (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_IN, GAIA_M);
}

static void
fnct_cvtFromFt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_FT, GAIA_M);
}

static void
fnct_cvtFromYd (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_YD, GAIA_M);
}

static void
fnct_cvtFromMi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_MI, GAIA_M);
}

static void
fnct_cvtFromFath (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_FATH, GAIA_M);
}

static void
fnct_cvtFromCh (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_CH, GAIA_M);
}

static void
fnct_cvtFromLink (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_LINK, GAIA_M);
}

static void
fnct_cvtFromUsIn (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_US_IN, GAIA_M);
}

static void
fnct_cvtFromUsFt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_US_FT, GAIA_M);
}

static void
fnct_cvtFromUsYd (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_US_YD, GAIA_M);
}

static void
fnct_cvtFromUsCh (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_US_CH, GAIA_M);
}

static void
fnct_cvtFromUsMi (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_US_MI, GAIA_M);
}

static void
fnct_cvtFromIndFt (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_IND_FT, GAIA_M);
}

static void
fnct_cvtFromIndYd (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_IND_YD, GAIA_M);
}

static void
fnct_cvtFromIndCh (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
    convertUnit (context, argc, argv, GAIA_IND_CH, GAIA_M);
}

#ifdef ENABLE_LIBXML2		/* including LIBXML2 */

static void
fnct_CreateStylingTables (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ CreateStylingTables()
/  or
/ CreateStylingTables(bool relaxed)
/
/ creates any SLD/SE related table 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int relaxed = 0;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 1)
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_INTEGER)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
	  relaxed = sqlite3_value_int (argv[0]);
      }

    if (!createStylingTables (sqlite, relaxed))
	goto error;
    updateSpatiaLiteHistory (sqlite, "*** SE Styling ***", NULL,
			     "Styling tables successfully created");
    sqlite3_result_int (context, 1);
    return;

  error:
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_RegisterExternalGraphic (sqlite3_context * context, int argc,
			      sqlite3_value ** argv)
{
/* SQL function:
/ RegisterExternalGraphic(String xlink_href, BLOB resource)
/  or
/ RegisterExternalGraphic(String xlink_href, BLOB resource, String file_name)
/
/ insert or updates an External Graphic 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int ret;
    const char *xlink_href;
    const char *title = NULL;
    const char *abstract = NULL;
    const char *file_name = NULL;
    const unsigned char *p_blob;
    int n_bytes;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (argc == 5)
      {
	  /* optional extra args */
	  if (sqlite3_value_type (argv[2]) != SQLITE_TEXT)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
	  if (sqlite3_value_type (argv[3]) != SQLITE_TEXT)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
	  if (sqlite3_value_type (argv[4]) != SQLITE_TEXT)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    xlink_href = (const char *) sqlite3_value_text (argv[0]);
    p_blob = sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    if (argc == 5)
      {
	  title = (const char *) sqlite3_value_text (argv[2]);
	  abstract = (const char *) sqlite3_value_text (argv[3]);
	  file_name = (const char *) sqlite3_value_text (argv[4]);
      }
    ret = register_external_graphic (sqlite, xlink_href, p_blob, n_bytes,
				     title, abstract, file_name);
    sqlite3_result_int (context, ret);
}

static void
fnct_RegisterVectorStyledLayer (sqlite3_context * context, int argc,
				sqlite3_value ** argv)
{
/* SQL function:
/ RegisterVectorStyledLayer(String f_table_name, String f_geometry_column, 
/			BLOB style)
/  or
/ RegisterVectorStyledLayer(String f_table_name, String f_geometry_column, 
/			Integer style_id, BLOB style)
/
/ inserts or updates a Vector Styled Layer 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int ret;
    const char *f_geometry_column;
    const char *f_table_name;
    int style_id = -1;
    const unsigned char *p_blob;
    int n_bytes;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (argc == 4)
      {
	  /* optional extra args */
	  if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
	  if (sqlite3_value_type (argv[3]) != SQLITE_BLOB)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    else
      {
	  /* no extra-args */
	  if (sqlite3_value_type (argv[2]) != SQLITE_BLOB)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    f_table_name = (const char *) sqlite3_value_text (argv[0]);
    f_geometry_column = (const char *) sqlite3_value_text (argv[1]);
    if (argc == 4)
      {
	  style_id = sqlite3_value_int (argv[2]);
	  p_blob = sqlite3_value_blob (argv[3]);
	  n_bytes = sqlite3_value_bytes (argv[3]);
      }
    else
      {
	  p_blob = sqlite3_value_blob (argv[2]);
	  n_bytes = sqlite3_value_bytes (argv[2]);
      }
    ret = register_vector_styled_layer (sqlite, f_table_name,
					f_geometry_column, style_id, p_blob,
					n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_RegisterRasterStyledLayer (sqlite3_context * context, int argc,
				sqlite3_value ** argv)
{
/* SQL function:
/ RegisterRasterStyledLayer(String coverage_name, BLOB style)
/  or
/ RegisterRasterStyledLayer(String coverage_name, Integer style_id,
/			BLOB style)
/
/ inserts or updates a Raster Styled Layer 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int ret;
    const char *coverage_name;
    int style_id = -1;
    const unsigned char *p_blob;
    int n_bytes;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (argc == 3)
      {
	  /* optional extra args */
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
	  if (sqlite3_value_type (argv[2]) != SQLITE_BLOB)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    else
      {
	  /* no extra-args */
	  if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    coverage_name = (const char *) sqlite3_value_text (argv[0]);
    if (argc == 3)
      {
	  style_id = sqlite3_value_int (argv[1]);
	  p_blob = sqlite3_value_blob (argv[2]);
	  n_bytes = sqlite3_value_bytes (argv[2]);
      }
    else
      {
	  p_blob = sqlite3_value_blob (argv[1]);
	  n_bytes = sqlite3_value_bytes (argv[1]);
      }
    ret = register_raster_styled_layer (sqlite, coverage_name, style_id,
					p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_RegisterStyledGroup (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ RegisterStyledGroup(String group_name, String coverage_name, 
/		      Integer style_id [, Integer paint_order)
/  or
/ RegisterStyledGroup(String group_name, String f_table_name,
/		      String f_geometry_column, Integer style_id
/		      [, Integer paint_order)
/
/ inserts or updates a Styled Group item 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int ret;
    const char *group_name;
    const char *f_table_name = NULL;
    const char *f_geometry_column = NULL;
    const char *coverage_name = NULL;
    int style_id;
    int paint_order = -1;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 3)
      {
	  /* raster layer - default */
	  if (sqlite3_value_type (argv[0]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[1]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	    {
		group_name = (const char *) sqlite3_value_text (argv[0]);
		coverage_name = (const char *) sqlite3_value_text (argv[1]);
		style_id = sqlite3_value_int (argv[2]);
	    }
	  else
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    else if (argc == 4)
      {
	  if (sqlite3_value_type (argv[0]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[1]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[2]) == SQLITE_INTEGER
	      && sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	    {
		/* raster layer - paint_order */
		group_name = (const char *) sqlite3_value_text (argv[0]);
		coverage_name = (const char *) sqlite3_value_text (argv[1]);
		style_id = sqlite3_value_int (argv[2]);
		paint_order = sqlite3_value_int (argv[3]);
	    }
	  else if (sqlite3_value_type (argv[0]) == SQLITE_TEXT
		   && sqlite3_value_type (argv[1]) == SQLITE_TEXT
		   && sqlite3_value_type (argv[2]) == SQLITE_TEXT
		   && sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
	    {
		/* vector layer - default */
		group_name = (const char *) sqlite3_value_text (argv[0]);
		f_table_name = (const char *) sqlite3_value_text (argv[1]);
		f_geometry_column = (const char *) sqlite3_value_text (argv[2]);
		style_id = sqlite3_value_int (argv[3]);
	    }
	  else
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    else if (argc == 5)
      {
	  /* vector layer - paint_order */
	  if (sqlite3_value_type (argv[0]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[1]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[2]) == SQLITE_TEXT
	      && sqlite3_value_type (argv[3]) == SQLITE_INTEGER
	      && sqlite3_value_type (argv[4]) == SQLITE_INTEGER)
	    {
		group_name = (const char *) sqlite3_value_text (argv[0]);
		f_table_name = (const char *) sqlite3_value_text (argv[1]);
		f_geometry_column = (const char *) sqlite3_value_text (argv[2]);
		style_id = sqlite3_value_int (argv[3]);
		paint_order = sqlite3_value_int (argv[4]);
	    }
	  else
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    ret = register_styled_group (sqlite, group_name, f_table_name,
				 f_geometry_column, coverage_name, style_id,
				 paint_order);
    sqlite3_result_int (context, ret);
}

static void
fnct_SetStyledGroupInfos (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ SetStyledGroupInfos(String group_name, String title, 
/		      String abstract)
/
/ inserts or updates the descriptive infos supporting a Styled Group 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int ret;
    const char *group_name;
    const char *title = NULL;
    const char *abstract = NULL;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[2]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    group_name = (const char *) sqlite3_value_text (argv[0]);
    title = (const char *) sqlite3_value_text (argv[1]);
    abstract = (const char *) sqlite3_value_text (argv[2]);
    ret = styled_group_set_infos (sqlite, group_name, title, abstract);
    sqlite3_result_int (context, ret);
}

static void
fnct_CreateIsoMetadataTables (sqlite3_context * context, int argc,
			      sqlite3_value ** argv)
{
/* SQL function:
/ CreateIsoMetadataTables()
/  or
/ CreateIsoMetadataTables(bool relaxed)
/
/ creates any ISO Metadata related table 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int relaxed = 0;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (argc == 1)
      {
	  if (sqlite3_value_type (argv[0]) != SQLITE_INTEGER)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
	  relaxed = sqlite3_value_int (argv[0]);
      }

    if (!createIsoMetadataTables (sqlite, relaxed))
	goto error;
    updateSpatiaLiteHistory (sqlite, "*** ISO Metadata ***", NULL,
			     "ISO Metadata tables successfully created");
    sqlite3_result_int (context, 1);
    return;

  error:
    sqlite3_result_int (context, 0);
    return;
}

static void
fnct_GetIsoMetadataId (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ GetIsoMetadataId(String fileIdentifier)
/
/ return the ID of the row corresponding to "fileIdentifier"
/ 0 on failure / -1 on invalid argument
*/
    const char *fileIdentifier;
    sqlite3_int64 id;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    fileIdentifier = (const char *) sqlite3_value_text (argv[0]);
    if (!get_iso_metadata_id (sqlite, fileIdentifier, &id))
	sqlite3_result_int (context, 0);
    else
	sqlite3_result_int64 (context, id);
}

static void
fnct_RegisterIsoMetadata (sqlite3_context * context, int argc,
			  sqlite3_value ** argv)
{
/* SQL function:
/ RegisterIsoMetadata(String scope, BLOB metadata)
/  or
/ RegisterIsoMetadata(String scope, BLOB metadata, 
/			Integer id)
/  or
/ RegisterIsoMetadata(String scope, BLOB metadata, 
/			String fileIdentifier)
/
/ insert or updates a Raster Styled Layer 
/ returns 1 on success
/ 0 on failure, -1 on invalid arguments
*/
    int ret;
    const char *scope;
    const unsigned char *p_blob;
    int n_bytes;
    sqlite3_int64 id = -1;
    const char *fileIdentifier = NULL;
    sqlite3 *sqlite = sqlite3_context_db_handle (context);
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (argc == 3)
      {
	  /* optional extra args */
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER ||
	      sqlite3_value_type (argv[2]) == SQLITE_TEXT)
	      ;
	  else
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    scope = (const char *) sqlite3_value_text (argv[0]);
    p_blob = sqlite3_value_blob (argv[1]);
    n_bytes = sqlite3_value_bytes (argv[1]);
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      id = sqlite3_value_int64 (argv[2]);
	  if (sqlite3_value_type (argv[2]) == SQLITE_TEXT)
	      fileIdentifier = (const char *) sqlite3_value_text (argv[2]);
      }
    ret = register_iso_metadata (sqlite, scope, p_blob, n_bytes, &id,
				 fileIdentifier);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_Create (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_Create(BLOB XMLdocument)
/ XB_Create(BLOB XMLdocument, bool compressed)
/ XB_Create(BLOB XMLdocument, bool compressed, text SchemaURI)
/ XB_Create(BLOB XMLdocument, bool compressed, int InternalSchemaURI)
/
/ returns the current XmlBlob by parsing an XMLdocument 
/ or NULL if any error is encountered
/
/ - the XMLdocument should be "well formed"
/ - if *compressed* is TRUE (default) the XmlBlob would be zipped
/ - if *SchemaURI* in not NULL then only XMLdocuments succesfully
/   passing a formal Schema Validation will be accepted as valid
/ - if *InternalSchamaURI* is defined (any numeric value) then an
/   attempt will be made in order to identify a SchemaURI defined
/   internally within the XMLDocument itself.
/   if such internal SchemaURI doesn't exists, or if the formal
/   Schema Validation fails, NULL will be returned.
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *xml;
    int xml_len;
    int compressed = 1;
    int use_internal_schema_uri = 0;
    const char *schemaURI = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc >= 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
	      use_internal_schema_uri = 1;
	  else if (sqlite3_value_type (argv[2]) != SQLITE_TEXT)
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    xml = (const unsigned char *) sqlite3_value_blob (argv[0]);
    xml_len = sqlite3_value_bytes (argv[0]);
    if (argc >= 2)
	compressed = sqlite3_value_int (argv[1]);
    if (use_internal_schema_uri)
      {
	  /* using the SchemaURI internally defined within the XMLDocument */
	  char *internalSchemaURI =
	      gaiaXmlGetInternalSchemaURI (sqlite3_user_data (context), xml,
					   xml_len);
	  if (internalSchemaURI == NULL)
	    {
		/* unable to identify the SchemaURI */
		p_result = NULL;
	    }
	  else
	    {
		/* ok, attempting to validate using the internal SchemaURI */
		gaiaXmlToBlob (sqlite3_user_data (context), xml, xml_len,
			       compressed, internalSchemaURI, &p_result,
			       &len, NULL, NULL);
		free (internalSchemaURI);
	    }
      }
    else
      {
	  if (argc == 3)
	      schemaURI = (const char *) sqlite3_value_text (argv[2]);
	  gaiaXmlToBlob (sqlite3_user_data (context), xml, xml_len,
			 compressed, schemaURI, &p_result, &len, NULL, NULL);
      }
    if (p_result == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_XB_LoadXML (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_LoadXML(text path-or-URL)
/
/ returns a generic Text by parsing an XML Document 
/ or NULL if any error is encountered
/
*/
    const char *path_or_url;
    unsigned char *xml;
    int xml_len;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    path_or_url = (const char *) sqlite3_value_text (argv[0]);

/* acquiring the XML Document as a Blob */
    ret = gaiaXmlLoad
	(sqlite3_user_data (context), path_or_url, &xml, &xml_len, NULL);
    if (!ret || xml == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }

    sqlite3_result_blob (context, xml, xml_len, free);
}

static void
fnct_XB_GetPayload (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetPayload(XmlBLOB)
/ XB_GetPayload(XmlBLOB, int format)
/
/ returns the current XMLDocument (as BLOB) by parsing an XmlBLOB 
/ or NULL if any error is encountered
/
/ the returned buffer will be always null-terminated
*/
    const unsigned char *p_blob;
    int n_bytes;
    unsigned char *out;
    int out_len;
    int indent = -1;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (argc == 2)
	indent = sqlite3_value_int (argv[1]);
    gaiaXmlFromBlob (p_blob, n_bytes, indent, &out, &out_len);
    if (out == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    sqlite3_result_blob (context, out, out_len, free);
}

static void
fnct_XB_StoreXML (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_StoreXML(XmlBLOB, text path)
/ XB_StoreXML(XmlBLOB, taxt path, int format)
/
/ exports the current XMLDocument into an external file by parsing an XmlBLOB 
/ return 1 on success, 0 on failure, -1 on invalid args
/
*/
    const unsigned char *p_blob;
    int n_bytes;
    const char *path;
    int indent = -1;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
	    {
		sqlite3_result_int (context, -1);
		return;
	    }
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    path = (const char *) sqlite3_value_text (argv[1]);
    if (argc == 3)
	indent = sqlite3_value_int (argv[2]);
    if (!gaiaXmlStore (p_blob, n_bytes, path, indent))
      {
	  sqlite3_result_int (context, 0);
	  return;
      }
    sqlite3_result_int (context, 1);
}

static void
fnct_XB_GetDocument (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetDocument(XmlBLOB)
/ XB_GetDocument(XmlBLOB, int indent)
/
/ returns the current XMLDocument (as UTF-8 TEXT) by parsing an XmlBLOB 
/ or NULL if any error is encountered
/
/ the returned buffer will be always null-terminated
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *xml;
    int len;
    int indent = -1;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 2)
      {
	  if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (argc == 2)
	indent = sqlite3_value_int (argv[1]);
    xml = gaiaXmlTextFromBlob (p_blob, n_bytes, indent);
    if (xml == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    len = strlen ((const char *) xml);
    sqlite3_result_text (context, (char *) xml, len, free);
}

static void
fnct_XB_SchemaValidate (sqlite3_context * context, int argc,
			sqlite3_value ** argv)
{
/* SQL function:
/ XB_SchemaValidate(XmlBLOB, text SchemaURI)
/ XB_SchemaValidate(XmlBLOB, text SchemaURI, bool compressed)
/ XB_SchemaValidate(XmlBLOB, int InternalSchemaURI)
/ XB_SchemaValidate(XmlBLOB, int InternalSchemaURI, bool compressed)
/
/ returns a validated XmlBLOB object if the SchemaValidation was succesfull
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *p_blob;
    int n_bytes;
    unsigned char *xml;
    int xml_len;
    int compressed = 1;
    const char *schemaURI = NULL;
    int use_internal_schema_uri = 0;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) == SQLITE_INTEGER)
	use_internal_schema_uri = 1;
    else if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (argc == 3)
      {
	  if (sqlite3_value_type (argv[2]) != SQLITE_INTEGER)
	    {
		sqlite3_result_null (context);
		return;
	    }
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    if (argc == 3)
	compressed = sqlite3_value_int (argv[2]);
    gaiaXmlFromBlob (p_blob, n_bytes, -1, &xml, &xml_len);
    if (xml == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (use_internal_schema_uri)
      {
	  /* using the SchemaURI internally defined within the XMLDocument */
	  char *internalSchemaURI =
	      gaiaXmlGetInternalSchemaURI (sqlite3_user_data (context), xml,
					   xml_len);
	  if (internalSchemaURI == NULL)
	    {
		/* unable to identify the SchemaURI */
		p_result = NULL;
	    }
	  else
	    {
		/* ok, attempting to validate using the internal SchemaURI */
		gaiaXmlToBlob (sqlite3_user_data (context), xml, xml_len,
			       compressed, internalSchemaURI, &p_result,
			       &len, NULL, NULL);
		free (internalSchemaURI);
	    }
      }
    else
      {
	  schemaURI = (const char *) sqlite3_value_text (argv[1]);
	  gaiaXmlToBlob (sqlite3_user_data (context), xml, xml_len,
			 compressed, schemaURI, &p_result, &len, NULL, NULL);
      }
    free (xml);
    if (p_result == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_XB_Compress (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_Compress(XmlBLOB)
/
/ returns a compressed XmlBLOB object 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *p_blob;
    int n_bytes;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    gaiaXmlBlobCompression (p_blob, n_bytes, 1, &p_result, &len);
    if (p_result == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_XB_Uncompress (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_Uncompress(XmlBLOB)
/
/ returns an uncompressed XmlBLOB object 
/ or NULL if any error is encountered
*/
    int len;
    unsigned char *p_result = NULL;
    const unsigned char *p_blob;
    int n_bytes;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    gaiaXmlBlobCompression (p_blob, n_bytes, 0, &p_result, &len);
    if (p_result == NULL)
      {
	  sqlite3_result_null (context);
	  return;
      }
    sqlite3_result_blob (context, p_result, len, free);
}

static void
fnct_XB_IsValid (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsValid(XmlBLOB)
/
/ returns TRUE if the current BLOB is an XmlBLOB, FALSE if not 
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsValidXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_IsCompressed (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsCompressed(XmlBLOB)
/
/ returns TRUE if the current BLOB is a compressed XmlBLOB,
/ FALSE if it's a valid uncompressed XmlBLOB 
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsCompressedXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_IsSchemaValidated (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsSchemaValidated(XmlBLOB)
/
/ returns TRUE if the current BLOB is a Schema validated XmlBLOB,
/ FALSE if it's a valid but not validated XmlBLOB 
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsSchemaValidatedXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_IsIsoMetadata (sqlite3_context * context, int argc,
		       sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsIsoMetadata(XmlBLOB)
/
/ returns TRUE if the current BLOB is an ISO Metadata XmlBLOB,
/ FALSE if it's a valid XmlBLOB but not an ISO Metadata
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsIsoMetadataXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_IsSldSeVectorStyle (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsSldSeVectorStyle(XmlBLOB)
/
/ returns TRUE if the current BLOB is an SLD/SE Vector Style XmlBLOB,
/ FALSE if it's a valid XmlBLOB but not an SLD/SE Style
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsSldSeVectorStyleXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_IsSldSeRasterStyle (sqlite3_context * context, int argc,
			    sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsSldSeRasterStyle(XmlBLOB)
/
/ returns TRUE if the current BLOB is an SLD/SE Raster Style XmlBLOB,
/ FALSE if it's a valid XmlBLOB but not an SLD/SE Style
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsSldSeRasterStyleXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_IsSvg (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsSvg(XmlBLOB)
/
/ returns TRUE if the current BLOB is an SLD/SE Style XmlBLOB,
/ FALSE if it's a valid XmlBLOB but not an SLD/SE Style
/ or -1 if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaIsSvgXmlBlob (p_blob, n_bytes);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_GetDocumentSize (sqlite3_context * context, int argc,
			 sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetDocumentSize(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB will return the XMLDocument size (in bytes)
/ or NULL if any error is encountered
*/
    unsigned char *p_blob;
    int n_bytes;
    int ret;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    ret = gaiaXmlBlobGetDocumentSize (p_blob, n_bytes);
    if (ret < 0)
	sqlite3_result_null (context);
    else
	sqlite3_result_int (context, ret);
}

static void
fnct_XB_GetSchemaURI (sqlite3_context * context, int argc,
		      sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetSchemaURI(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB containing a SchemaURI then
/ the SchemaURI will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *schema_uri;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    schema_uri = gaiaXmlBlobGetSchemaURI (p_blob, n_bytes);
    if (schema_uri == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, schema_uri, strlen (schema_uri), free);
}

static void
fnct_XB_GetFileId (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetFileId(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB containing a FileIdentifier then
/ the FileIdentifier will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *file_identifier;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    file_identifier = gaiaXmlBlobGetFileId (p_blob, n_bytes);
    if (file_identifier == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, file_identifier,
			     strlen (file_identifier), free);
}

static void
fnct_XB_GetParentId (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetParentId(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB containing a ParentIdentifier then
/ the ParentIdentifier will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *parent_identifier;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    parent_identifier = gaiaXmlBlobGetParentId (p_blob, n_bytes);
    if (parent_identifier == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, parent_identifier,
			     strlen (parent_identifier), free);
}

static void
fnct_XB_SetFileId (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_SetFileId(XmlBLOB, Text value)
/
/ if the BLOB is a valid XmlBLOB of the ISO-Metadata type
/ already containing a FileID then this function will 
/ return a new XmlBLOB containing the new FileID
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    unsigned char *new_blob;
    int new_bytes;
    const char *identifier;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    identifier = (const char *) sqlite3_value_text (argv[1]);
    if (!gaiaXmlBlobSetFileId
	(sqlite3_user_data (context), p_blob, n_bytes, identifier, &new_blob,
	 &new_bytes))
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, new_blob, new_bytes, free);
}

static void
fnct_XB_SetParentId (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_SetParentId(XmlBLOB, Text value)
/
/ if the BLOB is a valid XmlBLOB of the ISO-Metadata type
/ already containing a ParentID then this function will 
/ return a new XmlBLOB containing the new ParentID
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    unsigned char *new_blob;
    int new_bytes;
    const char *identifier;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    identifier = (const char *) sqlite3_value_text (argv[1]);
    if (!gaiaXmlBlobSetParentId
	(sqlite3_user_data (context), p_blob, n_bytes, identifier, &new_blob,
	 &new_bytes))
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, new_blob, new_bytes, free);
}

static void
fnct_XB_AddFileId (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_AddFileId(XmlBLOB, Text value, Text ns_id, Text uri_id, Text ns_charstr, Text uri_charstr)
/
/ if the BLOB is a valid XmlBLOB of the ISO-Metadata type
/ not containing a FileID then this function will 
/ return a new XmlBLOB containing the new FileID
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    unsigned char *new_blob;
    int new_bytes;
    const char *identifier;
    const char *ns_id = NULL;
    const char *uri_id = NULL;
    const char *ns_charstr = NULL;
    const char *uri_charstr = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[2]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[3]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[4]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[5]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[5]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    identifier = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) == SQLITE_TEXT)
	ns_id = (const char *) sqlite3_value_text (argv[2]);
    if (sqlite3_value_type (argv[3]) == SQLITE_TEXT)
	uri_id = (const char *) sqlite3_value_text (argv[3]);
    if (sqlite3_value_type (argv[4]) == SQLITE_TEXT)
	ns_charstr = (const char *) sqlite3_value_text (argv[4]);
    if (sqlite3_value_type (argv[5]) == SQLITE_TEXT)
	uri_charstr = (const char *) sqlite3_value_text (argv[5]);
    if (!gaiaXmlBlobAddFileId
	(sqlite3_user_data (context), p_blob, n_bytes, identifier, ns_id,
	 uri_id, ns_charstr, uri_charstr, &new_blob, &new_bytes))
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, new_blob, new_bytes, free);
}

static void
fnct_XB_AddParentId (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_AddParentId(XmlBLOB, Text value, Text ns_id, Text uri_id, Text ns_charstr, Text url_charstr)
/
/ if the BLOB is a valid XmlBLOB of the ISO-Metadata type
/ not containing a ParentID then this function will 
/ return a new XmlBLOB containing the new ParentID
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    unsigned char *new_blob;
    int new_bytes;
    const char *identifier;
    const char *ns_id = NULL;
    const char *uri_id = NULL;
    const char *ns_charstr = NULL;
    const char *uri_charstr = NULL;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[1]) != SQLITE_TEXT)
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[2]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[2]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[3]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[3]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[4]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[4]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    if (sqlite3_value_type (argv[5]) == SQLITE_TEXT
	|| sqlite3_value_type (argv[5]) == SQLITE_NULL)
	;
    else
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    identifier = (const char *) sqlite3_value_text (argv[1]);
    if (sqlite3_value_type (argv[2]) == SQLITE_TEXT)
	ns_id = (const char *) sqlite3_value_text (argv[2]);
    if (sqlite3_value_type (argv[3]) == SQLITE_TEXT)
	uri_id = (const char *) sqlite3_value_text (argv[3]);
    if (sqlite3_value_type (argv[4]) == SQLITE_TEXT)
	ns_charstr = (const char *) sqlite3_value_text (argv[4]);
    if (sqlite3_value_type (argv[5]) == SQLITE_TEXT)
	uri_charstr = (const char *) sqlite3_value_text (argv[5]);
    if (!gaiaXmlBlobAddParentId
	(sqlite3_user_data (context), p_blob, n_bytes, identifier, ns_id,
	 uri_id, ns_charstr, uri_charstr, &new_blob, &new_bytes))
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, new_blob, new_bytes, free);
}

static void
fnct_XB_GetTitle (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetTitle(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB containing a Title then
/ the Title will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *title;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    title = gaiaXmlBlobGetTitle (p_blob, n_bytes);
    if (title == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, title, strlen (title), free);
}

static void
fnct_XB_GetAbstract (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetAbstract(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB containing an Abstract then
/ the Abstract will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *abstract;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    abstract = gaiaXmlBlobGetAbstract (p_blob, n_bytes);
    if (abstract == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, abstract, strlen (abstract), free);
}

static void
fnct_XB_GetGeometry (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetGeometry(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB containing a Geometry then
/ the Geometry will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    int blob_len;
    unsigned char *blob;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    gaiaXmlBlobGetGeometry (p_blob, n_bytes, &blob, &blob_len);
    if (blob == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_blob (context, blob, blob_len, free);
}

static void
fnct_XB_GetEncoding (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetEncoding(XmlBLOB)
/
/ if the BLOB is a valid XmlBLOB explicitly defining an encoding then
/ the charset name will be returned
/ return NULL on any other case
*/
    const unsigned char *p_blob;
    int n_bytes;
    char *encoding;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    p_blob = sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes (argv[0]);
    encoding = gaiaXmlBlobGetEncoding (p_blob, n_bytes);
    if (encoding == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, encoding, strlen (encoding), free);
}

static void
fnct_XB_GetInternalSchemaURI (sqlite3_context * context, int argc,
			      sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetInternalSchemaURI(XmlDocument)
/
/ if the XMLDocument is valid and it contains an internally
/ defined SchemaURI then this SchemaURI will be returned
/ return NULL on any other case
*/
    const unsigned char *xml;
    int xml_len;
    char *schema_uri;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_BLOB)
      {
	  sqlite3_result_null (context);
	  return;
      }
    xml = sqlite3_value_blob (argv[0]);
    xml_len = sqlite3_value_bytes (argv[0]);
    schema_uri =
	gaiaXmlGetInternalSchemaURI (sqlite3_user_data (context), xml, xml_len);
    if (schema_uri == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, schema_uri, strlen (schema_uri), free);
}

static void
fnct_XB_GetLastParseError (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetLastParseError()
/
/ return the most recent XML Parse error/warning (if any)
/ return NULL on any other case
*/
    char *msg;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    msg = gaiaXmlBlobGetLastParseError (sqlite3_user_data (context));
    if (msg == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, msg, strlen (msg), SQLITE_STATIC);
}

static void
fnct_XB_GetLastValidateError (sqlite3_context * context, int argc,
			      sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetLastValidateError()
/
/ return the most recent XML Validate error/warning (if any)
/ return NULL on any other case
*/
    char *msg;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    msg = gaiaXmlBlobGetLastValidateError (sqlite3_user_data (context));
    if (msg == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, msg, strlen (msg), SQLITE_STATIC);
}

static void
fnct_XB_IsValidXPathExpression (sqlite3_context * context, int argc,
				sqlite3_value ** argv)
{
/* SQL function:
/ XB_IsValidXPathExpression(text XPathExpression)
/
/ returns TRUE if the current arg is a valid XPathExpression,
/ FALSE if it's not
/ or -1 if any error is encountered
*/
    int ret;
    const char *xpath;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
      {
	  sqlite3_result_int (context, -1);
	  return;
      }
    xpath = (const char *) sqlite3_value_text (argv[0]);
    ret = gaiaIsValidXPathExpression (sqlite3_user_data (context), xpath);
    sqlite3_result_int (context, ret);
}

static void
fnct_XB_GetLastXPathError (sqlite3_context * context, int argc,
			   sqlite3_value ** argv)
{
/* SQL function:
/ XB_GetLastXPathError()
/
/ return the most recent XML Validate error/warning (if any)
/ return NULL on any other case
*/
    char *msg;
    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */
    msg = gaiaXmlBlobGetLastXPathError (sqlite3_user_data (context));
    if (msg == NULL)
	sqlite3_result_null (context);
    else
	sqlite3_result_text (context, msg, strlen (msg), SQLITE_STATIC);
}

static void
fnct_XB_CacheFlush (sqlite3_context * context, int argc, sqlite3_value ** argv)
{
/* SQL function:
/ XB_CacheFlush()
/
/ resets the Internal XML Schema Cache to its initial empty state
/ 
/ returns TRUE on success
*/
    int i;
    struct splite_xmlSchema_cache_item *p_xmlSchema;
    struct splite_internal_cache *cache = sqlite3_user_data (context);

    GAIA_UNUSED ();		/* LCOV_EXCL_LINE */

    for (i = 0; i < MAX_XMLSCHEMA_CACHE; i++)
      {
	  /* freeing the XmlSchema cache */
	  p_xmlSchema = &(cache->xmlSchemaCache[i]);
	  splite_free_xml_schema_cache_item (p_xmlSchema);
      }
    sqlite3_result_int (context, 1);
}

#endif /* end including LIBXML2 */

SPATIALITE_DECLARE void *
spatialite_alloc_connection ()
{
/* allocating and initializing an empty internal cache */
    gaiaOutBufferPtr out;
    int i;
    struct splite_internal_cache *cache;
    struct splite_geos_cache_item *p;
    struct splite_xmlSchema_cache_item *p_xmlSchema;

    cache = malloc (sizeof (struct splite_internal_cache));
/* initializing the XML error buffers */
    out = malloc (sizeof (gaiaOutBuffer));
    gaiaOutBufferInitialize (out);
    cache->xmlParsingErrors = out;
    out = malloc (sizeof (gaiaOutBuffer));
    gaiaOutBufferInitialize (out);
    cache->xmlSchemaValidationErrors = out;
    out = malloc (sizeof (gaiaOutBuffer));
    gaiaOutBufferInitialize (out);
    cache->xmlXPathErrors = out;
/* initializing the GEOS cache */
    p = &(cache->cacheItem1);
    memset (p->gaiaBlob, '\0', 64);
    p->gaiaBlobSize = 0;
    p->crc32 = 0;
    p->geosGeom = NULL;
    p->preparedGeosGeom = NULL;
    p = &(cache->cacheItem2);
    memset (p->gaiaBlob, '\0', 64);
    p->gaiaBlobSize = 0;
    p->crc32 = 0;
    p->geosGeom = NULL;
    p->preparedGeosGeom = NULL;
    for (i = 0; i < MAX_XMLSCHEMA_CACHE; i++)
      {
	  /* initializing the XmlSchema cache */
	  p_xmlSchema = &(cache->xmlSchemaCache[i]);
	  p_xmlSchema->timestamp = 0;
	  p_xmlSchema->schemaURI = NULL;
	  p_xmlSchema->schemaDoc = NULL;
	  p_xmlSchema->parserCtxt = NULL;
	  p_xmlSchema->schema = NULL;
      }
    return cache;
}

SPATIALITE_PRIVATE void *
register_spatialite_sql_functions (void *p_db, void *p_cache)
{
    sqlite3 *db = p_db;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    const char *security_level;
    if (cache == NULL)
      {
	  /* dirty compatibility mode: allocating an internal cache */
	  cache = spatialite_alloc_connection ();
      }
    sqlite3_create_function (db, "spatialite_version", 0, SQLITE_ANY, 0,
			     fnct_spatialite_version, 0, 0);
    sqlite3_create_function (db, "proj4_version", 0, SQLITE_ANY, 0,
			     fnct_proj4_version, 0, 0);
    sqlite3_create_function (db, "geos_version", 0, SQLITE_ANY, 0,
			     fnct_geos_version, 0, 0);
    sqlite3_create_function (db, "lwgeom_version", 0, SQLITE_ANY, 0,
			     fnct_lwgeom_version, 0, 0);
    sqlite3_create_function (db, "libxml2_version", 0, SQLITE_ANY, 0,
			     fnct_libxml2_version, 0, 0);
    sqlite3_create_function (db, "HasProj", 0, SQLITE_ANY, 0,
			     fnct_has_proj, 0, 0);
    sqlite3_create_function (db, "HasGeos", 0, SQLITE_ANY, 0,
			     fnct_has_geos, 0, 0);
    sqlite3_create_function (db, "HasGeosAdvanced", 0, SQLITE_ANY, 0,
			     fnct_has_geos_advanced, 0, 0);
    sqlite3_create_function (db, "HasGeosTrunk", 0, SQLITE_ANY, 0,
			     fnct_has_geos_trunk, 0, 0);
    sqlite3_create_function (db, "HasLwGeom", 0, SQLITE_ANY, 0,
			     fnct_has_lwgeom, 0, 0);
    sqlite3_create_function (db, "HasMathSql", 0, SQLITE_ANY, 0,
			     fnct_has_math_sql, 0, 0);
    sqlite3_create_function (db, "HasGeoCallbacks", 0, SQLITE_ANY, 0,
			     fnct_has_geo_callbacks, 0, 0);
    sqlite3_create_function (db, "HasIconv", 0, SQLITE_ANY, 0,
			     fnct_has_iconv, 0, 0);
    sqlite3_create_function (db, "HasFreeXL", 0, SQLITE_ANY, 0,
			     fnct_has_freeXL, 0, 0);
    sqlite3_create_function (db, "HasEpsg", 0, SQLITE_ANY, 0,
			     fnct_has_epsg, 0, 0);
    sqlite3_create_function (db, "HasLibXML2", 0, SQLITE_ANY, 0,
			     fnct_has_libxml2, 0, 0);
    sqlite3_create_function (db, "GeometryConstraints", 3, SQLITE_ANY, 0,
			     fnct_GeometryConstraints, 0, 0);
    sqlite3_create_function (db, "GeometryConstraints", 4, SQLITE_ANY, 0,
			     fnct_GeometryConstraints, 0, 0);
    sqlite3_create_function (db, "RTreeAlign", 3, SQLITE_ANY, 0,
			     fnct_RTreeAlign, 0, 0);
    sqlite3_create_function (db, "IsValidNoDataPixel", 3, SQLITE_ANY, 0,
			     fnct_IsValidNoDataPixel, 0, 0);
    sqlite3_create_function (db, "IsPopulatedCoverage", 1, SQLITE_ANY, 0,
			     fnct_IsPopulatedCoverage, 0, 0);
    sqlite3_create_function (db, "CheckSpatialMetaData", 0, SQLITE_ANY, 0,
			     fnct_CheckSpatialMetaData, 0, 0);
    sqlite3_create_function (db, "AutoFDOStart", 0, SQLITE_ANY, 0,
			     fnct_AutoFDOStart, 0, 0);
    sqlite3_create_function (db, "AutoFDOStop", 0, SQLITE_ANY, 0,
			     fnct_AutoFDOStop, 0, 0);
    sqlite3_create_function (db, "InitFDOSpatialMetaData", 0, SQLITE_ANY, 0,
			     fnct_InitFDOSpatialMetaData, 0, 0);
    sqlite3_create_function (db, "AddFDOGeometryColumn", 6, SQLITE_ANY, 0,
			     fnct_AddFDOGeometryColumn, 0, 0);
    sqlite3_create_function (db, "RecoverFDOGeometryColumn", 6, SQLITE_ANY, 0,
			     fnct_RecoverFDOGeometryColumn, 0, 0);
    sqlite3_create_function (db, "DiscardFDOGeometryColumn", 2, SQLITE_ANY, 0,
			     fnct_DiscardFDOGeometryColumn, 0, 0);
    sqlite3_create_function (db, "InitSpatialMetaData", 0, SQLITE_ANY, 0,
			     fnct_InitSpatialMetaData, 0, 0);
    sqlite3_create_function (db, "InitSpatialMetaData", 1, SQLITE_ANY, 0,
			     fnct_InitSpatialMetaData, 0, 0);
    sqlite3_create_function (db, "InitSpatialMetaData", 2, SQLITE_ANY, 0,
			     fnct_InitSpatialMetaData, 0, 0);
    sqlite3_create_function (db, "InsertEpsgSrid", 1, SQLITE_ANY, 0,
			     fnct_InsertEpsgSrid, 0, 0);
    sqlite3_create_function (db, "AddGeometryColumn", 4, SQLITE_ANY, 0,
			     fnct_AddGeometryColumn, 0, 0);
    sqlite3_create_function (db, "AddGeometryColumn", 5, SQLITE_ANY, 0,
			     fnct_AddGeometryColumn, 0, 0);
    sqlite3_create_function (db, "AddGeometryColumn", 6, SQLITE_ANY, 0,
			     fnct_AddGeometryColumn, 0, 0);
    sqlite3_create_function (db, "RecoverGeometryColumn", 4, SQLITE_ANY, 0,
			     fnct_RecoverGeometryColumn, 0, 0);
    sqlite3_create_function (db, "RecoverGeometryColumn", 5, SQLITE_ANY, 0,
			     fnct_RecoverGeometryColumn, 0, 0);
    sqlite3_create_function (db, "DiscardGeometryColumn", 2, SQLITE_ANY, 0,
			     fnct_DiscardGeometryColumn, 0, 0);
    sqlite3_create_function (db, "RegisterVirtualGeometry", 1, SQLITE_ANY, 0,
			     fnct_RegisterVirtualGeometry, 0, 0);
    sqlite3_create_function (db, "DropVirtualGeometry", 1, SQLITE_ANY, 0,
			     fnct_DropVirtualGeometry, 0, 0);
    sqlite3_create_function (db, "RecoverSpatialIndex", 0, SQLITE_ANY, 0,
			     fnct_RecoverSpatialIndex, 0, 0);
    sqlite3_create_function (db, "RecoverSpatialIndex", 1, SQLITE_ANY, 0,
			     fnct_RecoverSpatialIndex, 0, 0);
    sqlite3_create_function (db, "RecoverSpatialIndex", 2, SQLITE_ANY, 0,
			     fnct_RecoverSpatialIndex, 0, 0);
    sqlite3_create_function (db, "RecoverSpatialIndex", 3, SQLITE_ANY, 0,
			     fnct_RecoverSpatialIndex, 0, 0);
    sqlite3_create_function (db, "CheckSpatialIndex", 0, SQLITE_ANY, 0,
			     fnct_CheckSpatialIndex, 0, 0);
    sqlite3_create_function (db, "CheckSpatialIndex", 2, SQLITE_ANY, 0,
			     fnct_CheckSpatialIndex, 0, 0);
    sqlite3_create_function (db, "CreateSpatialIndex", 2, SQLITE_ANY, 0,
			     fnct_CreateSpatialIndex, 0, 0);
    sqlite3_create_function (db, "CreateMbrCache", 2, SQLITE_ANY, 0,
			     fnct_CreateMbrCache, 0, 0);
    sqlite3_create_function (db, "DisableSpatialIndex", 2, SQLITE_ANY, 0,
			     fnct_DisableSpatialIndex, 0, 0);
    sqlite3_create_function (db, "RebuildGeometryTriggers", 2, SQLITE_ANY, 0,
			     fnct_RebuildGeometryTriggers, 0, 0);
    sqlite3_create_function (db, "UpdateLayerStatistics", 0, SQLITE_ANY, 0,
			     fnct_UpdateLayerStatistics, 0, 0);
    sqlite3_create_function (db, "UpdateLayerStatistics", 1, SQLITE_ANY, 0,
			     fnct_UpdateLayerStatistics, 0, 0);
    sqlite3_create_function (db, "UpdateLayerStatistics", 2, SQLITE_ANY, 0,
			     fnct_UpdateLayerStatistics, 0, 0);
    sqlite3_create_function (db, "GetLayerExtent", 1, SQLITE_ANY, 0,
			     fnct_GetLayerExtent, 0, 0);
    sqlite3_create_function (db, "GetLayerExtent", 2, SQLITE_ANY, 0,
			     fnct_GetLayerExtent, 0, 0);
    sqlite3_create_function (db, "GetLayerExtent", 3, SQLITE_ANY, 0,
			     fnct_GetLayerExtent, 0, 0);
    sqlite3_create_function (db, "CreateRasterCoveragesTable", 0, SQLITE_ANY,
			     0, fnct_CreateRasterCoveragesTable, 0, 0);
    sqlite3_create_function (db, "AsText", 1, SQLITE_ANY, 0, fnct_AsText, 0, 0);
    sqlite3_create_function (db, "ST_AsText", 1, SQLITE_ANY, 0, fnct_AsText, 0,
			     0);
    sqlite3_create_function (db, "AsWkt", 1, SQLITE_ANY, 0, fnct_AsWkt, 0, 0);
    sqlite3_create_function (db, "AsWkt", 2, SQLITE_ANY, 0, fnct_AsWkt, 0, 0);
    sqlite3_create_function (db, "AsSvg", 1, SQLITE_ANY, 0, fnct_AsSvg1, 0, 0);
    sqlite3_create_function (db, "AsSvg", 2, SQLITE_ANY, 0, fnct_AsSvg2, 0, 0);
    sqlite3_create_function (db, "AsSvg", 3, SQLITE_ANY, 0, fnct_AsSvg3, 0, 0);

#ifndef OMIT_PROJ		/* PROJ.4 is strictly required to support KML */
    sqlite3_create_function (db, "AsKml", 1, SQLITE_ANY, 0, fnct_AsKml, 0, 0);
    sqlite3_create_function (db, "AsKml", 2, SQLITE_ANY, 0, fnct_AsKml, 0, 0);
    sqlite3_create_function (db, "AsKml", 3, SQLITE_ANY, 0, fnct_AsKml, 0, 0);
    sqlite3_create_function (db, "AsKml", 4, SQLITE_ANY, 0, fnct_AsKml, 0, 0);
#endif /* end including PROJ.4 */

    sqlite3_create_function (db, "AsGml", 1, SQLITE_ANY, 0, fnct_AsGml, 0, 0);
    sqlite3_create_function (db, "AsGml", 2, SQLITE_ANY, 0, fnct_AsGml, 0, 0);
    sqlite3_create_function (db, "AsGml", 3, SQLITE_ANY, 0, fnct_AsGml, 0, 0);
    sqlite3_create_function (db, "GeomFromGml", 1, SQLITE_ANY, 0,
			     fnct_FromGml, 0, 0);
    sqlite3_create_function (db, "AsGeoJSON", 1, SQLITE_ANY, 0,
			     fnct_AsGeoJSON, 0, 0);
    sqlite3_create_function (db, "AsGeoJSON", 2, SQLITE_ANY, 0,
			     fnct_AsGeoJSON, 0, 0);
    sqlite3_create_function (db, "AsGeoJSON", 3, SQLITE_ANY, 0,
			     fnct_AsGeoJSON, 0, 0);
    sqlite3_create_function (db, "GeomFromGeoJSON", 1, SQLITE_ANY, 0,
			     fnct_FromGeoJSON, 0, 0);
    sqlite3_create_function (db, "GeomFromKml", 1, SQLITE_ANY, 0,
			     fnct_FromKml, 0, 0);
    sqlite3_create_function (db, "AsFGF", 2, SQLITE_ANY, 0, fnct_AsFGF, 0, 0);
    sqlite3_create_function (db, "GeomFromEWKB", 1, SQLITE_ANY, 0,
			     fnct_FromEWKB, 0, 0);
    sqlite3_create_function (db, "AsEWKB", 1, SQLITE_ANY, 0, fnct_ToEWKB, 0, 0);
    sqlite3_create_function (db, "AsEWKT", 1, SQLITE_ANY, 0, fnct_ToEWKT, 0, 0);
    sqlite3_create_function (db, "GeomFromEWKT", 1, SQLITE_ANY, 0,
			     fnct_FromEWKT, 0, 0);
    sqlite3_create_function (db, "AsBinary", 1, SQLITE_ANY, 0, fnct_AsBinary,
			     0, 0);
    sqlite3_create_function (db, "ST_AsBinary", 1, SQLITE_ANY, 0,
			     fnct_AsBinary, 0, 0);
    sqlite3_create_function (db, "GeomFromText", 1, SQLITE_ANY, 0,
			     fnct_GeomFromText1, 0, 0);
    sqlite3_create_function (db, "GeomFromText", 2, SQLITE_ANY, 0,
			     fnct_GeomFromText2, 0, 0);
    sqlite3_create_function (db, "GeometryFromText", 1, SQLITE_ANY, 0,
			     fnct_GeomFromText1, 0, 0);
    sqlite3_create_function (db, "GeometryFromText", 2, SQLITE_ANY, 0,
			     fnct_GeomFromText2, 0, 0);
    sqlite3_create_function (db, "GeomCollFromText", 1, SQLITE_ANY, 0,
			     fnct_GeomCollFromText1, 0, 0);
    sqlite3_create_function (db, "GeomCollFromText", 2, SQLITE_ANY, 0,
			     fnct_GeomCollFromText2, 0, 0);
    sqlite3_create_function (db, "GeometryCollectionFromText", 1, SQLITE_ANY,
			     0, fnct_GeomCollFromText1, 0, 0);
    sqlite3_create_function (db, "GeometryCollectionFromText", 2, SQLITE_ANY,
			     0, fnct_GeomCollFromText2, 0, 0);
    sqlite3_create_function (db, "PointFromText", 1, SQLITE_ANY, 0,
			     fnct_PointFromText1, 0, 0);
    sqlite3_create_function (db, "PointFromText", 2, SQLITE_ANY, 0,
			     fnct_PointFromText2, 0, 0);
    sqlite3_create_function (db, "LineFromText", 1, SQLITE_ANY, 0,
			     fnct_LineFromText1, 0, 0);
    sqlite3_create_function (db, "LineFromText", 2, SQLITE_ANY, 0,
			     fnct_LineFromText2, 0, 0);
    sqlite3_create_function (db, "LineStringFromText", 1, SQLITE_ANY, 0,
			     fnct_LineFromText1, 0, 0);
    sqlite3_create_function (db, "LineStringFromText", 2, SQLITE_ANY, 0,
			     fnct_LineFromText2, 0, 0);
    sqlite3_create_function (db, "PolyFromText", 1, SQLITE_ANY, 0,
			     fnct_PolyFromText1, 0, 0);
    sqlite3_create_function (db, "PolyFromText", 2, SQLITE_ANY, 0,
			     fnct_PolyFromText2, 0, 0);
    sqlite3_create_function (db, "PolygonFromText", 1, SQLITE_ANY, 0,
			     fnct_PolyFromText1, 0, 0);
    sqlite3_create_function (db, "PolygonFromText", 2, SQLITE_ANY, 0,
			     fnct_PolyFromText2, 0, 0);
    sqlite3_create_function (db, "MPointFromText", 1, SQLITE_ANY, 0,
			     fnct_MPointFromText1, 0, 0);
    sqlite3_create_function (db, "MPointFromText", 2, SQLITE_ANY, 0,
			     fnct_MPointFromText2, 0, 0);
    sqlite3_create_function (db, "MultiPointFromText", 1, SQLITE_ANY, 0,
			     fnct_MPointFromText1, 0, 0);
    sqlite3_create_function (db, "MultiPointFromText", 2, SQLITE_ANY, 0,
			     fnct_MPointFromText2, 0, 0);
    sqlite3_create_function (db, "MLineFromText", 1, SQLITE_ANY, 0,
			     fnct_MLineFromText1, 0, 0);
    sqlite3_create_function (db, "MLineFromText", 2, SQLITE_ANY, 0,
			     fnct_MLineFromText2, 0, 0);
    sqlite3_create_function (db, "MultiLineStringFromText", 1, SQLITE_ANY, 0,
			     fnct_MLineFromText1, 0, 0);
    sqlite3_create_function (db, "MultiLineStringFromText", 2, SQLITE_ANY, 0,
			     fnct_MLineFromText2, 0, 0);
    sqlite3_create_function (db, "MPolyFromText", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromText1, 0, 0);
    sqlite3_create_function (db, "MPolyFromText", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromText2, 0, 0);
    sqlite3_create_function (db, "MultiPolygonFromText", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromText1, 0, 0);
    sqlite3_create_function (db, "MultiPolygonFromText", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromText2, 0, 0);
    sqlite3_create_function (db, "GeomFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomFromWkb1, 0, 0);
    sqlite3_create_function (db, "GeomFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomFromWkb2, 0, 0);
    sqlite3_create_function (db, "GeometryFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomFromWkb1, 0, 0);
    sqlite3_create_function (db, "GeometryFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomFromWkb2, 0, 0);
    sqlite3_create_function (db, "GeomCollFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomCollFromWkb1, 0, 0);
    sqlite3_create_function (db, "GeomCollFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomCollFromWkb2, 0, 0);
    sqlite3_create_function (db, "GeometryCollectionFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomCollFromWkb1, 0, 0);
    sqlite3_create_function (db, "GeometryCollectionFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomCollFromWkb2, 0, 0);
    sqlite3_create_function (db, "PointFromWKB", 1, SQLITE_ANY, 0,
			     fnct_PointFromWkb1, 0, 0);
    sqlite3_create_function (db, "PointFromWKB", 2, SQLITE_ANY, 0,
			     fnct_PointFromWkb2, 0, 0);
    sqlite3_create_function (db, "LineFromWKB", 1, SQLITE_ANY, 0,
			     fnct_LineFromWkb1, 0, 0);
    sqlite3_create_function (db, "LineFromWKB", 2, SQLITE_ANY, 0,
			     fnct_LineFromWkb2, 0, 0);
    sqlite3_create_function (db, "LineStringFromWKB", 1, SQLITE_ANY, 0,
			     fnct_LineFromWkb1, 0, 0);
    sqlite3_create_function (db, "LineStringFromWKB", 2, SQLITE_ANY, 0,
			     fnct_LineFromWkb2, 0, 0);
    sqlite3_create_function (db, "PolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_PolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "PolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_PolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "PolygonFromWKB", 1, SQLITE_ANY, 0,
			     fnct_PolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "PolygonFromWKB", 2, SQLITE_ANY, 0,
			     fnct_PolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "MPointFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPointFromWkb1, 0, 0);
    sqlite3_create_function (db, "MPointFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPointFromWkb2, 0, 0);
    sqlite3_create_function (db, "MultiPointFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPointFromWkb1, 0, 0);
    sqlite3_create_function (db, "MultiPointFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPointFromWkb2, 0, 0);
    sqlite3_create_function (db, "MLineFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MLineFromWkb1, 0, 0);
    sqlite3_create_function (db, "MLineFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MLineFromWkb2, 0, 0);
    sqlite3_create_function (db, "MultiLineStringFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MLineFromWkb1, 0, 0);
    sqlite3_create_function (db, "MultiLineStringFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MLineFromWkb2, 0, 0);
    sqlite3_create_function (db, "MPolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "MPolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "MultiPolygonFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "MultiPolygonFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_WKTToSQL", 1, SQLITE_ANY, 0,
			     fnct_WktToSql, 0, 0);
    sqlite3_create_function (db, "ST_GeomFromText", 1, SQLITE_ANY, 0,
			     fnct_GeomFromText1, 0, 0);
    sqlite3_create_function (db, "ST_GeomFromText", 2, SQLITE_ANY, 0,
			     fnct_GeomFromText2, 0, 0);
    sqlite3_create_function (db, "ST_GeometryFromText", 1, SQLITE_ANY, 0,
			     fnct_GeomFromText1, 0, 0);
    sqlite3_create_function (db, "ST_GeometryFromText", 2, SQLITE_ANY, 0,
			     fnct_GeomFromText2, 0, 0);
    sqlite3_create_function (db, "ST_GeomCollFromText", 1, SQLITE_ANY, 0,
			     fnct_GeomCollFromText1, 0, 0);
    sqlite3_create_function (db, "ST_GeomCollFromText", 2, SQLITE_ANY, 0,
			     fnct_GeomCollFromText2, 0, 0);
    sqlite3_create_function (db, "ST_GeometryCollectionFromText", 1,
			     SQLITE_ANY, 0, fnct_GeomCollFromText1, 0, 0);
    sqlite3_create_function (db, "ST_GeometryCollectionFromText", 2,
			     SQLITE_ANY, 0, fnct_GeomCollFromText2, 0, 0);
    sqlite3_create_function (db, "ST_PointFromText", 1, SQLITE_ANY, 0,
			     fnct_PointFromText1, 0, 0);
    sqlite3_create_function (db, "ST_PointFromText", 2, SQLITE_ANY, 0,
			     fnct_PointFromText2, 0, 0);
    sqlite3_create_function (db, "ST_LineFromText", 1, SQLITE_ANY, 0,
			     fnct_LineFromText1, 0, 0);
    sqlite3_create_function (db, "ST_LineFromText", 2, SQLITE_ANY, 0,
			     fnct_LineFromText2, 0, 0);
    sqlite3_create_function (db, "ST_LineStringFromText", 1, SQLITE_ANY, 0,
			     fnct_LineFromText1, 0, 0);
    sqlite3_create_function (db, "ST_LineStringFromText", 2, SQLITE_ANY, 0,
			     fnct_LineFromText2, 0, 0);
    sqlite3_create_function (db, "ST_PolyFromText", 1, SQLITE_ANY, 0,
			     fnct_PolyFromText1, 0, 0);
    sqlite3_create_function (db, "ST_PolyFromText", 2, SQLITE_ANY, 0,
			     fnct_PolyFromText2, 0, 0);
    sqlite3_create_function (db, "ST_PolygonFromText", 1, SQLITE_ANY, 0,
			     fnct_PolyFromText1, 0, 0);
    sqlite3_create_function (db, "ST_PolygonFromText", 2, SQLITE_ANY, 0,
			     fnct_PolyFromText2, 0, 0);
    sqlite3_create_function (db, "ST_MPointFromText", 1, SQLITE_ANY, 0,
			     fnct_MPointFromText1, 0, 0);
    sqlite3_create_function (db, "ST_MPointFromText", 2, SQLITE_ANY, 0,
			     fnct_MPointFromText2, 0, 0);
    sqlite3_create_function (db, "ST_MultiPointFromText", 1, SQLITE_ANY, 0,
			     fnct_MPointFromText1, 0, 0);
    sqlite3_create_function (db, "ST_MultiPointFromText", 2, SQLITE_ANY, 0,
			     fnct_MPointFromText2, 0, 0);
    sqlite3_create_function (db, "ST_MLineFromText", 1, SQLITE_ANY, 0,
			     fnct_MLineFromText1, 0, 0);
    sqlite3_create_function (db, "ST_MLineFromText", 2, SQLITE_ANY, 0,
			     fnct_MLineFromText2, 0, 0);
    sqlite3_create_function (db, "ST_MultiLineStringFromText", 1, SQLITE_ANY,
			     0, fnct_MLineFromText1, 0, 0);
    sqlite3_create_function (db, "ST_MultiLineStringFromText", 2, SQLITE_ANY,
			     0, fnct_MLineFromText2, 0, 0);
    sqlite3_create_function (db, "ST_MPolyFromText", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromText1, 0, 0);
    sqlite3_create_function (db, "ST_MPolyFromText", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromText2, 0, 0);
    sqlite3_create_function (db, "ST_MultiPolygonFromText", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromText1, 0, 0);
    sqlite3_create_function (db, "ST_MultiPolygonFromText", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromText2, 0, 0);
    sqlite3_create_function (db, "ST_WKBToSQL", 1, SQLITE_ANY, 0,
			     fnct_WkbToSql, 0, 0);
    sqlite3_create_function (db, "ST_GeomFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_GeomFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_GeometryFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_GeometryFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_GeomCollFromWKB", 1, SQLITE_ANY, 0,
			     fnct_GeomCollFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_GeomCollFromWKB", 2, SQLITE_ANY, 0,
			     fnct_GeomCollFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_GeometryCollectionFromWKB", 1, SQLITE_ANY,
			     0, fnct_GeomCollFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_GeometryCollectionFromWKB", 2, SQLITE_ANY,
			     0, fnct_GeomCollFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_PointFromWKB", 1, SQLITE_ANY, 0,
			     fnct_PointFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_PointFromWKB", 2, SQLITE_ANY, 0,
			     fnct_PointFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_LineFromWKB", 1, SQLITE_ANY, 0,
			     fnct_LineFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_LineFromWKB", 2, SQLITE_ANY, 0,
			     fnct_LineFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_LineStringFromWKB", 1, SQLITE_ANY, 0,
			     fnct_LineFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_LineStringFromWKB", 2, SQLITE_ANY, 0,
			     fnct_LineFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_PolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_PolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_PolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_PolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_PolygonFromWKB", 1, SQLITE_ANY, 0,
			     fnct_PolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_PolygonFromWKB", 2, SQLITE_ANY, 0,
			     fnct_PolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_MPointFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPointFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_MPointFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPointFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_MultiPointFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPointFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_MultiPointFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPointFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_MLineFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MLineFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_MLineFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MLineFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_MultiLineStringFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MLineFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_MultiLineStringFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MLineFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_MPolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_MPolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "ST_MultiPolygonFromWKB", 1, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb1, 0, 0);
    sqlite3_create_function (db, "ST_MultiPolygonFromWKB", 2, SQLITE_ANY, 0,
			     fnct_MPolyFromWkb2, 0, 0);
    sqlite3_create_function (db, "GeomFromFGF", 1, SQLITE_ANY, 0,
			     fnct_GeometryFromFGF1, 0, 0);
    sqlite3_create_function (db, "GeomFromFGF", 2, SQLITE_ANY, 0,
			     fnct_GeometryFromFGF2, 0, 0);
    sqlite3_create_function (db, "CompressGeometry", 1, SQLITE_ANY, 0,
			     fnct_CompressGeometry, 0, 0);
    sqlite3_create_function (db, "UncompressGeometry", 1, SQLITE_ANY, 0,
			     fnct_UncompressGeometry, 0, 0);
    sqlite3_create_function (db, "SanitizeGeometry", 1, SQLITE_ANY, 0,
			     fnct_SanitizeGeometry, 0, 0);
    sqlite3_create_function (db, "CastToInteger", 1, SQLITE_ANY, 0,
			     fnct_CastToInteger, 0, 0);
    sqlite3_create_function (db, "CastToDouble", 1, SQLITE_ANY, 0,
			     fnct_CastToDouble, 0, 0);
    sqlite3_create_function (db, "CastToText", 1, SQLITE_ANY, 0,
			     fnct_CastToText, 0, 0);
    sqlite3_create_function (db, "CastToText", 2, SQLITE_ANY, 0,
			     fnct_CastToText, 0, 0);
    sqlite3_create_function (db, "CastToBlob", 1, SQLITE_ANY, 0,
			     fnct_CastToBlob, 0, 0);
    sqlite3_create_function (db, "CastToBlob", 2, SQLITE_ANY, 0,
			     fnct_CastToBlob, 0, 0);
    sqlite3_create_function (db, "ForceAsNull", 2, SQLITE_ANY, 0,
			     fnct_ForceAsNull, 0, 0);
    sqlite3_create_function (db, "CreateUUID", 0, SQLITE_ANY, 0,
			     fnct_CreateUUID, 0, 0);
    sqlite3_create_function (db, "MD5Checksum", 1, SQLITE_ANY, 0,
			     fnct_MD5Checksum, 0, 0);
    sqlite3_create_function (db, "MD5TotalChecksum", 1, SQLITE_ANY, 0, 0,
			     fnct_MD5TotalChecksum_step,
			     fnct_MD5TotalChecksum_final);
    sqlite3_create_function (db, "CastToPoint", 1, SQLITE_ANY, 0,
			     fnct_CastToPoint, 0, 0);
    sqlite3_create_function (db, "CastToLinestring", 1, SQLITE_ANY, 0,
			     fnct_CastToLinestring, 0, 0);
    sqlite3_create_function (db, "CastToPolygon", 1, SQLITE_ANY, 0,
			     fnct_CastToPolygon, 0, 0);
    sqlite3_create_function (db, "CastToMultiPoint", 1, SQLITE_ANY, 0,
			     fnct_CastToMultiPoint, 0, 0);
    sqlite3_create_function (db, "CastToMultiLinestring", 1, SQLITE_ANY, 0,
			     fnct_CastToMultiLinestring, 0, 0);
    sqlite3_create_function (db, "CastToMultiPolygon", 1, SQLITE_ANY, 0,
			     fnct_CastToMultiPolygon, 0, 0);
    sqlite3_create_function (db, "CastToGeometryCollection", 1, SQLITE_ANY, 0,
			     fnct_CastToGeometryCollection, 0, 0);
    sqlite3_create_function (db, "CastToMulti", 1, SQLITE_ANY, 0,
			     fnct_CastToMulti, 0, 0);
    sqlite3_create_function (db, "ST_Multi", 1, SQLITE_ANY, 0,
			     fnct_CastToMulti, 0, 0);
    sqlite3_create_function (db, "CastToSingle", 1, SQLITE_ANY, 0,
			     fnct_CastToSingle, 0, 0);
    sqlite3_create_function (db, "CastToXY", 1, SQLITE_ANY, 0, fnct_CastToXY,
			     0, 0);
    sqlite3_create_function (db, "CastToXYZ", 1, SQLITE_ANY, 0, fnct_CastToXYZ,
			     0, 0);
    sqlite3_create_function (db, "CastToXYM", 1, SQLITE_ANY, 0, fnct_CastToXYM,
			     0, 0);
    sqlite3_create_function (db, "CastToXYZM", 1, SQLITE_ANY, 0,
			     fnct_CastToXYZM, 0, 0);
    sqlite3_create_function (db, "ExtractMultiPoint", 1, SQLITE_ANY, 0,
			     fnct_ExtractMultiPoint, 0, 0);
    sqlite3_create_function (db, "ExtractMultiLinestring", 1, SQLITE_ANY, 0,
			     fnct_ExtractMultiLinestring, 0, 0);
    sqlite3_create_function (db, "ExtractMultiPolygon", 1, SQLITE_ANY, 0,
			     fnct_ExtractMultiPolygon, 0, 0);
    sqlite3_create_function (db, "ST_Reverse", 1, SQLITE_ANY, 0, fnct_Reverse,
			     0, 0);
    sqlite3_create_function (db, "ST_ForceLHR", 1, SQLITE_ANY, 0,
			     fnct_ForceLHR, 0, 0);
    sqlite3_create_function (db, "Dimension", 1, SQLITE_ANY, 0, fnct_Dimension,
			     0, 0);
    sqlite3_create_function (db, "ST_Dimension", 1, SQLITE_ANY, 0,
			     fnct_Dimension, 0, 0);
    sqlite3_create_function (db, "CoordDimension", 1, SQLITE_ANY, 0,
			     fnct_CoordDimension, 0, 0);
    sqlite3_create_function (db, "ST_NDims", 1, SQLITE_ANY, 0, fnct_NDims, 0,
			     0);
    sqlite3_create_function (db, "GeometryType", 1, SQLITE_ANY, 0,
			     fnct_GeometryType, 0, 0);
    sqlite3_create_function (db, "ST_GeometryType", 1, SQLITE_ANY, 0,
			     fnct_GeometryType, 0, 0);
    sqlite3_create_function (db, "GeometryAliasType", 1, SQLITE_ANY, 0,
			     fnct_GeometryAliasType, 0, 0);
    sqlite3_create_function (db, "SridFromAuthCRS", 2, SQLITE_ANY, 0,
			     fnct_SridFromAuthCRS, 0, 0);
    sqlite3_create_function (db, "SRID", 1, SQLITE_ANY, 0, fnct_SRID, 0, 0);
    sqlite3_create_function (db, "ST_SRID", 1, SQLITE_ANY, 0, fnct_SRID, 0, 0);
    sqlite3_create_function (db, "SetSRID", 2, SQLITE_ANY, 0, fnct_SetSRID, 0,
			     0);
    sqlite3_create_function (db, "IsEmpty", 1, SQLITE_ANY, 0, fnct_IsEmpty, 0,
			     0);
    sqlite3_create_function (db, "ST_IsEmpty", 1, SQLITE_ANY, 0, fnct_IsEmpty,
			     0, 0);
    sqlite3_create_function (db, "ST_Is3D", 1, SQLITE_ANY, 0, fnct_Is3D, 0, 0);
    sqlite3_create_function (db, "ST_IsMeasured", 1, SQLITE_ANY, 0,
			     fnct_IsMeasured, 0, 0);
    sqlite3_create_function (db, "Envelope", 1, SQLITE_ANY, 0, fnct_Envelope,
			     0, 0);
    sqlite3_create_function (db, "ST_Envelope", 1, SQLITE_ANY, 0,
			     fnct_Envelope, 0, 0);
    sqlite3_create_function (db, "ST_Expand", 2, SQLITE_ANY, 0, fnct_Expand,
			     0, 0);
    sqlite3_create_function (db, "X", 1, SQLITE_ANY, 0, fnct_X, 0, 0);
    sqlite3_create_function (db, "Y", 1, SQLITE_ANY, 0, fnct_Y, 0, 0);
    sqlite3_create_function (db, "Z", 1, SQLITE_ANY, 0, fnct_Z, 0, 0);
    sqlite3_create_function (db, "M", 1, SQLITE_ANY, 0, fnct_M, 0, 0);
    sqlite3_create_function (db, "ST_X", 1, SQLITE_ANY, 0, fnct_X, 0, 0);
    sqlite3_create_function (db, "ST_Y", 1, SQLITE_ANY, 0, fnct_Y, 0, 0);
    sqlite3_create_function (db, "ST_Z", 1, SQLITE_ANY, 0, fnct_Z, 0, 0);
    sqlite3_create_function (db, "ST_M", 1, SQLITE_ANY, 0, fnct_M, 0, 0);
    sqlite3_create_function (db, "ST_MinX", 1, SQLITE_ANY, 0, fnct_MbrMinX, 0,
			     0);
    sqlite3_create_function (db, "ST_MinY", 1, SQLITE_ANY, 0, fnct_MbrMinY, 0,
			     0);
    sqlite3_create_function (db, "ST_MinZ", 1, SQLITE_ANY, 0, fnct_MinZ, 0, 0);
    sqlite3_create_function (db, "ST_MinM", 1, SQLITE_ANY, 0, fnct_MinM, 0, 0);
    sqlite3_create_function (db, "ST_MaxX", 1, SQLITE_ANY, 0, fnct_MbrMaxX, 0,
			     0);
    sqlite3_create_function (db, "ST_MaxY", 1, SQLITE_ANY, 0, fnct_MbrMaxY, 0,
			     0);
    sqlite3_create_function (db, "ST_MaxZ", 1, SQLITE_ANY, 0, fnct_MaxZ, 0, 0);
    sqlite3_create_function (db, "ST_MaxM", 1, SQLITE_ANY, 0, fnct_MaxM, 0, 0);
    sqlite3_create_function (db, "NumPoints", 1, SQLITE_ANY, 0,
			     fnct_NumPoints, 0, 0);
    sqlite3_create_function (db, "ST_NumPoints", 1, SQLITE_ANY, 0,
			     fnct_NumPoints, 0, 0);
    sqlite3_create_function (db, "StartPoint", 1, SQLITE_ANY, 0,
			     fnct_StartPoint, 0, 0);
    sqlite3_create_function (db, "EndPoint", 1, SQLITE_ANY, 0, fnct_EndPoint,
			     0, 0);
    sqlite3_create_function (db, "ST_StartPoint", 1, SQLITE_ANY, 0,
			     fnct_StartPoint, 0, 0);
    sqlite3_create_function (db, "ST_EndPoint", 1, SQLITE_ANY, 0,
			     fnct_EndPoint, 0, 0);
    sqlite3_create_function (db, "PointN", 2, SQLITE_ANY, 0, fnct_PointN, 0, 0);
    sqlite3_create_function (db, "ST_PointN", 2, SQLITE_ANY, 0, fnct_PointN, 0,
			     0);
    sqlite3_create_function (db, "ExteriorRing", 1, SQLITE_ANY, 0,
			     fnct_ExteriorRing, 0, 0);
    sqlite3_create_function (db, "ST_ExteriorRing", 1, SQLITE_ANY, 0,
			     fnct_ExteriorRing, 0, 0);
    sqlite3_create_function (db, "NumInteriorRing", 1, SQLITE_ANY, 0,
			     fnct_NumInteriorRings, 0, 0);
    sqlite3_create_function (db, "NumInteriorRings", 1, SQLITE_ANY, 0,
			     fnct_NumInteriorRings, 0, 0);
    sqlite3_create_function (db, "ST_NumInteriorRing", 1, SQLITE_ANY, 0,
			     fnct_NumInteriorRings, 0, 0);
    sqlite3_create_function (db, "InteriorRingN", 2, SQLITE_ANY, 0,
			     fnct_InteriorRingN, 0, 0);
    sqlite3_create_function (db, "ST_InteriorRingN", 2, SQLITE_ANY, 0,
			     fnct_InteriorRingN, 0, 0);
    sqlite3_create_function (db, "NumGeometries", 1, SQLITE_ANY, 0,
			     fnct_NumGeometries, 0, 0);
    sqlite3_create_function (db, "ST_NumGeometries", 1, SQLITE_ANY, 0,
			     fnct_NumGeometries, 0, 0);
    sqlite3_create_function (db, "GeometryN", 2, SQLITE_ANY, 0, fnct_GeometryN,
			     0, 0);
    sqlite3_create_function (db, "ST_GeometryN", 2, SQLITE_ANY, 0,
			     fnct_GeometryN, 0, 0);
    sqlite3_create_function (db, "MBRContains", 2, SQLITE_ANY, 0,
			     fnct_MbrContains, 0, 0);
    sqlite3_create_function (db, "MbrDisjoint", 2, SQLITE_ANY, 0,
			     fnct_MbrDisjoint, 0, 0);
    sqlite3_create_function (db, "MBREqual", 2, SQLITE_ANY, 0, fnct_MbrEqual,
			     0, 0);
    sqlite3_create_function (db, "MbrIntersects", 2, SQLITE_ANY, 0,
			     fnct_MbrIntersects, 0, 0);
    sqlite3_create_function (db, "ST_EnvIntersects", 2, SQLITE_ANY, 0,
			     fnct_MbrIntersects, 0, 0);
    sqlite3_create_function (db, "ST_EnvIntersects", 5, SQLITE_ANY, 0,
			     fnct_EnvIntersects, 0, 0);
    sqlite3_create_function (db, "ST_EnvelopesIntersects", 2, SQLITE_ANY, 0,
			     fnct_MbrIntersects, 0, 0);
    sqlite3_create_function (db, "ST_EnvelopesIntersects", 5, SQLITE_ANY, 0,
			     fnct_EnvIntersects, 0, 0);
    sqlite3_create_function (db, "MBROverlaps", 2, SQLITE_ANY, 0,
			     fnct_MbrOverlaps, 0, 0);
    sqlite3_create_function (db, "MbrTouches", 2, SQLITE_ANY, 0,
			     fnct_MbrTouches, 0, 0);
    sqlite3_create_function (db, "MbrWithin", 2, SQLITE_ANY, 0, fnct_MbrWithin,
			     0, 0);
    sqlite3_create_function (db, "ShiftCoords", 3, SQLITE_ANY, 0,
			     fnct_ShiftCoords, 0, 0);
    sqlite3_create_function (db, "ShiftCoordinates", 3, SQLITE_ANY, 0,
			     fnct_ShiftCoords, 0, 0);
    sqlite3_create_function (db, "ST_Translate", 4, SQLITE_ANY, 0,
			     fnct_Translate, 0, 0);
    sqlite3_create_function (db, "ST_Shift_Longitude", 1, SQLITE_ANY, 0,
			     fnct_ShiftLongitude, 0, 0);
    sqlite3_create_function (db, "NormalizeLonLat", 1, SQLITE_ANY, 0,
			     fnct_NormalizeLonLat, 0, 0);
    sqlite3_create_function (db, "ScaleCoords", 2, SQLITE_ANY, 0,
			     fnct_ScaleCoords, 0, 0);
    sqlite3_create_function (db, "ScaleCoordinates", 2, SQLITE_ANY, 0,
			     fnct_ScaleCoords, 0, 0);
    sqlite3_create_function (db, "ScaleCoords", 3, SQLITE_ANY, 0,
			     fnct_ScaleCoords, 0, 0);
    sqlite3_create_function (db, "ScaleCoordinates", 3, SQLITE_ANY, 0,
			     fnct_ScaleCoords, 0, 0);
    sqlite3_create_function (db, "RotateCoords", 2, SQLITE_ANY, 0,
			     fnct_RotateCoords, 0, 0);
    sqlite3_create_function (db, "RotateCoordinates", 2, SQLITE_ANY, 0,
			     fnct_RotateCoords, 0, 0);
    sqlite3_create_function (db, "ReflectCoords", 3, SQLITE_ANY, 0,
			     fnct_ReflectCoords, 0, 0);
    sqlite3_create_function (db, "ReflectCoordinates", 3, SQLITE_ANY, 0,
			     fnct_ReflectCoords, 0, 0);
    sqlite3_create_function (db, "SwapCoords", 1, SQLITE_ANY, 0,
			     fnct_SwapCoords, 0, 0);
    sqlite3_create_function (db, "SwapCoordinates", 1, SQLITE_ANY, 0,
			     fnct_SwapCoords, 0, 0);
    sqlite3_create_function (db, "BuildMbr", 4, SQLITE_ANY, 0, fnct_BuildMbr1,
			     0, 0);
    sqlite3_create_function (db, "BuildMbr", 5, SQLITE_ANY, 0, fnct_BuildMbr2,
			     0, 0);
    sqlite3_create_function (db, "BuildCircleMbr", 3, SQLITE_ANY, 0,
			     fnct_BuildCircleMbr1, 0, 0);
    sqlite3_create_function (db, "BuildCircleMbr", 4, SQLITE_ANY, 0,
			     fnct_BuildCircleMbr2, 0, 0);
    sqlite3_create_function (db, "Extent", 1, SQLITE_ANY, 0, 0,
			     fnct_Extent_step, fnct_Extent_final);
    sqlite3_create_function (db, "MbrMinX", 1, SQLITE_ANY, 0, fnct_MbrMinX, 0,
			     0);
    sqlite3_create_function (db, "MbrMaxX", 1, SQLITE_ANY, 0, fnct_MbrMaxX, 0,
			     0);
    sqlite3_create_function (db, "MbrMinY", 1, SQLITE_ANY, 0, fnct_MbrMinY, 0,
			     0);
    sqlite3_create_function (db, "MbrMaxY", 1, SQLITE_ANY, 0, fnct_MbrMaxY, 0,
			     0);
    sqlite3_create_function (db, "ST_Point", 2, SQLITE_ANY, 0, fnct_MakePoint1,
			     0, 0);
    sqlite3_create_function (db, "MakePoint", 2, SQLITE_ANY, 0,
			     fnct_MakePoint1, 0, 0);
    sqlite3_create_function (db, "MakePoint", 3, SQLITE_ANY, 0,
			     fnct_MakePoint2, 0, 0);
    sqlite3_create_function (db, "MakePointZ", 3, SQLITE_ANY, 0,
			     fnct_MakePointZ1, 0, 0);
    sqlite3_create_function (db, "MakePointZ", 4, SQLITE_ANY, 0,
			     fnct_MakePointZ2, 0, 0);
    sqlite3_create_function (db, "MakePointM", 3, SQLITE_ANY, 0,
			     fnct_MakePointM1, 0, 0);
    sqlite3_create_function (db, "MakePointM", 4, SQLITE_ANY, 0,
			     fnct_MakePointM2, 0, 0);
    sqlite3_create_function (db, "MakePointZM", 4, SQLITE_ANY, 0,
			     fnct_MakePointZM1, 0, 0);
    sqlite3_create_function (db, "MakePointZM", 5, SQLITE_ANY, 0,
			     fnct_MakePointZM2, 0, 0);
    sqlite3_create_function (db, "MakeLine", 1, SQLITE_ANY, 0, 0,
			     fnct_MakeLine_step, fnct_MakeLine_final);
    sqlite3_create_function (db, "MakeLine", 2, SQLITE_ANY, 0, fnct_MakeLine,
			     0, 0);
    sqlite3_create_function (db, "MakeCircle", 3, SQLITE_ANY, 0,
			     fnct_MakeCircle, 0, 0);
    sqlite3_create_function (db, "MakeCircle", 4, SQLITE_ANY, 0,
			     fnct_MakeCircle, 0, 0);
    sqlite3_create_function (db, "MakeCircle", 5, SQLITE_ANY, 0,
			     fnct_MakeCircle, 0, 0);
    sqlite3_create_function (db, "MakeEllipse", 4, SQLITE_ANY, 0,
			     fnct_MakeEllipse, 0, 0);
    sqlite3_create_function (db, "MakeEllipse", 5, SQLITE_ANY, 0,
			     fnct_MakeEllipse, 0, 0);
    sqlite3_create_function (db, "MakeEllipse", 6, SQLITE_ANY, 0,
			     fnct_MakeEllipse, 0, 0);
    sqlite3_create_function (db, "MakeArc", 5, SQLITE_ANY, 0, fnct_MakeArc, 0,
			     0);
    sqlite3_create_function (db, "MakeArc", 6, SQLITE_ANY, 0, fnct_MakeArc, 0,
			     0);
    sqlite3_create_function (db, "MakeArc", 7, SQLITE_ANY, 0, fnct_MakeArc, 0,
			     0);
    sqlite3_create_function (db, "MakeEllipticArc", 6, SQLITE_ANY, 0,
			     fnct_MakeEllipticArc, 0, 0);
    sqlite3_create_function (db, "MakeEllipticArc", 7, SQLITE_ANY, 0,
			     fnct_MakeEllipticArc, 0, 0);
    sqlite3_create_function (db, "MakeEllipticArc", 8, SQLITE_ANY, 0,
			     fnct_MakeEllipticArc, 0, 0);
    sqlite3_create_function (db, "MakeCircularSector", 5, SQLITE_ANY, 0,
			     fnct_MakeCircularSector, 0, 0);
    sqlite3_create_function (db, "MakeCircularSector", 6, SQLITE_ANY, 0,
			     fnct_MakeCircularSector, 0, 0);
    sqlite3_create_function (db, "MakeCircularSector", 7, SQLITE_ANY, 0,
			     fnct_MakeCircularSector, 0, 0);
    sqlite3_create_function (db, "MakeCircularStripe", 6, SQLITE_ANY, 0,
			     fnct_MakeCircularStripe, 0, 0);
    sqlite3_create_function (db, "MakeCircularStripe", 7, SQLITE_ANY, 0,
			     fnct_MakeCircularStripe, 0, 0);
    sqlite3_create_function (db, "MakeCircularStripe", 8, SQLITE_ANY, 0,
			     fnct_MakeCircularStripe, 0, 0);
    sqlite3_create_function (db, "MakeEllipticSector", 6, SQLITE_ANY, 0,
			     fnct_MakeEllipticSector, 0, 0);
    sqlite3_create_function (db, "MakeEllipticSector", 7, SQLITE_ANY, 0,
			     fnct_MakeEllipticSector, 0, 0);
    sqlite3_create_function (db, "MakeEllipticSector", 8, SQLITE_ANY, 0,
			     fnct_MakeEllipticSector, 0, 0);
    sqlite3_create_function (db, "Collect", 1, SQLITE_ANY, 0, 0,
			     fnct_Collect_step, fnct_Collect_final);
    sqlite3_create_function (db, "Collect", 2, SQLITE_ANY, 0, fnct_Collect, 0,
			     0);
    sqlite3_create_function (db, "ST_Collect", 1, SQLITE_ANY, 0, 0,
			     fnct_Collect_step, fnct_Collect_final);
    sqlite3_create_function (db, "ST_Collect", 2, SQLITE_ANY, 0, fnct_Collect,
			     0, 0);
    sqlite3_create_function (db, "BuildMbrFilter", 4, SQLITE_ANY, 0,
			     fnct_BuildMbrFilter, 0, 0);
    sqlite3_create_function (db, "FilterMbrWithin", 4, SQLITE_ANY, 0,
			     fnct_FilterMbrWithin, 0, 0);
    sqlite3_create_function (db, "FilterMbrContains", 4, SQLITE_ANY, 0,
			     fnct_FilterMbrContains, 0, 0);
    sqlite3_create_function (db, "FilterMbrIntersects", 4, SQLITE_ANY, 0,
			     fnct_FilterMbrIntersects, 0, 0);
    sqlite3_create_function (db, "LinesFromRings", 1, SQLITE_ANY, 0,
			     fnct_LinesFromRings, 0, 0);
    sqlite3_create_function (db, "ST_LinesFromRings", 1, SQLITE_ANY, 0,
			     fnct_LinesFromRings, 0, 0);
    sqlite3_create_function (db, "LinesFromRings", 2, SQLITE_ANY, 0,
			     fnct_LinesFromRings, 0, 0);
    sqlite3_create_function (db, "ST_LinesFromRings", 2, SQLITE_ANY, 0,
			     fnct_LinesFromRings, 0, 0);
    sqlite3_create_function (db, "ST_NPoints", 1, SQLITE_ANY, 0, fnct_NPoints,
			     0, 0);
    sqlite3_create_function (db, "ST_nrings", 1, SQLITE_ANY, 0, fnct_NRings, 0,
			     0);
    sqlite3_create_function (db, "ToGARS", 1, SQLITE_ANY, 0, fnct_ToGARS, 0, 0);
    sqlite3_create_function (db, "GARSMbr", 1, SQLITE_ANY, 0, fnct_GARSMbr, 0,
			     0);
    sqlite3_create_function (db, "SnapToGrid", 2, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "ST_SnapToGrid", 2, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "SnapToGrid", 3, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "ST_SnapToGrid", 3, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "SnapToGrid", 5, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "ST_SnapToGrid", 5, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "SnapToGrid", 6, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "ST_SnapToGrid", 6, SQLITE_ANY, 0,
			     fnct_SnapToGrid, 0, 0);
    sqlite3_create_function (db, "AddPoint", 2, SQLITE_ANY, 0, fnct_AddPoint,
			     0, 0);
    sqlite3_create_function (db, "ST_AddPoint", 2, SQLITE_ANY, 0,
			     fnct_AddPoint, 0, 0);
    sqlite3_create_function (db, "AddPoint", 3, SQLITE_ANY, 0, fnct_AddPoint,
			     0, 0);
    sqlite3_create_function (db, "ST_AddPoint", 3, SQLITE_ANY, 0,
			     fnct_AddPoint, 0, 0);
    sqlite3_create_function (db, "RemovePoint", 2, SQLITE_ANY, 0,
			     fnct_RemovePoint, 0, 0);
    sqlite3_create_function (db, "ST_RemovePoint", 2, SQLITE_ANY, 0,
			     fnct_RemovePoint, 0, 0);
    sqlite3_create_function (db, "SetPoint", 3, SQLITE_ANY, 0, fnct_SetPoint,
			     0, 0);
    sqlite3_create_function (db, "ST_SetPoint", 3, SQLITE_ANY, 0,
			     fnct_SetPoint, 0, 0);

#ifndef OMIT_GEOS		/* including GEOS */
    sqlite3_create_function (db, "BuildArea", 1, SQLITE_ANY, 0,
			     fnct_BuildArea, 0, 0);
    sqlite3_create_function (db, "ST_BuildArea", 1, SQLITE_ANY, 0,
			     fnct_BuildArea, 0, 0);
    sqlite3_create_function (db, "Polygonize", 1, SQLITE_ANY, 0, 0,
			     fnct_Polygonize_step, fnct_Polygonize_final);
    sqlite3_create_function (db, "ST_Polygonize", 1, SQLITE_ANY, 0, 0,
			     fnct_Polygonize_step, fnct_Polygonize_final);
#endif /* end including GEOS */

    sqlite3_create_function (db, "DissolveSegments", 1, SQLITE_ANY, 0,
			     fnct_DissolveSegments, 0, 0);
    sqlite3_create_function (db, "ST_DissolveSegments", 1, SQLITE_ANY, 0,
			     fnct_DissolveSegments, 0, 0);
    sqlite3_create_function (db, "DissolvePoints", 1, SQLITE_ANY, 0,
			     fnct_DissolvePoints, 0, 0);
    sqlite3_create_function (db, "ST_DissolvePoints", 1, SQLITE_ANY, 0,
			     fnct_DissolvePoints, 0, 0);
    sqlite3_create_function (db, "CollectionExtract", 2, SQLITE_ANY, 0,
			     fnct_CollectionExtract, 0, 0);
    sqlite3_create_function (db, "ST_CollectionExtract", 2, SQLITE_ANY, 0,
			     fnct_CollectionExtract, 0, 0);
    sqlite3_create_function (db, "ST_Locate_Along_Measure", 2, SQLITE_ANY, 0,
			     fnct_LocateBetweenMeasures, 0, 0);
    sqlite3_create_function (db, "ST_LocateAlong", 2, SQLITE_ANY, 0,
			     fnct_LocateBetweenMeasures, 0, 0);
    sqlite3_create_function (db, "ST_Locate_Between_Measures", 3, SQLITE_ANY,
			     0, fnct_LocateBetweenMeasures, 0, 0);
    sqlite3_create_function (db, "ST_LocateBetween", 3, SQLITE_ANY, 0,
			     fnct_LocateBetweenMeasures, 0, 0);
#ifndef OMIT_GEOCALLBACKS	/* supporting RTree geometry callbacks */
    sqlite3_rtree_geometry_callback (db, "RTreeWithin", fnct_RTreeIntersects,
				     0);
    sqlite3_rtree_geometry_callback (db, "RTreeContains",
				     fnct_RTreeIntersects, 0);
    sqlite3_rtree_geometry_callback (db, "RTreeIntersects",
				     fnct_RTreeIntersects, 0);
    sqlite3_rtree_geometry_callback (db, "RTreeDistWithin",
				     fnct_RTreeDistWithin, 0);
#endif /* end RTree geometry callbacks */

/* some BLOB/JPEG/EXIF functions */
    sqlite3_create_function (db, "IsGeometryBlob", 1, SQLITE_ANY, 0,
			     fnct_IsGeometryBlob, 0, 0);
    sqlite3_create_function (db, "IsZipBlob", 1, SQLITE_ANY, 0,
			     fnct_IsZipBlob, 0, 0);
    sqlite3_create_function (db, "IsPdfBlob", 1, SQLITE_ANY, 0,
			     fnct_IsPdfBlob, 0, 0);
    sqlite3_create_function (db, "IsTiffBlob", 1, SQLITE_ANY, 0,
			     fnct_IsTiffBlob, 0, 0);
    sqlite3_create_function (db, "IsGifBlob", 1, SQLITE_ANY, 0,
			     fnct_IsGifBlob, 0, 0);
    sqlite3_create_function (db, "IsPngBlob", 1, SQLITE_ANY, 0,
			     fnct_IsPngBlob, 0, 0);
    sqlite3_create_function (db, "IsJpegBlob", 1, SQLITE_ANY, 0,
			     fnct_IsJpegBlob, 0, 0);
    sqlite3_create_function (db, "IsExifBlob", 1, SQLITE_ANY, 0,
			     fnct_IsExifBlob, 0, 0);
    sqlite3_create_function (db, "IsExifGpsBlob", 1, SQLITE_ANY, 0,
			     fnct_IsExifGpsBlob, 0, 0);
    sqlite3_create_function (db, "IsWebpBlob", 1, SQLITE_ANY, 0,
			     fnct_IsWebPBlob, 0, 0);
    sqlite3_create_function (db, "GeomFromExifGpsBlob", 1, SQLITE_ANY, 0,
			     fnct_GeomFromExifGpsBlob, 0, 0);
    sqlite3_create_function (db, "GetMimeType", 1, SQLITE_ANY, 0,
			     fnct_GetMimeType, 0, 0);
    sqlite3_create_function (db, "CountUnsafeTriggers", 0, SQLITE_ANY, 0,
			     fnct_CountUnsafeTriggers, 0, 0);

/*
// enabling BlobFromFile, BlobToFile and XB_LoadXML, XB_StoreXML, ExportDXF
//
// these functions could potentially introduce serious security issues,
// most notably when invoked from within some Trigger
// - BlobToFile: some arbitrary code, possibly harmfull (e.g. virus or 
//   trojan) could be installed on the local file-system, the user being
//   completely unaware of this
// - BlobFromFile: some file could be maliciously "stolen" from the local
//   file system and then inseted into the DB
// - the same is for XB_LoadXML and XB_StoreXML
// - ExportDXF could potentially flood the local file-system by
//   outputting a huge size of data
//
// so by default such functions are disabled.
// if for any good/legitimate reason the user really wants to enable them
// the following environment variable has to be explicitly declared:
//
// SPATIALITE_SECURITY=relaxed
//
*/
    security_level = getenv ("SPATIALITE_SECURITY");
    if (security_level == NULL)
	;
    else if (strcasecmp (security_level, "relaxed") == 0)
      {
	  sqlite3_create_function (db, "BlobFromFile", 1, SQLITE_ANY, 0,
				   fnct_BlobFromFile, 0, 0);
	  sqlite3_create_function (db, "BlobToFile", 2, SQLITE_ANY, 0,
				   fnct_BlobToFile, 0, 0);
	  sqlite3_create_function (db, "ExportDXF", 9, SQLITE_ANY, 0,
				   fnct_ExportDXF, 0, 0);
	  sqlite3_create_function (db, "ExportDXF", 10, SQLITE_ANY, 0,
				   fnct_ExportDXF, 0, 0);

#ifdef ENABLE_LIBXML2		/* including LIBXML2 */

	  sqlite3_create_function (db, "XB_LoadXML", 1, SQLITE_ANY, cache,
				   fnct_XB_LoadXML, 0, 0);
	  sqlite3_create_function (db, "XB_StoreXML", 2, SQLITE_ANY, 0,
				   fnct_XB_StoreXML, 0, 0);
	  sqlite3_create_function (db, "XB_StoreXML", 3, SQLITE_ANY, 0,
				   fnct_XB_StoreXML, 0, 0);

#endif /* end including LIBXML2 */

      }

/* some Geodesic functions */
    sqlite3_create_function (db, "GreatCircleLength", 1, SQLITE_ANY, 0,
			     fnct_GreatCircleLength, 0, 0);
    sqlite3_create_function (db, "GeodesicLength", 1, SQLITE_ANY, 0,
			     fnct_GeodesicLength, 0, 0);

/* some Length Unit conversion functions */
    sqlite3_create_function (db, "CvtToKm", 1, SQLITE_ANY, 0, fnct_cvtToKm, 0,
			     0);
    sqlite3_create_function (db, "CvtToDm", 1, SQLITE_ANY, 0, fnct_cvtToDm, 0,
			     0);
    sqlite3_create_function (db, "CvtToCm", 1, SQLITE_ANY, 0, fnct_cvtToCm, 0,
			     0);
    sqlite3_create_function (db, "CvtToMm", 1, SQLITE_ANY, 0, fnct_cvtToMm, 0,
			     0);
    sqlite3_create_function (db, "CvtToKmi", 1, SQLITE_ANY, 0, fnct_cvtToKmi,
			     0, 0);
    sqlite3_create_function (db, "CvtToIn", 1, SQLITE_ANY, 0, fnct_cvtToIn, 0,
			     0);
    sqlite3_create_function (db, "CvtToFt", 1, SQLITE_ANY, 0, fnct_cvtToFt, 0,
			     0);
    sqlite3_create_function (db, "CvtToYd", 1, SQLITE_ANY, 0, fnct_cvtToYd, 0,
			     0);
    sqlite3_create_function (db, "CvtToMi", 1, SQLITE_ANY, 0, fnct_cvtToMi, 0,
			     0);
    sqlite3_create_function (db, "CvtToFath", 1, SQLITE_ANY, 0,
			     fnct_cvtToFath, 0, 0);
    sqlite3_create_function (db, "CvtToCh", 1, SQLITE_ANY, 0, fnct_cvtToCh, 0,
			     0);
    sqlite3_create_function (db, "CvtToLink", 1, SQLITE_ANY, 0,
			     fnct_cvtToLink, 0, 0);
    sqlite3_create_function (db, "CvtToUsIn", 1, SQLITE_ANY, 0,
			     fnct_cvtToUsIn, 0, 0);
    sqlite3_create_function (db, "CvtToUsFt", 1, SQLITE_ANY, 0,
			     fnct_cvtToUsFt, 0, 0);
    sqlite3_create_function (db, "CvtToUsYd", 1, SQLITE_ANY, 0,
			     fnct_cvtToUsYd, 0, 0);
    sqlite3_create_function (db, "CvtToUsCh", 1, SQLITE_ANY, 0,
			     fnct_cvtToUsCh, 0, 0);
    sqlite3_create_function (db, "CvtToUsMi", 1, SQLITE_ANY, 0,
			     fnct_cvtToUsMi, 0, 0);
    sqlite3_create_function (db, "CvtToIndFt", 1, SQLITE_ANY, 0,
			     fnct_cvtToIndFt, 0, 0);
    sqlite3_create_function (db, "CvtToIndYd", 1, SQLITE_ANY, 0,
			     fnct_cvtToIndYd, 0, 0);
    sqlite3_create_function (db, "CvtToIndCh", 1, SQLITE_ANY, 0,
			     fnct_cvtToIndCh, 0, 0);
    sqlite3_create_function (db, "CvtFromKm", 1, SQLITE_ANY, 0,
			     fnct_cvtFromKm, 0, 0);
    sqlite3_create_function (db, "CvtFromDm", 1, SQLITE_ANY, 0,
			     fnct_cvtFromDm, 0, 0);
    sqlite3_create_function (db, "CvtFromCm", 1, SQLITE_ANY, 0,
			     fnct_cvtFromCm, 0, 0);
    sqlite3_create_function (db, "CvtFromMm", 1, SQLITE_ANY, 0,
			     fnct_cvtFromMm, 0, 0);
    sqlite3_create_function (db, "CvtFromKmi", 1, SQLITE_ANY, 0,
			     fnct_cvtFromKmi, 0, 0);
    sqlite3_create_function (db, "CvtFromIn", 1, SQLITE_ANY, 0,
			     fnct_cvtFromIn, 0, 0);
    sqlite3_create_function (db, "CvtFromFt", 1, SQLITE_ANY, 0,
			     fnct_cvtFromFt, 0, 0);
    sqlite3_create_function (db, "CvtFromYd", 1, SQLITE_ANY, 0,
			     fnct_cvtFromYd, 0, 0);
    sqlite3_create_function (db, "CvtFromMi", 1, SQLITE_ANY, 0,
			     fnct_cvtFromMi, 0, 0);
    sqlite3_create_function (db, "CvtFromFath", 1, SQLITE_ANY, 0,
			     fnct_cvtFromFath, 0, 0);
    sqlite3_create_function (db, "CvtFromCh", 1, SQLITE_ANY, 0,
			     fnct_cvtFromCh, 0, 0);
    sqlite3_create_function (db, "CvtFromLink", 1, SQLITE_ANY, 0,
			     fnct_cvtFromLink, 0, 0);
    sqlite3_create_function (db, "CvtFromUsIn", 1, SQLITE_ANY, 0,
			     fnct_cvtFromUsIn, 0, 0);
    sqlite3_create_function (db, "CvtFromUsFt", 1, SQLITE_ANY, 0,
			     fnct_cvtFromUsFt, 0, 0);
    sqlite3_create_function (db, "CvtFromUsYd", 1, SQLITE_ANY, 0,
			     fnct_cvtFromUsYd, 0, 0);
    sqlite3_create_function (db, "CvtFromUsCh", 1, SQLITE_ANY, 0,
			     fnct_cvtFromUsCh, 0, 0);
    sqlite3_create_function (db, "CvtFromUsMi", 1, SQLITE_ANY, 0,
			     fnct_cvtFromUsMi, 0, 0);
    sqlite3_create_function (db, "CvtFromIndFt", 1, SQLITE_ANY, 0,
			     fnct_cvtFromIndFt, 0, 0);
    sqlite3_create_function (db, "CvtFromIndYd", 1, SQLITE_ANY, 0,
			     fnct_cvtFromIndYd, 0, 0);
    sqlite3_create_function (db, "CvtFromIndCh", 1, SQLITE_ANY, 0,
			     fnct_cvtFromIndCh, 0, 0);

#ifndef OMIT_MATHSQL		/* supporting SQL math functions */

/* some extra math functions */
    sqlite3_create_function (db, "acos", 1, SQLITE_ANY, 0, fnct_math_acos, 0,
			     0);
    sqlite3_create_function (db, "asin", 1, SQLITE_ANY, 0, fnct_math_asin, 0,
			     0);
    sqlite3_create_function (db, "atan", 1, SQLITE_ANY, 0, fnct_math_atan, 0,
			     0);
    sqlite3_create_function (db, "ceil", 1, SQLITE_ANY, 0, fnct_math_ceil, 0,
			     0);
    sqlite3_create_function (db, "ceiling", 1, SQLITE_ANY, 0, fnct_math_ceil,
			     0, 0);
    sqlite3_create_function (db, "cos", 1, SQLITE_ANY, 0, fnct_math_cos, 0, 0);
    sqlite3_create_function (db, "cot", 1, SQLITE_ANY, 0, fnct_math_cot, 0, 0);
    sqlite3_create_function (db, "degrees", 1, SQLITE_ANY, 0,
			     fnct_math_degrees, 0, 0);
    sqlite3_create_function (db, "exp", 1, SQLITE_ANY, 0, fnct_math_exp, 0, 0);
    sqlite3_create_function (db, "floor", 1, SQLITE_ANY, 0, fnct_math_floor,
			     0, 0);
    sqlite3_create_function (db, "ln", 1, SQLITE_ANY, 0, fnct_math_logn, 0, 0);
    sqlite3_create_function (db, "log", 1, SQLITE_ANY, 0, fnct_math_logn, 0, 0);
    sqlite3_create_function (db, "log", 2, SQLITE_ANY, 0, fnct_math_logn2, 0,
			     0);
    sqlite3_create_function (db, "log2", 1, SQLITE_ANY, 0, fnct_math_log_2, 0,
			     0);
    sqlite3_create_function (db, "log10", 1, SQLITE_ANY, 0, fnct_math_log_10,
			     0, 0);
    sqlite3_create_function (db, "pi", 0, SQLITE_ANY, 0, fnct_math_pi, 0, 0);
    sqlite3_create_function (db, "pow", 2, SQLITE_ANY, 0, fnct_math_pow, 0, 0);
    sqlite3_create_function (db, "power", 2, SQLITE_ANY, 0, fnct_math_pow, 0,
			     0);
    sqlite3_create_function (db, "radians", 1, SQLITE_ANY, 0,
			     fnct_math_radians, 0, 0);
    sqlite3_create_function (db, "round", 1, SQLITE_ANY, 0, fnct_math_round,
			     0, 0);
    sqlite3_create_function (db, "sign", 1, SQLITE_ANY, 0, fnct_math_sign, 0,
			     0);
    sqlite3_create_function (db, "sin", 1, SQLITE_ANY, 0, fnct_math_sin, 0, 0);
    sqlite3_create_function (db, "stddev_pop", 1, SQLITE_ANY, 0, 0,
			     fnct_math_stddev_step, fnct_math_stddev_pop_final);
    sqlite3_create_function (db, "stddev_samp", 1, SQLITE_ANY, 0, 0,
			     fnct_math_stddev_step,
			     fnct_math_stddev_samp_final);
    sqlite3_create_function (db, "sqrt", 1, SQLITE_ANY, 0, fnct_math_sqrt, 0,
			     0);
    sqlite3_create_function (db, "tan", 1, SQLITE_ANY, 0, fnct_math_tan, 0, 0);
    sqlite3_create_function (db, "var_pop", 1, SQLITE_ANY, 0, 0,
			     fnct_math_stddev_step, fnct_math_var_pop_final);
    sqlite3_create_function (db, "var_samp", 1, SQLITE_ANY, 0, 0,
			     fnct_math_stddev_step, fnct_math_var_samp_final);

#endif /* end supporting SQL math functions */

#ifndef OMIT_PROJ		/* including PROJ.4 */

    sqlite3_create_function (db, "Transform", 2, SQLITE_ANY, 0,
			     fnct_Transform, 0, 0);
    sqlite3_create_function (db, "ST_Transform", 2, SQLITE_ANY, 0,
			     fnct_Transform, 0, 0);

#endif /* end including PROJ.4 */

#ifndef OMIT_GEOS		/* including GEOS */

    sqlite3_create_function (db, "Boundary", 1, SQLITE_ANY, 0, fnct_Boundary,
			     0, 0);
    sqlite3_create_function (db, "ST_Boundary", 1, SQLITE_ANY, 0,
			     fnct_Boundary, 0, 0);
    sqlite3_create_function (db, "IsClosed", 1, SQLITE_ANY, 0, fnct_IsClosed,
			     0, 0);
    sqlite3_create_function (db, "ST_IsClosed", 1, SQLITE_ANY, 0,
			     fnct_IsClosed, 0, 0);
    sqlite3_create_function (db, "IsSimple", 1, SQLITE_ANY, 0, fnct_IsSimple,
			     0, 0);
    sqlite3_create_function (db, "ST_IsSimple", 1, SQLITE_ANY, 0,
			     fnct_IsSimple, 0, 0);
    sqlite3_create_function (db, "IsRing", 1, SQLITE_ANY, 0, fnct_IsRing, 0, 0);
    sqlite3_create_function (db, "ST_IsRing", 1, SQLITE_ANY, 0, fnct_IsRing, 0,
			     0);
    sqlite3_create_function (db, "IsValid", 1, SQLITE_ANY, 0, fnct_IsValid, 0,
			     0);
    sqlite3_create_function (db, "ST_IsValid", 1, SQLITE_ANY, 0, fnct_IsValid,
			     0, 0);
    sqlite3_create_function (db, "GLength", 1, SQLITE_ANY, 0, fnct_Length, 0,
			     0);
    sqlite3_create_function (db, "GLength", 2, SQLITE_ANY, 0, fnct_Length, 0,
			     0);
    sqlite3_create_function (db, "ST_Length", 1, SQLITE_ANY, 0, fnct_Length, 0,
			     0);
    sqlite3_create_function (db, "ST_Length", 2, SQLITE_ANY, 0, fnct_Length, 0,
			     0);
    sqlite3_create_function (db, "Perimeter", 1, SQLITE_ANY, 0, fnct_Perimeter,
			     0, 0);
    sqlite3_create_function (db, "Perimeter", 2, SQLITE_ANY, 0, fnct_Perimeter,
			     0, 0);
    sqlite3_create_function (db, "ST_Perimeter", 1, SQLITE_ANY, 0,
			     fnct_Perimeter, 0, 0);
    sqlite3_create_function (db, "ST_Perimeter", 2, SQLITE_ANY, 0,
			     fnct_Perimeter, 0, 0);
    sqlite3_create_function (db, "Area", 1, SQLITE_ANY, 0, fnct_Area, 0, 0);
    sqlite3_create_function (db, "ST_Area", 1, SQLITE_ANY, 0, fnct_Area, 0, 0);
    sqlite3_create_function (db, "ST_Centroid", 1, SQLITE_ANY, 0,
			     fnct_Centroid, 0, 0);
    sqlite3_create_function (db, "Centroid", 1, SQLITE_ANY, 0, fnct_Centroid,
			     0, 0);
    sqlite3_create_function (db, "PointOnSurface", 1, SQLITE_ANY, 0,
			     fnct_PointOnSurface, 0, 0);
    sqlite3_create_function (db, "ST_PointOnSurface", 1, SQLITE_ANY, 0,
			     fnct_PointOnSurface, 0, 0);
    sqlite3_create_function (db, "Simplify", 2, SQLITE_ANY, 0, fnct_Simplify,
			     0, 0);
    sqlite3_create_function (db, "ST_Simplify", 2, SQLITE_ANY, 0,
			     fnct_Simplify, 0, 0);
    sqlite3_create_function (db, "ST_Generalize", 2, SQLITE_ANY, 0,
			     fnct_Simplify, 0, 0);
    sqlite3_create_function (db, "SimplifyPreserveTopology", 2, SQLITE_ANY, 0,
			     fnct_SimplifyPreserveTopology, 0, 0);
    sqlite3_create_function (db, "ST_SimplifyPreserveTopology", 2, SQLITE_ANY,
			     0, fnct_SimplifyPreserveTopology, 0, 0);
    sqlite3_create_function (db, "ConvexHull", 1, SQLITE_ANY, 0,
			     fnct_ConvexHull, 0, 0);
    sqlite3_create_function (db, "ST_ConvexHull", 1, SQLITE_ANY, 0,
			     fnct_ConvexHull, 0, 0);
    sqlite3_create_function (db, "Buffer", 2, SQLITE_ANY, 0, fnct_Buffer, 0, 0);
    sqlite3_create_function (db, "ST_Buffer", 2, SQLITE_ANY, 0, fnct_Buffer, 0,
			     0);
    sqlite3_create_function (db, "Intersection", 2, SQLITE_ANY, 0,
			     fnct_Intersection, 0, 0);
    sqlite3_create_function (db, "ST_Intersection", 2, SQLITE_ANY, 0,
			     fnct_Intersection, 0, 0);
    sqlite3_create_function (db, "GUnion", 1, SQLITE_ANY, 0, 0,
			     fnct_Union_step, fnct_Union_final);
    sqlite3_create_function (db, "GUnion", 2, SQLITE_ANY, 0, fnct_Union, 0, 0);
    sqlite3_create_function (db, "ST_Union", 1, SQLITE_ANY, 0, 0,
			     fnct_Union_step, fnct_Union_final);
    sqlite3_create_function (db, "ST_Union", 2, SQLITE_ANY, 0, fnct_Union, 0,
			     0);
    sqlite3_create_function (db, "Difference", 2, SQLITE_ANY, 0,
			     fnct_Difference, 0, 0);
    sqlite3_create_function (db, "ST_Difference", 2, SQLITE_ANY, 0,
			     fnct_Difference, 0, 0);
    sqlite3_create_function (db, "SymDifference", 2, SQLITE_ANY, 0,
			     fnct_SymDifference, 0, 0);
    sqlite3_create_function (db, "ST_SymDifference", 2, SQLITE_ANY, 0,
			     fnct_SymDifference, 0, 0);
    sqlite3_create_function (db, "Equals", 2, SQLITE_ANY, 0, fnct_Equals, 0, 0);
    sqlite3_create_function (db, "ST_Equals", 2, SQLITE_ANY, 0, fnct_Equals, 0,
			     0);
    sqlite3_create_function (db, "Intersects", 2, SQLITE_ANY, cache,
			     fnct_Intersects, 0, 0);
    sqlite3_create_function (db, "ST_Intersects", 2, SQLITE_ANY, cache,
			     fnct_Intersects, 0, 0);
    sqlite3_create_function (db, "Disjoint", 2, SQLITE_ANY, cache,
			     fnct_Disjoint, 0, 0);
    sqlite3_create_function (db, "ST_Disjoint", 2, SQLITE_ANY, cache,
			     fnct_Disjoint, 0, 0);
    sqlite3_create_function (db, "Overlaps", 2, SQLITE_ANY, cache,
			     fnct_Overlaps, 0, 0);
    sqlite3_create_function (db, "ST_Overlaps", 2, SQLITE_ANY, cache,
			     fnct_Overlaps, 0, 0);
    sqlite3_create_function (db, "Crosses", 2, SQLITE_ANY, cache, fnct_Crosses,
			     0, 0);
    sqlite3_create_function (db, "ST_Crosses", 2, SQLITE_ANY, cache,
			     fnct_Crosses, 0, 0);
    sqlite3_create_function (db, "Touches", 2, SQLITE_ANY, cache, fnct_Touches,
			     0, 0);
    sqlite3_create_function (db, "ST_Touches", 2, SQLITE_ANY, cache,
			     fnct_Touches, 0, 0);
    sqlite3_create_function (db, "Within", 2, SQLITE_ANY, cache, fnct_Within,
			     0, 0);
    sqlite3_create_function (db, "ST_Within", 2, SQLITE_ANY, cache,
			     fnct_Within, 0, 0);
    sqlite3_create_function (db, "Contains", 2, SQLITE_ANY, cache,
			     fnct_Contains, 0, 0);
    sqlite3_create_function (db, "ST_Contains", 2, SQLITE_ANY, cache,
			     fnct_Contains, 0, 0);
    sqlite3_create_function (db, "Relate", 3, SQLITE_ANY, 0, fnct_Relate, 0, 0);
    sqlite3_create_function (db, "ST_Relate", 3, SQLITE_ANY, 0, fnct_Relate, 0,
			     0);
    sqlite3_create_function (db, "Distance", 2, SQLITE_ANY, 0, fnct_Distance,
			     0, 0);
    sqlite3_create_function (db, "Distance", 3, SQLITE_ANY, 0, fnct_Distance,
			     0, 0);
    sqlite3_create_function (db, "ST_Distance", 2, SQLITE_ANY, 0,
			     fnct_Distance, 0, 0);
    sqlite3_create_function (db, "ST_Distance", 3, SQLITE_ANY, 0,
			     fnct_Distance, 0, 0);
    sqlite3_create_function (db, "PtDistWithin", 3, SQLITE_ANY, 0,
			     fnct_PtDistWithin, 0, 0);
    sqlite3_create_function (db, "PtDistWithin", 4, SQLITE_ANY, 0,
			     fnct_PtDistWithin, 0, 0);
    sqlite3_create_function (db, "BdPolyFromText", 1, SQLITE_ANY, 0,
			     fnct_BdPolyFromText1, 0, 0);
    sqlite3_create_function (db, "BdPolyFromText", 2, SQLITE_ANY, 0,
			     fnct_BdPolyFromText2, 0, 0);
    sqlite3_create_function (db, "BdMPolyFromText", 1, SQLITE_ANY, 0,
			     fnct_BdMPolyFromText1, 0, 0);
    sqlite3_create_function (db, "BdMPolyFromText", 2, SQLITE_ANY, 0,
			     fnct_BdMPolyFromText2, 0, 0);
    sqlite3_create_function (db, "BdPolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_BdPolyFromWKB1, 0, 0);
    sqlite3_create_function (db, "BdPolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_BdPolyFromWKB2, 0, 0);
    sqlite3_create_function (db, "BdMPolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_BdMPolyFromWKB1, 0, 0);
    sqlite3_create_function (db, "BdMPolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_BdMPolyFromWKB2, 0, 0);
    sqlite3_create_function (db, "ST_BdPolyFromText", 1, SQLITE_ANY, 0,
			     fnct_BdPolyFromText1, 0, 0);
    sqlite3_create_function (db, "ST_BdPolyFromText", 2, SQLITE_ANY, 0,
			     fnct_BdPolyFromText2, 0, 0);
    sqlite3_create_function (db, "ST_BdMPolyFromText", 1, SQLITE_ANY, 0,
			     fnct_BdMPolyFromText1, 0, 0);
    sqlite3_create_function (db, "ST_BdMPolyFromText", 2, SQLITE_ANY, 0,
			     fnct_BdMPolyFromText2, 0, 0);
    sqlite3_create_function (db, "ST_BdPolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_BdPolyFromWKB1, 0, 0);
    sqlite3_create_function (db, "ST_BdPolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_BdPolyFromWKB2, 0, 0);
    sqlite3_create_function (db, "ST_BdMPolyFromWKB", 1, SQLITE_ANY, 0,
			     fnct_BdMPolyFromWKB1, 0, 0);
    sqlite3_create_function (db, "ST_BdMPolyFromWKB", 2, SQLITE_ANY, 0,
			     fnct_BdMPolyFromWKB2, 0, 0);

#ifdef GEOS_ADVANCED		/* GEOS advanced features */

    sqlite3_create_function (db, "CreateTopologyTables", 2, SQLITE_ANY, 0,
			     fnct_CreateTopologyTables, 0, 0);
    sqlite3_create_function (db, "CreateTopologyTables", 3, SQLITE_ANY, 0,
			     fnct_CreateTopologyTables, 0, 0);
    sqlite3_create_function (db, "OffsetCurve", 3, SQLITE_ANY, 0,
			     fnct_OffsetCurve, 0, 0);
    sqlite3_create_function (db, "ST_OffsetCurve", 3, SQLITE_ANY, 0,
			     fnct_OffsetCurve, 0, 0);
    sqlite3_create_function (db, "SingleSidedBuffer", 3, SQLITE_ANY, 0,
			     fnct_SingleSidedBuffer, 0, 0);
    sqlite3_create_function (db, "ST_SingleSidedBuffer", 3, SQLITE_ANY, 0,
			     fnct_SingleSidedBuffer, 0, 0);
    sqlite3_create_function (db, "HausdorffDistance", 2, SQLITE_ANY, 0,
			     fnct_HausdorffDistance, 0, 0);
    sqlite3_create_function (db, "ST_HausdorffDistance", 2, SQLITE_ANY, 0,
			     fnct_HausdorffDistance, 0, 0);
    sqlite3_create_function (db, "SharedPaths", 2, SQLITE_ANY, 0,
			     fnct_SharedPaths, 0, 0);
    sqlite3_create_function (db, "ST_SharedPaths", 2, SQLITE_ANY, 0,
			     fnct_SharedPaths, 0, 0);
    sqlite3_create_function (db, "Covers", 2, SQLITE_ANY, cache, fnct_Covers,
			     0, 0);
    sqlite3_create_function (db, "ST_Covers", 2, SQLITE_ANY, cache,
			     fnct_Covers, 0, 0);
    sqlite3_create_function (db, "CoveredBy", 2, SQLITE_ANY, cache,
			     fnct_CoveredBy, 0, 0);
    sqlite3_create_function (db, "ST_CoveredBy", 2, SQLITE_ANY, cache,
			     fnct_CoveredBy, 0, 0);
    sqlite3_create_function (db, "Line_Interpolate_Point", 2, SQLITE_ANY, 0,
			     fnct_LineInterpolatePoint, 0, 0);
    sqlite3_create_function (db, "ST_Line_Interpolate_Point", 2, SQLITE_ANY,
			     0, fnct_LineInterpolatePoint, 0, 0);
    sqlite3_create_function (db, "Line_Interpolate_Equidistant_Points", 2,
			     SQLITE_ANY, 0,
			     fnct_LineInterpolateEquidistantPoints, 0, 0);
    sqlite3_create_function (db, "ST_Line_Interpolate_Equidistant_Points", 2,
			     SQLITE_ANY, 0,
			     fnct_LineInterpolateEquidistantPoints, 0, 0);
    sqlite3_create_function (db, "Line_Locate_Point", 2, SQLITE_ANY, 0,
			     fnct_LineLocatePoint, 0, 0);
    sqlite3_create_function (db, "ST_Line_Locate_Point", 2, SQLITE_ANY, 0,
			     fnct_LineLocatePoint, 0, 0);
    sqlite3_create_function (db, "Line_Substring", 3, SQLITE_ANY, 0,
			     fnct_LineSubstring, 0, 0);
    sqlite3_create_function (db, "ST_Line_Substring", 3, SQLITE_ANY, 0,
			     fnct_LineSubstring, 0, 0);
    sqlite3_create_function (db, "ClosestPoint", 2, SQLITE_ANY, 0,
			     fnct_ClosestPoint, 0, 0);
    sqlite3_create_function (db, "ST_ClosestPoint", 2, SQLITE_ANY, 0,
			     fnct_ClosestPoint, 0, 0);
    sqlite3_create_function (db, "ShortestLine", 2, SQLITE_ANY, 0,
			     fnct_ShortestLine, 0, 0);
    sqlite3_create_function (db, "ST_ShortestLine", 2, SQLITE_ANY, 0,
			     fnct_ShortestLine, 0, 0);
    sqlite3_create_function (db, "Snap", 3, SQLITE_ANY, 0, fnct_Snap, 0, 0);
    sqlite3_create_function (db, "ST_Snap", 3, SQLITE_ANY, 0, fnct_Snap, 0, 0);
    sqlite3_create_function (db, "LineMerge", 1, SQLITE_ANY, 0,
			     fnct_LineMerge, 0, 0);
    sqlite3_create_function (db, "ST_LineMerge", 1, SQLITE_ANY, 0,
			     fnct_LineMerge, 0, 0);
    sqlite3_create_function (db, "UnaryUnion", 1, SQLITE_ANY, 0,
			     fnct_UnaryUnion, 0, 0);
    sqlite3_create_function (db, "ST_UnaryUnion", 1, SQLITE_ANY, 0,
			     fnct_UnaryUnion, 0, 0);
    sqlite3_create_function (db, "SquareGrid", 2, SQLITE_ANY, 0,
			     fnct_SquareGrid, 0, 0);
    sqlite3_create_function (db, "SquareGrid", 3, SQLITE_ANY, 0,
			     fnct_SquareGrid, 0, 0);
    sqlite3_create_function (db, "SquareGrid", 4, SQLITE_ANY, 0,
			     fnct_SquareGrid, 0, 0);
    sqlite3_create_function (db, "ST_SquareGrid", 2, SQLITE_ANY, 0,
			     fnct_SquareGrid, 0, 0);
    sqlite3_create_function (db, "ST_SquareGrid", 3, SQLITE_ANY, 0,
			     fnct_SquareGrid, 0, 0);
    sqlite3_create_function (db, "ST_SquareGrid", 4, SQLITE_ANY, 0,
			     fnct_SquareGrid, 0, 0);
    sqlite3_create_function (db, "TriangularGrid", 2, SQLITE_ANY, 0,
			     fnct_TriangularGrid, 0, 0);
    sqlite3_create_function (db, "TriangularGrid", 3, SQLITE_ANY, 0,
			     fnct_TriangularGrid, 0, 0);
    sqlite3_create_function (db, "TriangularGrid", 4, SQLITE_ANY, 0,
			     fnct_TriangularGrid, 0, 0);
    sqlite3_create_function (db, "ST_TriangularGrid", 2, SQLITE_ANY, 0,
			     fnct_TriangularGrid, 0, 0);
    sqlite3_create_function (db, "ST_TriangularGrid", 3, SQLITE_ANY, 0,
			     fnct_TriangularGrid, 0, 0);
    sqlite3_create_function (db, "ST_TriangularGrid", 4, SQLITE_ANY, 0,
			     fnct_TriangularGrid, 0, 0);
    sqlite3_create_function (db, "HexagonalGrid", 2, SQLITE_ANY, 0,
			     fnct_HexagonalGrid, 0, 0);
    sqlite3_create_function (db, "HexagonalGrid", 3, SQLITE_ANY, 0,
			     fnct_HexagonalGrid, 0, 0);
    sqlite3_create_function (db, "HexagonalGrid", 4, SQLITE_ANY, 0,
			     fnct_HexagonalGrid, 0, 0);
    sqlite3_create_function (db, "ST_HexagonalGrid", 2, SQLITE_ANY, 0,
			     fnct_HexagonalGrid, 0, 0);
    sqlite3_create_function (db, "ST_HexagonalGrid", 3, SQLITE_ANY, 0,
			     fnct_HexagonalGrid, 0, 0);
    sqlite3_create_function (db, "ST_HexagonalGrid", 4, SQLITE_ANY, 0,
			     fnct_HexagonalGrid, 0, 0);
    sqlite3_create_function (db, "LinesCutAtNodes", 2, SQLITE_ANY, 0,
			     fnct_LinesCutAtNodes, 0, 0);
    sqlite3_create_function (db, "ST_LinesCutAtNodes", 2, SQLITE_ANY, 0,
			     fnct_LinesCutAtNodes, 0, 0);
    sqlite3_create_function (db, "RingsCutAtNodes", 1, SQLITE_ANY, 0,
			     fnct_RingsCutAtNodes, 0, 0);
    sqlite3_create_function (db, "ST_RingsCutAtNodes", 1, SQLITE_ANY, 0,
			     fnct_RingsCutAtNodes, 0, 0);

#endif /* end GEOS advanced features */

#ifdef GEOS_TRUNK		/* GEOS experimental features */

    sqlite3_create_function (db, "DelaunayTriangulation", 1, SQLITE_ANY, 0,
			     fnct_DelaunayTriangulation, 0, 0);
    sqlite3_create_function (db, "DelaunayTriangulation", 2, SQLITE_ANY, 0,
			     fnct_DelaunayTriangulation, 0, 0);
    sqlite3_create_function (db, "DelaunayTriangulation", 3, SQLITE_ANY, 0,
			     fnct_DelaunayTriangulation, 0, 0);
    sqlite3_create_function (db, "ST_DelaunayTriangulation", 1, SQLITE_ANY, 0,
			     fnct_DelaunayTriangulation, 0, 0);
    sqlite3_create_function (db, "ST_DelaunayTriangulation", 2, SQLITE_ANY, 0,
			     fnct_DelaunayTriangulation, 0, 0);
    sqlite3_create_function (db, "ST_DelaunayTriangulation", 3, SQLITE_ANY, 0,
			     fnct_DelaunayTriangulation, 0, 0);
    sqlite3_create_function (db, "VoronojDiagram", 1, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "VoronojDiagram", 2, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "VoronojDiagram", 3, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "VoronojDiagram", 4, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "ST_VoronojDiagram", 1, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "ST_VoronojDiagram", 2, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "ST_VoronojDiagram", 3, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "ST_VoronojDiagram", 4, SQLITE_ANY, 0,
			     fnct_VoronojDiagram, 0, 0);
    sqlite3_create_function (db, "ConcaveHull", 1, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ConcaveHull", 2, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ConcaveHull", 3, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ConcaveHull", 4, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ST_ConcaveHull", 1, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ST_ConcaveHull", 2, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ST_ConcaveHull", 3, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);
    sqlite3_create_function (db, "ST_ConcaveHull", 4, SQLITE_ANY, 0,
			     fnct_ConcaveHull, 0, 0);

#endif /* end GEOS experimental features */

#ifdef ENABLE_LWGEOM		/* enabling LWGEOM support */

    sqlite3_create_function (db, "MakeValid", 1, SQLITE_ANY, 0,
			     fnct_MakeValid, 0, 0);
    sqlite3_create_function (db, "ST_MakeValid", 1, SQLITE_ANY, 0,
			     fnct_MakeValid, 0, 0);
    sqlite3_create_function (db, "MakeValidDiscarded", 1, SQLITE_ANY, 0,
			     fnct_MakeValidDiscarded, 0, 0);
    sqlite3_create_function (db, "ST_MakeValidDiscarded", 1, SQLITE_ANY, 0,
			     fnct_MakeValidDiscarded, 0, 0);
    sqlite3_create_function (db, "Area", 2, SQLITE_ANY, 0, fnct_Area, 0, 0);
    sqlite3_create_function (db, "ST_Area", 2, SQLITE_ANY, 0, fnct_Area, 0, 0);
    sqlite3_create_function (db, "Segmentize", 2, SQLITE_ANY, 0,
			     fnct_Segmentize, 0, 0);
    sqlite3_create_function (db, "ST_Segmentize", 2, SQLITE_ANY, 0,
			     fnct_Segmentize, 0, 0);
    sqlite3_create_function (db, "Azimuth", 2, SQLITE_ANY, 0, fnct_Azimuth, 0,
			     0);
    sqlite3_create_function (db, "ST_Azimuth", 2, SQLITE_ANY, 0, fnct_Azimuth,
			     0, 0);
    sqlite3_create_function (db, "Project", 3, SQLITE_ANY, 0, fnct_Project, 0,
			     0);
    sqlite3_create_function (db, "ST_Project", 3, SQLITE_ANY, 0, fnct_Project,
			     0, 0);
    sqlite3_create_function (db, "GeoHash", 1, SQLITE_ANY, 0, fnct_GeoHash, 0,
			     0);
    sqlite3_create_function (db, "GeoHash", 2, SQLITE_ANY, 0, fnct_GeoHash, 0,
			     0);
    sqlite3_create_function (db, "ST_GeoHash", 1, SQLITE_ANY, 0, fnct_GeoHash,
			     0, 0);
    sqlite3_create_function (db, "ST_GeoHash", 2, SQLITE_ANY, 0, fnct_GeoHash,
			     0, 0);
    sqlite3_create_function (db, "AsX3D", 1, SQLITE_ANY, 0, fnct_AsX3D, 0, 0);
    sqlite3_create_function (db, "AsX3D", 2, SQLITE_ANY, 0, fnct_AsX3D, 0, 0);
    sqlite3_create_function (db, "AsX3D", 3, SQLITE_ANY, 0, fnct_AsX3D, 0, 0);
    sqlite3_create_function (db, "AsX3D", 4, SQLITE_ANY, 0, fnct_AsX3D, 0, 0);
    sqlite3_create_function (db, "ST_AsX3D", 1, SQLITE_ANY, 0, fnct_AsX3D,
			     0, 0);
    sqlite3_create_function (db, "ST_AsX3D", 2, SQLITE_ANY, 0, fnct_AsX3D,
			     0, 0);
    sqlite3_create_function (db, "ST_AsX3D", 3, SQLITE_ANY, 0, fnct_AsX3D,
			     0, 0);
    sqlite3_create_function (db, "ST_AsX3D", 4, SQLITE_ANY, 0, fnct_AsX3D,
			     0, 0);
    sqlite3_create_function (db, "ST_3DDistance", 2, SQLITE_ANY, 0,
			     fnct_3DDistance, 0, 0);
    sqlite3_create_function (db, "MaxDistance", 2, SQLITE_ANY, 0,
			     fnct_MaxDistance, 0, 0);
    sqlite3_create_function (db, "ST_MaxDistance", 2, SQLITE_ANY, 0,
			     fnct_MaxDistance, 0, 0);
    sqlite3_create_function (db, "ST_3DMaxDistance", 2, SQLITE_ANY, 0,
			     fnct_3DMaxDistance, 0, 0);
    sqlite3_create_function (db, "Split", 2, SQLITE_ANY, 0, fnct_Split, 0, 0);
    sqlite3_create_function (db, "ST_Split", 2, SQLITE_ANY, 0, fnct_Split,
			     0, 0);
    sqlite3_create_function (db, "SplitLeft", 2, SQLITE_ANY, 0,
			     fnct_SplitLeft, 0, 0);
    sqlite3_create_function (db, "ST_SplitLeft", 2, SQLITE_ANY, 0,
			     fnct_SplitLeft, 0, 0);
    sqlite3_create_function (db, "SplitRight", 2, SQLITE_ANY, 0,
			     fnct_SplitRight, 0, 0);
    sqlite3_create_function (db, "ST_SplitRight", 2, SQLITE_ANY, 0,
			     fnct_SplitRight, 0, 0);

#endif /* end LWGEOM support */

#endif /* end including GEOS */

#ifdef ENABLE_LIBXML2		/* including LIBXML2 */

    sqlite3_create_function (db, "CreateStylingTables", 0, SQLITE_ANY, 0,
			     fnct_CreateStylingTables, 0, 0);
    sqlite3_create_function (db, "CreateStylingTables", 1, SQLITE_ANY, 0,
			     fnct_CreateStylingTables, 0, 0);
    sqlite3_create_function (db, "RegisterExternalGraphic", 2, SQLITE_ANY, 0,
			     fnct_RegisterExternalGraphic, 0, 0);
    sqlite3_create_function (db, "RegisterExternalGraphic", 5, SQLITE_ANY, 0,
			     fnct_RegisterExternalGraphic, 0, 0);
    sqlite3_create_function (db, "RegisterVectorStyledLayer", 3, SQLITE_ANY,
			     0, fnct_RegisterVectorStyledLayer, 0, 0);
    sqlite3_create_function (db, "RegisterVectorStyledLayer", 4, SQLITE_ANY,
			     0, fnct_RegisterVectorStyledLayer, 0, 0);
    sqlite3_create_function (db, "RegisterRasterStyledLayer", 2, SQLITE_ANY,
			     0, fnct_RegisterRasterStyledLayer, 0, 0);
    sqlite3_create_function (db, "RegisterRasterStyledLayer", 3, SQLITE_ANY,
			     0, fnct_RegisterRasterStyledLayer, 0, 0);
    sqlite3_create_function (db, "RegisterStyledGroup", 3, SQLITE_ANY, 0,
			     fnct_RegisterStyledGroup, 0, 0);
    sqlite3_create_function (db, "RegisterStyledGroup", 4, SQLITE_ANY, 0,
			     fnct_RegisterStyledGroup, 0, 0);
    sqlite3_create_function (db, "RegisterStyledGroup", 5, SQLITE_ANY, 0,
			     fnct_RegisterStyledGroup, 0, 0);
    sqlite3_create_function (db, "SetStyledGroupInfos", 3, SQLITE_ANY, 0,
			     fnct_SetStyledGroupInfos, 0, 0);
    sqlite3_create_function (db, "CreateIsoMetadataTables", 0, SQLITE_ANY, 0,
			     fnct_CreateIsoMetadataTables, 0, 0);
    sqlite3_create_function (db, "CreateIsoMetadataTables", 1, SQLITE_ANY, 0,
			     fnct_CreateIsoMetadataTables, 0, 0);
    sqlite3_create_function (db, "GetIsoMetadataId", 1, SQLITE_ANY, 0,
			     fnct_GetIsoMetadataId, 0, 0);
    sqlite3_create_function (db, "RegisterIsoMetadata", 2, SQLITE_ANY, 0,
			     fnct_RegisterIsoMetadata, 0, 0);
    sqlite3_create_function (db, "RegisterIsoMetadata", 3, SQLITE_ANY, 0,
			     fnct_RegisterIsoMetadata, 0, 0);
    sqlite3_create_function (db, "XB_Create", 1, SQLITE_ANY, cache,
			     fnct_XB_Create, 0, 0);
    sqlite3_create_function (db, "XB_Create", 2, SQLITE_ANY, cache,
			     fnct_XB_Create, 0, 0);
    sqlite3_create_function (db, "XB_Create", 3, SQLITE_ANY, cache,
			     fnct_XB_Create, 0, 0);
    sqlite3_create_function (db, "XB_GetPayload", 1, SQLITE_ANY, 0,
			     fnct_XB_GetPayload, 0, 0);
    sqlite3_create_function (db, "XB_GetPayload", 2, SQLITE_ANY, 0,
			     fnct_XB_GetPayload, 0, 0);
    sqlite3_create_function (db, "XB_GetDocument", 1, SQLITE_ANY, 0,
			     fnct_XB_GetDocument, 0, 0);
    sqlite3_create_function (db, "XB_GetDocument", 2, SQLITE_ANY, 0,
			     fnct_XB_GetDocument, 0, 0);
    sqlite3_create_function (db, "XB_SchemaValidate", 2, SQLITE_ANY, cache,
			     fnct_XB_SchemaValidate, 0, 0);
    sqlite3_create_function (db, "XB_Compress", 1, SQLITE_ANY, 0,
			     fnct_XB_Compress, 0, 0);
    sqlite3_create_function (db, "XB_Uncompress", 1, SQLITE_ANY, 0,
			     fnct_XB_Uncompress, 0, 0);
    sqlite3_create_function (db, "XB_IsValid", 1, SQLITE_ANY, 0,
			     fnct_XB_IsValid, 0, 0);
    sqlite3_create_function (db, "XB_IsSchemaValidated", 1, SQLITE_ANY, 0,
			     fnct_XB_IsSchemaValidated, 0, 0);
    sqlite3_create_function (db, "XB_IsCompressed", 1, SQLITE_ANY, 0,
			     fnct_XB_IsCompressed, 0, 0);
    sqlite3_create_function (db, "XB_IsIsoMetadata", 1, SQLITE_ANY, 0,
			     fnct_XB_IsIsoMetadata, 0, 0);
    sqlite3_create_function (db, "XB_IsSldSeVectorStyle", 1, SQLITE_ANY, 0,
			     fnct_XB_IsSldSeVectorStyle, 0, 0);
    sqlite3_create_function (db, "XB_IsSldSeRasterStyle", 1, SQLITE_ANY, 0,
			     fnct_XB_IsSldSeRasterStyle, 0, 0);
    sqlite3_create_function (db, "XB_IsSvg", 1, SQLITE_ANY, 0, fnct_XB_IsSvg,
			     0, 0);
    sqlite3_create_function (db, "XB_GetSchemaURI", 1, SQLITE_ANY, 0,
			     fnct_XB_GetSchemaURI, 0, 0);
    sqlite3_create_function (db, "XB_GetInternalSchemaURI", 1, SQLITE_ANY,
			     cache, fnct_XB_GetInternalSchemaURI, 0, 0);
    sqlite3_create_function (db, "XB_GetFileId", 1, SQLITE_ANY, 0,
			     fnct_XB_GetFileId, 0, 0);
    sqlite3_create_function (db, "XB_GetParentId", 1, SQLITE_ANY, 0,
			     fnct_XB_GetParentId, 0, 0);
    sqlite3_create_function (db, "XB_SetFileId", 2, SQLITE_ANY, cache,
			     fnct_XB_SetFileId, 0, 0);
    sqlite3_create_function (db, "XB_SetParentId", 2, SQLITE_ANY, cache,
			     fnct_XB_SetParentId, 0, 0);
    sqlite3_create_function (db, "XB_AddFileId", 6, SQLITE_ANY, cache,
			     fnct_XB_AddFileId, 0, 0);
    sqlite3_create_function (db, "XB_AddParentId", 6, SQLITE_ANY, cache,
			     fnct_XB_AddParentId, 0, 0);
    sqlite3_create_function (db, "XB_GetTitle", 1, SQLITE_ANY, 0,
			     fnct_XB_GetTitle, 0, 0);
    sqlite3_create_function (db, "XB_GetAbstract", 1, SQLITE_ANY, 0,
			     fnct_XB_GetAbstract, 0, 0);
    sqlite3_create_function (db, "XB_GetGeometry", 1, SQLITE_ANY, 0,
			     fnct_XB_GetGeometry, 0, 0);
    sqlite3_create_function (db, "XB_GetDocumentSize", 1, SQLITE_ANY, 0,
			     fnct_XB_GetDocumentSize, 0, 0);
    sqlite3_create_function (db, "XB_GetEncoding", 1, SQLITE_ANY, 0,
			     fnct_XB_GetEncoding, 0, 0);
    sqlite3_create_function (db, "XB_GetLastParseError", 0, SQLITE_ANY, cache,
			     fnct_XB_GetLastParseError, 0, 0);
    sqlite3_create_function (db, "XB_GetLastValidateError", 0, SQLITE_ANY,
			     cache, fnct_XB_GetLastValidateError, 0, 0);
    sqlite3_create_function (db, "XB_IsValidXPathExpression", 1, SQLITE_ANY,
			     cache, fnct_XB_IsValidXPathExpression, 0, 0);
    sqlite3_create_function (db, "XB_GetLastXPathError", 0, SQLITE_ANY, cache,
			     fnct_XB_GetLastXPathError, 0, 0);
    sqlite3_create_function (db, "XB_CacheFlush", 0, SQLITE_ANY, cache,
			     fnct_XB_CacheFlush, 0, 0);

#endif /* end including LIBXML2 */

#ifdef ENABLE_GEOPACKAGE
    /* not yet finalised geopackage raster functions, plus some convenience API */
    sqlite3_create_function (db, "gpkgCreateBaseTables", 0, SQLITE_ANY, 0,
			     fnct_gpkgCreateBaseTables, 0, 0);
    sqlite3_create_function (db, "gpkgCreateTilesTable", 2, SQLITE_ANY, 0,
			     fnct_gpkgCreateTilesTable, 0, 0);
    sqlite3_create_function (db, "gpkgCreateTilesZoomLevel", 4, SQLITE_ANY, 0,
			     fnct_gpkgCreateTilesZoomLevel, 0, 0);
    sqlite3_create_function (db, "gpkgAddTileTriggers", 1, SQLITE_ANY, 0,
			     fnct_gpkgAddTileTriggers, 0, 0);
    sqlite3_create_function (db, "gpkgAddRtMetadataTriggers", 1, SQLITE_ANY,
			     0, fnct_gpkgAddRtMetadataTriggers, 0, 0);
    sqlite3_create_function (db, "gpkgGetNormalZoom", 2, SQLITE_ANY, 0,
			     fnct_gpkgGetNormalZoom, 0, 0);
    sqlite3_create_function (db, "gpkgGetNormalRow", 3, SQLITE_ANY, 0,
			     fnct_gpkgGetNormalRow, 0, 0);
    sqlite3_create_function (db, "gpkgGetImageType", 1, SQLITE_ANY, 0,
			     fnct_gpkgGetImageType, 0, 0);
    sqlite3_create_function (db, "gpkgPointToTile", 5, SQLITE_ANY, 0,
			     fnct_gpkgPointToTile, 0, 0);

#endif /* enabling GeoPackage extensions */

    return cache;
}

SPATIALITE_PRIVATE void
init_spatialite_virtualtables (void *p_db, void *p_cache)
{
    sqlite3 *db = (sqlite3 *) p_db;

#ifndef OMIT_ICONV		/* when ICONV is disabled SHP/DBF/TXT cannot be supported */
/* initializing the VirtualShape  extension */
    virtualshape_extension_init (db);
/* initializing the VirtualDbf  extension */
    virtualdbf_extension_init (db);
/* initializing the VirtualText extension */
    virtualtext_extension_init (db);

#ifndef OMIT_FREEXL
/* initializing the VirtualXL  extension */
    virtualXL_extension_init (db);
#endif /* FreeXL enabled/disable */
#endif /* ICONV enabled/disabled */

/* initializing the VirtualNetwork  extension */
    virtualnetwork_extension_init (db);
/* initializing the MbrCache  extension */
    mbrcache_extension_init (db);
/* initializing the VirtualFDO  extension */
    virtualfdo_extension_init (db);
/* initializing the VirtualSpatialIndex  extension */
    virtual_spatialindex_extension_init (db);

#ifdef ENABLE_LIBXML2		/* including LIBXML2 */
/* initializing the VirtualXPath extension */
    virtual_xpath_extension_init (db, p_cache);
#endif /* LIBXML2 enabled/disable */
}

#ifdef LOADABLE_EXTENSION	/* loadable-extension only */
static int
init_spatialite_extension (sqlite3 * db, char **pzErrMsg,
			   const sqlite3_api_routines * pApi)
{
    void *p_cache;
    SQLITE_EXTENSION_INIT2 (pApi);
/* setting the POSIX locale for numeric */
    setlocale (LC_NUMERIC, "POSIX");
    *pzErrMsg = NULL;

    p_cache = register_spatialite_sql_functions (db, NULL);

    init_spatialite_virtualtables (db, p_cache);

/* setting a timeout handler */
    sqlite3_busy_timeout (db, 5000);

    return 0;
}
#endif

SPATIALITE_DECLARE void
spatialite_init_geos (void)
{
/* initializes GEOS (or resets to initial state - as required by LWGEOM) */
#ifndef OMIT_GEOS		/* initializing GEOS */
    initGEOS (geos_warning, geos_error);
#endif /* end GEOS  */
}

SPATIALITE_PRIVATE void
spatialite_splash_screen (int verbose)
{
    if (isatty (1))
      {
	  /* printing "hello" message only when stdout is on console */
	  if (verbose)
	    {
		spatialite_i ("SpatiaLite version ..: %s",
			      spatialite_version ());
		spatialite_i ("\tSupported Extensions:\n");
#ifndef OMIT_ICONV		/* ICONV is required by SHP/DBF/TXT */
		spatialite_i
		    ("\t- 'VirtualShape'\t[direct Shapefile access]\n");
		spatialite_i ("\t- 'VirtualDbf'\t\t[direct DBF access]\n");
#ifndef OMIT_FREEXL
		spatialite_i ("\t- 'VirtualXL'\t\t[direct XLS access]\n");
#endif /* end FreeXL conditional */
		spatialite_i ("\t- 'VirtualText'\t\t[direct CSV/TXT access]\n");
#endif /* end ICONV conditional */
		spatialite_i
		    ("\t- 'VirtualNetwork'\t[Dijkstra shortest path]\n");
		spatialite_i ("\t- 'RTree'\t\t[Spatial Index - R*Tree]\n");
		spatialite_i
		    ("\t- 'MbrCache'\t\t[Spatial Index - MBR cache]\n");
		spatialite_i
		    ("\t- 'VirtualSpatialIndex'\t[R*Tree metahandler]\n");

#ifdef ENABLE_LIBXML2		/* VirtualXPath is supported */
		spatialite_i
		    ("\t- 'VirtualXPath'\t[XML Path Language - XPath]\n");
#endif /* end including LIBXML2 */

		spatialite_i
		    ("\t- 'VirtualFDO'\t\t[FDO-OGR interoperability]\n");
		spatialite_i ("\t- 'SpatiaLite'\t\t[Spatial SQL - OGC]\n");
	    }
#ifndef OMIT_PROJ		/* PROJ.4 version */
	  if (verbose)
	      spatialite_i ("PROJ.4 version ......: %s\n", pj_get_release ());
#endif /* end including PROJ.4 */
#ifndef OMIT_GEOS		/* GEOS version */
	  if (verbose)
	      spatialite_i ("GEOS version ........: %s\n", GEOSversion ());
#endif /* end GEOS version */
#ifdef ENABLE_LWGEOM		/* LWGEOM version */
	  if (verbose)
	      spatialite_i ("LWGEOM version ......: %s\n",
			    splite_lwgeom_version ());
#endif /* end LWGEOM version */
      }
}

#ifndef LOADABLE_EXTENSION
SPATIALITE_DECLARE void
spatialite_init_ex (sqlite3 * db_handle, void *p_cache, int verbose)
{
/* used when SQLite initializes as an ordinary lib */
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;

/* setting the POSIX locale for numeric */
    setlocale (LC_NUMERIC, "POSIX");

#ifndef OMIT_GEOS		/* initializing GEOS */
    initGEOS (geos_warning, geos_error);
#endif /* end GEOS  */

#ifdef POSTGIS_2_1		/* initializing liblwgeom from PostGIS 2.1.x (or later) */
    splite_lwgeom_init ();
#endif /* end POSTGIS_2_1 */

    register_spatialite_sql_functions (db_handle, cache);

    init_spatialite_virtualtables (db_handle, p_cache);
    spatialite_splash_screen (verbose);

/* setting a timeout handler */
    sqlite3_busy_timeout (db_handle, 5000);
}

static void
free_internal_cache (struct splite_internal_cache *cache)
{
/* freeing an internal cache */
    struct splite_geos_cache_item *p;
#ifdef ENABLE_LIBXML2
    int i;
    struct splite_xmlSchema_cache_item *p_xmlSchema;
#endif
/* freeing the XML error buffers */
    gaiaOutBufferReset (cache->xmlParsingErrors);
    gaiaOutBufferReset (cache->xmlSchemaValidationErrors);
    gaiaOutBufferReset (cache->xmlXPathErrors);
    free (cache->xmlParsingErrors);
    free (cache->xmlSchemaValidationErrors);
    free (cache->xmlXPathErrors);

/* freeing the GEOS cache */
    p = &(cache->cacheItem1);
    splite_free_geos_cache_item (p);
    p = &(cache->cacheItem2);
    splite_free_geos_cache_item (p);
#ifdef ENABLE_LIBXML2
    for (i = 0; i < MAX_XMLSCHEMA_CACHE; i++)
      {
	  /* freeing the XmlSchema cache */
	  p_xmlSchema = &(cache->xmlSchemaCache[i]);
	  splite_free_xml_schema_cache_item (p_xmlSchema);
      }
#endif
/* freeing the cache itself */
    free (cache);
}

SPATIALITE_DECLARE void
spatialite_cleanup_ex (void *ptr)
{
    struct splite_internal_cache *cache = (struct splite_internal_cache *) ptr;

#ifndef OMIT_GEOS
    finishGEOS ();
    gaiaResetGeosMsg ();
#endif

#ifdef ENABLE_LWGEOM
    gaiaResetLwGeomMsg ();
#endif

    free_internal_cache (cache);
    sqlite3_reset_auto_extension ();
}
#endif /* not built as loadable-extension only */

#ifdef LOADABLE_EXTENSION	/* loadable-extension only */
#if !(defined _WIN32) || defined(__MINGW32__)
/* MSVC is unable to understand this declaration */
__attribute__ ((visibility ("default")))
#endif
     SPATIALITE_DECLARE int
	 sqlite3_extension_init (sqlite3 * db, char **pzErrMsg,
				 const sqlite3_api_routines * pApi)
{
/* SQLite invokes this routine once when it dynamically loads the extension. */

#ifndef OMIT_GEOS		/* initializing GEOS */
    initGEOS (geos_warning, geos_error);
#endif /* end GEOS  */

#ifdef POSTGIS_2_1		/* initializing liblwgeom from PostGIS 2.1.x (or later) */
    splite_lwgeom_init ();
#endif /* end POSTGIS_2_1 */

    return init_spatialite_extension (db, pzErrMsg, pApi);
}
#endif

SPATIALITE_DECLARE sqlite3_int64
math_llabs (sqlite3_int64 value)
{
/* replacing the C99 llabs() function */
    return value < 0 ? -value : value;
}

SPATIALITE_DECLARE double
math_round (double value)
{
/* replacing the C99 round() function */
    double min = floor (value);
    if (fabs (value - min) < 0.5)
	return min;
    return min + 1.0;
}
