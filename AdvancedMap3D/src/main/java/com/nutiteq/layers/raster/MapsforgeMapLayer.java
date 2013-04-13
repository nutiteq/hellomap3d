package com.nutiteq.layers.raster;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import org.mapsforge.android.maps.DebugSettings;
import org.mapsforge.android.maps.mapgenerator.JobParameters;
import org.mapsforge.android.maps.mapgenerator.JobTheme;
import org.mapsforge.android.maps.mapgenerator.MapGenerator;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorFactory;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorInternal;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorJob;
import org.mapsforge.android.maps.mapgenerator.databaserenderer.DatabaseRenderer;
import org.mapsforge.core.Tile;
import org.mapsforge.map.reader.MapDatabase;
import org.mapsforge.map.reader.header.FileOpenResult;

import android.graphics.Bitmap;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.NetFetchTileTask;

public class MapsforgeMapLayer extends RasterLayer {

  private MapGenerator mapGenerator;
  private JobTheme theme;
  private MapDatabase mapDatabase;

  public MapsforgeMapLayer(Projection projection, int minZoom, int maxZoom, int id, String path, JobTheme theme) {
    super(projection, minZoom, maxZoom, id, path);
    mapGenerator = MapGeneratorFactory.createMapGenerator(MapGeneratorInternal.DATABASE_RENDERER);

    mapDatabase = new MapDatabase();
    mapDatabase.closeFile();
    FileOpenResult fileOpenResult = mapDatabase.openFile(new File("/"
        + path));
    if (fileOpenResult.isSuccess()) {
      Log.debug("MapsforgeLayer MapDatabase opened ok: " + path);
    }

    ((DatabaseRenderer) mapGenerator).setMapDatabase(mapDatabase);
    this.theme = theme;
  }

  @Override
  public void fetchTile(MapTile tile) {
    components.rasterTaskPool.execute(new MapsforgeFetchTileTask(tile, components, tileIdOffset, mapGenerator, theme));
  }

  @Override
  public void flush() {

  }


/**
 * Task to load data from MapsForge renderer. 
 *   Extends NetFetchTileTask just to use persistent caching
 * @author jaak
 *
 */
public class MapsforgeFetchTileTask extends NetFetchTileTask {

  private static final int TILE_SIZE = 256;
  private int z;
  private int x;
  private int y;
  // private Bitmap tileBitmap;
  private MapGenerator mapGenerator;
  private JobTheme theme;

  private static final float DEFAULT_TEXT_SCALE = 1;

  public MapsforgeFetchTileTask(MapTile tile, Components components,
      long tileIdOffset, MapGenerator mapGenerator, JobTheme theme) {
    super(tile, components, tileIdOffset, "");
    this.mapGenerator = mapGenerator;
    this.z = tile.zoom;
    this.x = tile.x;
    this.y = tile.y;
    this.theme = theme;
  }

  @Override
  public void run() {
    Log.debug("MapsforgeLayer: Start loading " + " zoom=" + z + " x=" + x + " y=" + y);
    long startTime = System.currentTimeMillis();
    MapGeneratorJob mapGeneratorJob = new MapGeneratorJob(new Tile(x, y,
        (byte) z), "1", new JobParameters(theme, DEFAULT_TEXT_SCALE),
        new DebugSettings(false, false, false));
    Bitmap tileBitmap = Bitmap.createBitmap(TILE_SIZE, TILE_SIZE,
        Bitmap.Config.RGB_565);
    boolean success = this.mapGenerator.executeJob(mapGeneratorJob, tileBitmap);
    
    
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    tileBitmap.compress(Bitmap.CompressFormat.PNG, 90, bos);
    
    long endTime = System.currentTimeMillis();
    Log.debug("MapsforgeFetchTileTask run success=" + success + "time: " + (endTime-startTime)+ " ms");
    try {
      bos.close();
    } catch (IOException e) {
      e.printStackTrace();
    }
    finished(bos.toByteArray());
    cleanUp();
  }
}

/**
 * 
 * Modified InternalRenderTheme, as mapsforge bundled does not find theme path
 * @author jaak
 *
 */
public enum InternalRenderTheme implements JobTheme {
    /**
     * A render-theme similar to the OpenStreetMap Osmarender style.
     * 
     * @see <a href="http://wiki.openstreetmap.org/wiki/Osmarender">Osmarender</a>
     */
    OSMARENDER("/org/mapsforge/android/maps/rendertheme/osmarender/osmarender.xml");

    private final String path;
    
    private InternalRenderTheme(String path) {
        this.path = path;
    }

    @Override
    public InputStream getRenderThemeAsStream() {
        return getClass().getResourceAsStream(this.path);
    }
}


public MapDatabase getMapDatabase() {
    return mapDatabase;
}
  
}
