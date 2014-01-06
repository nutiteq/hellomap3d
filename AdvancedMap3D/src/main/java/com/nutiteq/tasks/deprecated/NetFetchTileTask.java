package com.nutiteq.tasks.deprecated;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Map;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.tasks.FetchTileTask;

/**
 * Task for fetching raster tiles using HTTP protocol.
 */
public class NetFetchTileTask extends FetchTileTask {
  private static final int CONNECTION_TIMEOUT = 5 * 1000;
  private static final int READ_TIMEOUT = 5 * 1000;

  protected String url;

  private HttpURLConnection urlConnection;
  private Map<String, String> httpHeaders;
  private boolean memoryCaching;
  private boolean persistentCaching;

  /**
   * Runs networking (HTTP GET request) task to load a map tile
   * @param tile Map tile to be loaded
   * @param components package of application state parameters
   * @param tileIdOffset tile unique ID counter start, take from RasterLayer (superclass)
   * @param url tile URL
   */
  public NetFetchTileTask(MapTile tile, Components components, long tileIdOffset,
      String url) {
    this(tile, components, tileIdOffset,url, null);
    this.memoryCaching = true;
    this.persistentCaching = true;
  }
  
  /**
   * Runs networking (HTTP GET request) task to load a map tile
   * @param tile Map tile to be loaded
   * @param components package of application state parameters
   * @param tileIdOffset tile unique ID counter start, take from RasterLayer (superclass)
   * @param url tile URL
   * @param httpHeaders set of HTTP headers to be added to the request, e.g. referrer, user-agent etc
   */
  public NetFetchTileTask(MapTile tile, Components components, long tileIdOffset,
      String url, Map<String, String> httpHeaders) {
    super(tile, components, tileIdOffset);
    this.httpHeaders = httpHeaders;
    this.url = url;
    this.memoryCaching = true;
    this.persistentCaching = true;
  }

  /**
   * Runs networking (HTTP GET request) task to load a map tile
   * @param tile Map tile to be loaded
   * @param components package of application state parameters
   * @param tileIdOffset tile unique ID counter start, take from RasterLayer (superclass)
   * @param url tile URL
   * @param httpHeaders set of HTTP headers to be added to the request, e.g. referrer, user-agent etc
   * @param memoryCaching flag to enable or disable persistent caching
   * @param persistentCaching flag to enable or disable persistent caching
   */
  public NetFetchTileTask(MapTile tile, Components components, long tileIdOffset,
      String url, Map<String, String> httpHeaders, boolean memoryCaching, boolean persistentCaching) {
    super(tile, components, tileIdOffset);
    this.httpHeaders = httpHeaders;
    this.url = url;
    this.memoryCaching = memoryCaching;
    this.persistentCaching = persistentCaching;
  }

  @Override
  public void run() {
    super.run();
    try {
      Log.info("NetFetchTileTask loading task " + url);

      urlConnection = (HttpURLConnection) (new URL(url)).openConnection();
      urlConnection.setConnectTimeout(CONNECTION_TIMEOUT);
      urlConnection.setReadTimeout(READ_TIMEOUT);
      
      if (this.httpHeaders != null){
        for (Map.Entry<String, String> entry : this.httpHeaders.entrySet()) {
          urlConnection.addRequestProperty(entry.getKey(), entry.getValue());  
        }
      }
      
      BufferedInputStream inputStream = new BufferedInputStream(urlConnection.getInputStream(), BUFFER_SIZE);
      readStream(inputStream);

    } catch (IOException e) {
      Log.error(getClass().getName() + ": Failed to fetch tile. " + e.getMessage());
    } finally {
      if (urlConnection != null) {
        urlConnection.disconnect();
      }
    }
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
        if (persistentCaching) {
          // Add the compressed image to persistentCache
          components.persistentCache.add(rasterTileId, data);
        }
        if (memoryCaching) {
          // Add the compressed image to compressedMemoryCache
          components.compressedMemoryCache.add(rasterTileId, data);
        }
        // If not corrupt, add to the textureMemoryCache
        components.textureMemoryCache.add(rasterTileId, bitmap);
      }
    }
  }
}
