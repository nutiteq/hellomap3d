package com.nutiteq.layers.raster.deprecated;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.FetchTileTask;

@Deprecated
public class TileDebugMapLayer extends RasterLayer {

    private int tileSize = 256;
    private Context context;

    public TileDebugMapLayer(Projection projection, int minZoom, int maxZoom,
            int id, int tileSize, Context context) {
        super(projection, minZoom, maxZoom, id, "");
        this.tileSize = tileSize;
        this.context = context;

    }


    // some of source: http://www.skoumal.net/en/android-how-draw-text-bitmap
    public byte[] drawTextToBitmap(Context gContext, 
            String gText) {
        Resources resources = gContext.getResources();
        float scale = resources.getDisplayMetrics().density;
        /*
            // create boxed white bitmap
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
         */
        //            Bitmap bitmap = Bitmap.createBitmap(bitMapData, 256, 256,
        //                    Bitmap.Config.ARGB_8888);
        Bitmap bitmap = Bitmap.createBitmap(tileSize, tileSize,
                Bitmap.Config.ARGB_8888);

        // draw text on it
        android.graphics.Bitmap.Config bitmapConfig =
                bitmap.getConfig();
        // set default bitmap config if none
        if(bitmapConfig == null) {
            bitmapConfig = android.graphics.Bitmap.Config.ARGB_8888;
        }
        // resource bitmaps are imutable, 
        // so we need to convert it to mutable one
        bitmap = bitmap.copy(bitmapConfig, true);

        Canvas canvas = new Canvas(bitmap);
        // new antialised Paint
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        // text color - #3D3D3D
        paint.setColor(Color.rgb(61, 61, 61));
        // text size in pixels
        paint.setTextSize(32);
        // text shadow
        paint.setShadowLayer(1f, 0f, 1f, Color.WHITE);

        // draw text to the Canvas center
        Rect bounds = new Rect();
        paint.getTextBounds(gText, 0, gText.length(), bounds);
        int x = (/*bitmap.getWidth()*/ 256 - bounds.width())/2;
        int y = (bitmap.getHeight() + bounds.height())/2;

        //            canvas.drawText(gText, x * scale, y * scale, paint);
        canvas.drawText(gText, x, y, paint);

        canvas.drawLine(0, 0, 0, tileSize, paint);
        canvas.drawLine(0, tileSize, tileSize, tileSize, paint);
        canvas.drawLine(tileSize, tileSize, tileSize, 0, paint);
        canvas.drawLine(tileSize, 0, 0, 0, paint);

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 90, bos);
        try {
            bos.close();
        } catch (IOException e) {
            e.printStackTrace();
        }

        return bos.toByteArray();
    }

    @Override
    public void fetchTile(MapTile tile) {
        executeFetchTask(new DebugTileTask(tile, components, getTileIdOffset(), "" + tile.zoom + "/"
                + tile.x + "/" + tile.y, context));
    }

    @Override
    public void flush() {

    }

    public class DebugTileTask extends FetchTileTask {

        String text = "";
        Context context;

        public DebugTileTask(MapTile tile, Components components,
                long tileIdOffset, String text, Context context) {
            super(tile, components, tileIdOffset);
            this.context = context;
            this.text = text;
        }

        @Override
        public void run() {
            super.run();
            finished(drawTextToBitmap(context,text));
        }
    }

}
