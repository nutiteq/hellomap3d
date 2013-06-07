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
fnct_gpkgCreateTilesTable (sqlite3_context * context, int argc __attribute__ ((unused)),
			   sqlite3_value ** argv)
{
/* SQL function:
/ gpkgCreateTilesTable(table_name, srid)
/
/ Create a new (empty) Tiles table and the associated metadata table, and the triggers for those tables
/ It also adds in the matching entries into geopackage_contents, raster_columns and tile_table_metadata
/
/ TODO: consider adding description and identifier to geopackage_contents
/
/ returns nothing on success, raises exception on error
/ 
/ This function assumes usual tile conventions, including that the tiles are power-of-two-zoom.
/
*/
    const unsigned char *table;
    int srid = -1;
    char *sql_stmt = NULL;
    sqlite3 *sqlite = NULL;
    char *errMsg = NULL;
    int ret = 0;
    int i = 0;
    
    const char* tableSchemas[] = {
	"INSERT INTO geopackage_contents (table_name, data_type) VALUES (%Q, 'tiles')",
	
	"INSERT INTO tile_table_metadata VALUES (%Q, 1)",

	"CREATE TABLE %q (\n"
	"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
	"zoom_level INTEGER NOT NULL DEFAULT 0,\n"
	"tile_column INTEGER NOT NULL DEFAULT 0,\n"
	"tile_row INTEGER NOT NULL DEFAULT 0,\n"
	"tile_data BLOB NOT NULL,\n"
	"UNIQUE (zoom_level, tile_column, tile_row))",
	
	"CREATE TABLE %q_rt_metadata (\n"
	"row_id_value INTEGER NOT NULL,\n"
	"r_raster_column TEXT NOT NULL DEFAULT 'tile_data',\n"
	"georectification INTEGER NOT NULL DEFAULT 0,\n"
	"min_x DOUBLE NOT NULL DEFAULT -180.0,\n"
	"min_y DOUBLE NOT NULL DEFAULT -90.0,\n"
	"max_x DOUBLE NOT NULL DEFAULT 180.0,\n"
	"max_y DOUBLE NOT NULL DEFAULT 90.0,\n"
	"compr_qual_factor INTEGER NOT NULL DEFAULT 100,\n"
	"CONSTRAINT pk_smt_rm PRIMARY KEY (row_id_value, r_raster_column) ON CONFLICT ROLLBACK)",
	
	"SELECT gpkgAddTileTriggers(%Q)",
	
	"SELECT gpkgAddRtMetadataTriggers(%Q)",

	NULL
    };
    
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
    {
	sqlite3_result_error(context, "gpkgCreateTilesTable() error: argument 1 [table] is not of the String type", -1);
	return;
    }
    table = sqlite3_value_text (argv[0]);

    if (sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
    {
	sqlite3_result_error(context, "gpkgCreateTilesTable() error: argument 2 [srid] is not of the integer type", -1);
	return;
    }
    srid = sqlite3_value_int (argv[1]);

    sqlite = sqlite3_context_db_handle (context);

    sql_stmt = sqlite3_mprintf("INSERT INTO raster_columns (r_table_name, r_raster_column, georectification, srid) VALUES (%Q, 'tile_data', 1, %i)", table, srid);
    ret = sqlite3_exec (sqlite, sql_stmt, NULL, NULL, &errMsg);
    sqlite3_free(sql_stmt);
    if (ret != SQLITE_OK)
    {
	sqlite3_result_error(context, errMsg, -1);
	sqlite3_free(errMsg);
	return;
    }

    for (i = 0; tableSchemas[i] != NULL; ++i)
    {
	sql_stmt = sqlite3_mprintf(tableSchemas[i], table);    
	ret = sqlite3_exec (sqlite, sql_stmt, NULL, NULL, &errMsg);
	sqlite3_free(sql_stmt);
	if (ret != SQLITE_OK)
	{
	    sqlite3_result_error(context, errMsg, -1);
	    sqlite3_free(errMsg);
	    return;
	}
    }    
}
#endif
