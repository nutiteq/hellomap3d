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
fnct_gpkgCreateBaseTables (sqlite3_context * context, int argc __attribute__ ((unused)),
			   sqlite3_value ** argv)
{
/* SQL function:
/ gpkgCreateBaseTables()
/
/ Create base tables for an "empty" GeoPackage
/ returns nothing on success, raises exception on error
/
*/
    char *sql_stmt = NULL;
    sqlite3 *sqlite = NULL;
    char *errMsg = NULL;
    int ret = 0;
    int i = 0;
    
    const char* tableSchemas[] = {
	/* GeoPackage specification Table 2 */
	"CREATE TABLE geopackage_contents (\n"
	"table_name TEXT NOT NULL PRIMARY KEY,\n"
	"data_type TEXT NOT NULL,\n"
	"identifier TEXT NOT NULL DEFAULT '',\n"
	"description TEXT NOT NULL DEFAULT '',\n"
	"last_change TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ',CURRENT_TIMESTAMP)),\n"
	"min_x DOUBLE NOT NULL DEFAULT -180.0,\n"
	"min_y DOUBLE NOT NULL DEFAULT -90.0,\n"
	"max_x DOUBLE NOT NULL DEFAULT 180.0,\n"
	"max_y DOUBLE NOT NULL DEFAULT 90.0,\n"
	"srid INTEGER NOT NULL DEFAULT 0,\n"
	"CONSTRAINT fk_gc_r_srid FOREIGN KEY (srid) REFERENCES spatial_ref_sys(srid));",
	
	/* GeoPackage specification Table 23/24 */
	"CREATE TABLE raster_columns (\n"
	"r_table_name TEXT NOT NULL,\n"
	"r_raster_column TEXT NOT NULL,\n"
	"compr_qual_factor INTEGER NOT NULL DEFAULT 100,\n"
	"georectification INTEGER NOT NULL DEFAULT 0,\n"
	"srid INTEGER NOT NULL DEFAULT 0,\n"
	"CONSTRAINT pk_rc PRIMARY KEY (r_table_name, r_raster_column) ON CONFLICT ROLLBACK,\n"
	"CONSTRAINT fk_rc_r_srid FOREIGN KEY (srid) REFERENCES spatial_ref_sys(srid),"
	"CONSTRAINT fk_rc_r_gc FOREIGN KEY (r_table_name) REFERENCES geopackage_contents(table_name));",

	/* The next four triggers are from GeoPackage specification Table 21 */
	"CREATE TRIGGER 'raster_columns_r_table_name_insert'\n"
	"BEFORE INSERT ON 'raster_columns'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'insert on raster_columns violates constraint: r_table_name value must not contain a single quote')\n"
	"WHERE NEW.r_table_name LIKE ('%''''%');\n"
	"SELECT RAISE(ABORT, 'insert on raster_columns violates constraint: r_table_name value must not contain a double quote')\n"
	"WHERE NEW.r_table_name LIKE ('%\"%');\n"
	"SELECT RAISE(ABORT, 'insert on raster_columns violates constraint: r_table_name value must be lower case')\n"
	"WHERE NEW.r_table_name <> lower(NEW.r_table_name);\n"
	"END;",

	"CREATE TRIGGER 'raster_columns_r_table_name_update'\n"
	"BEFORE UPDATE OF 'r_table_name' ON 'raster_columns'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT,'update on raster_columns violates constraint: r_table_name value must not contain a single quote')\n"
	"WHERE NEW.r_table_name LIKE ('%''''%');\n"
	"SELECT RAISE(ABORT,'update on raster_columns violates constraint: r_table_name value must not contain a double quote')\n"
	"WHERE NEW.r_table_name LIKE ('%\"%');\n"
	"SELECT RAISE(ABORT,'update on raster_columns violates constraint: r_table_name value must be lower case')\n"
	"WHERE NEW.r_table_name <> lower(NEW.r_table_name);\n"
	"END;",

	"CREATE TRIGGER 'raster_columns_r_raster_column_insert'\n"
	"BEFORE INSERT ON 'raster_columns'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT,'insert on raster_columns violates constraint: r_raster_column value must not contain a single quote')\n"
	"WHERE NEW.r_raster_column LIKE ('%''''%');\n"
	"SELECT RAISE(ABORT,'insert on raster_columns violates constraint: r_raster_column value must not contain a double quote')\n"
	"WHERE NEW.r_raster_column LIKE ('%\"%');\n"
	"SELECT RAISE(ABORT,'insert on raster_columns violates constraint: r_raster_column value must be lower case')\n"
	"WHERE NEW.r_raster_column <> lower(NEW.r_raster_column);\n"
	"END;",

	"CREATE TRIGGER 'raster_columns_r_raster_column_update'\n"
	"BEFORE UPDATE OF r_raster_column ON 'raster_columns'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT,'update on raster_columns violates constraint: r_raster_column value must not contain a single quote')\n"
	"WHERE NEW.r_raster_column LIKE ('%''''%');\n"
	"SELECT RAISE(ABORT,'update on raster_columns violates constraint: r_raster_column value must not contain a double quote')\n"
	"WHERE NEW.r_raster_column LIKE ('%\"%');\n"
	"SELECT RAISE(ABORT,'update on raster_columns violates constraint: r_raster_column value must be lower case')\n"
	"WHERE NEW.r_raster_column <> lower(NEW.r_raster_column);\n"
	"END;",

	/* GeoPackage specification Table 23/24 */
	/* TODO: see if there is a nicer way to manage this using a VIEW */
	"CREATE TABLE tile_table_metadata (\n"
	"t_table_name TEXT NOT NULL PRIMARY KEY,\n"
	"is_times_two_zoom INTEGER NOT NULL DEFAULT 1\n"
	");",
	
	/* The next four triggers are from GeoPackage specification Table 25 */
	"CREATE TRIGGER 'tile_table_metadata_t_table_name_insert'\n"
	"BEFORE INSERT ON 'tile_table_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''tile_table_metadata'' violates constraint: t_table_name not in raster_columns.r_table_name values')\n"
	"WHERE NOT (NEW.t_table_name IN (SELECT DISTINCT r_table_name FROM raster_columns));\n"
	"END;",

	"CREATE TRIGGER 'tile_table_metadata_t_table_name_update'\n"
	"BEFORE UPDATE OF t_table_name ON 'tile_table_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''tile_table_metadata'' violates constraint: t_table_name not in raster_columns.r_table_name values')\n"
	"WHERE NOT (NEW.t_table_name IN (SELECT DISTINCT r_table_name FROM raster_columns));\n"
	"END;",

	"CREATE TRIGGER 'tile_table_metadata_is_times_two_zoom_insert'\n"
	"BEFORE INSERT ON 'tile_table_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'insert on tile_table_metadata violates constraint: is_time_two_zoom must be one of 0|1')\n"
	"WHERE NOT(NEW.is_times_two_zoom IN (0,1));\n"
	"END;",

	"CREATE TRIGGER 'tile_table_metadata_is_times_two_zoom_update'\n"
	"BEFORE UPDATE OF is_times_two_zoom ON 'tile_table_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ABORT, 'update of tile_table_metadata violates constraint: is_time_two_zoom must be one of 0|1')\n"
	"WHERE NOT(NEW.is_times_two_zoom IN (0,1));\n"
	"END;",

	/* GeoPackage specification Table 27/28 */
	"CREATE TABLE tile_matrix_metadata (\n"
	"t_table_name TEXT NOT NULL,\n"
	"zoom_level INTEGER NOT NULL,\n"
	"matrix_width INTEGER NOT NULL,\n"
	"matrix_height INTEGER NOT NULL,\n"
	"tile_width INTEGER NOT NULL,\n"
	"tile_height INTEGER NOT NULL,\n"
	"pixel_x_size DOUBLE NOT NULL,\n"
	"pixel_y_size DOUBLE NOT NULL,\n"
	"CONSTRAINT pk_ttm PRIMARY KEY (t_table_name, zoom_level) ON CONFLICT ROLLBACK,\n"
	"CONSTRAINT fk_ttm_t_table_name FOREIGN KEY (t_table_name) REFERENCES tile_table_metadata(t_table_name));",

	/* The next ten triggers are from GeoPackage specification Table 25 */
	"CREATE TRIGGER 'tile_matrix_metadata_zoom_level_insert'\n"
	"BEFORE INSERT ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''tile_matrix_metadata'' violates constraint: zoom_level cannot be less than 0')\n"
	"WHERE (NEW.zoom_level < 0);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_zoom_level_update'\n"
	"BEFORE UPDATE of zoom_level ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''tile_matrix_metadata'' violates constraint: zoom_level cannot be less than 0')\n"
	"WHERE (NEW.zoom_level < 0);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_matrix_width_insert'\n"
	"BEFORE INSERT ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''tile_matrix_metadata'' violates constraint: matrix_width cannot be less than 1')\n"
	"WHERE (NEW.matrix_width < 1);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_matrix_width_update'\n"
	"BEFORE UPDATE OF matrix_width ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''tile_matrix_metadata'' violates constraint: matrix_width cannot be less than 1')\n"
	"WHERE (NEW.matrix_width < 1);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_matrix_height_insert'\n"
	"BEFORE INSERT ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''tile_matrix_metadata'' violates constraint: matrix_height cannot be less than 1')\n"
	"WHERE (NEW.matrix_height < 1);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_matrix_height_update'\n"
	"BEFORE UPDATE OF matrix_height ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''tile_matrix_metadata'' violates constraint: matrix_height cannot be less than 1')\n"
	"WHERE (NEW.matrix_height < 1);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_pixel_x_size_insert'\n"
	"BEFORE INSERT ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''tile_matrix_metadata'' violates constraint: pixel_x_size must be greater than 0')\n"
	"WHERE NOT (NEW.pixel_x_size > 0);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_pixel_x_size_update'\n"
	"BEFORE UPDATE OF pixel_x_size ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''tile_matrix_metadata'' violates constraint: pixel_x_size must be greater than 0')\n"
	"WHERE NOT (NEW.pixel_x_size > 0);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_pixel_y_size_insert'\n"
	"BEFORE INSERT ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table ''tile_matrix_metadata'' violates constraint: pixel_y_size must be greater than 0')\n"
	"WHERE NOT (NEW.pixel_y_size > 0);\n"
	"END;",

	"CREATE TRIGGER 'tile_matrix_metadata_pixel_y_size_update'\n"
	"BEFORE UPDATE OF pixel_y_size ON 'tile_matrix_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table ''tile_matrix_metadata'' violates constraint: pixel_y_size must be greater than 0')\n"
	"WHERE NOT (NEW.pixel_y_size > 0);\n"
	"END;",

	/* GeoPackage specification Table 43/45 */
	"CREATE TABLE xml_metadata (\n"
	"id INTEGER CONSTRAINT xm_pk PRIMARY KEY ASC ON CONFLICT ROLLBACK AUTOINCREMENT NOT NULL UNIQUE,\n"
	"md_scope TEXT NOT NULL DEFAULT 'dataset',\n"
	"metadata_standard_URI TEXT NOT NULL DEFAULT 'http://schemas.opengis.net/iso/19139/',\n"
	"metadata BLOB NOT NULL DEFAULT (zeroblob(4))\n"
	");",

	/* The next two triggers are from GeoPackage Table 46 */
	"CREATE TRIGGER 'xml_metadata_md_scope_insert'\n"
	"BEFORE INSERT ON 'xml_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table xml_metadata violates constraint: md_scope must be one of undefined | fieldSession | collectionSession | series | dataset | featureType | feature | attributeType | attribute | tile | model | catalogue | schema | taxonomy  software | service | collectionHardware | nonGeographicDataset | dimensionGroup')\n"
	"WHERE NOT(NEW.md_scope IN ('undefined','fieldSession','collectionSession','series','dataset','featureType', 'feature','attributeType','attribute','tile','model','catalogue','schema','taxonomy', 'software','service','collectionHardware','nonGeographicDataset','dimensionGroup'));\n"
	"END;",

	"CREATE TRIGGER 'xml_metadata_md_scope_update'\n"
	"BEFORE UPDATE OF 'md_scope' ON 'xml_metadata'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table xml_metadata violates constraint: md_scope must be one of undefined | fieldSession | collectionSession | series | dataset | featureType | feature | attributeType | attribute | tile | model | catalogue | schema | taxonomy  software | service | collectionHardware | nonGeographicDataset | dimensionGroup')\n"
	"WHERE NOT(NEW.md_scope IN ('undefined','fieldSession','collectionSession','series','dataset','featureType', 'feature','attributeType','attribute','tile','model','catalogue','schema','taxonomy', 'software','service','collectionHardware','nonGeographicDataset','dimensionGroup'));\n"
	"END;",
	
	/* GeoPackage Table 47 */
	"INSERT INTO xml_metadata VALUES (0, 'undefined', 'http://schemas.opengis.net/iso/19139/', (zeroblob(4)));",
	
	/* GeoPackage specification Table 48/49 */
	"CREATE TABLE metadata_reference ("
	"reference_scope TEXT NOT NULL DEFAULT \"table\","  
	"table_name TEXT NOT NULL DEFAULT \"undefined\","
	"column_name TEXT NOT NULL DEFAULT \"undefined\","
	"row_id_value INTEGER NOT NULL DEFAULT 0,"
	"timestamp TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ',CURRENT_TIMESTAMP)),"
	"md_file_id INTEGER NOT NULL DEFAULT 0,"
	"md_parent_id INTEGER NOT NULL DEFAULT 0,"
	"CONSTRAINT crmr_mfi_fk FOREIGN KEY (md_file_id) REFERENCES xml_metadata(id),"
	"CONSTRAINT crmr_mpi_fk FOREIGN KEY (md_parent_id) REFERENCES xml_metadata(id)"
	");",

	/* The next 10 triggers are from GeoPackage specification Table 50 */
	"CREATE TRIGGER 'metadata_reference_reference_scope_insert'\n"
	"BEFORE INSERT ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: reference_scope must be one of \"table\", \"column\", \"row\", \"row/col\"')\n"
	"WHERE NOT NEW.reference_scope IN ('table','column','row','row/col');\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_reference_scope_update'\n"
	"BEFORE UPDATE OF 'reference_scope' ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: referrence_scope must be one of \"table\", \"column\", \"row\", \"row/col\"')\n"
	"WHERE NOT NEW.reference_scope IN ('table','column','row','row/col');\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_table_name_insert'\n"
	"BEFORE INSERT ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: table_name must be the name of a table in geometry_columns or raster_columns')\n"
	"WHERE NOT NEW.table_name IN (\n"
	"SELECT f_table_name AS table_name FROM geometry_columns\n"
	"UNION ALL\n"
	"SELECT r_table_name AS table_name FROM raster_columns);\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_table_name_update'\n"
	"BEFORE UPDATE OF 'table_name' ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: table_name must be the name of a table in geometry_columns or 	raster_columns')\n"
	"WHERE NOT NEW.table_name IN (\n"
	"SELECT f_table_name AS table_name FROM geometry_columns\n"
	"UNION ALL\n"
	"SELECT r_table_name AS table_name FROM raster_columns);\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_column_name_insert'\n"
	"BEFORE INSERT ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: column name must be \"undefined\" when reference_scope is \"table\" or \"row\"')\n"
	"WHERE (NEW.reference_scope IN ('table','row')\n"
	"AND NEW.column_name <> 'undefined');\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: column name must be defined for the specified table when reference_scope is \"column\" or \"row/col\"')\n"
	"WHERE (NEW.reference_scope IN ('column','row/col')\n"
	"AND NOT NEW.table_name IN (\n"
	"SELECT name FROM SQLITE_MASTER WHERE type = 'table'\n"
	"AND name = NEW.table_name\n"
	"AND sql LIKE ('%' || NEW.column_name || '%')));\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_column_name_update'\n"
	"BEFORE UPDATE OF column_name ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: column name must be \"undefined\" when reference_scope is 	\"table\" or \"row\"')\n"
	"WHERE (NEW.reference_scope IN ('table','row')\n"
	"AND NEW.column_name <> 'undefined');\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: column name must be defined for the specified table when reference_scope is \"column\" or \"row/col\"')\n"
	"WHERE (NEW.reference_scope IN ('column','row/col')\n"
	"AND NOT NEW.table_name IN (\n"
	"SELECT name FROM SQLITE_MASTER WHERE type = 'table'\n" 
	"AND name = NEW.table_name\n"
	"AND sql LIKE ('%' || NEW.column_name || '%')));\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_row_id_value_insert'\n"
	"BEFORE INSERT ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: row_id_value must be 0 when reference_scope is \"table\" or \"column\"')\n"
	"WHERE NEW.reference_scope IN ('table','column')\n"
	"AND NEW.row_id_value <> 0;\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: row_id_value must exist in specified table when reference_scope is \"row\" or \"row/col\"')\n"
	"WHERE NEW.reference_scope IN ('row','row/col')\n"
	"AND NOT EXISTS (SELECT rowid\n"
	"FROM (SELECT NEW.table_name AS table_name) WHERE rowid = NEW.row_id_value);\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_row_id_value_update'\n"
	"BEFORE UPDATE OF 'row_id_value' ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: row_id_value must be 0 when reference_scope is \"table\" or \"column\"')\n"
	"WHERE NEW.reference_scope IN ('table','column')\n"
	"AND NEW.row_id_value <> 0;\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: row_id_value must exist in specified table when reference_scope is \"row\" or \"row/col\"')\n"
	"WHERE NEW.reference_scope IN ('row','row/col')\n"
	"AND NOT EXISTS (SELECT rowid\n"
	"FROM (SELECT NEW.table_name AS table_name) WHERE rowid = NEW.row_id_value);\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_timestamp_insert'\n"
	"BEFORE INSERT ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'insert on table metadata_reference violates constraint: timestamp must be a valid time in ISO 8601 \"yyyy-mm-ddThh-mm-ss.cccZ\" form')\n"
	"WHERE NOT (NEW.timestamp GLOB '[1-2][0-9][0-9][0-9]-[0-1][0-9]-[1-3][1-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z'\n"
	"AND strftime('%s',NEW.timestamp) NOT NULL);\n"
	"END;",

	"CREATE TRIGGER 'metadata_reference_timestamp_update'\n"
	"BEFORE UPDATE OF 'timestamp' ON 'metadata_reference'\n"
	"FOR EACH ROW BEGIN\n"
	"SELECT RAISE(ROLLBACK, 'update on table metadata_reference violates constraint: timestamp must be a valid time in ISO 8601 \"yyyy-mm-ddThh-mm-ss.cccZ\" form')\n"
	"WHERE NOT (NEW.timestamp GLOB '[1-2][0-9][0-9][0-9]-[0-1][0-9]-[1-3][1-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z'\n"
	"AND strftime('%s',NEW.timestamp) NOT NULL);\n"
	"END;",
	
	"CREATE TABLE manifest (\n"
	"id TEXT NOT NULL PRIMARY KEY,\n"
	"manifest TEXT NOT NULL\n"
	");",

	NULL
    };
    
    for (i = 0; tableSchemas[i] != NULL; ++i)
    {
	sql_stmt = sqlite3_mprintf("%s", tableSchemas[i]);    
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
