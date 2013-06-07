/*

 gg_relations.c -- Gaia spatial relations
    
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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#include "config-msvc.h"
#else
#include "config.h"
#endif

#ifndef OMIT_GEOS		/* including GEOS */
#include <geos_c.h>
#endif

#include <spatialite_private.h>
#include <spatialite/sqlite.h>

#include <spatialite/gaiageo.h>

/* GLOBAL variables */
char *gaia_geos_error_msg = NULL;
char *gaia_geos_warning_msg = NULL;
char *gaia_geosaux_error_msg = NULL;

SPATIALITE_PRIVATE void
splite_free_geos_cache_item (struct splite_geos_cache_item *p)
{
#ifndef OMIT_GEOS		/* including GEOS */
    if (p->preparedGeosGeom)
	GEOSPreparedGeom_destroy (p->preparedGeosGeom);
    if (p->geosGeom)
	GEOSGeom_destroy (p->geosGeom);
#endif
    p->geosGeom = NULL;
    p->preparedGeosGeom = NULL;
}

GAIAGEO_DECLARE void
gaiaResetGeosMsg ()
{
/* resets the GEOS error and warning messages */
    if (gaia_geos_error_msg != NULL)
	free (gaia_geos_error_msg);
    if (gaia_geos_warning_msg != NULL)
	free (gaia_geos_warning_msg);
    if (gaia_geosaux_error_msg != NULL)
	free (gaia_geosaux_error_msg);
    gaia_geos_error_msg = NULL;
    gaia_geos_warning_msg = NULL;
    gaia_geosaux_error_msg = NULL;
}

GAIAGEO_DECLARE const char *
gaiaGetGeosErrorMsg ()
{
/* return the latest GEOS error message */
    return gaia_geos_error_msg;
}

GAIAGEO_DECLARE const char *
gaiaGetGeosWarningMsg ()
{
/* return the latest GEOS error message */
    return gaia_geos_warning_msg;
}

GAIAGEO_DECLARE const char *
gaiaGetGeosAuxErrorMsg ()
{
/* return the latest GEOS (auxialiary) error message */
    return gaia_geosaux_error_msg;
}

GAIAGEO_DECLARE void
gaiaSetGeosErrorMsg (const char *msg)
{
/* return the latest GEOS error message */
    int len;
    if (gaia_geos_error_msg != NULL)
	free (gaia_geos_error_msg);
    gaia_geos_error_msg = NULL;
    if (msg == NULL)
	return;
    len = strlen (msg);
    gaia_geos_error_msg = malloc (len + 1);
    strcpy (gaia_geos_error_msg, msg);
}

GAIAGEO_DECLARE void
gaiaSetGeosWarningMsg (const char *msg)
{
/* return the latest GEOS error message */
    int len;
    if (gaia_geos_warning_msg != NULL)
	free (gaia_geos_warning_msg);
    gaia_geos_warning_msg = NULL;
    if (msg == NULL)
	return;
    len = strlen (msg);
    gaia_geos_warning_msg = malloc (len + 1);
    strcpy (gaia_geos_warning_msg, msg);
}

GAIAGEO_DECLARE void
gaiaSetGeosAuxErrorMsg (const char *msg)
{
/* return the latest GEOS (auxiliary) error message */
    int len;
    if (gaia_geosaux_error_msg != NULL)
	free (gaia_geosaux_error_msg);
    gaia_geosaux_error_msg = NULL;
    if (msg == NULL)
	return;
    len = strlen (msg);
    gaia_geosaux_error_msg = malloc (len + 1);
    strcpy (gaia_geosaux_error_msg, msg);
}

static int
check_point (double *coords, int points, double x, double y)
{
/* checks if [X,Y] point is defined into this coordinate array [Linestring or Ring] */
    int iv;
    double xx;
    double yy;
    for (iv = 0; iv < points; iv++)
      {
	  gaiaGetPoint (coords, iv, &xx, &yy);
	  if (xx == x && yy == y)
	      return 1;
      }
    return 0;
}

GAIAGEO_DECLARE int
gaiaLinestringEquals (gaiaLinestringPtr line1, gaiaLinestringPtr line2)
{
/* checks if two Linestrings are "spatially equal" */
    int iv;
    double x;
    double y;
    if (line1->Points != line2->Points)
	return 0;
    for (iv = 0; iv < line1->Points; iv++)
      {
	  gaiaGetPoint (line1->Coords, iv, &x, &y);
	  if (!check_point (line2->Coords, line2->Points, x, y))
	      return 0;
      }
    return 1;
}

GAIAGEO_DECLARE int
gaiaPolygonEquals (gaiaPolygonPtr polyg1, gaiaPolygonPtr polyg2)
{
/* checks if two Polygons are "spatially equal" */
    int ib;
    int ib2;
    int iv;
    int ok2;
    double x;
    double y;
    gaiaRingPtr ring1;
    gaiaRingPtr ring2;
    if (polyg1->NumInteriors != polyg2->NumInteriors)
	return 0;
/* checking the EXTERIOR RINGs */
    ring1 = polyg1->Exterior;
    ring2 = polyg2->Exterior;
    if (ring1->Points != ring2->Points)
	return 0;
    for (iv = 0; iv < ring1->Points; iv++)
      {
	  gaiaGetPoint (ring1->Coords, iv, &x, &y);
	  if (!check_point (ring2->Coords, ring2->Points, x, y))
	      return 0;
      }
    for (ib = 0; ib < polyg1->NumInteriors; ib++)
      {
	  /* checking the INTERIOR RINGS */
	  int ok = 0;
	  ring1 = polyg1->Interiors + ib;
	  for (ib2 = 0; ib2 < polyg2->NumInteriors; ib2++)
	    {
		ok2 = 1;
		ring2 = polyg2->Interiors + ib2;
		for (iv = 0; iv < ring1->Points; iv++)
		  {
		      gaiaGetPoint (ring1->Coords, iv, &x, &y);
		      if (!check_point (ring2->Coords, ring2->Points, x, y))
			{
			    ok2 = 0;
			    break;
			}
		  }
		if (ok2)
		  {
		      ok = 1;
		      break;
		  }
	    }
	  if (!ok)
	      return 0;
      }
    return 1;
}

#ifndef OMIT_GEOS		/* including GEOS */

static int
splite_mbr_overlaps (gaiaGeomCollPtr g1, gaiaGeomCollPtr g2)
{
/* checks if two MBRs do overlap */
    if (g1->MaxX < g2->MinX)
	return 0;
    if (g1->MinX > g2->MaxX)
	return 0;
    if (g1->MaxY < g2->MinY)
	return 0;
    if (g1->MinY > g2->MaxY)
	return 0;
    return 1;
}

static int
splite_mbr_contains (gaiaGeomCollPtr g1, gaiaGeomCollPtr g2)
{
/* checks if MBR#1 fully contains MBR#2 */
    if (g2->MinX < g1->MinX)
	return 0;
    if (g2->MaxX > g1->MaxX)
	return 0;
    if (g2->MinY < g1->MinY)
	return 0;
    if (g2->MaxY > g1->MaxY)
	return 0;
    return 1;
}

static int
splite_mbr_within (gaiaGeomCollPtr g1, gaiaGeomCollPtr g2)
{
/* checks if MBR#1 is fully contained within MBR#2 */
    if (g1->MinX < g2->MinX)
	return 0;
    if (g1->MaxX > g2->MaxX)
	return 0;
    if (g1->MinY < g2->MinY)
	return 0;
    if (g1->MaxY > g2->MaxY)
	return 0;
    return 1;
}

static int
splite_mbr_equals (gaiaGeomCollPtr g1, gaiaGeomCollPtr g2)
{
/* checks if MBR#1 equals MBR#2 */
    if (g1->MinX != g2->MinX)
	return 0;
    if (g1->MaxX != g2->MaxX)
	return 0;
    if (g1->MinY != g2->MinY)
	return 0;
    if (g1->MaxY != g2->MaxY)
	return 0;
    return 1;
}

static int
evalGeosCacheItem (unsigned char *blob, int blob_size, uLong crc,
		   struct splite_geos_cache_item *p)
{
/* evaluting if this one could be a valid cache hit */
    if (blob_size != p->gaiaBlobSize)
      {
	  /* surely not a match; different size */
	  return 0;
      }
    if (crc != p->crc32)
      {
	  /* surely not a match: different CRC32 */
	  return 0;
      }

/* the first 46 bytes of the BLOB contain the MBR,
   the SRID and the Type; so are assumed to represent 
   a valid signature */
    if (memcmp (blob, p->gaiaBlob, 46) == 0)
	return 1;
    return 0;
}

static int
evalGeosCache (struct splite_internal_cache *cache, gaiaGeomCollPtr geom1,
	       unsigned char *blob1, int size1, gaiaGeomCollPtr geom2,
	       unsigned char *blob2, int size2, GEOSPreparedGeometry ** gPrep,
	       gaiaGeomCollPtr * geom)
{
/* handling the internal GEOS cache */
#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */
    struct splite_geos_cache_item *p1 = &(cache->cacheItem1);
    struct splite_geos_cache_item *p2 = &(cache->cacheItem2);
    uLong crc1 = crc32 (0L, blob1, size1);
    uLong crc2 = crc32 (0L, blob2, size2);

/* checking the first cache item */
    if (evalGeosCacheItem (blob1, size1, crc1, p1))
      {
	  /* found a matching item */
	  if (p1->preparedGeosGeom == NULL)
	    {
		/* preparing the GeosGeometries */
		p1->geosGeom = gaiaToGeos (geom1);
		if (p1->geosGeom)
		  {
		      p1->preparedGeosGeom =
			  (void *) GEOSPrepare (p1->geosGeom);
		      if (p1->preparedGeosGeom == NULL)
			{
			    /* unexpected failure */
			    GEOSGeom_destroy (p1->geosGeom);
			    p1->geosGeom = NULL;
			}
		  }
	    }
	  if (p1->preparedGeosGeom)
	    {
		/* returning the corresponding GeosPreparedGeometry */
		*gPrep = p1->preparedGeosGeom;
		*geom = geom2;
		return 1;
	    }
	  return 0;
      }

/* checking the second cache item */
    if (evalGeosCacheItem (blob2, size2, crc2, p2))
      {
	  /* found a matching item */
	  if (p2->preparedGeosGeom == NULL)
	    {
		/* preparing the GeosGeometries */
		p2->geosGeom = gaiaToGeos (geom2);
		if (p2->geosGeom)
		  {
		      p2->preparedGeosGeom =
			  (void *) GEOSPrepare (p2->geosGeom);
		      if (p2->preparedGeosGeom == NULL)
			{
			    /* unexpected failure */
			    GEOSGeom_destroy (p2->geosGeom);
			    p2->geosGeom = NULL;
			}
		  }
	    }
	  if (p2->preparedGeosGeom)
	    {
		/* returning the corresponding GeosPreparedGeometry */
		*gPrep = p2->preparedGeosGeom;
		*geom = geom1;
		return 1;
	    }
	  return 0;
      }

/* updating the GEOS cache item#1 */
    memcpy (p1->gaiaBlob, blob1, 46);
    p1->gaiaBlobSize = size1;
    p1->crc32 = crc1;
    if (p1->preparedGeosGeom)
	GEOSPreparedGeom_destroy (p1->preparedGeosGeom);
    if (p1->geosGeom)
	GEOSGeom_destroy (p1->geosGeom);
    p1->geosGeom = NULL;
    p1->preparedGeosGeom = NULL;

/* updating the GEOS cache item#2 */
    memcpy (p2->gaiaBlob, blob2, 46);
    p2->gaiaBlobSize = size2;
    p2->crc32 = crc2;
    if (p2->preparedGeosGeom)
	GEOSPreparedGeom_destroy (p2->preparedGeosGeom);
    if (p2->geosGeom)
	GEOSGeom_destroy (p2->geosGeom);
    p2->geosGeom = NULL;
    p2->preparedGeosGeom = NULL;
#endif /* end GEOS_ADVANCED */

    return 0;
}

