/*

 srs_init.c -- populating the SPATIAL_REF_SYS table

 version 4.1, 2013 May 8

 Author: Sandro Furieri a.furieri@lqt.it

 -----------------------------------------------------------------------------
 
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <spatialite/sqlite.h>
#include <spatialite/debug.h>

#ifdef _WIN32
#define strcasecmp	_stricmp
#endif /* not WIN32 */

#include <spatialite.h>
#include <spatialite_private.h>

static void
free_epsg_def (struct epsg_defs *ptr)
{
/* memory cleanup - destroying an EPSG def item */
    if (ptr->auth_name)
	free (ptr->auth_name);
    if (ptr->ref_sys_name)
	free (ptr->ref_sys_name);
    if (ptr->proj4text)
	free (ptr->proj4text);
    if (ptr->srs_wkt)
	free (ptr->srs_wkt);
    free (ptr);
}

SPATIALITE_PRIVATE struct epsg_defs *
add_epsg_def (int filter_srid, struct epsg_defs **first,
	      struct epsg_defs **last, int srid, const char *auth_name,
	      int auth_srid, const char *ref_sys_name)
{
/* appending an EPSG def to the list */
    int len;
    struct epsg_defs *p;
    if (filter_srid == GAIA_EPSG_NONE)
	return NULL;
    if (filter_srid == GAIA_EPSG_ANY || filter_srid == GAIA_EPSG_WGS84_ONLY)
	;
    else if (srid != filter_srid)
	return NULL;
    p = malloc (sizeof (struct epsg_defs));
    if (!p)
	return NULL;
    p->srid = srid;
    p->auth_name = NULL;
    p->auth_srid = auth_srid;
    p->ref_sys_name = NULL;
    p->proj4text = NULL;
    p->srs_wkt = NULL;
    p->next = NULL;
    if (auth_name)
      {
	  len = strlen (auth_name);
	  if (len > 0)
	    {
		p->auth_name = malloc (len + 1);
		if (p->auth_name == NULL)
		    goto error;
		strcpy (p->auth_name, auth_name);
	    }
      }
    if (ref_sys_name)
      {
	  len = strlen (ref_sys_name);
	  if (len > 0)
	    {
		p->ref_sys_name = malloc (len + 1);
		if (p->ref_sys_name == NULL)
		    goto error;
		strcpy (p->ref_sys_name, ref_sys_name);
	    }
      }
    if (*first == NULL)
	*first = p;
    if (*last != NULL)
	(*last)->next = p;
    *last = p;
    return p;
  error:
    free_epsg_def (p);
    return NULL;
}

SPATIALITE_PRIVATE void
add_proj4text (struct epsg_defs *p, int count, const char *text)
{
/* creating the PROJ4TEXT string */
    int len;
    int olen;
    char *string;
    if (p == NULL || text == NULL)
	return;
    len = strlen (text);
    if (!count)
      {
	  p->proj4text = malloc (len + 1);
	  if (p->proj4text == NULL)
	      return;
	  strcpy (p->proj4text, text);
	  return;
      }
    if (p->proj4text == NULL)
	return;
    olen = strlen (p->proj4text);
    string = malloc (len + olen + 1);
    if (string == NULL)
	return;
    strcpy (string, p->proj4text);
    free (p->proj4text);
    p->proj4text = string;
    strcat (p->proj4text, text);
}

SPATIALITE_PRIVATE void
add_srs_wkt (struct epsg_defs *p, int count, const char *text)
{
/* creating the SRS_WKT string */
    int len;
    int olen;
    char *string;
    if (p == NULL || text == NULL)
	return;
    len = strlen (text);
    if (!count)
      {
	  p->srs_wkt = malloc (len + 1);
	  if (p->srs_wkt == NULL)
	      return;
	  strcpy (p->srs_wkt, text);
	  return;
      }
    if (p->srs_wkt == NULL)
	return;
    olen = strlen (p->srs_wkt);
    string = malloc (len + olen + 1);
    if (string == NULL)
	return;
    strcpy (string, p->srs_wkt);
    free (p->srs_wkt);
    p->srs_wkt = string;
    strcat (p->srs_wkt, text);
}

