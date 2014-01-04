package com.nutiteq.datasources.raster;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.TileBitmap;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.AbstractRasterDataSource;

/**
 * Data source for showing tile ids.
 * It is a virtual data source that creates bitmaps with texts describing tile coordinates and zoom level.
 */
public class TileDebugRasterDataSource extends AbstractRasterDataSource {
    private final int tileSize;

    public TileDebugRasterDataSource(Projection projection, int minZoom, int maxZoom, int tileSize) {
        super(projection, minZoom, maxZoom);
        this.tileSize = tileSize;
    }

    @Override
    public TileBitmap loadTile(MapTile tile) {
        String tileId = "" + tile.zoom + "/" + tile.x + "/" + tile.y;
        return new TileBitmap(drawTextToBitmap(tileId));
    }

    // some of source: http://www.skoumal.net/en/android-how-draw-text-bitmap
    private Bitmap drawTextToBitmap(String text) {
        Bitmap bitmap = Bitmap.createBitmap(tileSize, tileSize, Bitmap.Config.ARGB_8888);
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
        paint.getTextBounds(text, 0, text.length(), bounds);
        int x = (/*bitmap.getWidth()*/ 256 - bounds.width())/2;
        int y = (bitmap.getHeight() + bounds.height())/2;

        // Resources resources = context.getResources();
        // float scale = resources.getDisplayMetrics().density;
        // canvas.drawText(gText, x * scale, y * scale, paint);
        canvas.drawText(text, x, y, paint);

        canvas.drawLine(0, 0, 0, tileSize, paint);
        canvas.drawLine(0, tileSize, tileSize, tileSize, paint);
        canvas.drawLine(tileSize, tileSize, tileSize, 0, paint);
        canvas.drawLine(tileSize, 0, 0, 0, paint);
        return bitmap;
    }

}