GAIAGEO_DECLARE int
gaiaGeomCollEquals (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if two Geometries are "spatially equal" */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_equals (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSEquals (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollIntersects (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if two Geometries do "spatially intersects" */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSIntersects (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedIntersects (void *p_cache, gaiaGeomCollPtr geom1,
				unsigned char *blob1, int size1,
				gaiaGeomCollPtr geom2, unsigned char *blob2,
				int size2)
{
/* checks if two Geometries do "spatially intersects" */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  ret = GEOSPreparedIntersects (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSIntersects (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollDisjoint (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if two Geometries are "spatially disjoint" */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 1;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSDisjoint (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedDisjoint (void *p_cache, gaiaGeomCollPtr geom1,
			      unsigned char *blob1, int size1,
			      gaiaGeomCollPtr geom2, unsigned char *blob2,
			      int size2)
{
/* checks if two Geometries are "spatially disjoint" */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 1;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  ret = GEOSPreparedDisjoint (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSDisjoint (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollOverlaps (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if two Geometries do "spatially overlaps" */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSOverlaps (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}


#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedOverlaps (void *p_cache, gaiaGeomCollPtr geom1,
			      unsigned char *blob1, int size1,
			      gaiaGeomCollPtr geom2, unsigned char *blob2,
			      int size2)
{
/* checks if two Geometries do "spatially overlaps" */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  ret = GEOSPreparedOverlaps (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSOverlaps (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollCrosses (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if two Geometries do "spatially crosses" */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSCrosses (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}


#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedCrosses (void *p_cache, gaiaGeomCollPtr geom1,
			     unsigned char *blob1, int size1,
			     gaiaGeomCollPtr geom2, unsigned char *blob2,
			     int size2)
{
/* checks if two Geometries do "spatially crosses" */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  ret = GEOSPreparedCrosses (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSCrosses (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollTouches (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if two Geometries do "spatially touches" */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSTouches (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedTouches (void *p_cache, gaiaGeomCollPtr geom1,
			     unsigned char *blob1, int size1,
			     gaiaGeomCollPtr geom2, unsigned char *blob2,
			     int size2)
{
/* checks if two Geometries do "spatially touches" */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSPreparedGeometry *gPrep;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  ret = GEOSPreparedTouches (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSTouches (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollWithin (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if GEOM-1 is completely contained within GEOM-2 */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_within (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSWithin (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedWithin (void *p_cache, gaiaGeomCollPtr geom1,
			    unsigned char *blob1, int size1,
			    gaiaGeomCollPtr geom2, unsigned char *blob2,
			    int size2)
{
/* checks if GEOM-1 is completely contained within GEOM-2 */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_within (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  if (geom == geom2)
	      ret = GEOSPreparedWithin (gPrep, g2);
	  else
	      ret = GEOSPreparedContains (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSWithin (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollContains (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if GEOM-1 completely contains GEOM-2 */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_contains (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSContains (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#ifdef GEOS_ADVANCED		/* only if GEOS advanced features are enable */

GAIAGEO_DECLARE int
gaiaGeomCollPreparedContains (void *p_cache, gaiaGeomCollPtr geom1,
			      unsigned char *blob1, int size1,
			      gaiaGeomCollPtr geom2, unsigned char *blob2,
			      int size2)
{
/* checks if GEOM-1 completely contains GEOM-2 */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_contains (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  if (geom == geom2)
	      ret = GEOSPreparedContains (gPrep, g2);
	  else
	      ret = GEOSPreparedWithin (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSContains (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return ret;
}

#endif /* end GEOS_ADVANCED */

GAIAGEO_DECLARE int
gaiaGeomCollRelate (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2,
		    const char *pattern)
{
/* checks if if GEOM-1 and GEOM-2 have a spatial relationship as specified by the pattern Matrix */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return -1;
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSRelatePattern (g1, g2, pattern);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollLength (gaiaGeomCollPtr geom, double *xlength)
{
/* computes the total length for this Geometry */
    double length;
    int ret;
    GEOSGeometry *g;
    if (!geom)
	return 0;
    if (gaiaIsToxic (geom))
	return 0;
    g = gaiaToGeos (geom);
    ret = GEOSLength (g, &length);
    GEOSGeom_destroy (g);
    if (ret)
	*xlength = length;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollLengthOrPerimeter (gaiaGeomCollPtr geom, int perimeter,
			       double *xlength)
{
/* computes the total length or perimeter for this Geometry */
    double length;
    int ret;
    GEOSGeometry *g;
    int mode = GAIA2GEOS_ONLY_LINESTRINGS;
    if (perimeter)
	mode = GAIA2GEOS_ONLY_POLYGONS;
    if (!geom)
	return 0;
    if (gaiaIsToxic (geom))
	return 0;
    g = gaiaToGeosSelective (geom, mode);
    if (g == NULL)
      {
	  *xlength = 0.0;
	  return 1;
      }
    ret = GEOSLength (g, &length);
    GEOSGeom_destroy (g);
    if (ret)
	*xlength = length;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollArea (gaiaGeomCollPtr geom, double *xarea)
{
/* computes the total area for this Geometry */
    double area;
    int ret;
    GEOSGeometry *g;
    if (!geom)
	return 0;
    if (gaiaIsToxic (geom))
	return 0;
    g = gaiaToGeos (geom);
    ret = GEOSArea (g, &area);
    GEOSGeom_destroy (g);
    if (ret)
	*xarea = area;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollDistance (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2,
		      double *xdist)
{
/* computes the minimum distance intercurring between GEOM-1 and GEOM-2 */
    double dist;
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return 0;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return 0;
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSDistance (g1, g2, &dist);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret)
	*xdist = dist;
    return ret;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeometryIntersection (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* builds a new geometry representing the "spatial intersection" of GEOM-1 and GEOM-2 */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSGeometry *g3;
    if (!geom1 || !geom2)
	return NULL;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return NULL;

/* quick check based on MBRs comparison */
    if (!splite_mbr_overlaps (geom1, geom2))
	return NULL;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    g3 = GEOSIntersection (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (!g3)
	return NULL;
    if (geom1->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g3);
    else if (geom1->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g3);
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g3);
    else
	geo = gaiaFromGeos_XY (g3);
    GEOSGeom_destroy (g3);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom1->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeometryUnion (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* builds a new geometry representing the "spatial union" of GEOM-1 and GEOM-2 */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSGeometry *g3;
    if (!geom1 || !geom2)
	return NULL;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return NULL;
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    g3 = GEOSUnion (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (geom1->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g3);
    else if (geom1->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g3);
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g3);
    else
	geo = gaiaFromGeos_XY (g3);
    GEOSGeom_destroy (g3);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom1->Srid;
    if (geo->DeclaredType == GAIA_POINT &&
	geom1->DeclaredType == GAIA_MULTIPOINT)
	geo->DeclaredType = GAIA_MULTIPOINT;
    if (geo->DeclaredType == GAIA_LINESTRING &&
	geom1->DeclaredType == GAIA_MULTILINESTRING)
	geo->DeclaredType = GAIA_MULTILINESTRING;
    if (geo->DeclaredType == GAIA_POLYGON &&
	geom1->DeclaredType == GAIA_MULTIPOLYGON)
	geo->DeclaredType = GAIA_MULTIPOLYGON;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaUnionCascaded (gaiaGeomCollPtr geom)
{
/* UnionCascaded (single Collection of polygons) */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr result;
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;

/* testing if geom only contains Polygons */
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
	return NULL;
    if (!pgs)
	return NULL;

    g1 = gaiaToGeos (geom);
    g2 = GEOSUnionCascaded (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g2);
    else
	result = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (result == NULL)
	return NULL;
    result->Srid = geom->Srid;
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeometryDifference (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* builds a new geometry representing the "spatial difference" of GEOM-1 and GEOM-2 */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSGeometry *g3;
    if (!geom1 || !geom2)
	return NULL;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return NULL;
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    g3 = GEOSDifference (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (!g3)
	return NULL;
    if (geom1->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g3);
    else if (geom1->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g3);
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g3);
    else
	geo = gaiaFromGeos_XY (g3);
    GEOSGeom_destroy (g3);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom1->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeometrySymDifference (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* builds a new geometry representing the "spatial symmetric difference" of GEOM-1 and GEOM-2 */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSGeometry *g3;
    if (!geom1 || !geom2)
	return NULL;
    if (gaiaIsToxic (geom1) || gaiaIsToxic (geom2))
	return NULL;
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    g3 = GEOSSymDifference (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (!g3)
	return NULL;
    if (geom1->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g3);
    else if (geom1->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g3);
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g3);
    else
	geo = gaiaFromGeos_XY (g3);
    GEOSGeom_destroy (g3);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom1->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaBoundary (gaiaGeomCollPtr geom)
{
/* builds a new geometry representing the combinatorial boundary of GEOM */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSBoundary (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

GAIAGEO_DECLARE int
gaiaGeomCollCentroid (gaiaGeomCollPtr geom, double *x, double *y)
{
/* returns a Point representing the centroid for this Geometry */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return 0;
    if (gaiaIsToxic (geom))
      {
	  return 0;
      }
    g1 = gaiaToGeos (geom);
    g2 = GEOSGetCentroid (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return 0;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return 0;
    if (geo->FirstPoint)
      {
	  *x = geo->FirstPoint->X;
	  *y = geo->FirstPoint->Y;
	  gaiaFreeGeomColl (geo);
	  return 1;
      }
    gaiaFreeGeomColl (geo);
    return 0;
}

GAIAGEO_DECLARE int
gaiaGetPointOnSurface (gaiaGeomCollPtr geom, double *x, double *y)
{
/* returns a Point guaranteed to lie on the Surface */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return 0;
    if (gaiaIsToxic (geom))
      {
	  return 0;
      }
    g1 = gaiaToGeos (geom);
    g2 = GEOSPointOnSurface (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return 0;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return 0;
    if (geo->FirstPoint)
      {
	  *x = geo->FirstPoint->X;
	  *y = geo->FirstPoint->Y;
	  gaiaFreeGeomColl (geo);
	  return 1;
      }
    gaiaFreeGeomColl (geo);
    return 0;
}

GAIAGEO_DECLARE int
gaiaIsSimple (gaiaGeomCollPtr geom)
{
/* checks if this GEOMETRYCOLLECTION is a simple one */
    int ret;
    GEOSGeometry *g;
    if (!geom)
	return -1;
    if (gaiaIsToxic (geom))
	return 0;
    g = gaiaToGeos (geom);
    ret = GEOSisSimple (g);
    GEOSGeom_destroy (g);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaIsRing (gaiaLinestringPtr line)
{
/* checks if this LINESTRING can be a valid RING */
    gaiaGeomCollPtr geo;
    gaiaLinestringPtr line2;
    int ret;
    int iv;
    double x;
    double y;
    double z;
    double m;
    GEOSGeometry *g;
    if (!line)
	return -1;
    if (line->DimensionModel == GAIA_XY_Z)
	geo = gaiaAllocGeomCollXYZ ();
    else if (line->DimensionModel == GAIA_XY_M)
	geo = gaiaAllocGeomCollXYM ();
    else if (line->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaAllocGeomCollXYZM ();
    else
	geo = gaiaAllocGeomColl ();
    line2 = gaiaAddLinestringToGeomColl (geo, line->Points);
    for (iv = 0; iv < line2->Points; iv++)
      {
	  z = 0.0;
	  m = 0.0;
	  if (line->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (line->Coords, iv, &x, &y, &z);
	    }
	  else if (line->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (line->Coords, iv, &x, &y, &m);
	    }
	  else if (line->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (line->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (line->Coords, iv, &x, &y);
	    }
	  if (line2->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (line2->Coords, iv, x, y, z);
	    }
	  else if (line2->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (line2->Coords, iv, x, y, m);
	    }
	  else if (line2->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (line2->Coords, iv, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (line2->Coords, iv, x, y);
	    }
      }
    if (gaiaIsToxic (geo))
      {
	  gaiaFreeGeomColl (geo);
	  return -1;
      }
    g = gaiaToGeos (geo);
    gaiaFreeGeomColl (geo);
    ret = GEOSisRing (g);
    GEOSGeom_destroy (g);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaIsValid (gaiaGeomCollPtr geom)
{
/* checks if this GEOMETRYCOLLECTION is a valid one */
    int ret;
    GEOSGeometry *g;
    gaiaResetGeosMsg ();
    if (!geom)
	return -1;
    if (gaiaIsToxic (geom))
	return 0;
    if (gaiaIsNotClosedGeomColl (geom))
	return 0;
    g = gaiaToGeos (geom);
    ret = GEOSisValid (g);
    GEOSGeom_destroy (g);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaIsClosedGeom (gaiaGeomCollPtr geom)
{
/* checks if this geometry is a closed linestring (or multilinestring) */
    int ret = 0;
    gaiaLinestringPtr ln;
    if (!geom)
	return -1;
    if (gaiaIsToxic (geom))
	return 0;
    ln = geom->FirstLinestring;
    while (ln)
      {
	  /* unhappily GEOS v3.2.2 [system package on Debian Lenny and Ubuntu 12.04]
	   * doesn't exposes the GEOSisClosed() API at all !!!!
	   *
	   GEOSGeometry *g;
	   gaiaGeomCollPtr geoColl = gaiaAllocGeomColl();
	   gaiaInsertLinestringInGeomColl(geoColl, gaiaCloneLinestring(ln));
	   g = gaiaToGeos (geoColl);
	   ret = GEOSisClosed (g);
	   GEOSGeom_destroy (g);
	   gaiaFreeGeomColl(geoColl);
	   */

	  /* so we'll use this internal default in order to circumvent the above issue */
	  double x1;
	  double y1;
	  double z1;
	  double m1;
	  double x2;
	  double y2;
	  double z2;
	  double m2;
	  gaiaLineGetPoint (ln, 0, &x1, &y1, &z1, &m1);
	  gaiaLineGetPoint (ln, ln->Points - 1, &x2, &y2, &z2, &m2);
	  if (x1 == x2 && y1 == y2 && z1 == z2)
	      ret = 1;
	  else
	      ret = 0;
	  if (ret == 0)
	    {
		/* this line isn't closed, so we don't need to continue */
		break;
	    }
	  ln = ln->Next;
      }
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeomCollSimplify (gaiaGeomCollPtr geom, double tolerance)
{
/* builds a simplified geometry using the Douglas-Peuker algorihtm */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSSimplify (g1, tolerance);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeomCollSimplifyPreserveTopology (gaiaGeomCollPtr geom, double tolerance)
{
/* builds a simplified geometry using the Douglas-Peuker algorihtm [preserving topology] */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSTopologyPreserveSimplify (g1, tolerance);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaConvexHull (gaiaGeomCollPtr geom)
{
/* builds a geometry that is the convex hull of GEOM */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSConvexHull (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaGeomCollBuffer (gaiaGeomCollPtr geom, double radius, int points)
{
/* builds a geometry that is the GIS buffer of GEOM */
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSBuffer (g1, radius, points);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

static void
auxFromGeosPolygon (const GEOSGeometry * geos, gaiaGeomCollPtr result)
{
/* converting a Polygon from GEOS to SpatiaLite */
    const GEOSGeometry *geos_ring;
    const GEOSCoordSequence *coords;
    unsigned int pts;
    unsigned int geos_dims;
    int interiors;
    int iv;
    int ib;
    double x;
    double y;
    double z;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;

    geos_ring = GEOSGetExteriorRing (geos);
    interiors = GEOSGetNumInteriorRings (geos);
    coords = GEOSGeom_getCoordSeq (geos_ring);
    GEOSCoordSeq_getDimensions (coords, &geos_dims);
    GEOSCoordSeq_getSize (coords, &pts);

    pg = gaiaAddPolygonToGeomColl (result, pts, interiors);
/* setting up the Exterior ring */
    rng = pg->Exterior;
    for (iv = 0; iv < (int) pts; iv++)
      {
	  if (geos_dims == 3)
	    {
		GEOSCoordSeq_getX (coords, iv, &x);
		GEOSCoordSeq_getY (coords, iv, &y);
		GEOSCoordSeq_getZ (coords, iv, &z);
	    }
	  else
	    {
		GEOSCoordSeq_getX (coords, iv, &x);
		GEOSCoordSeq_getY (coords, iv, &y);
		z = 0.0;
	    }
	  if (rng->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (rng->Coords, iv, x, y, z);
	    }
	  else if (rng->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (rng->Coords, iv, x, y, 0.0);
	    }
	  else if (rng->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (rng->Coords, iv, x, y, z, 0.0);
	    }
	  else
	    {
		gaiaSetPoint (rng->Coords, iv, x, y);
	    }
      }

    for (ib = 0; ib < interiors; ib++)
      {
	  /* setting up any interior ring */
	  geos_ring = GEOSGetInteriorRingN (geos, ib);
	  coords = GEOSGeom_getCoordSeq (geos_ring);
	  GEOSCoordSeq_getDimensions (coords, &geos_dims);
	  GEOSCoordSeq_getSize (coords, &pts);
	  rng = gaiaAddInteriorRing (pg, ib, pts);
	  for (iv = 0; iv < (int) pts; iv++)
	    {
		if (geos_dims == 3)
		  {
		      GEOSCoordSeq_getX (coords, iv, &x);
		      GEOSCoordSeq_getY (coords, iv, &y);
		      GEOSCoordSeq_getZ (coords, iv, &z);
		  }
		else
		  {
		      GEOSCoordSeq_getX (coords, iv, &x);
		      GEOSCoordSeq_getY (coords, iv, &y);
		      z = 0.0;
		  }
		if (rng->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaSetPointXYZ (rng->Coords, iv, x, y, z);
		  }
		else if (rng->DimensionModel == GAIA_XY_M)
		  {
		      gaiaSetPointXYM (rng->Coords, iv, x, y, 0.0);
		  }
		else if (rng->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaSetPointXYZM (rng->Coords, iv, x, y, z, 0.0);
		  }
		else
		  {
		      gaiaSetPoint (rng->Coords, iv, x, y);
		  }
	    }
      }
}

static void
auxGeosMbr (const GEOSCoordSequence * cs, unsigned int pts, double *min_x,
	    double *min_y, double *max_x, double *max_y)
{
/* computing the MBR */
    int iv;
    double x;
    double y;
    *min_x = DBL_MAX;
    *min_y = DBL_MAX;
    *max_x = 0 - DBL_MAX;
    *max_y = 0 - DBL_MAX;
    for (iv = 0; iv < (int) pts; iv++)
      {
	  GEOSCoordSeq_getX (cs, iv, &x);
	  GEOSCoordSeq_getY (cs, iv, &y);
	  if (x < *min_x)
	      *min_x = x;
	  if (x > *max_x)
	      *max_x = x;
	  if (y < *min_y)
	      *min_y = y;
	  if (y > *max_y)
	      *max_y = y;
      }
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaPolygonize (gaiaGeomCollPtr geom, int force_multi)
{
/* attempts to rearrange a generic Geometry into a (multi)polygon */
    int ig;
    int ib;
    int iv;
    int interiors;
    int geos_dims = 2;
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    int items;
    int error;
    double x;
    double y;
    double z;
    double m;
    gaiaGeomCollPtr result = NULL;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    GEOSCoordSequence *cs;
    const GEOSGeometry *const *geos_list = NULL;
    GEOSGeometry **p_item;
    GEOSGeometry *geos;
    const GEOSGeometry *geos_item;
    const GEOSGeometry *geos_item2;
    const GEOSGeometry *geos_ring;
    char *valid_polygons = NULL;
    const GEOSCoordSequence *coords;
    unsigned int pts1;
    unsigned int pts2;
    double min_x1;
    double max_x1;
    double min_y1;
    double max_y1;
    double min_x2;
    double max_x2;
    double min_y2;
    double max_y2;

    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    pt = geom->FirstPoint;
    while (pt)
      {
	  pts++;
	  pt = pt->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  pgs++;
	  pg = pg->Next;
      }
    if (pts || pgs)
	return NULL;
    ln = geom->FirstLinestring;
    while (ln)
      {
	  lns++;
	  ln = ln->Next;
      }
    if (!lns)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z
	|| geom->DimensionModel == GAIA_XY_Z_M)
	geos_dims = 3;

/* allocating GEOS linestrings */
    geos_list = malloc (sizeof (const GEOSGeometry * const *) * lns);
    p_item = (GEOSGeometry **) geos_list;
    for (iv = 0; iv < lns; iv++)
      {
	  /* initializing to NULL */
	  *p_item++ = NULL;
      }
    p_item = (GEOSGeometry **) geos_list;

/* initializing GEOS linestrings */
    ln = geom->FirstLinestring;
    while (ln)
      {
	  cs = GEOSCoordSeq_create (ln->Points, geos_dims);
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		/* exterior ring segments */
		z = 0.0;
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		  }
		if (geos_dims == 3)
		  {
		      GEOSCoordSeq_setX (cs, iv, x);
		      GEOSCoordSeq_setY (cs, iv, y);
		      GEOSCoordSeq_setZ (cs, iv, z);
		  }
		else
		  {
		      GEOSCoordSeq_setX (cs, iv, x);
		      GEOSCoordSeq_setY (cs, iv, y);
		  }
	    }
	  *p_item++ = GEOSGeom_createLineString (cs);
	  ln = ln->Next;
      }

/* calling GEOSPolygonize */
    geos = GEOSPolygonize (geos_list, lns);
    if (geos == NULL)
	goto cleanup;

/*
/ 
/ GEOSPolygonize is expected to return a collection of Polygons
/
/ CAVEAT: internal holes are returned as such (interior rings in
/         some Polygon), but are returned as distinct Polygons too
/
/ we must check this, so to *not* return Polygons representing holes
/
*/
    error = 0;
    items = GEOSGetNumGeometries (geos);
    for (ig = 0; ig < items; ig++)
      {
	  /* looping on elementaty GEOS geometries */
	  geos_item = GEOSGetGeometryN (geos, ig);
	  if (GEOSGeomTypeId (geos_item) != GEOS_POLYGON)
	    {
		/* not a Polygon ... ouch ... */
		error = 1;
		goto cleanup;
	    }
      }

/* identifying valid Polygons [excluding holes] */
    valid_polygons = malloc (items);
    for (ig = 0; ig < items; ig++)
	valid_polygons[ig] = 'Y';
    for (ig = 0; ig < items; ig++)
      {
	  /* looping on elementaty GEOS Polygons */
	  geos_item = GEOSGetGeometryN (geos, ig);
	  interiors = GEOSGetNumInteriorRings (geos_item);
	  for (ib = 0; ib < interiors; ib++)
	    {
		/* looping on any interior ring */
		geos_ring = GEOSGetInteriorRingN (geos_item, ib);
		coords = GEOSGeom_getCoordSeq (geos_ring);
		GEOSCoordSeq_getSize (coords, &pts1);
		auxGeosMbr (coords, pts1, &min_x1, &min_y1, &max_x1, &max_y1);
		for (iv = 0; iv < items; iv++)
		  {
		      if (iv == ig)
			{
			    /* skipping the Polygon itself */
			    continue;
			}
		      if (valid_polygons[iv] == 'N')
			{
			    /* skipping any already invalid Polygon */
			    continue;
			}
		      geos_item2 = GEOSGetGeometryN (geos, iv);
		      if (GEOSGetNumInteriorRings (geos_item2) > 0)
			{
			    /* this Polygon contains holes [surely valid] */
			    continue;
			}
		      geos_ring = GEOSGetExteriorRing (geos_item2);
		      coords = GEOSGeom_getCoordSeq (geos_ring);
		      GEOSCoordSeq_getSize (coords, &pts2);
		      if (pts1 == pts2)
			{
			    auxGeosMbr (coords, pts2, &min_x2, &min_y2, &max_x2,
					&max_y2);
			    if (min_x1 == min_x2 && min_y1 == min_y2
				&& max_x1 == max_x2 && max_y1 == max_y2)
			      {
				  /* same #points, same MBRs: invalidating */
				  valid_polygons[iv] = 'N';
			      }
			}
		  }
	    }
      }

/* creating the Geometry to be returned */
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaAllocGeomCollXYZ ();
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaAllocGeomCollXYM ();
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaAllocGeomCollXYZM ();
    else
	result = gaiaAllocGeomColl ();
    if (result == NULL)
	return NULL;
    result->Srid = geom->Srid;
    if (force_multi)
	result->DeclaredType = GAIA_MULTIPOLYGON;

    for (ig = 0; ig < items; ig++)
      {
	  /* looping on GEOS Polygons */
	  geos_item = GEOSGetGeometryN (geos, ig);
	  if (valid_polygons[ig] == 'Y')
	      auxFromGeosPolygon (geos_item, result);
      }

  cleanup:
    if (valid_polygons != NULL)
	free (valid_polygons);
    if (geos_list != NULL)
      {
	  /* memory cleanup */
	  p_item = (GEOSGeometry **) geos_list;
	  for (iv = 0; iv < lns; iv++)
	    {
		if (*p_item != NULL)
		    GEOSGeom_destroy (*p_item);
		p_item++;
	    }
	  p_item = (GEOSGeometry **) geos_list;
	  free (p_item);
      }
    if (geos != NULL)
	GEOSGeom_destroy (geos);
    if (error || result->FirstPolygon == NULL)
      {
	  gaiaFreeGeomColl (result);
	  return NULL;
      }
    return result;
}

#ifdef GEOS_ADVANCED		/* GEOS advanced features */

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaOffsetCurve (gaiaGeomCollPtr geom, double radius, int points,
		 int left_right)
{
/*
// builds a geometry that is the OffsetCurve of GEOM 
// (which is expected to be of the LINESTRING type)
//
*/
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    int closed = 0;
    if (!geom)
	return NULL;

/* checking the input geometry for validity */
    pt = geom->FirstPoint;
    while (pt)
      {
	  /* counting how many POINTs are there */
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  /* counting how many LINESTRINGs are there */
	  lns++;
	  if (gaiaIsClosed (ln))
	      closed++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  /* counting how many POLYGON are there */
	  pgs++;
	  pg = pg->Next;
      }
    if (pts > 0 || pgs > 0 || lns > 1 || closed > 0)
	return NULL;

/* all right: this one simply is a LINESTRING */
    geom->DeclaredType = GAIA_LINESTRING;

    g1 = gaiaToGeos (geom);
    g2 = GEOSSingleSidedBuffer (g1, radius, points, GEOSBUF_JOIN_ROUND, 5.0,
				left_right);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaSingleSidedBuffer (gaiaGeomCollPtr geom, double radius, int points,
		       int left_right)
{
/*
// builds a geometry that is the SingleSided BUFFER of GEOM 
// (which is expected to be of the LINESTRING type)
//
*/
    gaiaGeomCollPtr geo;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSBufferParams *params = NULL;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    int closed = 0;
    if (!geom)
	return NULL;

/* checking the input geometry for validity */
    pt = geom->FirstPoint;
    while (pt)
      {
	  /* counting how many POINTs are there */
	  pts++;
	  pt = pt->Next;
      }
    ln = geom->FirstLinestring;
    while (ln)
      {
	  /* counting how many LINESTRINGs are there */
	  lns++;
	  if (gaiaIsClosed (ln))
	      closed++;
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  /* counting how many POLYGON are there */
	  pgs++;
	  pg = pg->Next;
      }
    if (pts > 0 || pgs > 0 || lns > 1 || closed > 0)
	return NULL;

/* all right: this one simply is a LINESTRING */
    geom->DeclaredType = GAIA_LINESTRING;

    g1 = gaiaToGeos (geom);
/* setting up Buffer params */
    params = GEOSBufferParams_create ();
    GEOSBufferParams_setJoinStyle (params, GEOSBUF_JOIN_ROUND);
    GEOSBufferParams_setMitreLimit (params, 5.0);
    GEOSBufferParams_setQuadrantSegments (params, points);
    GEOSBufferParams_setSingleSided (params, 1);

/* creating the SingleSided Buffer */
    if (left_right == 0)
      {
	  /* right-sided requires NEGATIVE radius */
	  radius *= -1.0;
      }
    g2 = GEOSBufferWithParams (g1, params, radius);
    GEOSGeom_destroy (g1);
    GEOSBufferParams_destroy (params);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g2);
    else
	geo = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom->Srid;
    return geo;
}

GAIAGEO_DECLARE int
gaiaHausdorffDistance (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2,
		       double *xdist)
{
/* 
/ computes the (discrete) Hausdorff distance intercurring 
/ between GEOM-1 and GEOM-2 
*/
    double dist;
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return 0;
    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSHausdorffDistance (g1, g2, &dist);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret)
	*xdist = dist;
    return ret;
}

static gaiaGeomCollPtr
geom_as_lines (gaiaGeomCollPtr geom)
{
/* transforms a Geometry into a LINESTRING/MULTILINESTRING (if possible) */
    gaiaGeomCollPtr result;
    gaiaLinestringPtr ln;
    gaiaLinestringPtr new_ln;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    int iv;
    int ib;
    double x;
    double y;
    double z;
    double m;

    if (!geom)
	return NULL;
    if (geom->FirstPoint != NULL)
      {
	  /* invalid: GEOM contains at least one POINT */
	  return NULL;
      }

    switch (geom->DimensionModel)
      {
      case GAIA_XY_Z_M:
	  result = gaiaAllocGeomCollXYZM ();
	  break;
      case GAIA_XY_Z:
	  result = gaiaAllocGeomCollXYZ ();
	  break;
      case GAIA_XY_M:
	  result = gaiaAllocGeomCollXYM ();
	  break;
      default:
	  result = gaiaAllocGeomColl ();
	  break;
      };
    result->Srid = geom->Srid;
    ln = geom->FirstLinestring;
    while (ln)
      {
	  /* copying any Linestring */
	  new_ln = gaiaAddLinestringToGeomColl (result, ln->Points);
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      gaiaSetPointXYZ (new_ln->Coords, iv, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      gaiaSetPointXYM (new_ln->Coords, iv, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      gaiaSetPointXYZM (new_ln->Coords, iv, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      gaiaSetPoint (new_ln->Coords, iv, x, y);
		  }
	    }
	  ln = ln->Next;
      }
    pg = geom->FirstPolygon;
    while (pg)
      {
	  /* copying any Polygon Ring (as Linestring) */
	  rng = pg->Exterior;
	  new_ln = gaiaAddLinestringToGeomColl (result, rng->Points);
	  for (iv = 0; iv < rng->Points; iv++)
	    {
		/* exterior Ring */
		if (rng->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
		      gaiaSetPointXYZ (new_ln->Coords, iv, x, y, z);
		  }
		else if (rng->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
		      gaiaSetPointXYM (new_ln->Coords, iv, x, y, m);
		  }
		else if (rng->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
		      gaiaSetPointXYZM (new_ln->Coords, iv, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (rng->Coords, iv, &x, &y);
		      gaiaSetPoint (new_ln->Coords, iv, x, y);
		  }
	    }
	  for (ib = 0; ib < pg->NumInteriors; ib++)
	    {
		rng = pg->Interiors + ib;
		new_ln = gaiaAddLinestringToGeomColl (result, rng->Points);
		for (iv = 0; iv < rng->Points; iv++)
		  {
		      /* any interior Ring */
		      if (rng->DimensionModel == GAIA_XY_Z)
			{
			    gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
			    gaiaSetPointXYZ (new_ln->Coords, iv, x, y, z);
			}
		      else if (rng->DimensionModel == GAIA_XY_M)
			{
			    gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
			    gaiaSetPointXYM (new_ln->Coords, iv, x, y, m);
			}
		      else if (rng->DimensionModel == GAIA_XY_Z_M)
			{
			    gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
			    gaiaSetPointXYZM (new_ln->Coords, iv, x, y, z, m);
			}
		      else
			{
			    gaiaGetPoint (rng->Coords, iv, &x, &y);
			    gaiaSetPoint (new_ln->Coords, iv, x, y);
			}
		  }
	    }
	  pg = pg->Next;
      }
    return result;
}

static void
add_shared_linestring (gaiaGeomCollPtr geom, gaiaDynamicLinePtr dyn)
{
/* adding a LINESTRING from Dynamic Line */
    int count = 0;
    gaiaLinestringPtr ln;
    gaiaPointPtr pt;
    int iv;

    if (!geom)
	return;
    if (!dyn)
	return;
    pt = dyn->First;
    while (pt)
      {
	  /* counting how many Points are there */
	  count++;
	  pt = pt->Next;
      }
    if (count == 0)
	return;
    ln = gaiaAddLinestringToGeomColl (geom, count);
    iv = 0;
    pt = dyn->First;
    while (pt)
      {
	  /* copying points into the LINESTRING */
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (ln->Coords, iv, pt->X, pt->Y, pt->Z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (ln->Coords, iv, pt->X, pt->Y, pt->M);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (ln->Coords, iv, pt->X, pt->Y, pt->Z, pt->M);
	    }
	  else
	    {
		gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
	    }
	  iv++;
	  pt = pt->Next;
      }
}

static void
append_shared_path (gaiaDynamicLinePtr dyn, gaiaLinestringPtr ln, int order)
{
/* appends a Shared Path item to Dynamic Line */
    int iv;
    double x;
    double y;
    double z;
    double m;
    if (order)
      {
	  /* reversed order */
	  for (iv = ln->Points - 1; iv >= 0; iv--)
	    {
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      if (x == dyn->Last->X && y == dyn->Last->Y
			  && z == dyn->Last->Z)
			  ;
		      else
			  gaiaAppendPointZToDynamicLine (dyn, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      if (x == dyn->Last->X && y == dyn->Last->Y
			  && m == dyn->Last->M)
			  ;
		      else
			  gaiaAppendPointMToDynamicLine (dyn, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      if (x == dyn->Last->X && y == dyn->Last->Y
			  && z == dyn->Last->Z && m == dyn->Last->M)
			  ;
		      else
			  gaiaAppendPointZMToDynamicLine (dyn, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      if (x == dyn->Last->X && y == dyn->Last->Y)
			  ;
		      else
			  gaiaAppendPointToDynamicLine (dyn, x, y);
		  }
	    }
      }
    else
      {
	  /* conformant order */
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      if (x == dyn->Last->X && y == dyn->Last->Y
			  && z == dyn->Last->Z)
			  ;
		      else
			  gaiaAppendPointZToDynamicLine (dyn, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      if (x == dyn->Last->X && y == dyn->Last->Y
			  && m == dyn->Last->M)
			  ;
		      else
			  gaiaAppendPointMToDynamicLine (dyn, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      if (x == dyn->Last->X && y == dyn->Last->Y
			  && z == dyn->Last->Z && m == dyn->Last->M)
			  ;
		      else
			  gaiaAppendPointZMToDynamicLine (dyn, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      if (x == dyn->Last->X && y == dyn->Last->Y)
			  ;
		      else
			  gaiaAppendPointToDynamicLine (dyn, x, y);
		  }
	    }
      }
}

static void
prepend_shared_path (gaiaDynamicLinePtr dyn, gaiaLinestringPtr ln, int order)
{
/* prepends a Shared Path item to Dynamic Line */
    int iv;
    double x;
    double y;
    double z;
    double m;
    if (order)
      {
	  /* reversed order */
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      if (x == dyn->First->X && y == dyn->First->Y
			  && z == dyn->First->Z)
			  ;
		      else
			  gaiaPrependPointZToDynamicLine (dyn, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      if (x == dyn->First->X && y == dyn->First->Y
			  && m == dyn->First->M)
			  ;
		      else
			  gaiaPrependPointMToDynamicLine (dyn, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      if (x == dyn->First->X && y == dyn->First->Y
			  && z == dyn->First->Z && m == dyn->First->M)
			  ;
		      else
			  gaiaPrependPointZMToDynamicLine (dyn, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      if (x == dyn->First->X && y == dyn->First->Y)
			  ;
		      else
			  gaiaPrependPointToDynamicLine (dyn, x, y);
		  }
	    }
      }
    else
      {
	  /* conformant order */
	  for (iv = ln->Points - 1; iv >= 0; iv--)
	    {
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      if (x == dyn->First->X && y == dyn->First->Y
			  && z == dyn->First->Z)
			  ;
		      else
			  gaiaPrependPointZToDynamicLine (dyn, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      if (x == dyn->First->X && y == dyn->First->Y
			  && m == dyn->First->M)
			  ;
		      else
			  gaiaPrependPointMToDynamicLine (dyn, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      if (x == dyn->First->X && y == dyn->First->Y
			  && z == dyn->First->Z && m == dyn->First->M)
			  ;
		      else
			  gaiaPrependPointZMToDynamicLine (dyn, x, y, z, m);
		  }
		else
		  {
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      if (x == dyn->First->X && y == dyn->First->Y)
			  ;
		      else
			  gaiaPrependPointToDynamicLine (dyn, x, y);
		  }
	    }
      }
}

static gaiaGeomCollPtr
arrange_shared_paths (gaiaGeomCollPtr geom)
{
/* final aggregation step for shared paths */
    gaiaLinestringPtr ln;
    gaiaLinestringPtr *ln_array;
    gaiaGeomCollPtr result;
    gaiaDynamicLinePtr dyn;
    int count;
    int i;
    int i2;
    int iv;
    double x;
    double y;
    double z;
    double m;
    int ok;
    int ok2;

    if (!geom)
	return NULL;
    count = 0;
    ln = geom->FirstLinestring;
    while (ln)
      {
	  /* counting how many Linestrings are there */
	  count++;
	  ln = ln->Next;
      }
    if (count == 0)
	return NULL;

    ln_array = malloc (sizeof (gaiaLinestringPtr) * count);
    i = 0;
    ln = geom->FirstLinestring;
    while (ln)
      {
	  /* populating the Linestring references array */
	  ln_array[i++] = ln;
	  ln = ln->Next;
      }

/* allocating a new Geometry [MULTILINESTRING] */
    switch (geom->DimensionModel)
      {
      case GAIA_XY_Z_M:
	  result = gaiaAllocGeomCollXYZM ();
	  break;
      case GAIA_XY_Z:
	  result = gaiaAllocGeomCollXYZ ();
	  break;
      case GAIA_XY_M:
	  result = gaiaAllocGeomCollXYM ();
	  break;
      default:
	  result = gaiaAllocGeomColl ();
	  break;
      };
    result->Srid = geom->Srid;
    result->DeclaredType = GAIA_MULTILINESTRING;

    ok = 1;
    while (ok)
      {
	  /* looping until we have processed any input item */
	  ok = 0;
	  for (i = 0; i < count; i++)
	    {
		if (ln_array[i] != NULL)
		  {
		      /* starting a new LINESTRING */
		      dyn = gaiaAllocDynamicLine ();
		      ln = ln_array[i];
		      ln_array[i] = NULL;
		      for (iv = 0; iv < ln->Points; iv++)
			{
			    /* inserting the 'seed' path */
			    if (ln->DimensionModel == GAIA_XY_Z)
			      {
				  gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
				  gaiaAppendPointZToDynamicLine (dyn, x, y, z);
			      }
			    else if (ln->DimensionModel == GAIA_XY_M)
			      {
				  gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
				  gaiaAppendPointMToDynamicLine (dyn, x, y, m);
			      }
			    else if (ln->DimensionModel == GAIA_XY_Z_M)
			      {
				  gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z,
						    &m);
				  gaiaAppendPointZMToDynamicLine (dyn, x, y, z,
								  m);
			      }
			    else
			      {
				  gaiaGetPoint (ln->Coords, iv, &x, &y);
				  gaiaAppendPointToDynamicLine (dyn, x, y);
			      }
			}
		      ok2 = 1;
		      while (ok2)
			{
			    /* looping until we have checked any other item */
			    ok2 = 0;
			    for (i2 = 0; i2 < count; i2++)
			      {
				  /* expanding the 'seed' path */
				  if (ln_array[i2] == NULL)
				      continue;
				  ln = ln_array[i2];
				  /* checking the first vertex */
				  iv = 0;
				  if (ln->DimensionModel == GAIA_XY_Z)
				    {
					gaiaGetPointXYZ (ln->Coords, iv, &x, &y,
							 &z);
				    }
				  else if (ln->DimensionModel == GAIA_XY_M)
				    {
					gaiaGetPointXYM (ln->Coords, iv, &x, &y,
							 &m);
				    }
				  else if (ln->DimensionModel == GAIA_XY_Z_M)
				    {
					gaiaGetPointXYZM (ln->Coords, iv, &x,
							  &y, &z, &m);
				    }
				  else
				    {
					gaiaGetPoint (ln->Coords, iv, &x, &y);
				    }
				  if (x == dyn->Last->X && y == dyn->Last->Y)
				    {
					/* appending this item to the 'seed' (conformant order) */
					append_shared_path (dyn, ln, 0);
					ln_array[i2] = NULL;
					ok2 = 1;
					continue;
				    }
				  if (x == dyn->First->X && y == dyn->First->Y)
				    {
					/* prepending this item to the 'seed' (reversed order) */
					prepend_shared_path (dyn, ln, 1);
					ln_array[i2] = NULL;
					ok2 = 1;
					continue;
				    }
				  /* checking the last vertex */
				  iv = ln->Points - 1;
				  if (ln->DimensionModel == GAIA_XY_Z)
				    {
					gaiaGetPointXYZ (ln->Coords, iv, &x, &y,
							 &z);
				    }
				  else if (ln->DimensionModel == GAIA_XY_M)
				    {
					gaiaGetPointXYM (ln->Coords, iv, &x, &y,
							 &m);
				    }
				  else if (ln->DimensionModel == GAIA_XY_Z_M)
				    {
					gaiaGetPointXYZM (ln->Coords, iv, &x,
							  &y, &z, &m);
				    }
				  else
				    {
					gaiaGetPoint (ln->Coords, iv, &x, &y);
				    }
				  if (x == dyn->Last->X && y == dyn->Last->Y)
				    {
					/* appending this item to the 'seed' (reversed order) */
					append_shared_path (dyn, ln, 1);
					ln_array[i2] = NULL;
					ok2 = 1;
					continue;
				    }
				  if (x == dyn->First->X && y == dyn->First->Y)
				    {
					/* prepending this item to the 'seed' (conformant order) */
					prepend_shared_path (dyn, ln, 0);
					ln_array[i2] = NULL;
					ok2 = 1;
					continue;
				    }
			      }
			}
		      add_shared_linestring (result, dyn);
		      gaiaFreeDynamicLine (dyn);
		      ok = 1;
		      break;
		  }
	    }
      }
    free (ln_array);
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaSharedPaths (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/*
// builds a geometry containing Shared Paths commons to GEOM1 & GEOM2 
// (which are expected to be of the LINESTRING/MULTILINESTRING type)
//
*/
    gaiaGeomCollPtr geo;
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr line1;
    gaiaGeomCollPtr line2;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSGeometry *g3;
    if (!geom1)
	return NULL;
    if (!geom2)
	return NULL;
/* transforming input geoms as Lines */
    line1 = geom_as_lines (geom1);
    line2 = geom_as_lines (geom2);
    if (line1 == NULL || line2 == NULL)
      {
	  if (line1)
	      gaiaFreeGeomColl (line1);
	  if (line2)
	      gaiaFreeGeomColl (line2);
	  return NULL;
      }

    g1 = gaiaToGeos (line1);
    g2 = gaiaToGeos (line2);
    gaiaFreeGeomColl (line1);
    gaiaFreeGeomColl (line2);
    g3 = GEOSSharedPaths (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (!g3)
	return NULL;
    if (geom1->DimensionModel == GAIA_XY_Z)
	geo = gaiaFromGeos_XYZ (g3);
    else if (geom1->DimensionModel == GAIA_XY_M)
	geo = gaiaFromGeos_XYM (g3);
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	geo = gaiaFromGeos_XYZM (g3);
    else
	geo = gaiaFromGeos_XY (g3);
    GEOSGeom_destroy (g3);
    if (geo == NULL)
	return NULL;
    geo->Srid = geom1->Srid;
    result = arrange_shared_paths (geo);
    gaiaFreeGeomColl (geo);
    return result;
}

GAIAGEO_DECLARE int
gaiaGeomCollCovers (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if geom1 "spatially covers" geom2 */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_contains (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSCovers (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollPreparedCovers (void *p_cache, gaiaGeomCollPtr geom1,
			    unsigned char *blob1, int size1,
			    gaiaGeomCollPtr geom2, unsigned char *blob2,
			    int size2)
{
/* checks if geom1 "spatially covers" geom2 */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_contains (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  if (geom == geom2)
	      ret = GEOSPreparedCovers (gPrep, g2);
	  else
	      ret = GEOSPreparedCoveredBy (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  if (ret == 2)
	      return -1;
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSCovers (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollCoveredBy (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* checks if geom1 is "spatially covered by" geom2 */
    int ret;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_within (geom1, geom2))
	return 0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSCoveredBy (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE int
gaiaGeomCollPreparedCoveredBy (void *p_cache, gaiaGeomCollPtr geom1,
			       unsigned char *blob1, int size1,
			       gaiaGeomCollPtr geom2, unsigned char *blob2,
			       int size2)
{
/* checks if geom1 is "spatially covered by" geom2 */
    int ret;
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    GEOSPreparedGeometry *gPrep;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr geom;
    if (!geom1 || !geom2)
	return -1;

/* quick check based on MBRs comparison */
    if (!splite_mbr_within (geom1, geom2))
	return 0;

/* handling the internal GEOS cache */
    if (evalGeosCache
	(cache, geom1, blob1, size1, geom2, blob2, size2, &gPrep, &geom))
      {
	  g2 = gaiaToGeos (geom);
	  if (geom == geom2)
	      ret = GEOSPreparedCoveredBy (gPrep, g2);
	  else
	      ret = GEOSPreparedCovers (gPrep, g2);
	  GEOSGeom_destroy (g2);
	  if (ret == 2)
	      return -1;
	  return ret;
      }

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    ret = GEOSCoveredBy (g1, g2);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (ret == 2)
	return -1;
    return ret;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaLineInterpolatePoint (gaiaGeomCollPtr geom, double fraction)
{
/*
 * attempts to intepolate a point on line at dist "fraction" 
 *
 * the fraction is expressed into the range from 0.0 to 1.0
 */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaGeomCollPtr result;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    GEOSGeometry *g;
    GEOSGeometry *g_pt;
    double length;
    double projection;
    if (!geom)
	return NULL;

/* checking if a single Linestring has been passed */
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
	;
    else
	return NULL;

    g = gaiaToGeos (geom);
    if (GEOSLength (g, &length))
      {
	  /* transforming fraction to length */
	  if (fraction < 0.0)
	      fraction = 0.0;
	  if (fraction > 1.0)
	      fraction = 1.0;
	  projection = length * fraction;
      }
    else
      {
	  GEOSGeom_destroy (g);
	  return NULL;
      }
    g_pt = GEOSInterpolate (g, projection);
    GEOSGeom_destroy (g);
    if (!g_pt)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g_pt);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g_pt);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g_pt);
    else
	result = gaiaFromGeos_XY (g_pt);
    GEOSGeom_destroy (g_pt);
    if (result == NULL)
	return NULL;
    result->Srid = geom->Srid;
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaLineInterpolateEquidistantPoints (gaiaGeomCollPtr geom, double distance)
{
/*
 * attempts to intepolate a set of points on line at regular distances 
 */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr xpt;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    GEOSGeometry *g;
    GEOSGeometry *g_pt;
    double length;
    double current_length = 0.0;
    if (!geom)
	return NULL;
    if (distance <= 0.0)
	return NULL;

/* checking if a single Linestring has been passed */
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
	;
    else
	return NULL;

    g = gaiaToGeos (geom);
    if (GEOSLength (g, &length))
      {
	  if (length <= distance)
	    {
		/* the line is too short to apply interpolation */
		GEOSGeom_destroy (g);
		return NULL;
	    }
      }
    else
      {
	  GEOSGeom_destroy (g);
	  return NULL;
      }

/* creating the MultiPoint [always supporting M] */
    if (geom->DimensionModel == GAIA_XY_Z
	|| geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaAllocGeomCollXYZM ();
    else
	result = gaiaAllocGeomCollXYM ();
    if (result == NULL)
      {
	  GEOSGeom_destroy (g);
	  return NULL;
      }

    while (1)
      {
	  /* increasing the current distance */
	  current_length += distance;
	  if (current_length >= length)
	      break;
	  /* interpolating a point */
	  g_pt = GEOSInterpolate (g, current_length);
	  if (!g_pt)
	      goto error;
	  if (geom->DimensionModel == GAIA_XY_Z)
	    {
		xpt = gaiaFromGeos_XYZ (g_pt);
		if (!xpt)
		    goto error;
		pt = xpt->FirstPoint;
		if (!pt)
		    goto error;
		gaiaAddPointToGeomCollXYZM (result, pt->X, pt->Y, pt->Z,
					    current_length);
	    }
	  else if (geom->DimensionModel == GAIA_XY_M)
	    {
		xpt = gaiaFromGeos_XYM (g_pt);
		if (!xpt)
		    goto error;
		pt = xpt->FirstPoint;
		if (!pt)
		    goto error;
		gaiaAddPointToGeomCollXYM (result, pt->X, pt->Y,
					   current_length);
	    }
	  else if (geom->DimensionModel == GAIA_XY_Z_M)
	    {
		xpt = gaiaFromGeos_XYZM (g_pt);
		if (!xpt)
		    goto error;
		pt = xpt->FirstPoint;
		if (!pt)
		    goto error;
		gaiaAddPointToGeomCollXYZM (result, pt->X, pt->Y, pt->Z,
					    current_length);
	    }
	  else
	    {
		xpt = gaiaFromGeos_XY (g_pt);
		if (!xpt)
		    goto error;
		pt = xpt->FirstPoint;
		if (!pt)
		    goto error;
		gaiaAddPointToGeomCollXYM (result, pt->X, pt->Y,
					   current_length);
	    }
	  GEOSGeom_destroy (g_pt);
	  gaiaFreeGeomColl (xpt);
      }
    GEOSGeom_destroy (g);
    result->Srid = geom->Srid;
    result->DeclaredType = GAIA_MULTIPOINT;
    return result;

  error:
    if (g_pt)
	GEOSGeom_destroy (g_pt);
    GEOSGeom_destroy (g);
    gaiaFreeGeomColl (result);
    return NULL;
}

GAIAGEO_DECLARE double
gaiaLineLocatePoint (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* 
 * attempts to compute the location of the closest point on LineString 
 * to the given Point, as a fraction of total 2d line length 
 *
 * the fraction is expressed into the range from 0.0 to 1.0
 */
    int pts1 = 0;
    int lns1 = 0;
    int pgs1 = 0;
    int pts2 = 0;
    int lns2 = 0;
    int pgs2 = 0;
    double length;
    double projection;
    double result;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    if (!geom1 || !geom2)
	return -1.0;

/* checking if a single Linestring has been passed */
    pt = geom1->FirstPoint;
    while (pt)
      {
	  pts1++;
	  pt = pt->Next;
      }
    ln = geom1->FirstLinestring;
    while (ln)
      {
	  lns1++;
	  ln = ln->Next;
      }
    pg = geom1->FirstPolygon;
    while (pg)
      {
	  pgs1++;
	  pg = pg->Next;
      }
    if (pts1 == 0 && lns1 >= 1 && pgs1 == 0)
	;
    else
	return -1.0;

/* checking if a single Point has been passed */
    pt = geom2->FirstPoint;
    while (pt)
      {
	  pts2++;
	  pt = pt->Next;
      }
    ln = geom2->FirstLinestring;
    while (ln)
      {
	  lns2++;
	  ln = ln->Next;
      }
    pg = geom2->FirstPolygon;
    while (pg)
      {
	  pgs2++;
	  pg = pg->Next;
      }
    if (pts2 == 1 && lns2 == 0 && pgs2 == 0)
	;
    else
	return -1.0;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    projection = GEOSProject (g1, g2);
    if (GEOSLength (g1, &length))
      {
	  /* normalizing as a fraction between 0.0 and 1.0 */
	  result = projection / length;
      }
    else
	result = -1.0;
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaLineSubstring (gaiaGeomCollPtr geom, double start_fraction,
		   double end_fraction)
{
/* 
 * attempts to build a new Linestring being a substring of the input one starting 
 * and ending at the given fractions of total 2d length 
 */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    gaiaGeomCollPtr result;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaLinestringPtr out;
    gaiaPolygonPtr pg;
    GEOSGeometry *g;
    GEOSGeometry *g_start;
    GEOSGeometry *g_end;
    GEOSCoordSequence *cs;
    const GEOSCoordSequence *in_cs;
    GEOSGeometry *segm;
    double length;
    double total = 0.0;
    double start;
    double end;
    int iv;
    int i_start = -1;
    int i_end = -1;
    int points;
    double x;
    double y;
    double z;
    double m;
    unsigned int dims;
    if (!geom)
	return NULL;

/* checking if a single Linestring has been passed */
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
	;
    else
	return NULL;

    if (start_fraction < 0.0)
	start_fraction = 0.0;
    if (start_fraction > 1.0)
	start_fraction = 1.0;
    if (end_fraction < 0.0)
	end_fraction = 0.0;
    if (end_fraction > 1.0)
	end_fraction = 1.0;
    if (start_fraction >= end_fraction)
	return NULL;
    g = gaiaToGeos (geom);
    if (GEOSLength (g, &length))
      {
	  start = length * start_fraction;
	  end = length * end_fraction;
      }
    else
      {
	  GEOSGeom_destroy (g);
	  return NULL;
      }
    g_start = GEOSInterpolate (g, start);
    g_end = GEOSInterpolate (g, end);
    GEOSGeom_destroy (g);
    if (!g_start || !g_end)
	return NULL;

/* identifying first and last valid vertex */
    ln = geom->FirstLinestring;
    for (iv = 0; iv < ln->Points; iv++)
      {

	  double x0;
	  double y0;
	  switch (ln->DimensionModel)
	    {
	    case GAIA_XY_Z:
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		break;
	    case GAIA_XY_M:
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		break;
	    case GAIA_XY_Z_M:
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		break;
	    default:
		gaiaGetPoint (ln->Coords, iv, &x, &y);
		break;
	    };

	  if (iv > 0)
	    {
		cs = GEOSCoordSeq_create (2, 2);
		GEOSCoordSeq_setX (cs, 0, x0);
		GEOSCoordSeq_setY (cs, 0, y0);
		GEOSCoordSeq_setX (cs, 1, x);
		GEOSCoordSeq_setY (cs, 1, y);
		segm = GEOSGeom_createLineString (cs);
		GEOSLength (segm, &length);
		total += length;
		GEOSGeom_destroy (segm);
		if (total > start && i_start < 0)
		    i_start = iv;
		if (total < end)
		    i_end = iv;
	    }
	  x0 = x;
	  y0 = y;
      }
    if (i_start < 0 || i_end < 0)
      {
	  i_start = -1;
	  i_end = -1;
	  points = 2;
      }
    else
	points = i_end - i_start + 3;

/* creating the output geometry */
    switch (ln->DimensionModel)
      {
      case GAIA_XY_Z:
	  result = gaiaAllocGeomCollXYZ ();
	  break;
      case GAIA_XY_M:
	  result = gaiaAllocGeomCollXYM ();
	  break;
      case GAIA_XY_Z_M:
	  result = gaiaAllocGeomCollXYZM ();
	  break;
      default:
	  result = gaiaAllocGeomColl ();
	  break;
      };
    result->Srid = geom->Srid;
    out = gaiaAddLinestringToGeomColl (result, points);

/* start vertex */
    points = 0;
    in_cs = GEOSGeom_getCoordSeq (g_start);
    GEOSCoordSeq_getDimensions (in_cs, &dims);
    if (dims == 3)
      {
	  GEOSCoordSeq_getX (in_cs, 0, &x);
	  GEOSCoordSeq_getY (in_cs, 0, &y);
	  GEOSCoordSeq_getZ (in_cs, 0, &z);
	  m = 0.0;
      }
    else
      {
	  GEOSCoordSeq_getX (in_cs, 0, &x);
	  GEOSCoordSeq_getY (in_cs, 0, &y);
	  z = 0.0;
	  m = 0.0;
      }
    GEOSGeom_destroy (g_start);
    switch (out->DimensionModel)
      {
      case GAIA_XY_Z:
	  gaiaSetPointXYZ (out->Coords, points, x, y, z);
	  break;
      case GAIA_XY_M:
	  gaiaSetPointXYM (out->Coords, points, x, y, 0.0);
	  break;
      case GAIA_XY_Z_M:
	  gaiaSetPointXYZM (out->Coords, points, x, y, z, 0.0);
	  break;
      default:
	  gaiaSetPoint (out->Coords, points, x, y);
	  break;
      };
    points++;

    if (i_start < 0 || i_end < 0)
	;
    else
      {
	  for (iv = i_start; iv <= i_end; iv++)
	    {
		z = 0.0;
		m = 0.0;
		switch (ln->DimensionModel)
		  {
		  case GAIA_XY_Z:
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      break;
		  case GAIA_XY_M:
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      break;
		  case GAIA_XY_Z_M:
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      break;
		  default:
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      break;
		  };
		switch (out->DimensionModel)
		  {
		  case GAIA_XY_Z:
		      gaiaSetPointXYZ (out->Coords, points, x, y, z);
		      break;
		  case GAIA_XY_M:
		      gaiaSetPointXYM (out->Coords, points, x, y, 0.0);
		      break;
		  case GAIA_XY_Z_M:
		      gaiaSetPointXYZM (out->Coords, points, x, y, z, 0.0);
		      break;
		  default:
		      gaiaSetPoint (out->Coords, points, x, y);
		      break;
		  };
		points++;
	    }
      }

/* end vertex */
    in_cs = GEOSGeom_getCoordSeq (g_end);
    GEOSCoordSeq_getDimensions (in_cs, &dims);
    if (dims == 3)
      {
	  GEOSCoordSeq_getX (in_cs, 0, &x);
	  GEOSCoordSeq_getY (in_cs, 0, &y);
	  GEOSCoordSeq_getZ (in_cs, 0, &z);
	  m = 0.0;
      }
    else
      {
	  GEOSCoordSeq_getX (in_cs, 0, &x);
	  GEOSCoordSeq_getY (in_cs, 0, &y);
	  z = 0.0;
	  m = 0.0;
      }
    GEOSGeom_destroy (g_end);
    switch (out->DimensionModel)
      {
      case GAIA_XY_Z:
	  gaiaSetPointXYZ (out->Coords, points, x, y, z);
	  break;
      case GAIA_XY_M:
	  gaiaSetPointXYM (out->Coords, points, x, y, 0.0);
	  break;
      case GAIA_XY_Z_M:
	  gaiaSetPointXYZM (out->Coords, points, x, y, z, 0.0);
	  break;
      default:
	  gaiaSetPoint (out->Coords, points, x, y);
	  break;
      };
    return result;
}

static GEOSGeometry *
buildGeosPoints (const gaiaGeomCollPtr gaia)
{
/* converting a GAIA Geometry into a GEOS Geometry of POINTS */
    int pts = 0;
    unsigned int dims;
    int iv;
    int ib;
    int nItem;
    double x;
    double y;
    double z;
    double m;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    GEOSGeometry *geos;
    GEOSGeometry *geos_item;
    GEOSGeometry **geos_coll;
    GEOSCoordSequence *cs;
    if (!gaia)
	return NULL;
    pt = gaia->FirstPoint;
    while (pt)
      {
	  /* counting how many POINTs are there */
	  pts++;
	  pt = pt->Next;
      }
    ln = gaia->FirstLinestring;
    while (ln)
      {
	  /* counting how many POINTs are there */
	  pts += ln->Points;
	  ln = ln->Next;
      }
    pg = gaia->FirstPolygon;
    while (pg)
      {
	  /* counting how many POINTs are there */
	  rng = pg->Exterior;
	  pts += rng->Points - 1;	/* exterior ring */
	  for (ib = 0; ib < pg->NumInteriors; ib++)
	    {
		/* interior ring */
		rng = pg->Interiors + ib;
		pts += rng->Points - 1;
	    }
	  pg = pg->Next;
      }
    if (pts == 0)
	return NULL;
    switch (gaia->DimensionModel)
      {
      case GAIA_XY_Z:
      case GAIA_XY_Z_M:
	  dims = 3;
	  break;
      default:
	  dims = 2;
	  break;
      };
    nItem = 0;
    geos_coll = malloc (sizeof (GEOSGeometry *) * (pts));
    pt = gaia->FirstPoint;
    while (pt)
      {
	  cs = GEOSCoordSeq_create (1, dims);
	  switch (pt->DimensionModel)
	    {
	    case GAIA_XY_Z:
	    case GAIA_XY_Z_M:
		GEOSCoordSeq_setX (cs, 0, pt->X);
		GEOSCoordSeq_setY (cs, 0, pt->Y);
		GEOSCoordSeq_setZ (cs, 0, pt->Z);
		break;
	    default:
		GEOSCoordSeq_setX (cs, 0, pt->X);
		GEOSCoordSeq_setY (cs, 0, pt->Y);
		break;
	    };
	  geos_item = GEOSGeom_createPoint (cs);
	  *(geos_coll + nItem++) = geos_item;
	  pt = pt->Next;
      }
    ln = gaia->FirstLinestring;
    while (ln)
      {
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		z = 0.0;
		m = 0.0;
		switch (ln->DimensionModel)
		  {
		  case GAIA_XY_Z:
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      break;
		  case GAIA_XY_M:
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      break;
		  case GAIA_XY_Z_M:
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      break;
		  default:
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      break;
		  };
		cs = GEOSCoordSeq_create (1, dims);
		if (dims == 3)
		  {
		      GEOSCoordSeq_setX (cs, 0, x);
		      GEOSCoordSeq_setY (cs, 0, y);
		      GEOSCoordSeq_setZ (cs, 0, z);
		  }
		else
		  {
		      GEOSCoordSeq_setX (cs, 0, x);
		      GEOSCoordSeq_setY (cs, 0, y);
		  }
		geos_item = GEOSGeom_createPoint (cs);
		*(geos_coll + nItem++) = geos_item;
	    }
	  ln = ln->Next;
      }
    pg = gaia->FirstPolygon;
    while (pg)
      {
	  rng = pg->Exterior;
	  for (iv = 1; iv < rng->Points; iv++)
	    {
		/* exterior ring */
		z = 0.0;
		m = 0.0;
		switch (rng->DimensionModel)
		  {
		  case GAIA_XY_Z:
		      gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
		      break;
		  case GAIA_XY_M:
		      gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
		      break;
		  case GAIA_XY_Z_M:
		      gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
		      break;
		  default:
		      gaiaGetPoint (rng->Coords, iv, &x, &y);
		      break;
		  };
		cs = GEOSCoordSeq_create (1, dims);
		if (dims == 3)
		  {
		      GEOSCoordSeq_setX (cs, 0, x);
		      GEOSCoordSeq_setY (cs, 0, y);
		      GEOSCoordSeq_setZ (cs, 0, z);
		  }
		else
		  {
		      GEOSCoordSeq_setX (cs, 0, x);
		      GEOSCoordSeq_setY (cs, 0, y);
		  }
		geos_item = GEOSGeom_createPoint (cs);
		*(geos_coll + nItem++) = geos_item;
	    }
	  for (ib = 0; ib < pg->NumInteriors; ib++)
	    {
		/* interior ring */
		rng = pg->Interiors + ib;
		for (iv = 1; iv < rng->Points; iv++)
		  {
		      /* exterior ring */
		      z = 0.0;
		      m = 0.0;
		      switch (rng->DimensionModel)
			{
			case GAIA_XY_Z:
			    gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
			    break;
			case GAIA_XY_M:
			    gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
			    break;
			case GAIA_XY_Z_M:
			    gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
			    break;
			default:
			    gaiaGetPoint (rng->Coords, iv, &x, &y);
			    break;
			};
		      cs = GEOSCoordSeq_create (1, dims);
		      if (dims == 3)
			{
			    GEOSCoordSeq_setX (cs, 0, x);
			    GEOSCoordSeq_setY (cs, 0, y);
			    GEOSCoordSeq_setZ (cs, 0, z);
			}
		      else
			{
			    GEOSCoordSeq_setX (cs, 0, x);
			    GEOSCoordSeq_setY (cs, 0, y);
			}
		      geos_item = GEOSGeom_createPoint (cs);
		      *(geos_coll + nItem++) = geos_item;
		  }
	    }
	  pg = pg->Next;
      }
    geos = GEOSGeom_createCollection (GEOS_MULTIPOINT, geos_coll, pts);
    free (geos_coll);
    GEOSSetSRID (geos, gaia->Srid);
    return geos;
}

static GEOSGeometry *
buildGeosSegments (const gaiaGeomCollPtr gaia)
{
/* converting a GAIA Geometry into a GEOS Geometry of SEGMENTS */
    int segms = 0;
    unsigned int dims;
    int iv;
    int ib;
    int nItem;
    double x;
    double y;
    double z;
    double m;
    double x0;
    double y0;
    double z0;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    GEOSGeometry *geos;
    GEOSGeometry *geos_item;
    GEOSGeometry **geos_coll;
    GEOSCoordSequence *cs;
    if (!gaia)
	return NULL;
    ln = gaia->FirstLinestring;
    while (ln)
      {
	  /* counting how many SEGMENTs are there */
	  segms += ln->Points - 1;
	  ln = ln->Next;
      }
    pg = gaia->FirstPolygon;
    while (pg)
      {
	  /* counting how many SEGMENTs are there */
	  rng = pg->Exterior;
	  segms += rng->Points - 1;	/* exterior ring */
	  for (ib = 0; ib < pg->NumInteriors; ib++)
	    {
		/* interior ring */
		rng = pg->Interiors + ib;
		segms += rng->Points - 1;
	    }
	  pg = pg->Next;
      }
    if (segms == 0)
	return NULL;
    switch (gaia->DimensionModel)
      {
      case GAIA_XY_Z:
      case GAIA_XY_Z_M:
	  dims = 3;
	  break;
      default:
	  dims = 2;
	  break;
      };
    nItem = 0;
    geos_coll = malloc (sizeof (GEOSGeometry *) * (segms));
    ln = gaia->FirstLinestring;
    while (ln)
      {
	  for (iv = 0; iv < ln->Points; iv++)
	    {
		z = 0.0;
		m = 0.0;
		switch (ln->DimensionModel)
		  {
		  case GAIA_XY_Z:
		      gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
		      break;
		  case GAIA_XY_M:
		      gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
		      break;
		  case GAIA_XY_Z_M:
		      gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
		      break;
		  default:
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      break;
		  };
		if (iv > 0)
		  {
		      cs = GEOSCoordSeq_create (2, dims);
		      if (dims == 3)
			{
			    GEOSCoordSeq_setX (cs, 0, x0);
			    GEOSCoordSeq_setY (cs, 0, y0);
			    GEOSCoordSeq_setZ (cs, 0, z0);
			    GEOSCoordSeq_setX (cs, 1, x);
			    GEOSCoordSeq_setY (cs, 1, y);
			    GEOSCoordSeq_setZ (cs, 1, z);
			}
		      else
			{
			    GEOSCoordSeq_setX (cs, 0, x0);
			    GEOSCoordSeq_setY (cs, 0, y0);
			    GEOSCoordSeq_setX (cs, 1, x);
			    GEOSCoordSeq_setY (cs, 1, y);
			}
		      geos_item = GEOSGeom_createLineString (cs);
		      *(geos_coll + nItem++) = geos_item;
		  }
		x0 = x;
		y0 = y;
		z0 = z;
	    }
	  ln = ln->Next;
      }
    pg = gaia->FirstPolygon;
    while (pg)
      {
	  rng = pg->Exterior;
	  for (iv = 0; iv < rng->Points; iv++)
	    {
		/* exterior ring */
		z = 0.0;
		m = 0.0;
		switch (rng->DimensionModel)
		  {
		  case GAIA_XY_Z:
		      gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
		      break;
		  case GAIA_XY_M:
		      gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
		      break;
		  case GAIA_XY_Z_M:
		      gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
		      break;
		  default:
		      gaiaGetPoint (rng->Coords, iv, &x, &y);
		      break;
		  };
		if (iv > 0)
		  {
		      cs = GEOSCoordSeq_create (2, dims);
		      if (dims == 3)
			{
			    GEOSCoordSeq_setX (cs, 0, x0);
			    GEOSCoordSeq_setY (cs, 0, y0);
			    GEOSCoordSeq_setZ (cs, 0, z0);
			    GEOSCoordSeq_setX (cs, 1, x);
			    GEOSCoordSeq_setY (cs, 1, y);
			    GEOSCoordSeq_setZ (cs, 1, z);
			}
		      else
			{
			    GEOSCoordSeq_setX (cs, 0, x0);
			    GEOSCoordSeq_setY (cs, 0, y0);
			    GEOSCoordSeq_setX (cs, 1, x);
			    GEOSCoordSeq_setY (cs, 1, y);
			}
		      geos_item = GEOSGeom_createLineString (cs);
		      *(geos_coll + nItem++) = geos_item;
		  }
		x0 = x;
		y0 = y;
		z0 = z;
	    }
	  for (ib = 0; ib < pg->NumInteriors; ib++)
	    {
		/* interior ring */
		rng = pg->Interiors + ib;
		for (iv = 0; iv < rng->Points; iv++)
		  {
		      /* exterior ring */
		      z = 0.0;
		      m = 0.0;
		      switch (rng->DimensionModel)
			{
			case GAIA_XY_Z:
			    gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
			    break;
			case GAIA_XY_M:
			    gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
			    break;
			case GAIA_XY_Z_M:
			    gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
			    break;
			default:
			    gaiaGetPoint (rng->Coords, iv, &x, &y);
			    break;
			};
		      if (iv > 0)
			{
			    cs = GEOSCoordSeq_create (2, dims);
			    if (dims == 3)
			      {
				  GEOSCoordSeq_setX (cs, 0, x0);
				  GEOSCoordSeq_setY (cs, 0, y0);
				  GEOSCoordSeq_setZ (cs, 0, z0);
				  GEOSCoordSeq_setX (cs, 1, x);
				  GEOSCoordSeq_setY (cs, 1, y);
				  GEOSCoordSeq_setZ (cs, 1, z);
			      }
			    else
			      {
				  GEOSCoordSeq_setX (cs, 0, x0);
				  GEOSCoordSeq_setY (cs, 0, y0);
				  GEOSCoordSeq_setX (cs, 1, x);
				  GEOSCoordSeq_setY (cs, 1, y);
			      }
			    geos_item = GEOSGeom_createLineString (cs);
			    *(geos_coll + nItem++) = geos_item;
			}
		      x0 = x;
		      y0 = y;
		      z0 = z;
		  }
	    }
	  pg = pg->Next;
      }
    geos = GEOSGeom_createCollection (GEOS_MULTILINESTRING, geos_coll, segms);
    free (geos_coll);
    GEOSSetSRID (geos, gaia->Srid);
    return geos;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaShortestLine (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* attempts to compute the the shortest line between two geometries */
    GEOSGeometry *g1_points;
    GEOSGeometry *g1_segments;
    const GEOSGeometry *g1_item;
    GEOSGeometry *g2_points;
    GEOSGeometry *g2_segments;
    const GEOSGeometry *g2_item;
    const GEOSCoordSequence *cs;
    GEOSGeometry *g_pt;
    gaiaGeomCollPtr result;
    gaiaLinestringPtr ln;
    int nItems1;
    int nItems2;
    int it1;
    int it2;
    unsigned int dims;
    double x_ini;
    double y_ini;
    double z_ini;
    double x_fin;
    double y_fin;
    double z_fin;
    double dist;
    double min_dist = DBL_MAX;
    double projection;
    if (!geom1 || !geom2)
	return NULL;

    g1_points = buildGeosPoints (geom1);
    g1_segments = buildGeosSegments (geom1);
    g2_points = buildGeosPoints (geom2);
    g2_segments = buildGeosSegments (geom2);

    if (g1_points && g2_points)
      {
	  /* computing distances between POINTs */
	  nItems1 = GEOSGetNumGeometries (g1_points);
	  nItems2 = GEOSGetNumGeometries (g2_points);
	  for (it1 = 0; it1 < nItems1; it1++)
	    {
		g1_item = GEOSGetGeometryN (g1_points, it1);
		for (it2 = 0; it2 < nItems2; it2++)
		  {
		      g2_item = GEOSGetGeometryN (g2_points, it2);
		      if (GEOSDistance (g1_item, g2_item, &dist))
			{
			    if (dist < min_dist)
			      {
				  /* saving min-dist points */
				  min_dist = dist;
				  cs = GEOSGeom_getCoordSeq (g1_item);
				  GEOSCoordSeq_getDimensions (cs, &dims);
				  if (dims == 3)
				    {
					GEOSCoordSeq_getX (cs, 0, &x_ini);
					GEOSCoordSeq_getY (cs, 0, &y_ini);
					GEOSCoordSeq_getZ (cs, 0, &z_ini);
				    }
				  else
				    {
					GEOSCoordSeq_getX (cs, 0, &x_ini);
					GEOSCoordSeq_getY (cs, 0, &y_ini);
					z_ini = 0.0;
				    }
				  cs = GEOSGeom_getCoordSeq (g2_item);
				  GEOSCoordSeq_getDimensions (cs, &dims);
				  if (dims == 3)
				    {
					GEOSCoordSeq_getX (cs, 0, &x_fin);
					GEOSCoordSeq_getY (cs, 0, &y_fin);
					GEOSCoordSeq_getZ (cs, 0, &z_fin);
				    }
				  else
				    {
					GEOSCoordSeq_getX (cs, 0, &x_fin);
					GEOSCoordSeq_getY (cs, 0, &y_fin);
					z_fin = 0.0;
				    }
			      }
			}
		  }
	    }
      }

    if (g1_points && g2_segments)
      {
	  /* computing distances between POINTs (g1) and SEGMENTs (g2) */
	  nItems1 = GEOSGetNumGeometries (g1_points);
	  nItems2 = GEOSGetNumGeometries (g2_segments);
	  for (it1 = 0; it1 < nItems1; it1++)
	    {
		g1_item = GEOSGetGeometryN (g1_points, it1);
		for (it2 = 0; it2 < nItems2; it2++)
		  {
		      g2_item = GEOSGetGeometryN (g2_segments, it2);
		      if (GEOSDistance (g1_item, g2_item, &dist))
			{
			    if (dist < min_dist)
			      {
				  /* saving min-dist points */
				  projection = GEOSProject (g2_item, g1_item);
				  g_pt = GEOSInterpolate (g2_item, projection);
				  if (g_pt)
				    {
					min_dist = dist;
					cs = GEOSGeom_getCoordSeq (g1_item);
					GEOSCoordSeq_getDimensions (cs, &dims);
					if (dims == 3)
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_ini);
					      GEOSCoordSeq_getY (cs, 0, &y_ini);
					      GEOSCoordSeq_getZ (cs, 0, &z_ini);
					  }
					else
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_ini);
					      GEOSCoordSeq_getY (cs, 0, &y_ini);
					      z_ini = 0.0;
					  }
					cs = GEOSGeom_getCoordSeq (g_pt);
					GEOSCoordSeq_getDimensions (cs, &dims);
					if (dims == 3)
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_fin);
					      GEOSCoordSeq_getY (cs, 0, &y_fin);
					      GEOSCoordSeq_getZ (cs, 0, &z_fin);
					  }
					else
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_fin);
					      GEOSCoordSeq_getY (cs, 0, &y_fin);
					      z_fin = 0.0;
					  }
					GEOSGeom_destroy (g_pt);
				    }
			      }
			}
		  }
	    }
      }

    if (g1_segments && g2_points)
      {
	  /* computing distances between SEGMENTs (g1) and POINTs (g2) */
	  nItems1 = GEOSGetNumGeometries (g1_segments);
	  nItems2 = GEOSGetNumGeometries (g2_points);
	  for (it1 = 0; it1 < nItems1; it1++)
	    {
		g1_item = GEOSGetGeometryN (g1_segments, it1);
		for (it2 = 0; it2 < nItems2; it2++)
		  {
		      g2_item = GEOSGetGeometryN (g2_points, it2);
		      if (GEOSDistance (g1_item, g2_item, &dist))
			{
			    if (dist < min_dist)
			      {
				  /* saving min-dist points */
				  projection = GEOSProject (g1_item, g2_item);
				  g_pt = GEOSInterpolate (g1_item, projection);
				  if (g_pt)
				    {
					min_dist = dist;
					cs = GEOSGeom_getCoordSeq (g_pt);
					GEOSCoordSeq_getDimensions (cs, &dims);
					if (dims == 3)
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_ini);
					      GEOSCoordSeq_getY (cs, 0, &y_ini);
					      GEOSCoordSeq_getZ (cs, 0, &z_ini);
					  }
					else
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_ini);
					      GEOSCoordSeq_getY (cs, 0, &y_ini);
					      z_ini = 0.0;
					  }
					cs = GEOSGeom_getCoordSeq (g2_item);
					GEOSCoordSeq_getDimensions (cs, &dims);
					if (dims == 3)
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_fin);
					      GEOSCoordSeq_getY (cs, 0, &y_fin);
					      GEOSCoordSeq_getZ (cs, 0, &z_fin);
					  }
					else
					  {
					      GEOSCoordSeq_getX (cs, 0, &x_fin);
					      GEOSCoordSeq_getY (cs, 0, &y_fin);
					      z_fin = 0.0;
					  }
					GEOSGeom_destroy (g_pt);
				    }
			      }
			}
		  }
	    }
      }
    if (g1_points)
	GEOSGeom_destroy (g1_points);
    if (g1_segments)
	GEOSGeom_destroy (g1_segments);
    if (g2_points)
	GEOSGeom_destroy (g2_points);
    if (g2_segments)
	GEOSGeom_destroy (g2_segments);
    if (min_dist == DBL_MAX || min_dist <= 0.0)
	return NULL;

/* building the shortest line */
    switch (geom1->DimensionModel)
      {
      case GAIA_XY_Z:
	  result = gaiaAllocGeomCollXYZ ();
	  break;
      case GAIA_XY_M:
	  result = gaiaAllocGeomCollXYM ();
	  break;
      case GAIA_XY_Z_M:
	  result = gaiaAllocGeomCollXYZM ();
	  break;
      default:
	  result = gaiaAllocGeomColl ();
	  break;
      };
    result->Srid = geom1->Srid;
    ln = gaiaAddLinestringToGeomColl (result, 2);
    switch (ln->DimensionModel)
      {
      case GAIA_XY_Z:
	  gaiaSetPointXYZ (ln->Coords, 0, x_ini, y_ini, z_ini);
	  gaiaSetPointXYZ (ln->Coords, 1, x_fin, y_fin, z_fin);
	  break;
      case GAIA_XY_M:
	  gaiaSetPointXYM (ln->Coords, 0, x_ini, y_ini, 0.0);
	  gaiaSetPointXYM (ln->Coords, 1, x_fin, y_fin, 0.0);
	  break;
      case GAIA_XY_Z_M:
	  gaiaSetPointXYZM (ln->Coords, 0, x_ini, y_ini, z_ini, 0.0);
	  gaiaSetPointXYZM (ln->Coords, 1, x_fin, y_fin, z_fin, 0.0);
	  break;
      default:
	  gaiaSetPoint (ln->Coords, 0, x_ini, y_ini);
	  gaiaSetPoint (ln->Coords, 1, x_fin, y_fin);
	  break;
      };
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaSnap (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2, double tolerance)
{
/* attempts to "snap" geom1 on geom2 using the given tolerance */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    GEOSGeometry *g3;
    gaiaGeomCollPtr result;
    if (!geom1 || !geom2)
	return NULL;

    g1 = gaiaToGeos (geom1);
    g2 = gaiaToGeos (geom2);
    g3 = GEOSSnap (g1, g2, tolerance);
    GEOSGeom_destroy (g1);
    GEOSGeom_destroy (g2);
    if (!g3)
	return NULL;
    if (geom1->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g3);
    else if (geom1->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g3);
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g3);
    else
	result = gaiaFromGeos_XY (g3);
    GEOSGeom_destroy (g3);
    if (result == NULL)
	return NULL;
    result->Srid = geom1->Srid;
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaLineMerge (gaiaGeomCollPtr geom)
{
/* attempts to reassemble lines from a collection of sparse fragments */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr result;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;

    g1 = gaiaToGeos (geom);
    g2 = GEOSLineMerge (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g2);
    else
	result = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (result == NULL)
	return NULL;
    result->Srid = geom->Srid;
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaUnaryUnion (gaiaGeomCollPtr geom)
{
/* Unary Union (single Collection) */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr result;
    if (!geom)
	return NULL;
    if (gaiaIsToxic (geom))
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSUnaryUnion (g1);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g2);
    else
	result = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (result == NULL)
	return NULL;
    result->Srid = geom->Srid;
    return result;
}

static void
rotateRingBeforeCut (gaiaLinestringPtr ln, gaiaPointPtr node)
{
/* rotating a Ring, so to ensure that Start/End points match the node */
    int io = 0;
    int iv;
    int copy = 0;
    int base_idx = -1;
    double x;
    double y;
    double z;
    double m;
    gaiaLinestringPtr new_ln = NULL;

    if (ln->DimensionModel == GAIA_XY_Z)
	new_ln = gaiaAllocLinestringXYZ (ln->Points);
    else if (ln->DimensionModel == GAIA_XY_M)
	new_ln = gaiaAllocLinestringXYM (ln->Points);
    else if (ln->DimensionModel == GAIA_XY_Z_M)
	new_ln = gaiaAllocLinestringXYZM (ln->Points);
    else
	new_ln = gaiaAllocLinestring (ln->Points);

/* first pass */
    for (iv = 0; iv < ln->Points; iv++)
      {
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
	    }
	  if (!copy)		/* CAZZO */
	    {
		if (ln->DimensionModel == GAIA_XY_Z
		    || ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      if (node->X == x && node->Y == y && node->Z == z)
			{
			    base_idx = iv;
			    copy = 1;
			}
		  }
		else if (node->X == x && node->Y == y)
		  {
		      base_idx = iv;
		      copy = 1;
		  }
	    }
	  if (copy)
	    {
		/* copying points */
		if (ln->DimensionModel == GAIA_XY_Z)
		  {
		      gaiaSetPointXYZ (new_ln->Coords, io, x, y, z);
		  }
		else if (ln->DimensionModel == GAIA_XY_M)
		  {
		      gaiaSetPointXYM (new_ln->Coords, io, x, y, m);
		  }
		else if (ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      gaiaSetPointXYZM (new_ln->Coords, io, x, y, z, m);
		  }
		else
		  {
		      gaiaSetPoint (new_ln->Coords, io, x, y);
		  }
		io++;
	    }
      }
    if (base_idx <= 0)
      {
	  gaiaFreeLinestring (new_ln);
	  return;
      }

/* second pass */
    for (iv = 1; iv <= base_idx; iv++)
      {
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
	    }
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (new_ln->Coords, io, x, y, z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (new_ln->Coords, io, x, y, m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (new_ln->Coords, io, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (new_ln->Coords, io, x, y);
	    }
	  io++;
      }

/* copying back */
    for (iv = 0; iv < new_ln->Points; iv++)
      {
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (new_ln->Coords, iv, &x, &y, &z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (new_ln->Coords, iv, &x, &y, &m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (new_ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (new_ln->Coords, iv, &x, &y);
	    }
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (ln->Coords, iv, x, y, z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (ln->Coords, iv, x, y, m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (ln->Coords, iv, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (ln->Coords, iv, x, y);
	    }
      }
    gaiaFreeLinestring (new_ln);
}

static void
extractSubLine (gaiaGeomCollPtr result, gaiaLinestringPtr ln, int i_start,
		int i_end)
{
/* extracting s SubLine */
    int iv;
    int io = 0;
    int pts = i_end - i_start + 1;
    gaiaLinestringPtr new_ln = NULL;
    double x;
    double y;
    double z;
    double m;

    new_ln = gaiaAddLinestringToGeomColl (result, pts);

    for (iv = i_start; iv <= i_end; iv++)
      {
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
	    }
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (new_ln->Coords, io, x, y, z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (new_ln->Coords, io, x, y, m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (new_ln->Coords, io, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (new_ln->Coords, io, x, y);
	    }
	  io++;
      }
}

static void
cutLineAtNodes (gaiaLinestringPtr ln, gaiaPointPtr pt_base,
		gaiaGeomCollPtr result)
{
/* attempts to cut a single Line accordingly to given nodes */
    int closed = 0;
    int match = 0;
    int iv;
    int i_start;
    double x;
    double y;
    double z;
    double m;
    gaiaPointPtr pt;
    gaiaPointPtr node = NULL;

    if (gaiaIsClosed (ln))
	closed = 1;
/* pre-check */
    for (iv = 0; iv < ln->Points; iv++)
      {
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
	    }
	  pt = pt_base;
	  while (pt)
	    {
		if (ln->DimensionModel == GAIA_XY_Z
		    || ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      if (pt->X == x && pt->Y == y && pt->Z == z)
			{
			    node = pt;
			    match++;
			}
		  }
		else if (pt->X == x && pt->Y == y)
		  {
		      node = pt;
		      match++;
		  }
		pt = pt->Next;
	    }
      }

    if (closed && node)
	rotateRingBeforeCut (ln, node);

    i_start = 0;
    for (iv = 1; iv < ln->Points - 1; iv++)
      {
	  /* identifying sub-linestrings */
	  if (ln->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
	    }
	  else if (ln->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
	    }
	  else if (ln->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln->Coords, iv, &x, &y);
	    }
	  match = 0;
	  pt = pt_base;
	  while (pt)
	    {
		if (ln->DimensionModel == GAIA_XY_Z
		    || ln->DimensionModel == GAIA_XY_Z_M)
		  {
		      if (pt->X == x && pt->Y == y && pt->Z == z)
			{
			    match = 1;
			    break;
			}
		  }
		else if (pt->X == x && pt->Y == y)
		  {
		      match = 1;
		      break;
		  }
		pt = pt->Next;
	    }
	  if (match)
	    {
		/* cutting the line */
		extractSubLine (result, ln, i_start, iv);
		i_start = iv;
	    }
      }
    if (i_start != 0 && i_start != ln->Points - 1)
      {
	  /* extracting the last SubLine */
	  extractSubLine (result, ln, i_start, ln->Points - 1);
      }
    else
      {
	  /* cloning the untouched Line */
	  extractSubLine (result, ln, 0, ln->Points - 1);
      }
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaLinesCutAtNodes (gaiaGeomCollPtr geom1, gaiaGeomCollPtr geom2)
{
/* attempts to cut lines accordingly to nodes */
    int pts1 = 0;
    int lns1 = 0;
    int pgs1 = 0;
    int pts2 = 0;
    int lns2 = 0;
    int pgs2 = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaGeomCollPtr result = NULL;

    if (!geom1)
	return NULL;
    if (!geom2)
	return NULL;

/* both Geometryes should have identical Dimensions */
    if (geom1->DimensionModel != geom2->DimensionModel)
	return NULL;

    pt = geom1->FirstPoint;
    while (pt)
      {
	  pts1++;
	  pt = pt->Next;
      }
    ln = geom1->FirstLinestring;
    while (ln)
      {
	  lns1++;
	  ln = ln->Next;
      }
    pg = geom1->FirstPolygon;
    while (pg)
      {
	  pgs1++;
	  pg = pg->Next;
      }
    pt = geom2->FirstPoint;
    while (pt)
      {
	  pts2++;
	  pt = pt->Next;
      }
    ln = geom2->FirstLinestring;
    while (ln)
      {
	  lns2++;
	  ln = ln->Next;
      }
    pg = geom2->FirstPolygon;
    while (pg)
      {
	  pgs2++;
	  pg = pg->Next;
      }

/* the first Geometry is expected to contain one or more Linestring(s) */
    if (pts1 == 0 && lns1 > 0 && pgs1 == 0)
	;
    else
	return NULL;
/* the second Geometry is expected to contain one or more Point(s) */
    if (pts2 > 0 && lns2 == 0 && pgs2 == 0)
	;
    else
	return NULL;

/* attempting to cut Lines accordingly to Nodes */
    if (geom1->DimensionModel == GAIA_XY_Z)
	result = gaiaAllocGeomCollXYZ ();
    else if (geom1->DimensionModel == GAIA_XY_M)
	result = gaiaAllocGeomCollXYM ();
    else if (geom1->DimensionModel == GAIA_XY_Z_M)
	result = gaiaAllocGeomCollXYZM ();
    else
	result = gaiaAllocGeomColl ();
    ln = geom1->FirstLinestring;
    while (ln)
      {
	  cutLineAtNodes (ln, geom2->FirstPoint, result);
	  ln = ln->Next;
      }
    if (result->FirstLinestring == NULL)
      {
	  gaiaFreeGeomColl (result);
	  return NULL;
      }
    result->Srid = geom1->Srid;
    return result;
}

#endif /* end GEOS advanced features */

#ifdef GEOS_TRUNK		/* GEOS experimental features */

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaDelaunayTriangulation (gaiaGeomCollPtr geom, double tolerance,
			   int only_edges)
{
/* Delaunay Triangulation */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr result;
    if (!geom)
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSDelaunayTriangulation (g1, tolerance, only_edges);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g2);
    else
	result = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (result == NULL)
	return NULL;
    result->Srid = geom->Srid;
    if (only_edges)
	result->DeclaredType = GAIA_MULTILINESTRING;
    else
	result->DeclaredType = GAIA_MULTIPOLYGON;
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaVoronojDiagram (gaiaGeomCollPtr geom, double extra_frame_size,
		    double tolerance, int only_edges)
{
/* Voronoj Diagram */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr result;
    gaiaPolygonPtr pg;
    int pgs = 0;
    int errs = 0;
    void *voronoj;
    if (!geom)
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSDelaunayTriangulation (g1, tolerance, 0);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g2);
    else
	result = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (result == NULL)
	return NULL;
    pg = result->FirstPolygon;
    while (pg)
      {
	  /* counting how many triangles are in Delaunay */
	  if (delaunay_triangle_check (pg))
	      pgs++;
	  else
	      errs++;
	  pg = pg->Next;
      }
    if (pgs == 0 || errs)
      {
	  gaiaFreeGeomColl (result);
	  return NULL;
      }

/* building the Voronoj Diagram from Delaunay */
    voronoj = voronoj_build (pgs, result->FirstPolygon, extra_frame_size);
    gaiaFreeGeomColl (result);

/* creating the Geometry representing Voronoj */
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaAllocGeomCollXYZ ();
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaAllocGeomCollXYM ();
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaAllocGeomCollXYZM ();
    else
	result = gaiaAllocGeomColl ();
    result = voronoj_export (voronoj, result, only_edges);
    voronoj_free (voronoj);

    result->Srid = geom->Srid;
    if (only_edges)
	result->DeclaredType = GAIA_MULTILINESTRING;
    else
	result->DeclaredType = GAIA_MULTIPOLYGON;
    return result;
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaConcaveHull (gaiaGeomCollPtr geom, double factor, double tolerance,
		 int allow_holes)
{
/* Concave Hull */
    GEOSGeometry *g1;
    GEOSGeometry *g2;
    gaiaGeomCollPtr result;
    gaiaGeomCollPtr concave_hull;
    gaiaPolygonPtr pg;
    int pgs = 0;
    int errs = 0;
    if (!geom)
	return NULL;
    g1 = gaiaToGeos (geom);
    g2 = GEOSDelaunayTriangulation (g1, tolerance, 0);
    GEOSGeom_destroy (g1);
    if (!g2)
	return NULL;
    if (geom->DimensionModel == GAIA_XY_Z)
	result = gaiaFromGeos_XYZ (g2);
    else if (geom->DimensionModel == GAIA_XY_M)
	result = gaiaFromGeos_XYM (g2);
    else if (geom->DimensionModel == GAIA_XY_Z_M)
	result = gaiaFromGeos_XYZM (g2);
    else
	result = gaiaFromGeos_XY (g2);
    GEOSGeom_destroy (g2);
    if (result == NULL)
	return NULL;
    pg = result->FirstPolygon;
    while (pg)
      {
	  /* counting how many triangles are in Delaunay */
	  if (delaunay_triangle_check (pg))
	      pgs++;
	  else
	      errs++;
	  pg = pg->Next;
      }
    if (pgs == 0 || errs)
      {
	  gaiaFreeGeomColl (result);
	  return NULL;
      }

/* building the Concave Hull from Delaunay */
    concave_hull =
	concave_hull_build (result->FirstPolygon, geom->DimensionModel, factor,
			    allow_holes);
    gaiaFreeGeomColl (result);
    if (!concave_hull)
	return NULL;
    result = concave_hull;

    result->Srid = geom->Srid;
    return result;
}

#endif /* end GEOS experimental features */

#endif /* end including GEOS */
