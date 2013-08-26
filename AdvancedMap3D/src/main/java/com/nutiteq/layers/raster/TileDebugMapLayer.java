package com.nutiteq.layers.raster;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

import android.graphics.Bitmap;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.FetchTileTask;

public class TileDebugMapLayer extends RasterLayer {
    
    private byte[] image;

    private int tileSize = 256;

    public TileDebugMapLayer(Projection projection, int minZoom, int maxZoom,
            int id, int tileSize) {
        super(projection, minZoom, maxZoom, id, "");
        this.tileSize = tileSize;
        
        int[] bitMapData = new int[tileSize * tileSize];

        for (int i = 0; i < tileSize; i++) {
            for (int j = 0; j < tileSize; j++) {
                int pix = android.graphics.Color.TRANSPARENT;
                if(i==0 || j==0 || i==tileSize-1 || j==tileSize-1){
                    pix = android.graphics.Color.DKGRAY;
                }
                bitMapData[i * tileSize + j] = pix;
            }
        }

    Bitmap bitmap = Bitmap.createBitmap(bitMapData, 256, 256,
            Bitmap.Config.ARGB_8888);
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    bitmap.compress(Bitmap.CompressFormat.PNG, 90, bos);
    try {
        bos.close();
    } catch (IOException e) {
        e.printStackTrace();
    }
    image = bos.toByteArray();

    }


    @Override
    public void fetchTile(MapTile tile) {
        Log.debug("TestOverlayLayer: Start loading " + " zoom=" + tile.zoom + " x="
                + tile.x + " y=" + tile.y);
        
        executeFetchTask(new DebugTileTask(tile, components, getTileIdOffset(), image));
    }

    @Override
    public void flush() {

    }
    
    public class DebugTileTask extends FetchTileTask {

        private byte[] image;

        public DebugTileTask(MapTile tile, Components components,
                long tileIdOffset, byte[] image) {
            super(tile, components, tileIdOffset);
            this.image = image;
        }

        @Override
        public void run() {
            super.run();
            finished(image);
        }
    }
    
}
