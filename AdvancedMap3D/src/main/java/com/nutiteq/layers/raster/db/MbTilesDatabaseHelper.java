package com.nutiteq.layers.raster.db;

import java.util.HashMap;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

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

  public byte[] getGrid(int zoom, int x, int y) {
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

  public Cursor getGridValue(int x, int y, int zoom, String key, int radius) {
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

}
