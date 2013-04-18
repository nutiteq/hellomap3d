package com.nutiteq.layers.raster.db;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.utils.UtfGridHelper;
import com.nutiteq.utils.UtfGridHelper.MBTileUTFGrid;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

/**
 * Universal helper for SQLite tile database.
 * 
 * @author jaak
 */
public class MbTilesDatabaseHelper {

  private static final int DATABASE_VERSION = 1;
  
  private final Context ctx;
  private DatabaseHelper databaseHelper;
  private final String databaseName;
  public SQLiteDatabase database;

  private static final String LOG_TAG = "TileDatabaseHelper";

  // according to mbtiles.org format
  private static final String TILE_TABLE = "tiles";
  private static final String GRID_TABLE = "grids";
  private static final String DATA_TABLE = "grid_data";

  private static final String KEY_ZOOM = "zoom_level";
  private static final String KEY_X = "tile_column";
  private static final String KEY_Y = "tile_row";
  private static final String KEY_TILE_DATA = "tile_data";
  private static final String KEY_GRID = "grid";
  private static final String KEY_GRID_NAME = "key_name";
  private static final String KEY_GRID_JSON = "key_json";

  private static final String TABLE_WHERE = KEY_ZOOM + " = ? and " + KEY_X + " = ? and " + KEY_Y + " = ?";
  
  private static final int UTFGRID_RADIUS = 10; // pixels

  private static class DatabaseHelper extends SQLiteOpenHelper {
    public DatabaseHelper(final Context context, final String databaseName) {
      super(context, databaseName, null, DATABASE_VERSION);
    }

    @Override
    public void onCreate(final SQLiteDatabase db) {
      // to nada, db is expected to exist
    }

    @Override
    public void onUpgrade(final SQLiteDatabase db, final int oldVersion,
        final int newVersion) {
      // to nada, db is expected to exist
    }
  }


  
  /**
   * Construct database helper with own tile table format.
   * 
   * @param ctx
   *            Android application context
   * @param databaseName
   *            Database file path
   * @param tileTable
   *            Tile table name, e.g. "tiles"
   * @param keyZoom
   *            column name for zoom
   * @param keyX
   *            table column name for x/column
   * @param keyY
   *            table column name for y/row
   * @param keyData
   *            column for binary data (blob)
   * @param tableWhere
   *            where SQL clause for tile row search, as prepared statement
   *            e.g. "zoom_level = ? and tile_column = ? and tile_row = ?"
   * @param tableWhere2 
   * @param keyGridJson 
   * @param keyGridName 
   * @param keyGrid 
   * @param keyTileData 
   */
  public MbTilesDatabaseHelper(final Context ctx,  final String databaseName) {
    this.ctx = ctx;
    this.databaseName = databaseName;
  }

  public void open() {
    Log.d(LOG_TAG,"Opening db "+databaseName);
    databaseHelper = new DatabaseHelper(ctx, databaseName);
    database = databaseHelper.getReadableDatabase();
  }

  public void close() {
    databaseHelper.close();
  }

  public boolean containsKey(final int z, final int x, final int y, final String table) {
    final long start = System.currentTimeMillis();
    final Cursor c = database.query(table, new String[] { KEY_X }, TABLE_WHERE, new String[] { String.valueOf(z),
        String.valueOf(x), String.valueOf(y) }, null, null,
        null);
    final boolean hasKey = c.moveToFirst();
    c.close();
    Log.d(LOG_TAG,table+
        " containsKey execution time "
        + (System.currentTimeMillis() - start));
    return hasKey;
  }

  public byte[] getTileImg(final int z, final int x, final int y) {
    final long start = System.currentTimeMillis();
    final Cursor c = database.query(TILE_TABLE, new String[] { KEY_TILE_DATA }, TABLE_WHERE,
        new String[] { String.valueOf(z),
        String.valueOf(x), String.valueOf(y) }, null, null,
        null);
    if (!c.moveToFirst()) {
      Log.d(LOG_TAG,
          "not found z=" + String.valueOf(z) + " x=" + String.valueOf(x) + " y=" + String.valueOf(y));
      c.close();
      return null;
    }
    final byte[] data = c.getBlob(c.getColumnIndexOrThrow(KEY_TILE_DATA));
    c.close();
    // Log.d(LOG_TAG, "get execution time "
    // + (System.currentTimeMillis() - start));
    return data;
  }

