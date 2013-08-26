package com.nutiteq.layers.raster;

import java.util.Map;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.tasks.NetFetchTileTask;

public class TmsMapLayerNoCache extends TMSMapLayer {


    public TmsMapLayerNoCache(Projection projection, int minZoom, int maxZoom,
            int id, String baseUrl, String separator, String format) {
        super(projection, minZoom, maxZoom, id, baseUrl, separator, format);
    }

    @Override
    public void fetchTile(MapTile tile) {
        if (tile.zoom < minZoom || tile.zoom > maxZoom) {
            return;
          }

          StringBuffer url = new StringBuffer(location);
          url.append(tile.zoom);
          url.append(separator);
          url.append(tile.x);
          url.append(separator);
          if(!tmsY){
            url.append(tile.y);
          }else{
            // flip Y coordinate for standard TMS
            url.append( (1<<(tile.zoom))-1-tile.y);
          }
          url.append(format);
          String urlString = url.toString();
          executeFetchTask(new NoCacheNetFetchTileTask(tile, components, tileIdOffset, urlString, this.httpHeaders));

    }
    public class NoCacheNetFetchTileTask extends NetFetchTileTask {
        public NoCacheNetFetchTileTask(MapTile tile, Components components, long tileIdOffset,
            String url, Map<String, String> httpHeaders) {
          super(tile, components, tileIdOffset, url, httpHeaders);
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
              // DISABLED here
//               components.persistentCache.add(rasterTileId, data);
              // Add the compressed image to compressedMemoryCache
              components.compressedMemoryCache.add(rasterTileId, data);
              // If not corrupt, add to the textureMemoryCache
              components.textureMemoryCache.add(rasterTileId, bitmap);
            }
          }
        }
      }
    
    

}
