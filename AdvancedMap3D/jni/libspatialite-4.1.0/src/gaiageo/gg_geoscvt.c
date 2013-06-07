/*

 gg_geoscvt.c -- Gaia / GEOS conversion [Geometry]
    
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

#if defined(_WIN32) && !defined(__MINGW32__)
#include "config-msvc.h"
#else
#include "config.h"
#endif

#ifndef OMIT_GEOS		/* including GEOS */
#include <geos_c.h>
#endif

#include <spatialite/sqlite.h>

#include <spatialite/gaiageo.h>

#ifndef OMIT_GEOS		/* including GEOS */

static GEOSGeometry *
toGeosGeometry (const gaiaGeomCollPtr gaia, int mode)
{
/* converting a GAIA Geometry into a GEOS Geometry */
    int pts = 0;
    int lns = 0;
    int pgs = 0;
    int type;
    int geos_type;
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
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    GEOSGeometry *geos;
    GEOSGeometry *geos_ext;
    GEOSGeometry *geos_int;
    GEOSGeometry *geos_item;
    GEOSGeometry **geos_holes;
    GEOSGeometry **geos_coll;
    GEOSCoordSequence *cs;
    int ring_points;
    int n_items;
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
	  /* counting how many LINESTRINGs are there */
	  lns++;
	  ln = ln->Next;
      }
    pg = gaia->FirstPolygon;
    while (pg)
      {
	  /* counting how many POLYGONs are there */
	  pgs++;
	  pg = pg->Next;
      }
    if (mode == GAIA2GEOS_ONLY_POINTS && pts == 0)
	return NULL;
    if (mode == GAIA2GEOS_ONLY_LINESTRINGS && lns == 0)
	return NULL;
    if (mode == GAIA2GEOS_ONLY_POLYGONS && pgs == 0)
	return NULL;
    if (pts == 0 && lns == 0 && pgs == 0)
	return NULL;
    else if (pts == 1 && lns == 0 && pgs == 0)
      {
	  if (gaia->DeclaredType == GAIA_MULTIPOINT)
	      type = GAIA_MULTIPOINT;
	  else if (gaia->DeclaredType == GAIA_GEOMETRYCOLLECTION)
	      type = GAIA_GEOMETRYCOLLECTION;
	  else
	      type = GAIA_POINT;
      }
    else if (pts == 0 && lns == 1 && pgs == 0)
      {
	  if (gaia->DeclaredType == GAIA_MULTILINESTRING)
	      type = GAIA_MULTILINESTRING;
	  else if (gaia->DeclaredType == GAIA_GEOMETRYCOLLECTION)
	      type = GAIA_GEOMETRYCOLLECTION;
	  else
	      type = GAIA_LINESTRING;
      }
    else if (pts == 0 && lns == 0 && pgs == 1)
      {
	  if (gaia->DeclaredType == GAIA_MULTIPOLYGON)
	      type = GAIA_MULTIPOLYGON;
	  else if (gaia->DeclaredType == GAIA_GEOMETRYCOLLECTION)
	      type = GAIA_GEOMETRYCOLLECTION;
	  else
	      type = GAIA_POLYGON;
      }
    else if (pts > 1 && lns == 0 && pgs == 0)
      {
	  if (gaia->DeclaredType == GAIA_GEOMETRYCOLLECTION)
	      type = GAIA_GEOMETRYCOLLECTION;
	  else
	      type = GAIA_MULTIPOINT;
      }
    else if (pts == 0 && lns > 1 && pgs == 0)
      {
	  if (gaia->DeclaredType == GAIA_GEOMETRYCOLLECTION)
	      type = GAIA_GEOMETRYCOLLECTION;
	  else
	      type = GAIA_MULTILINESTRING;
      }
    else if (pts == 0 && lns == 0 && pgs > 1)
      {
	  if (gaia->DeclaredType == GAIA_GEOMETRYCOLLECTION)
	      type = GAIA_GEOMETRYCOLLECTION;
	  else
	      type = GAIA_MULTIPOLYGON;
      }
    else
	type = GAIA_GEOMETRYCOLLECTION;
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
    switch (type)
      {
      case GAIA_POINT:
	  if (mode == GAIA2GEOS_ALL || mode == GAIA2GEOS_ONLY_POINTS)
	    {
		pt = gaia->FirstPoint;
		cs = GEOSCoordSeq_create (1, dims);
		switch (gaia->DimensionModel)
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
		geos = GEOSGeom_createPoint (cs);
	    }
	  break;
      case GAIA_LINESTRING:
	  if (mode == GAIA2GEOS_ALL || mode == GAIA2GEOS_ONLY_LINESTRINGS)
	    {
		ln = gaia->FirstLinestring;
		cs = GEOSCoordSeq_create (ln->Points, dims);
		for (iv = 0; iv < ln->Points; iv++)
		  {
		      switch (ln->DimensionModel)
			{
			case GAIA_XY_Z:
			    gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    GEOSCoordSeq_setZ (cs, iv, z);
			    break;
			case GAIA_XY_M:
			    gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    break;
			case GAIA_XY_Z_M:
			    gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z, &m);
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    GEOSCoordSeq_setZ (cs, iv, z);
			    break;
			default:
			    gaiaGetPoint (ln->Coords, iv, &x, &y);
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    break;
			};
		  }
		geos = GEOSGeom_createLineString (cs);
	    }
	  break;
      case GAIA_POLYGON:
	  if (mode == GAIA2GEOS_ALL || mode == GAIA2GEOS_ONLY_POLYGONS)
	    {
		pg = gaia->FirstPolygon;
		rng = pg->Exterior;
		/* exterior ring */
		ring_points = rng->Points;
		if (gaiaIsNotClosedRing (rng))
		    ring_points++;
		cs = GEOSCoordSeq_create (ring_points, dims);
		for (iv = 0; iv < rng->Points; iv++)
		  {
		      switch (rng->DimensionModel)
			{
			case GAIA_XY_Z:
			    gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
			    if (iv == 0)
			      {
				  /* saving the first vertex */
				  x0 = x;
				  y0 = y;
				  z0 = z;
			      }
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    GEOSCoordSeq_setZ (cs, iv, z);
			    break;
			case GAIA_XY_M:
			    gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
			    if (iv == 0)
			      {
				  /* saving the first vertex */
				  x0 = x;
				  y0 = y;
			      }
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    break;
			case GAIA_XY_Z_M:
			    gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z, &m);
			    if (iv == 0)
			      {
				  /* saving the first vertex */
				  x0 = x;
				  y0 = y;
				  z0 = z;
			      }
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    GEOSCoordSeq_setZ (cs, iv, z);
			    break;
			default:
			    gaiaGetPoint (rng->Coords, iv, &x, &y);
			    if (iv == 0)
			      {
				  /* saving the first vertex */
				  x0 = x;
				  y0 = y;
			      }
			    GEOSCoordSeq_setX (cs, iv, x);
			    GEOSCoordSeq_setY (cs, iv, y);
			    break;
			};
		  }
		if (ring_points > rng->Points)
		  {
		      /* ensuring Ring's closure */
		      iv = ring_points - 1;
		      switch (rng->DimensionModel)
			{
			case GAIA_XY_Z:
			case GAIA_XY_Z_M:
			    GEOSCoordSeq_setX (cs, iv, x0);
			    GEOSCoordSeq_setY (cs, iv, y0);
			    GEOSCoordSeq_setZ (cs, iv, z0);
			    break;
			default:
			    GEOSCoordSeq_setX (cs, iv, x0);
			    GEOSCoordSeq_setY (cs, iv, y0);
			    break;
			};
		  }
		geos_ext = GEOSGeom_createLinearRing (cs);
		geos_holes = NULL;
		if (pg->NumInteriors > 0)
		  {
		      geos_holes =
			  malloc (sizeof (GEOSGeometry *) * pg->NumInteriors);
		      for (ib = 0; ib < pg->NumInteriors; ib++)
			{
			    /* interior ring */
			    rng = pg->Interiors + ib;
			    ring_points = rng->Points;
			    if (gaiaIsNotClosedRing (rng))
				ring_points++;
			    cs = GEOSCoordSeq_create (ring_points, dims);
			    for (iv = 0; iv < rng->Points; iv++)
			      {
				  switch (rng->DimensionModel)
				    {
				    case GAIA_XY_Z:
					gaiaGetPointXYZ (rng->Coords, iv, &x,
							 &y, &z);
					if (iv == 0)
					  {
					      /* saving the first vertex */
					      x0 = x;
					      y0 = y;
					      z0 = z;
					  }
					GEOSCoordSeq_setX (cs, iv, x);
					GEOSCoordSeq_setY (cs, iv, y);
					GEOSCoordSeq_setZ (cs, iv, z);
					break;
				    case GAIA_XY_M:
					gaiaGetPointXYM (rng->Coords, iv, &x,
							 &y, &m);
					if (iv == 0)
					  {
					      /* saving the first vertex */
					      x0 = x;
					      y0 = y;
					  }
					GEOSCoordSeq_setX (cs, iv, x);
					GEOSCoordSeq_setY (cs, iv, y);
					break;
				    case GAIA_XY_Z_M:
					gaiaGetPointXYZM (rng->Coords, iv, &x,
							  &y, &z, &m);
					if (iv == 0)
					  {
					      /* saving the first vertex */
					      x0 = x;
					      y0 = y;
					      z0 = z;
					  }
					GEOSCoordSeq_setX (cs, iv, x);
					GEOSCoordSeq_setY (cs, iv, y);
					GEOSCoordSeq_setZ (cs, iv, z);
					break;
				    default:
					gaiaGetPoint (rng->Coords, iv, &x, &y);
					if (iv == 0)
					  {
					      /* saving the first vertex */
					      x0 = x;
					      y0 = y;
					  }
					GEOSCoordSeq_setX (cs, iv, x);
					GEOSCoordSeq_setY (cs, iv, y);
					break;
				    };
			      }
			    if (ring_points > rng->Points)
			      {
				  /* ensuring Ring's closure */
				  iv = ring_points - 1;
				  switch (rng->DimensionModel)
				    {
				    case GAIA_XY_Z:
				    case GAIA_XY_Z_M:
					GEOSCoordSeq_setX (cs, iv, x0);
					GEOSCoordSeq_setY (cs, iv, y0);
					GEOSCoordSeq_setZ (cs, iv, z0);
					break;
				    default:
					GEOSCoordSeq_setX (cs, iv, x0);
					GEOSCoordSeq_setY (cs, iv, y0);
					break;
				    };
			      }
			    geos_int = GEOSGeom_createLinearRing (cs);
			    *(geos_holes + ib) = geos_int;
			}
		  }
		geos =
		    GEOSGeom_createPolygon (geos_ext, geos_holes,
					    pg->NumInteriors);
		if (geos_holes)
		    free (geos_holes);
	    }
	  break;
      case GAIA_MULTIPOINT:
      case GAIA_MULTILINESTRING:
      case GAIA_MULTIPOLYGON:
      case GAIA_GEOMETRYCOLLECTION:
	  nItem = 0;
	  if (mode == GAIA2GEOS_ONLY_POINTS)
	    {
		geos_coll = malloc (sizeof (GEOSGeometry *) * (pts));
		n_items = pts;
	    }
	  else if (mode == GAIA2GEOS_ONLY_LINESTRINGS)
	    {
		geos_coll = malloc (sizeof (GEOSGeometry *) * (lns));
		n_items = lns;
	    }
	  else if (mode == GAIA2GEOS_ONLY_POLYGONS)
	    {
		geos_coll = malloc (sizeof (GEOSGeometry *) * (pgs));
		n_items = pgs;
	    }
	  else
	    {
		geos_coll =
		    malloc (sizeof (GEOSGeometry *) * (pts + lns + pgs));
		n_items = pts + lns + pgs;
	    }
	  if (mode == GAIA2GEOS_ALL || mode == GAIA2GEOS_ONLY_POINTS)
	    {
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
	    }
	  if (mode == GAIA2GEOS_ALL || mode == GAIA2GEOS_ONLY_LINESTRINGS)
	    {
		ln = gaia->FirstLinestring;
		while (ln)
		  {
		      cs = GEOSCoordSeq_create (ln->Points, dims);
		      for (iv = 0; iv < ln->Points; iv++)
			{
			    switch (ln->DimensionModel)
			      {
			      case GAIA_XY_Z:
				  gaiaGetPointXYZ (ln->Coords, iv, &x, &y, &z);
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  GEOSCoordSeq_setZ (cs, iv, z);
				  break;
			      case GAIA_XY_M:
				  gaiaGetPointXYM (ln->Coords, iv, &x, &y, &m);
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  break;
			      case GAIA_XY_Z_M:
				  gaiaGetPointXYZM (ln->Coords, iv, &x, &y, &z,
						    &m);
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  GEOSCoordSeq_setZ (cs, iv, z);
				  break;
			      default:
				  gaiaGetPoint (ln->Coords, iv, &x, &y);
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  break;
			      };
			}
		      geos_item = GEOSGeom_createLineString (cs);
		      *(geos_coll + nItem++) = geos_item;
		      ln = ln->Next;
		  }
	    }
	  if (mode == GAIA2GEOS_ALL || mode == GAIA2GEOS_ONLY_POLYGONS)
	    {
		pg = gaia->FirstPolygon;
		while (pg)
		  {
		      rng = pg->Exterior;
		      /* exterior ring */
		      ring_points = rng->Points;
		      if (gaiaIsNotClosedRing (rng))
			  ring_points++;
		      cs = GEOSCoordSeq_create (ring_points, dims);
		      for (iv = 0; iv < rng->Points; iv++)
			{
			    switch (rng->DimensionModel)
			      {
			      case GAIA_XY_Z:
				  gaiaGetPointXYZ (rng->Coords, iv, &x, &y, &z);
				  if (iv == 0)
				    {
					/* saving the first vertex */
					x0 = x;
					y0 = y;
					z0 = z;
				    }
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  GEOSCoordSeq_setZ (cs, iv, z);
				  break;
			      case GAIA_XY_M:
				  gaiaGetPointXYM (rng->Coords, iv, &x, &y, &m);
				  if (iv == 0)
				    {
					/* saving the first vertex */
					x0 = x;
					y0 = y;
				    }
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  break;
			      case GAIA_XY_Z_M:
				  gaiaGetPointXYZM (rng->Coords, iv, &x, &y, &z,
						    &m);
				  if (iv == 0)
				    {
					/* saving the first vertex */
					x0 = x;
					y0 = y;
					z0 = z;
				    }
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  GEOSCoordSeq_setZ (cs, iv, z);
				  break;
			      default:
				  gaiaGetPoint (rng->Coords, iv, &x, &y);
				  if (iv == 0)
				    {
					/* saving the first vertex */
					x0 = x;
					y0 = y;
				    }
				  GEOSCoordSeq_setX (cs, iv, x);
				  GEOSCoordSeq_setY (cs, iv, y);
				  break;
			      };
			}
		      if (ring_points > rng->Points)
			{
			    /* ensuring Ring's closure */
			    iv = ring_points - 1;
			    switch (rng->DimensionModel)
			      {
			      case GAIA_XY_Z:
			      case GAIA_XY_Z_M:
				  GEOSCoordSeq_setX (cs, iv, x0);
				  GEOSCoordSeq_setY (cs, iv, y0);
				  GEOSCoordSeq_setZ (cs, iv, z0);
				  break;
			      default:
				  GEOSCoordSeq_setX (cs, iv, x0);
				  GEOSCoordSeq_setY (cs, iv, y0);
				  break;
			      };
			}
		      geos_ext = GEOSGeom_createLinearRing (cs);
		      geos_holes = NULL;
		      if (pg->NumInteriors > 0)
			{
			    geos_holes =
				malloc (sizeof (GEOSGeometry *) *
					pg->NumInteriors);
			    for (ib = 0; ib < pg->NumInteriors; ib++)
			      {
				  /* interior ring */
				  rng = pg->Interiors + ib;
				  ring_points = rng->Points;
				  if (gaiaIsNotClosedRing (rng))
				      ring_points++;
				  cs = GEOSCoordSeq_create (ring_points, dims);
				  for (iv = 0; iv < rng->Points; iv++)
				    {
					switch (rng->DimensionModel)
					  {
					  case GAIA_XY_Z:
					      gaiaGetPointXYZ (rng->Coords, iv,
							       &x, &y, &z);
					      if (iv == 0)
						{
						    /* saving the first vertex */
						    x0 = x;
						    y0 = y;
						    z0 = z;
						}
					      GEOSCoordSeq_setX (cs, iv, x);
					      GEOSCoordSeq_setY (cs, iv, y);
					      GEOSCoordSeq_setZ (cs, iv, z);
					      break;
					  case GAIA_XY_M:
					      gaiaGetPointXYM (rng->Coords, iv,
							       &x, &y, &m);
					      if (iv == 0)
						{
						    /* saving the first vertex */
						    x0 = x;
						    y0 = y;
						}
					      GEOSCoordSeq_setX (cs, iv, x);
					      GEOSCoordSeq_setY (cs, iv, y);
					      break;
					  case GAIA_XY_Z_M:
					      gaiaGetPointXYZM (rng->Coords, iv,
								&x, &y, &z, &m);
					      if (iv == 0)
						{
						    /* saving the first vertex */
						    x0 = x;
						    y0 = y;
						    z0 = z;
						}
					      GEOSCoordSeq_setX (cs, iv, x);
					      GEOSCoordSeq_setY (cs, iv, y);
					      GEOSCoordSeq_setZ (cs, iv, z);
					      break;
					  default:
					      gaiaGetPoint (rng->Coords, iv, &x,
							    &y);
					      if (iv == 0)
						{
						    /* saving the first vertex */
						    x0 = x;
						    y0 = y;
						}
					      GEOSCoordSeq_setX (cs, iv, x);
					      GEOSCoordSeq_setY (cs, iv, y);
					      break;
					  };
				    }
				  if (ring_points > rng->Points)
				    {
					/* ensuring Ring's closure */
					iv = ring_points - 1;
					switch (rng->DimensionModel)
					  {
					  case GAIA_XY_Z:
					  case GAIA_XY_Z_M:
					      GEOSCoordSeq_setX (cs, iv, x0);
					      GEOSCoordSeq_setY (cs, iv, y0);
					      GEOSCoordSeq_setZ (cs, iv, z0);
					      break;
					  default:
					      GEOSCoordSeq_setX (cs, iv, x0);
					      GEOSCoordSeq_setY (cs, iv, y0);
					      break;
					  };
				    }
				  geos_int = GEOSGeom_createLinearRing (cs);
				  *(geos_holes + ib) = geos_int;
			      }
			}
		      geos_item =
			  GEOSGeom_createPolygon (geos_ext, geos_holes,
						  pg->NumInteriors);
		      if (geos_holes)
			  free (geos_holes);
		      *(geos_coll + nItem++) = geos_item;
		      pg = pg->Next;
		  }
	    }
	  geos_type = GEOS_GEOMETRYCOLLECTION;
	  if (type == GAIA_MULTIPOINT)
	      geos_type = GEOS_MULTIPOINT;
	  if (type == GAIA_MULTILINESTRING)
	      geos_type = GEOS_MULTILINESTRING;
	  if (type == GAIA_MULTIPOLYGON)
	      geos_type = GEOS_MULTIPOLYGON;
	  geos = GEOSGeom_createCollection (geos_type, geos_coll, n_items);
	  if (geos_coll)
	      free (geos_coll);
	  break;
      default:
	  geos = NULL;
      };
    if (geos)
	GEOSSetSRID (geos, gaia->Srid);
    return geos;
}