  private byte[] getGrid(int zoom, int x, int y) {
    final Cursor c = database.query(GRID_TABLE, new String[] { KEY_GRID }, TABLE_WHERE,
        new String[] { String.valueOf(zoom),
        String.valueOf(x), String.valueOf(y) }, null, null,
        null);
    if (!c.moveToFirst()) {
      Log.d(LOG_TAG,
          "getGrid not found " + String.valueOf(zoom) + " " + String.valueOf(x)
          + " " + String.valueOf(y));
      c.close();
      return null;
    }
    final byte[] data = c.getBlob(c.getColumnIndexOrThrow(KEY_GRID));
    c.close();
    return data;
  }

  public int[] getZoomRange() {
    // select zoom_level from tiles order by zoom_level limit 1

    final Cursor c = database.rawQuery("select min(zoom_level),max(zoom_level) from tiles",new String[]{});
    if (!c.moveToFirst()) {
      Log.d(LOG_TAG, "zoomrange not found");
      c.close();
      return null;
    }
    int[] zooms = new int[]{c.getInt(0),c.getInt(1)};
    c.close();
    return zooms;
  }

  public int[] tileBounds(int zoom) {
    // select min(tile_column),max(tile_column),min(tile_row),max(tile_row)
    // from tiles where tile_zoom = <zoom>
    final Cursor c = database
        .rawQuery(
            "select min(tile_column),max(tile_column),min(tile_row),max(tile_row) from tiles where zoom_level = ?",
            new String[] { String.valueOf(zoom) });
    if (!c.moveToFirst()) {
      Log.d(LOG_TAG, "tileBounds not found");
      c.close();
      return null;
    }
    int[] ret = new int[4];
    ret[0] = c.getInt(0);
    ret[1] = c.getInt(1);
    ret[2] = c.getInt(2);
    ret[3] = c.getInt(3);

    c.close();
    return ret;
  }

  private Cursor getGridValue(int x, int y, int zoom, String key, int radius) {
    if(radius==0){
      return database.query(DATA_TABLE, new String[] { KEY_GRID_JSON }, KEY_ZOOM + " = ? and " + KEY_X + " = ? and "
          + KEY_Y + " = ?" + " and " + KEY_GRID_NAME + " = ? AND " + KEY_GRID_JSON + "<>'{\"NAME\":\"\"}'",
          new String[] { String.valueOf(zoom),
          String.valueOf(x), String.valueOf(y), key}, null, null,
          null);
    }else{
      return database.query(DATA_TABLE, new String[] { KEY_GRID_JSON }, KEY_ZOOM + " = ? AND " + KEY_X + " >= ? AND "
          + KEY_X + " <= ? AND " + KEY_Y + " >= ? AND " + KEY_Y + " <= ? AND " + KEY_GRID_NAME + " = ?",
          new String[] { String.valueOf(zoom),
          String.valueOf(x-radius),String.valueOf(x+radius), String.valueOf(y-radius), String.valueOf(y+radius), key}, null, null,
          null);
    }
  }

  public Cursor getTables(){
    return database
        .rawQuery(
            "select name from SQLITE_MASTER where type = 'table' OR type = 'view'",
            new String[] { });

  }

