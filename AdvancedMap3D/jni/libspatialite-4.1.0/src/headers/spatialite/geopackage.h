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

/**
 \file gaiaexif.h

 EXIF/image: supporting functions and constants
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef DLL_EXPORT
#define GEOPACKAGE_DECLARE __declspec(dllexport)
#else
#define GEOPACKAGE_DECLARE extern
#endif
#endif

#ifndef _GEOPACKAGE_H
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define _GEOPACKAGE_H
#endif

#include "sqlite.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Internal geopackage SQL function implementation */
    GEOPACKAGE_DECLARE void fnct_gpkgCreateBaseTables (sqlite3_context *
						       context, int argc,
						       sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgCreateTilesTable (sqlite3_context *
						       context, int argc,
						       sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgCreateTilesZoomLevel (sqlite3_context *
							   context, int argc,
							   sqlite3_value **
							   argv);

    GEOPACKAGE_DECLARE void fnct_gpkgAddTileTriggers (sqlite3_context * context,
						      int argc,
						      sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgAddRasterTriggers (sqlite3_context *
							context, int argc,
							sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgAddRtMetadataTriggers (sqlite3_context *
							    context, int argc,
							    sqlite3_value **
							    argv);
    GEOPACKAGE_DECLARE void fnct_gpkgGetNormalRow (sqlite3_context * context,
						   int argc,
						   sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgGetNormalZoom (sqlite3_context * context,
						    int argc,
						    sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgGetImageType (sqlite3_context * context,
						   int argc,
						   sqlite3_value ** argv);
    GEOPACKAGE_DECLARE void fnct_gpkgPointToTile (sqlite3_context * context,
						  int argc,
						  sqlite3_value ** argv);

/* Markers for unused arguments / variable */
#if __GNUC__
#define UNUSED __attribute__ ((__unused__))
#else
#define UNUSED
#endif

#ifdef __cplusplus
}
#endif

#endif
