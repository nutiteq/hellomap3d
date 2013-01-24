package com.nutiteq.layers.raster;

import android.content.Context;
import android.content.res.Resources;

import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.IntFetchTileTask;

/**
 * A raster layer class that uses the map bundled in the /res/raw/ folder as a source. The request are generate in this
 * manner:
 * <p>
 * <p>
 * "/res/raw/" + name + zoom + "_" + x + "_" + y
 * <p>
 * <p>
 * For example if: name = "t"
 * <p>
 * "/res/raw/" + "t" + "0" + "_" + "0" + "_" + "0"
 * <p>
 * Result: /res/raw/t0_0_0
 */
public class PackagedMapLayer extends RasterLayer {
  protected Resources resources;

  protected String name;

  /**
   * Class constructor. Creates a new raster layer that uses the res/raw/ folder as a source for the tile data. Tiles
   * that are out of the specified minimum / maximum zoom range are not downloaded. The id used should be unique to each
   * raster layer, if two or more raster layers use the same id, they will also share the tiles in the cache. Supported
   * image formats are .jpg and .png, the image type is determined automatically.
   * 
   * @param projection
   *          the desired projection
   * @param minZoom
   *          the minimum zoom
   * @param maxZoom
   *          the maximum zoom
   * @param id
   *          the user generated id for this raster layer
   * @param name
   *          the prefix for the file name
   * @param context
   *          the activity or appliaction context
   */
  public PackagedMapLayer(Projection projection, int minZoom, int maxZoom, int id, String name,
      Context context) {
    super(projection, minZoom, maxZoom, id, context.getPackageName() + ":raw/");
    this.resources = context.getResources();
    this.name = name;
  }

  @Override
  public void fetchTile(MapTile tile) {
    if (tile.zoom < minZoom || tile.zoom > maxZoom) {
        return;
    }
      
    final StringBuffer result = new StringBuffer(location);
    result.append(name);
    result.append(tile.zoom);
    result.append('_');
    result.append(tile.x);
    result.append('_');
    result.append(tile.y);
    String resultString = result.toString();
    Log.info("PackagedMapLayer: Start loading " + resultString);
    executeFetchTask(new IntFetchTileTask(tile, components, tileIdOffset, resultString, resources));
  }

  @Override
  public void flush() {

  }

}