  public HashMap<String, String> getMetadata() {
    HashMap<String, String> metadata = new HashMap<String, String>();

    final Cursor c = database
        .rawQuery("SELECT name,value FROM metadata",null);
    while(c.moveToNext()){
      metadata.put(c.getString(0), c.getString(1));
    } while (c.moveToNext());

    c.close();
    return metadata;
  }
  
/**
 * Query UTFGrid tooltip values from database for given hover/clicked location
 * 
 * @param p position on map, in EPSG3857
 * @param zoom current map zoom
 * @return KV map with data from JSON. If template is given then templated_teaser and templated_full values are added with HTML
 */
public Map<String, String> getUtfGridTooltips(MapTile clickedTile, MutableMapPos tilePos) {
      
    Map<String, String> data = new HashMap<String, String>();

      // what is current tile in clicked location, and specific pixel of this
      int tileSize = 256;
      int zoom = clickedTile.zoom;
      
      int clickedX = (int) (tilePos.x * 256);
      int clickedY = 256 - (int) (tilePos.y * 256);
      
      //Log.debug("clicked on tile "+zoom+"/"+clickedTile.x+"/"+clickedTile.y+" point:"+clickedX+":"+clickedY);

      // get UTFGrid data for the tile
      MBTileUTFGrid grid = getUTFGrid((int)zoom, clickedTile.x, (1 << (zoom)) - 1 - clickedTile.y);

      if(grid == null){ // no grid found
          Log.d(LOG_TAG,"no UTFgrid in the MBTiles database");
          return null;
      }

      int id = UtfGridHelper.utfGridCode(tileSize, clickedX, clickedY, grid);

      String gridDataJson = getUTFGridValue(clickedTile.x, clickedTile.y, (int)zoom, grid.keys[id], UTFGRID_RADIUS);
      if(gridDataJson == null){
          Log.d(LOG_TAG, "no gridDataJson value for "+id+ " in "+Arrays.toString(grid.keys)+ " tile:"+clickedTile.x +" "+ clickedTile.y);
          return null;
      }
      try {
          // convert JSON to a HashMap
          JSONObject root = new JSONObject(gridDataJson);
          JSONArray names = root.names();
          for(int i = 0; i < names.length(); i++){
              String name = names.getString(i);
              String value = root.getString(name);
              data.put(name,value);
          }

          
          // resolve Mustache template using https://github.com/samskivert/jmustache lib
          String templateString = getMetadata().get("template");

          if(templateString != null){
              
              // use jMustache templating
              Template tmpl = Mustache.compiler().compile(templateString);

              // add one more element to activate "teaser" section
              // options: __teaser__, __full__, __location__
              data.put("__teaser__", "1");
              
              String teaser = (tmpl.execute(data));
              
              // replace it with "full" to get full HTML also
              data.remove("__teaser__");
              data.put("__full__", "1");
              
              String fullToolTip = (tmpl.execute(data));             
              data.remove("__full__");
              
              data.put("__location__", "1");
              
              String location = (tmpl.execute(data));             
              data.remove("__location__");
              
              Log.d(LOG_TAG,"teaser:"+teaser);
              Log.d(LOG_TAG,"fullToolTip:"+fullToolTip);
              Log.d(LOG_TAG,"location:"+location);
              

              data.put(UtfGridHelper.TEMPLATED_TEASER_KEY,teaser);
              data.put(UtfGridHelper.TEMPLATED_FULL_KEY,fullToolTip);
              data.put(UtfGridHelper.TEMPLATED_LOCATION_KEY,location);
          }
  
      } catch (JSONException e) {
         Log.e(LOG_TAG,"UTFGrid JSON parsing error "+e.getMessage());
      }
    return data;
  }


  
  private MBTileUTFGrid getUTFGrid(int zoom, int x, int y) {
//      for (AndroidTileDatabaseHelper db : databases) {
              byte[] gridBytes = getGrid(zoom, x, y);
              if(gridBytes == null){
                  Log.d(LOG_TAG,"no grid for "+zoom+"/"+x+"/"+y);
                  return null;
              }
        try {
            return UtfGridHelper.decodeUtfGrid(gridBytes);
        } catch (IOException e) {
            Log.e(LOG_TAG, "cannot inflate utfgrid data " + e.getMessage());
            e.printStackTrace();
        } catch (JSONException e) {
            Log.e(LOG_TAG, "JSON parser exception " + e.getMessage());
            e.printStackTrace();
        }
        
        return null;

//      }
  }


  private String getUTFGridValue(int x, int y, int zoom, String string, int radius) {
   //   for (AndroidTileDatabaseHelper db: databases){
          Cursor c = getGridValue(x, y , zoom, string, radius);
          while (c.moveToNext()){
              return c.getString(c.getColumnIndex(KEY_GRID_JSON));
          }
     // }
      return null; // if not found from any of the db-s
  }
  
}
