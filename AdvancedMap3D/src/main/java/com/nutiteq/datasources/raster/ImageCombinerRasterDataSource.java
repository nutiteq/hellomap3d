package com.nutiteq.datasources.raster;

import java.util.List;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.TileBitmap;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.AbstractRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;

/**
 * Data source for combining (blending) multiple source tiles.
 * This can be used to synchronize raster tile displaying (zoom levels of blended tiles always match)
 * and to optimize texture cache usage. Note that the original data sources are queried consecutively,
 * thus if both are high-latency data sources (network sources) then this data sources will have even higher latency.
 */
public class ImageCombinerRasterDataSource extends AbstractRasterDataSource {
  private final List<RasterDataSource> dataSources;
  
  private static Projection getProjection(List<RasterDataSource> dataSources) {
    return dataSources.get(0).getProjection();
  }
  
  private static int getMinZoom(List<RasterDataSource> dataSources) {
    int minZoom = 0;
    for (RasterDataSource dataSource : dataSources) {
      minZoom = Math.max(minZoom, dataSource.getMinZoom());
    }
    return minZoom;
  }
  
  private static int getMaxZoom(List<RasterDataSource> dataSources) {
    int maxZoom = Integer.MAX_VALUE;
    for (RasterDataSource dataSource : dataSources) {
      maxZoom = Math.min(maxZoom, dataSource.getMaxZoom());
    }
    return maxZoom;
  }
  
  /**
   * Default constructor.
   * 
   * @param dataSources
   *          list of tile datasources to combine. Note: the list must be non-empty!
   */
  public ImageCombinerRasterDataSource(List<RasterDataSource> dataSources) {
    super(getProjection(dataSources), getMinZoom(dataSources), getMaxZoom(dataSources));
    this.dataSources = dataSources;
  }

  /**
   * Constructor with explicit list of datasources.
   * 
   * @param dataSources
   *          tile datasources to combine
   */
  public ImageCombinerRasterDataSource(RasterDataSource... dataSources) {
    this(java.util.Arrays.asList(dataSources));
  }
  
  @Override
  public TileBitmap loadTile(MapTile tile) {
    Bitmap bitmap = null;
    Canvas canvas = null;
    Paint paint = new Paint();
    paint.setARGB(255, 255, 255, 255);
    for (RasterDataSource dataSource : dataSources) {
      TileBitmap tileBitmap = dataSource.loadTile(tile);
      if (tileBitmap == null) {
        continue;
      }
      if (bitmap == null) {
        bitmap = tileBitmap.getBitmap().copy(Bitmap.Config.ARGB_8888, true);
        canvas = new Canvas(bitmap);
      } else {
        canvas.drawBitmap(tileBitmap.getBitmap(), 0, 0, paint);
      }
    }
    if (bitmap == null) {
      return null;
    }
    return new TileBitmap(bitmap);
  }

  @Override
  public void addOnChangeListener(OnChangeListener listener) {
    super.addOnChangeListener(listener);
    for (RasterDataSource dataSource : dataSources) {
      dataSource.addOnChangeListener(listener);
    }
  }

  @Override
  public void removeOnChangeListener(OnChangeListener listener) {
    for (RasterDataSource dataSource : dataSources) {
      dataSource.removeOnChangeListener(listener);
    }
    super.removeOnChangeListener(listener);
  }
}
