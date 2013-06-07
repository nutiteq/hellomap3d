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
fnct_gpkgAddRtMetadataTriggers (sqlite3_context * context, int argc UNUSED,
			sqlite3_value ** argv)
{
/* SQL function:
/ gpkgAddRtMetadataTriggers(table)
/
/ Adds Geopackage table triggers for the metadata table associated with the 
/ named table. NOTE: pass the data table name, not the metadata table name.
/
/ returns nothing on success, raises exception on error
*/
    const unsigned char *table;
    char *sql_stmt = NULL;
    sqlite3 *sqlite = NULL;
    char *errMsg = NULL;
    int ret = 0;
    int i = 0;
    /* Note: the code below relies on there being five (or less) varargs, all of which are the table name */
    const char* trigger_stmts[] = {
	"CREATE TRIGGER '%q_rt_metadata_r_raster_column_insert'\n"
	"BEFORE INSERT ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'insert on table ''%q_rt_metadata'' violates constraint: r_raster_column must be specified for table %s in table raster_columns')\n"
	"WHERE (NOT (NEW.r_raster_column IN (SELECT DISTINCT r_raster_column FROM raster_columns WHERE r_table_name = '%s')));\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_r_raster_column_update'\n"
	"BEFORE UPDATE OF r_raster_column ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'update on table ''%q_rt_metadata'' violates constraint: r_raster_column must be specified for table %s in table raster_columns')\n"
	"WHERE (NOT (NEW.r_raster_column IN (SELECT DISTINCT r_raster_column FROM raster_columns WHERE r_table_name = '%s')));\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_georectification_insert'\n"
	"BEFORE INSERT ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'insert on table ''%q_rt_metadata'' violates constraint: georectification must be 0, 1 or 2')\n"
	"WHERE (NOT (NEW.georectification IN (0, 1, 2)));\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_georectification_update'\n"
	"BEFORE UPDATE OF georectification ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'update on table ''%q_rt_metadata'' violates constraint: georectification must be 0, 1 or 2')\n"
	"WHERE (NOT (NEW.georectification IN (0, 1, 2)));\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_compr_qual_factor_insert'\n"
	"BEFORE INSERT ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''%q_rt_metadata'' violates constraint: compr_qual_factor < 1, must be between 1 and 100')\n"
	"WHERE NEW.compr_qual_factor < 1;\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''%q_rt_metadata'' violates constraint: compr_qual_factor > 100, must be between 1 and 100')\n"
	"WHERE NEW.compr_qual_factor > 100;\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_compr_qual_factor_update'\n"
	"BEFORE UPDATE OF compr_qual_factor ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''%q_rt_metadata'' violates constraint: compr_qual_factor < 1, must be between 1 and 100')\n"
	"WHERE NEW.compr_qual_factor < 1;\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''%q_rt_metadata'' violates constraint: compr_qual_factor > 100, must be between 1 and 100')\n"
	"WHERE NEW.compr_qual_factor > 100;\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_row_id_value_insert'\n"
	"BEFORE INSERT ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table %q_rt_metadata violates constraint: row_id_value must exist in %q table')\n"
	"WHERE NOT EXISTS (SELECT rowid FROM '%q' WHERE rowid = NEW.row_id_value);\n"
	"END",

	"CREATE TRIGGER '%q_rt_metadata_row_id_value_update'\n"
	"BEFORE UPDATE OF 'row_id_value' ON '%q_rt_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table %q_rt_metadata violates constraint: row_id_value must exist in %qtable')\n"
	"WHERE NOT EXISTS (SELECT rowid FROM '%q' WHERE rowid = NEW.row_id_value);\n"
	"END",

	NULL
    };
    
    if (sqlite3_value_type (argv[0]) != SQLITE_TEXT)
    {
	sqlite3_result_error(context, "gpkgAddRtMetadataTriggers() error: argument 1 [table] is not of the String type", -1);
	return;
    }
    table = sqlite3_value_text (argv[0]);

    for (i = 0; trigger_stmts[i] != NULL; ++i)
    {
	sql_stmt = sqlite3_mprintf(trigger_stmts[i], table, table, table, table, table);    
	sqlite = sqlite3_context_db_handle (context);
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