static gaiaGeomCollPtr
fromGeosGeometry (const GEOSGeometry * geos, const int dimension_model)
{
/* converting a GEOS Geometry into a GAIA Geometry */
    int type;
    int itemType;
    unsigned int dims;
    int iv;
    int ib;
    int it;
    int sub_it;
    int nItems;
    int nSubItems;
    int holes;
    unsigned int points;
    double x;
    double y;
    double z;
    const GEOSCoordSequence *cs;
    const GEOSGeometry *geos_ring;
    const GEOSGeometry *geos_item;
    const GEOSGeometry *geos_sub_item;
    gaiaGeomCollPtr gaia = NULL;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    if (!geos)
	return NULL;
    type = GEOSGeomTypeId (geos);
    switch (type)
      {
      case GEOS_POINT:
	  if (dimension_model == GAIA_XY_Z)
	      gaia = gaiaAllocGeomCollXYZ ();
	  else if (dimension_model == GAIA_XY_M)
	      gaia = gaiaAllocGeomCollXYM ();
	  else if (dimension_model == GAIA_XY_Z_M)
	      gaia = gaiaAllocGeomCollXYZM ();
	  else
	      gaia = gaiaAllocGeomColl ();
	  gaia->DeclaredType = GAIA_POINT;
	  gaia->Srid = GEOSGetSRID (geos);
	  cs = GEOSGeom_getCoordSeq (geos);
	  GEOSCoordSeq_getDimensions (cs, &dims);
	  if (dims == 3)
	    {
		GEOSCoordSeq_getX (cs, 0, &x);
		GEOSCoordSeq_getY (cs, 0, &y);
		GEOSCoordSeq_getZ (cs, 0, &z);
	    }
	  else
	    {
		GEOSCoordSeq_getX (cs, 0, &x);
		GEOSCoordSeq_getY (cs, 0, &y);
		z = 0.0;
	    }
	  if (dimension_model == GAIA_XY_Z)
	      gaiaAddPointToGeomCollXYZ (gaia, x, y, z);
	  else if (dimension_model == GAIA_XY_M)
	      gaiaAddPointToGeomCollXYM (gaia, x, y, 0.0);
	  else if (dimension_model == GAIA_XY_Z_M)
	      gaiaAddPointToGeomCollXYZM (gaia, x, y, z, 0.0);
	  else
	      gaiaAddPointToGeomColl (gaia, x, y);
	  break;
      case GEOS_LINESTRING:
	  if (dimension_model == GAIA_XY_Z)
	      gaia = gaiaAllocGeomCollXYZ ();
	  else if (dimension_model == GAIA_XY_M)
	      gaia = gaiaAllocGeomCollXYM ();
	  else if (dimension_model == GAIA_XY_Z_M)
	      gaia = gaiaAllocGeomCollXYZM ();
	  else
	      gaia = gaiaAllocGeomColl ();
	  gaia->DeclaredType = GAIA_LINESTRING;
	  gaia->Srid = GEOSGetSRID (geos);
	  cs = GEOSGeom_getCoordSeq (geos);
	  GEOSCoordSeq_getDimensions (cs, &dims);
	  GEOSCoordSeq_getSize (cs, &points);
	  ln = gaiaAddLinestringToGeomColl (gaia, points);
	  for (iv = 0; iv < (int) points; iv++)
	    {
		if (dims == 3)
		  {
		      GEOSCoordSeq_getX (cs, iv, &x);
		      GEOSCoordSeq_getY (cs, iv, &y);
		      GEOSCoordSeq_getZ (cs, iv, &z);
		  }
		else
		  {
		      GEOSCoordSeq_getX (cs, iv, &x);
		      GEOSCoordSeq_getY (cs, iv, &y);
		      z = 0.0;
		  }
		if (dimension_model == GAIA_XY_Z)
		  {
		      gaiaSetPointXYZ (ln->Coords, iv, x, y, z);
		  }
		else if (dimension_model == GAIA_XY_M)
		  {
		      gaiaSetPointXYM (ln->Coords, iv, x, y, 0.0);
		  }
		else if (dimension_model == GAIA_XY_Z_M)
		  {
		      gaiaSetPointXYZM (ln->Coords, iv, x, y, z, 0.0);
		  }
		else
		  {
		      gaiaSetPoint (ln->Coords, iv, x, y);
		  }
	    }
	  break;
      case GEOS_POLYGON:
	  if (dimension_model == GAIA_XY_Z)
	      gaia = gaiaAllocGeomCollXYZ ();
	  else if (dimension_model == GAIA_XY_M)
	      gaia = gaiaAllocGeomCollXYM ();
	  else if (dimension_model == GAIA_XY_Z_M)
	      gaia = gaiaAllocGeomCollXYZM ();
	  else
	      gaia = gaiaAllocGeomColl ();
	  gaia->DeclaredType = GAIA_POLYGON;
	  gaia->Srid = GEOSGetSRID (geos);
	  /* exterior ring */
	  holes = GEOSGetNumInteriorRings (geos);
	  geos_ring = GEOSGetExteriorRing (geos);
	  cs = GEOSGeom_getCoordSeq (geos_ring);
	  GEOSCoordSeq_getDimensions (cs, &dims);
	  GEOSCoordSeq_getSize (cs, &points);
	  pg = gaiaAddPolygonToGeomColl (gaia, points, holes);
	  rng = pg->Exterior;
	  for (iv = 0; iv < (int) points; iv++)
	    {
		if (dims == 3)
		  {
		      GEOSCoordSeq_getX (cs, iv, &x);
		      GEOSCoordSeq_getY (cs, iv, &y);
		      GEOSCoordSeq_getZ (cs, iv, &z);
		  }
		else
		  {
		      GEOSCoordSeq_getX (cs, iv, &x);
		      GEOSCoordSeq_getY (cs, iv, &y);
		      z = 0.0;
		  }
		if (dimension_model == GAIA_XY_Z)
		  {
		      gaiaSetPointXYZ (rng->Coords, iv, x, y, z);
		  }
		else if (dimension_model == GAIA_XY_M)
		  {
		      gaiaSetPointXYM (rng->Coords, iv, x, y, 0.0);
		  }
		else if (dimension_model == GAIA_XY_Z_M)
		  {
		      gaiaSetPointXYZM (rng->Coords, iv, x, y, z, 0.0);
		  }
		else
		  {
		      gaiaSetPoint (rng->Coords, iv, x, y);
		  }
	    }
	  for (ib = 0; ib < holes; ib++)
	    {
		/* interior rings */
		geos_ring = GEOSGetInteriorRingN (geos, ib);
		cs = GEOSGeom_getCoordSeq (geos_ring);
		GEOSCoordSeq_getDimensions (cs, &dims);
		GEOSCoordSeq_getSize (cs, &points);
		rng = gaiaAddInteriorRing (pg, ib, points);
		for (iv = 0; iv < (int) points; iv++)
		  {
		      if (dims == 3)
			{
			    GEOSCoordSeq_getX (cs, iv, &x);
			    GEOSCoordSeq_getY (cs, iv, &y);
			    GEOSCoordSeq_getZ (cs, iv, &z);
			}
		      else
			{
			    GEOSCoordSeq_getX (cs, iv, &x);
			    GEOSCoordSeq_getY (cs, iv, &y);
			    z = 0.0;
			}
		      if (dimension_model == GAIA_XY_Z)
			{
			    gaiaSetPointXYZ (rng->Coords, iv, x, y, z);
			}
		      else if (dimension_model == GAIA_XY_M)
			{
			    gaiaSetPointXYM (rng->Coords, iv, x, y, 0.0);
			}
		      else if (dimension_model == GAIA_XY_Z_M)
			{
			    gaiaSetPointXYZM (rng->Coords, iv, x, y, z, 0.0);
			}
		      else
			{
			    gaiaSetPoint (rng->Coords, iv, x, y);
			}
		  }
	    }
	  break;
      case GEOS_MULTIPOINT:
      case GEOS_MULTILINESTRING:
      case GEOS_MULTIPOLYGON:
      case GEOS_GEOMETRYCOLLECTION:
	  if (dimension_model == GAIA_XY_Z)
	      gaia = gaiaAllocGeomCollXYZ ();
	  else if (dimension_model == GAIA_XY_M)
	      gaia = gaiaAllocGeomCollXYM ();
	  else if (dimension_model == GAIA_XY_Z_M)
	      gaia = gaiaAllocGeomCollXYZM ();
	  else
	      gaia = gaiaAllocGeomColl ();
	  if (type == GEOS_MULTIPOINT)
	      gaia->DeclaredType = GAIA_MULTIPOINT;
	  else if (type == GEOS_MULTILINESTRING)
	      gaia->DeclaredType = GAIA_MULTILINESTRING;
	  else if (type == GEOS_MULTIPOLYGON)
	      gaia->DeclaredType = GAIA_MULTIPOLYGON;
	  else
	      gaia->DeclaredType = GAIA_GEOMETRYCOLLECTION;
	  gaia->Srid = GEOSGetSRID (geos);
	  nItems = GEOSGetNumGeometries (geos);
	  for (it = 0; it < nItems; it++)
	    {
		/* looping on elementaty geometries */
		geos_item = GEOSGetGeometryN (geos, it);
		itemType = GEOSGeomTypeId (geos_item);
		switch (itemType)
		  {
		  case GEOS_POINT:
		      cs = GEOSGeom_getCoordSeq (geos_item);
		      GEOSCoordSeq_getDimensions (cs, &dims);
		      if (dims == 3)
			{
			    GEOSCoordSeq_getX (cs, 0, &x);
			    GEOSCoordSeq_getY (cs, 0, &y);
			    GEOSCoordSeq_getZ (cs, 0, &z);
			}
		      else
			{
			    GEOSCoordSeq_getX (cs, 0, &x);
			    GEOSCoordSeq_getY (cs, 0, &y);
			    z = 0.0;
			}
		      if (dimension_model == GAIA_XY_Z)
			  gaiaAddPointToGeomCollXYZ (gaia, x, y, z);
		      else if (dimension_model == GAIA_XY_M)
			  gaiaAddPointToGeomCollXYM (gaia, x, y, 0.0);
		      else if (dimension_model == GAIA_XY_Z_M)
			  gaiaAddPointToGeomCollXYZM (gaia, x, y, z, 0.0);
		      else
			  gaiaAddPointToGeomColl (gaia, x, y);
		      break;
		  case GEOS_LINESTRING:
		      cs = GEOSGeom_getCoordSeq (geos_item);
		      GEOSCoordSeq_getDimensions (cs, &dims);
		      GEOSCoordSeq_getSize (cs, &points);
		      ln = gaiaAddLinestringToGeomColl (gaia, points);
		      for (iv = 0; iv < (int) points; iv++)
			{
			    if (dims == 3)
			      {
				  GEOSCoordSeq_getX (cs, iv, &x);
				  GEOSCoordSeq_getY (cs, iv, &y);
				  GEOSCoordSeq_getZ (cs, iv, &z);
			      }
			    else
			      {
				  GEOSCoordSeq_getX (cs, iv, &x);
				  GEOSCoordSeq_getY (cs, iv, &y);
				  z = 0.0;
			      }
			    if (dimension_model == GAIA_XY_Z)
			      {
				  gaiaSetPointXYZ (ln->Coords, iv, x, y, z);
			      }
			    else if (dimension_model == GAIA_XY_M)
			      {
				  gaiaSetPointXYM (ln->Coords, iv, x, y, 0.0);
			      }
			    else if (dimension_model == GAIA_XY_Z_M)
			      {
				  gaiaSetPointXYZM (ln->Coords, iv, x, y, z,
						    0.0);
			      }
			    else
			      {
				  gaiaSetPoint (ln->Coords, iv, x, y);
			      }
			}
		      break;
		  case GEOS_MULTILINESTRING:
		      nSubItems = GEOSGetNumGeometries (geos_item);
		      for (sub_it = 0; sub_it < nSubItems; sub_it++)
			{
			    /* looping on elementaty geometries */
			    geos_sub_item =
				GEOSGetGeometryN (geos_item, sub_it);
			    cs = GEOSGeom_getCoordSeq (geos_sub_item);
			    GEOSCoordSeq_getDimensions (cs, &dims);
			    GEOSCoordSeq_getSize (cs, &points);
			    ln = gaiaAddLinestringToGeomColl (gaia, points);
			    for (iv = 0; iv < (int) points; iv++)
			      {
				  if (dims == 3)
				    {
					GEOSCoordSeq_getX (cs, iv, &x);
					GEOSCoordSeq_getY (cs, iv, &y);
					GEOSCoordSeq_getZ (cs, iv, &z);
				    }
				  else
				    {
					GEOSCoordSeq_getX (cs, iv, &x);
					GEOSCoordSeq_getY (cs, iv, &y);
					z = 0.0;
				    }
				  if (dimension_model == GAIA_XY_Z)
				    {
					gaiaSetPointXYZ (ln->Coords, iv, x, y,
							 z);
				    }
				  else if (dimension_model == GAIA_XY_M)
				    {
					gaiaSetPointXYM (ln->Coords, iv, x, y,
							 0.0);
				    }
				  else if (dimension_model == GAIA_XY_Z_M)
				    {
					gaiaSetPointXYZM (ln->Coords, iv, x, y,
							  z, 0.0);
				    }
				  else
				    {
					gaiaSetPoint (ln->Coords, iv, x, y);
				    }
			      }
			}
		      break;
		  case GEOS_POLYGON:
		      /* exterior ring */
		      holes = GEOSGetNumInteriorRings (geos_item);
		      geos_ring = GEOSGetExteriorRing (geos_item);
		      cs = GEOSGeom_getCoordSeq (geos_ring);
		      GEOSCoordSeq_getDimensions (cs, &dims);
		      GEOSCoordSeq_getSize (cs, &points);
		      pg = gaiaAddPolygonToGeomColl (gaia, points, holes);
		      rng = pg->Exterior;
		      for (iv = 0; iv < (int) points; iv++)
			{
			    if (dims == 3)
			      {
				  GEOSCoordSeq_getX (cs, iv, &x);
				  GEOSCoordSeq_getY (cs, iv, &y);
				  GEOSCoordSeq_getZ (cs, iv, &z);
			      }
			    else
			      {
				  GEOSCoordSeq_getX (cs, iv, &x);
				  GEOSCoordSeq_getY (cs, iv, &y);
				  z = 0.0;
			      }
			    if (dimension_model == GAIA_XY_Z)
			      {
				  gaiaSetPointXYZ (rng->Coords, iv, x, y, z);
			      }
			    else if (dimension_model == GAIA_XY_M)
			      {
				  gaiaSetPointXYM (rng->Coords, iv, x, y, 0.0);
			      }
			    else if (dimension_model == GAIA_XY_Z_M)
			      {
				  gaiaSetPointXYZM (rng->Coords, iv, x, y, z,
						    0.0);
			      }
			    else
			      {
				  gaiaSetPoint (rng->Coords, iv, x, y);
			      }
			}
		      for (ib = 0; ib < holes; ib++)
			{
			    /* interior rings */
			    geos_ring = GEOSGetInteriorRingN (geos_item, ib);
			    cs = GEOSGeom_getCoordSeq (geos_ring);
			    GEOSCoordSeq_getDimensions (cs, &dims);
			    GEOSCoordSeq_getSize (cs, &points);
			    rng = gaiaAddInteriorRing (pg, ib, points);
			    for (iv = 0; iv < (int) points; iv++)
			      {
				  if (dims == 3)
				    {
					GEOSCoordSeq_getX (cs, iv, &x);
					GEOSCoordSeq_getY (cs, iv, &y);
					GEOSCoordSeq_getZ (cs, iv, &z);
				    }
				  else
				    {
					GEOSCoordSeq_getX (cs, iv, &x);
					GEOSCoordSeq_getY (cs, iv, &y);
					z = 0.0;
				    }
				  if (dimension_model == GAIA_XY_Z)
				    {
					gaiaSetPointXYZ (rng->Coords, iv, x, y,
							 z);
				    }
				  else if (dimension_model == GAIA_XY_M)
				    {
					gaiaSetPointXYM (rng->Coords, iv, x, y,
							 0.0);
				    }
				  else if (dimension_model == GAIA_XY_Z_M)
				    {
					gaiaSetPointXYZM (rng->Coords, iv, x, y,
							  z, 0.0);
				    }
				  else
				    {
					gaiaSetPoint (rng->Coords, iv, x, y);
				    }
			      }
			}
		      break;
		  };
	    }
	  break;
      };
    return gaia;
}

