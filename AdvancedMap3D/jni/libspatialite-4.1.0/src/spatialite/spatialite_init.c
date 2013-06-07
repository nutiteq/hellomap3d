/*

 spatialite_init.c -- SQLite3 spatial extension

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

#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#include "config-msvc.h"
#else
#include "config.h"
#endif

#define LOADABLE_EXTENSION
#include <spatialite/sqlite.h>
#undef LOADABLE_EXTENSION

#include <spatialite/spatialite.h>
#include <spatialite.h>
#include <spatialite_private.h>

#ifndef OMIT_GEOS		/* including GEOS */
#include <geos_c.h>
#endif

SQLITE_EXTENSION_INIT1 static int
init_spatialite_extension (sqlite3 * db, char **pzErrMsg,
			   const sqlite3_api_routines * pApi)
{
    void *p_cache = spatialite_alloc_connection ();
    struct splite_internal_cache *cache =
	(struct splite_internal_cache *) p_cache;
    SQLITE_EXTENSION_INIT2 (pApi);

/* setting the POSIX locale for numeric */
    setlocale (LC_NUMERIC, "POSIX");
    *pzErrMsg = NULL;

    register_spatialite_sql_functions (db, cache);

    init_spatialite_virtualtables (db, p_cache);

/* setting a timeout handler */
    sqlite3_busy_timeout (db, 5000);

    return 0;
}

SPATIALITE_DECLARE void
spatialite_init (int verbose)
{
/* used when SQLite initializes as an ordinary lib 
   OBSOLETE - strongly discuraged !!!!!
*/

#ifndef OMIT_GEOS		/* initializing GEOS */
    initGEOS (geos_warning, geos_error);
#endif /* end GEOS  */

    sqlite3_auto_extension ((void (*)(void)) init_spatialite_extension);
    spatialite_splash_screen (verbose);
}

SPATIALITE_DECLARE void
spatialite_cleanup ()
{
#ifndef OMIT_GEOS
    finishGEOS ();
#endif

#ifdef ENABLE_LWGEOM
    gaiaResetLwGeomMsg ();
#endif

    sqlite3_reset_auto_extension ();
}

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

    return init_spatialite_extension (db, pzErrMsg, pApi);
}
