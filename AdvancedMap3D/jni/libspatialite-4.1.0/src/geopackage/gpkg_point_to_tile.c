/*

    GeoPackage extensions for SpatiaLite / SQLite
 
Version: MPL 1.1/GPL 2.0/LGPL 2.1

The contents of this file are subject to the Mozilla Public License Version
1.1 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/
 
Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is GeoPackage Extensions

The Initial Developer of the Original Code is Brad Hards (bradh@frogmouth.net)
 
Portions created by the Initial Developer are Copyright (C) 2012
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

#include "spatialite/geopackage.h"
#include "config.h"
#include "geopackage_internal.h"

#ifdef ENABLE_GEOPACKAGE
GEOPACKAGE_DECLARE void
fnct_gpkgPointToTile (sqlite3_context * context, int argc UNUSED,
			sqlite3_value ** argv)
{
/* SQL function:
/ gpkgPointToTile (table, srid, x, y, zoom)
/
/ Returns tile from tile matrix for specified srid, point and zoom level
*/
    const unsigned char *table;
    int srid = 0;
    int target_srid = -1;
    double x_coord, y_coord;
    int int_value;
    int zoom;
    char *sql_stmt = NULL;
    sqlite3 *sqlite = NULL;
    sqlite3_stmt *stmt;
    int ret;
    
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: argument 1 [table] is not of the String type", -1);
	return;
    }
    table = sqlite3_value_text (argv[0]);
    
    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: argument 2 [srid] is not of the integer type", -1);
	return;
    }
    srid = sqlite3_value_int (argv[1]);
    if (sqlite3_value_type (argv[2]) == SQLITE_FLOAT)
    {
	x_coord = sqlite3_value_double (argv[2]);
    }
    else if (sqlite3_value_type (argv[2]) == SQLITE_INTEGER)
    {
	int_value = sqlite3_value_int (argv[2]);
	x_coord = int_value;
    }
    else
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: argument 3 [x coordinate] is not of a numerical type", -1);
	return;
    }

    if (sqlite3_value_type (argv[3]) == SQLITE_FLOAT)
    {
	y_coord = sqlite3_value_double (argv[3]);
    }
    else if (sqlite3_value_type (argv[3]) == SQLITE_INTEGER)
    {
	int_value = sqlite3_value_int (argv[3]);
	y_coord = int_value;
    }
    else
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: argument 4 [y coordinate] is not of a numerical type", -1);
	return;
    }
    
    if (sqlite3_value_type (argv[4]) != SQLITE_INTEGER)
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: argument 5 [zoom level] is not of the integer type", -1);
	return;
    }
    zoom = sqlite3_value_int (argv[4]);
    
    /* project into right coordinate basis if the input isn't already there */
    /* Get the target table SRID */
    sql_stmt = sqlite3_mprintf("SELECT srid FROM raster_columns WHERE r_table_name=%Q AND r_raster_column='tile_data'", table);

    sqlite = sqlite3_context_db_handle (context);
    ret = sqlite3_prepare_v2 (sqlite, sql_stmt, strlen(sql_stmt), &stmt, NULL);
    sqlite3_free(sql_stmt);
    if (ret != SQLITE_OK)
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: failed to prepare SQL SRID select statement", -1);
        return;
    }
    ret = sqlite3_step (stmt);
    if (ret != SQLITE_ROW)
    {
	sqlite3_finalize (stmt);
	sqlite3_result_error(context, "gpkgPointToTile() error: Could not find SRID for specified table", -1);
        return;
    }
    if (sqlite3_column_type (stmt, 0) != SQLITE_INTEGER)
    {
	sqlite3_finalize (stmt);
	sqlite3_result_error(context, "gpkgPointToTile() error: SRID for table is not an integer. Corrupt GeoPackage?", -1);
	return;
    }
    target_srid = sqlite3_column_int(stmt, 0);
    sqlite3_finalize (stmt);
    
    if (srid != target_srid)
    {
	/* project input coordinates */
	sql_stmt = sqlite3_mprintf("SELECT ST_X(projected),ST_Y(projected) FROM (SELECT Transform(MakePoint(%f, %f, %i), %i) AS projected)",
				   x_coord, y_coord, srid, target_srid);

	sqlite = sqlite3_context_db_handle (context);
	ret = sqlite3_prepare_v2 (sqlite, sql_stmt, strlen(sql_stmt), &stmt, NULL);
	sqlite3_free(sql_stmt);
	if (ret != SQLITE_OK)
	{
	    sqlite3_result_error(context, "gpkgPointToTile() error: failed to prepare SQL Transform statement", -1);
	    return;
	}
	ret = sqlite3_step (stmt);
	if (ret == SQLITE_ROW)
	{
	    if ((sqlite3_column_type (stmt, 0) == SQLITE_FLOAT) && (sqlite3_column_type (stmt, 1) == SQLITE_FLOAT))
	    {
		x_coord = sqlite3_column_double(stmt, 0);
		y_coord = sqlite3_column_double(stmt, 1);
	    }
	}
	ret = sqlite3_finalize (stmt);
    }
    
    /* now we can get the tile blob */
    sql_stmt = sqlite3_mprintf("SELECT tile_data FROM \"%q\",\"%s_rt_metadata\" WHERE %q.id=%s_rt_metadata.id AND zoom_level=%i AND min_x <= %g AND max_x >=%g AND min_y <= %g AND max_y >= %g",
			table, table, table, table, zoom, x_coord, x_coord, y_coord, y_coord);

    sqlite = sqlite3_context_db_handle (context);
    ret = sqlite3_prepare_v2 (sqlite, sql_stmt, strlen(sql_stmt), &stmt, NULL);
    sqlite3_free(sql_stmt);
    if (ret != SQLITE_OK)
    {
	sqlite3_result_error(context, "gpkgPointToTile() error: failed to prepare SQL statement", -1);
        return;
    }
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_ROW)
    {
	if (sqlite3_column_type (stmt, 0) == SQLITE_BLOB)
	{
	    sqlite3_result_value (context, sqlite3_column_value(stmt, 0));
	}
    }
    ret = sqlite3_finalize (stmt);
}
#endif