static void
free_epsg (struct epsg_defs *first)
{
/* memory cleanup - destroying the EPSG list */
    struct epsg_defs *p = first;
    struct epsg_defs *pn;
    while (p)
      {
	  pn = p->next;
	  free_epsg_def (p);
	  p = pn;
      }
}

static int
populate_spatial_ref_sys (sqlite3 * handle, int mode)
{
/* populating the EPSG dataset into the SPATIAL_REF_SYS table */
    struct epsg_defs *first = NULL;
    struct epsg_defs *last = NULL;
    struct epsg_defs *p;
    char sql[1024];
    int ret;
    sqlite3_stmt *stmt;

/* initializing the EPSG defs list */
    initialize_epsg (mode, &first, &last);

/* preparing the SQL parameterized statement */
    strcpy (sql, "INSERT INTO spatial_ref_sys ");
    strcat (sql,
	    "(srid, auth_name, auth_srid, ref_sys_name, proj4text, srtext) ");
    strcat (sql, "VALUES (?, ?, ?, ?, ?, ?)");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("%s\n", sqlite3_errmsg (handle));
	  goto error;
      }
    p = first;
    while (p)
      {
	  if (p->auth_name == NULL)
	      break;
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_int (stmt, 1, p->srid);
	  sqlite3_bind_text (stmt, 2, p->auth_name, strlen (p->auth_name),
			     SQLITE_STATIC);
	  sqlite3_bind_int (stmt, 3, p->auth_srid);
	  sqlite3_bind_text (stmt, 4, p->ref_sys_name, strlen (p->ref_sys_name),
			     SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 5, p->proj4text, strlen (p->proj4text),
			     SQLITE_STATIC);
	  if (strlen (p->srs_wkt) == 0)
	      sqlite3_bind_text (stmt, 6, "Undefined", 9, SQLITE_STATIC);
	  else
	      sqlite3_bind_text (stmt, 6, p->srs_wkt, strlen (p->srs_wkt),
				 SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		spatialite_e ("%s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto error;
	    }
	  p = p->next;
      }
    sqlite3_finalize (stmt);

/* freeing the EPSG defs list */
    free_epsg (first);

    return 1;
  error:

/* freeing the EPSG defs list */
    free_epsg (first);

    return 0;
}

