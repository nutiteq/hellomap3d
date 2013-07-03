package com.nutiteq.layers.raster;

import java.util.Map;

import com.nutiteq.components.MapTile;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.NetFetchTileTask;

/**
 * A raster layer class that uses an URL as a source for the map tile data. The request are generate in this manner:
 * <p>
 * <p>
 * baseUrl + QuadKey + extension
 * QuadKey single tile number, as used for Bing maps: http://msdn.microsoft.com/en-us/library/bb259689.aspx
 * <p>
 * <p>
 * For example if: baseUrl = "http://ecn.t3.tiles.virtualearth.net/tiles/r", extension = ".png?g=1&mkt=en-US&shading=hill&n=z"
 * <p>
 * QuadKey (as used in Bing maps API): 123
 * <p>
 * Result: http://ecn.t3.tiles.virtualearth.net/tiles/r123.png?g=1&mkt=en-US&shading=hill&n=z
 * 
 * Note: If you use Bing Maps tiles make sure you follow Microsoft Terms of Service. It may or it may not be legal for commercial applications.
 */
public class QuadKeyLayer extends RasterLayer {
  protected final String extension;
  private Map<String, String> httpHeaders;

  /**
   * Creates a new raster layer that uses a specified URL as a source for the tile data. Tiles that
   * are out of the specified minimum / maximum zoom range are not downloaded. The id used should be unique to each
   * rasterlayer, if two or more raster layers use the same id, they will also share the tiles in the cache. Supported
   * image formats are .jpg and .png.
   * 
   * @param projection
   *          the desired projection
   * @param minZoom
   *          the minimum zoom
   * @param maxZoom
   *          the maximum zoom
   * @param id
   *          the user generated id for this raster layer
   * @param baseUrl
   *          the base URL
   * @param extension
   *          URL extension, usually starts with the image format 
   */
  public QuadKeyLayer(Projection projection, int minZoom, int maxZoom, int id, String baseUrl,
      String extension) {
    super(projection, minZoom, maxZoom, id, baseUrl);
    this.extension = extension;
  }
  
  
  /**
   * Add HTTP headers. Useful for referer, basic-auth etc.
   * @param httpHeaders
   */
  public void setHttpHeaders(Map<String, String> httpHeaders) {
    this.httpHeaders = httpHeaders;
  }

  @Override
  public void fetchTile(MapTile tile) {
    
    if (tile.zoom < minZoom || tile.zoom > maxZoom) {
      return;
    }
    
    int x = tile.x;
    int y = tile.y;

    StringBuffer url = new StringBuffer(location);

    for (int i = tile.zoom - 1; i >= 0; i--) {
        url.append((((y >> i) & 1) << 1) + ((x >> i) & 1));
    }

    url.append(extension);
    String urlString = url.toString();
    executeFetchTask(new NetFetchTileTask(tile, components, tileIdOffset, urlString, this.httpHeaders));
  }

  @Override
  public void flush() {
  }
}
