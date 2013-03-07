package com.nutiteq.layers.raster;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.layers.raster.db.MbTilesDatabaseHelper;
import com.nutiteq.log.Log;
import com.nutiteq.tasks.FetchTileTask;

/**
 * Not part of public API.
 * @pad.exclude
 */
public class DbFetchTileTask extends FetchTileTask {

  private MbTilesDatabaseHelper db;
  private int z;
  private int x;
  private int y;

  public DbFetchTileTask(MapTile tile, Components components, long tileIdOffset, MbTilesDatabaseHelper db) {
    super(tile, components, tileIdOffset);
    this.db = db;
    this.z = tile.zoom;
    this.x = tile.x;
    this.y = tile.y;
  }

  @Override
  public void run() {
    super.run();
    Log.debug("DbMapLayer task: Start loading " + " zoom=" + z + " x=" + x + " y=" + y);
    
    // y is flipped (origin=sw in mbtiles)
    finished(db.getTileImg(z, x, (1 << (z)) - 1 - y));
    cleanUp();
  }

}
