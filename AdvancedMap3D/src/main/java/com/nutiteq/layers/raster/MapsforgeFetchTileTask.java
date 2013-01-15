package com.nutiteq.layers.raster;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

import org.mapsforge.android.maps.DebugSettings;
import org.mapsforge.android.maps.mapgenerator.JobParameters;
import org.mapsforge.android.maps.mapgenerator.JobTheme;
import org.mapsforge.android.maps.mapgenerator.MapGenerator;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorJob;
import org.mapsforge.core.Tile;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.nutiteq.components.Components;
import com.nutiteq.components.TileQuadTreeNode;
import com.nutiteq.log.Log;
import com.nutiteq.tasks.FetchTileTask;

public class MapsforgeFetchTileTask extends FetchTileTask {

  private static final int TILE_SIZE = 256;
  private int z;
  private int x;
  private int y;
  // private Bitmap tileBitmap;
  private MapGenerator mapGenerator;
  private JobTheme theme;

  private static final float DEFAULT_TEXT_SCALE = 1;

  public MapsforgeFetchTileTask(TileQuadTreeNode tile, Components components,
      long tileIdOffset, MapGenerator mapGenerator, JobTheme theme) {
    super(tile, components, tileIdOffset);
    this.mapGenerator = mapGenerator;
    this.z = tile.zoom;
    this.x = tile.x;
    this.y = tile.y;
    this.theme = theme;
  }

  @Override
  public void run() {
    super.run();
    Log.debug("MapsforgeLayer: Start loading " + " zoom=" + z + " x=" + x + " y=" + y);

    MapGeneratorJob mapGeneratorJob = new MapGeneratorJob(new Tile(x, y,
        (byte) z), "1", new JobParameters(theme, DEFAULT_TEXT_SCALE),
        new DebugSettings(false, false, false));
    Bitmap tileBitmap = Bitmap.createBitmap(TILE_SIZE, TILE_SIZE,
        Bitmap.Config.RGB_565);
    boolean success = this.mapGenerator.executeJob(mapGeneratorJob, tileBitmap);
    Log.debug("MapsforgeFetchTileTask run success=" + success);
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    tileBitmap.compress(Bitmap.CompressFormat.PNG, 90, bos);
    try {
      bos.close();
    } catch (IOException e) {
      e.printStackTrace();
    }
    finished(bos.toByteArray());
    cleanUp();
  }

  protected void finished(byte[] data) {
    if (data == null) {
      Log.error(getClass().getName() + " : No data.");
    } else {
      BitmapFactory.Options opts = new BitmapFactory.Options();
      opts.inScaled = false;
      Bitmap bitmap = BitmapFactory.decodeByteArray(data, 0, data.length, opts);
      if (bitmap == null) {
        // If the compressed image is corrupt, delete it
        Log.error(getClass().getName() + " : Failed to decode the image.");
      } else {
     // Add the compressed image to persistentCache
        components.persistentCache.add(rasterTileId, data);
        // Add the compressed image to compressedMemoryCache
        components.compressedMemoryCache.add(rasterTileId, data);
        // If not corrupt, add to the textureMemoryCache
        components.textureMemoryCache.add(rasterTileId, bitmap);
      }
    }
  }
}
