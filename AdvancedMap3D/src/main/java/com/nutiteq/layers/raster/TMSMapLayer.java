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
 * baseUrl + zoom + separator + x + separator + y + format
 * <p>
 * <p>
 * For example if: baseUrl = "http://tile.openstreetmap.org/", separator = "/", format = ".png"
 * <p>
 * "http://tile.openstreetmap.org/" + "0" + "/" + "0" + "/" + "0" + ".png" 
 * <p>
 * Result: http://tile.openstreetmap.org/0/0/0.png
 */
public class TMSMapLayer extends RasterLayer {
  protected final String separator;
  protected final String format;
  private boolean tmsY = false;
  private int offsetX = 0;
  private int offsetY = 0;
  private int offsetZoom = 0;
  private Map<String, String> httpHeaders;

  /**
   * Class constructor. Creates a new raster layer that uses a specified URL as a source for the tile data. Tiles that
   * are out of the specified minimum / maximum zoom range are not downloaded. The id used should be unique to each
   * rasterlayer, if two or more raster layers use the same id, they will also share the tiles in the cache. Supported
   * image formats are .jpg and .png.
   * Note: by default is based on "OSM tile schema", with flipped Y coordinate, change it using setTmsY()
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
   * @param separator
   *          the seperator for the URL
   * @param format
   *          the image format for the url
   */
  public TMSMapLayer(Projection projection, int minZoom, int maxZoom, int id, String baseUrl,
      String separator, String format) {
    super(projection, minZoom, maxZoom, id, baseUrl);
    this.separator = separator;
    this.format = format;
  }
  
  
  /**
   * Use this to set that Y axis is according to TMS standard, ie with origin in southeast. 
   * Otherwise it is in northeast, like in common public servers: OpenStreetMap and others
   * @param tmsY true if TMS standard Y orientation is needed. Default is false
   */
  public void setTmsY(boolean tmsY){
    this.tmsY = tmsY;
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
    
    // adjust to WorldTile (if default 0,0,0 then no change)
    int zoomPow2 = 2 << (tile.zoom-1); // same as Math.pow(2,tile.zoom)
    int tileX = offsetX * zoomPow2 + tile.x;
    int tileY = offsetY * zoomPow2 + tile.y;
    int tileZoom = tile.zoom + offsetZoom;
    
    if (tileZoom < minZoom || tileZoom > maxZoom) {
      return;
    }

    StringBuffer url = new StringBuffer(location);
    url.append(tileZoom);
    url.append(separator);
    url.append(tileX);
    url.append(separator);
    if(!tmsY){
      url.append(tileY);
    }else{
      // flip Y coordinate for standard TMS
      url.append( (1<<(tileZoom))-1-tileY);
    }
    url.append(format);
    String urlString = url.toString();
    executeFetchTask(new NetFetchTileTask(tile, components, tileIdOffset, urlString, this.httpHeaders));
  }

  @Override
  public void flush() {
  }
}