static int
exists_spatial_ref_sys (sqlite3 * handle)
{
/* checking if the SPATIAL_REF_SYS table exists */
    int ret;
    int ok = 0;
    char sql[1024];
    char **results;
    int n_rows;
    int n_columns;
    char *err_msg = NULL;

    strcpy (sql,
	    "SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spatial_ref_sys'");
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  spatialite_e ("XX %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    if (n_rows > 0)
	ok = 1;
    sqlite3_free_table (results);
    return ok;
}

static int
check_spatial_ref_sys (sqlite3 * handle)
{
/* checking if the SPATIAL_REF_SYS table has an appropriate layout */
    int ret;
    int i;
    const char *name;
    char sql[1024];
    char **results;
    int n_rows;
    int n_columns;
    char *err_msg = NULL;
    int rs_srid = 0;
    int auth_name = 0;
    int auth_srid = 0;
    int ref_sys_name = 0;
    int proj4text = 0;
    int srtext = 0;

    strcpy (sql, "PRAGMA table_info(spatial_ref_sys)");
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  spatialite_e ("%s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    if (n_rows > 0)
      {
	  for (i = 1; i <= n_rows; i++)
	    {
		name = results[(i * n_columns) + 1];
		if (strcasecmp (name, "srid") == 0)
		    rs_srid = 1;
		if (strcasecmp (name, "auth_name") == 0)
		    auth_name = 1;
		if (strcasecmp (name, "auth_srid") == 0)
		    auth_srid = 1;
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
	ret = 1;
    else
	ret = 0;
    return ret;
}

static int
spatial_ref_sys_count (sqlite3 * handle)
{
/* checking if the SPATIAL_REF_SYS table is empty */
    int ret;
    int i;
    int count = 0;
    char sql[1024];
    char **results;
    int n_rows;
    int n_columns;
    char *err_msg = NULL;

    strcpy (sql, "SELECT Count(*) FROM spatial_ref_sys");
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  spatialite_e ("%s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    if (n_rows > 0)
      {
	  for (i = 1; i <= n_rows; i++)
	    {
		count = atoi (results[(i * n_columns) + 0]);
	    }
      }
    sqlite3_free_table (results);
    return count;
}

SPATIALITE_DECLARE int
spatial_ref_sys_init (sqlite3 * handle, int verbose)
{
/* 
/ deprecated function 
/ [still supported simply not to break API-level back-compatibility] 
*/
    return spatial_ref_sys_init2 (handle, GAIA_EPSG_ANY, verbose);
}

SPATIALITE_DECLARE int
spatial_ref_sys_init2 (sqlite3 * handle, int mode, int verbose)
{
/* populating the EPSG dataset into the SPATIAL_REF_SYS table */
    if (!exists_spatial_ref_sys (handle))
      {
	  if (verbose)
	      spatialite_e ("the SPATIAL_REF_SYS table doesn't exists\n");
	  return 0;
      }
    if (!check_spatial_ref_sys (handle))
      {
	  if (verbose)
	      spatialite_e
		  ("the SPATIAL_REF_SYS table has an unsupported layout\n");
	  return 0;
      }
    if (spatial_ref_sys_count (handle))
      {
	  if (verbose)
	      spatialite_e
		  ("the SPATIAL_REF_SYS table already contains some row(s)\n");
	  return 0;
      }
    if (mode == GAIA_EPSG_ANY || mode == GAIA_EPSG_NONE
	|| mode == GAIA_EPSG_WGS84_ONLY)
	;
    else
	mode = GAIA_EPSG_ANY;
    if (populate_spatial_ref_sys (handle, mode))
      {
	  if (verbose && mode != GAIA_EPSG_NONE)
	      spatialite_e
		  ("OK: the SPATIAL_REF_SYS table was successfully populated\n");
	  return 1;
      }
    return 0;
}

SPATIALITE_DECLARE int
insert_epsg_srid (sqlite3 * handle, int srid)
{
/* inserting a single EPSG definition into the SPATIAL_REF_SYS table */
    struct epsg_defs *first = NULL;
    struct epsg_defs *last = NULL;
    char sql[1024];
    int ret;
    int error = 0;
    sqlite3_stmt *stmt;

    if (!exists_spatial_ref_sys (handle))
      {
	  spatialite_e ("the SPATIAL_REF_SYS table doesn't exists\n");
	  return 0;
      }
    if (!check_spatial_ref_sys (handle))
      {
	  spatialite_e
	      ("the SPATIAL_REF_SYS table has an unsupported layout\n");
	  return 0;
      }

/* initializing the EPSG defs list */
    initialize_epsg (srid, &first, &last);
    if (first == NULL)
      {
	  spatialite_e ("SRID=%d isn't defined in the EPSG inlined dataset\n",
			srid);
	  return 0;
      }

/* preparing the SQL parameterized statement */
    strcpy (sql, "INSERT INTO spatial_ref_sys ");
    strcat (sql,
	    "(srid, auth_name, auth_srid, ref_sys_name, proj4text, srtext) ");
    strcat (sql, "VALUES (?, ?, ?, ?, ?, ?)");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  spatialite_e ("%s\n", sqlite3_errmsg (handle));
	  error = 1;
	  goto stop;
      }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_int (stmt, 1, first->srid);
    sqlite3_bind_text (stmt, 2, first->auth_name, strlen (first->auth_name),
		       SQLITE_STATIC);
    sqlite3_bind_int (stmt, 3, first->auth_srid);
    sqlite3_bind_text (stmt, 4, first->ref_sys_name,
		       strlen (first->ref_sys_name), SQLITE_STATIC);
    sqlite3_bind_text (stmt, 5, first->proj4text, strlen (first->proj4text),
		       SQLITE_STATIC);
    if (strlen (first->srs_wkt) == 0)
	sqlite3_bind_text (stmt, 6, "Undefined", 9, SQLITE_STATIC);
    else
	sqlite3_bind_text (stmt, 6, first->srs_wkt, strlen (first->srs_wkt),
			   SQLITE_STATIC);
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  spatialite_e ("%s\n", sqlite3_errmsg (handle));
	  error = 1;
	  goto stop;
      }
  stop:
    if (stmt != NULL)
	sqlite3_finalize (stmt);

/* freeing the EPSG defs list */
    free_epsg (first);
    if (error)
	return 0;
    return 1;
}