GAIAGEO_DECLARE void *
gaiaToGeos (const gaiaGeomCollPtr gaia)
{
/* converting a GAIA Geometry into a GEOS Geometry */
    return toGeosGeometry (gaia, GAIA2GEOS_ALL);
}

GAIAGEO_DECLARE void *
gaiaToGeosSelective (const gaiaGeomCollPtr gaia, int mode)
{
/* converting a GAIA Geometry into a GEOS Geometry (selected type) */
    if (mode == GAIA2GEOS_ONLY_POINTS || mode == GAIA2GEOS_ONLY_LINESTRINGS
	|| mode == GAIA2GEOS_ONLY_POLYGONS)
	;
    else
	mode = GAIA2GEOS_ALL;
    return toGeosGeometry (gaia, mode);
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaFromGeos_XY (const void *xgeos)
{
/* converting a GEOS Geometry into a GAIA Geometry [XY] */
    const GEOSGeometry *geos = xgeos;
    return fromGeosGeometry (geos, GAIA_XY);
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaFromGeos_XYZ (const void *xgeos)
{
/* converting a GEOS Geometry into a GAIA Geometry [XYZ] */
    const GEOSGeometry *geos = xgeos;
    return fromGeosGeometry (geos, GAIA_XY_Z);
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaFromGeos_XYM (const void *xgeos)
{
/* converting a GEOS Geometry into a GAIA Geometry [XYM] */
    const GEOSGeometry *geos = xgeos;
    return fromGeosGeometry (geos, GAIA_XY_M);
}

GAIAGEO_DECLARE gaiaGeomCollPtr
gaiaFromGeos_XYZM (const void *xgeos)
{
/* converting a GEOS Geometry into a GAIA Geometry [XYZM] */
    const GEOSGeometry *geos = xgeos;
    return fromGeosGeometry (geos, GAIA_XY_Z_M);
}

#endif /* end including GEOS */
