package com.nutiteq.datasources.raster;

import java.io.File;
import java.io.IOException;

import org.json.JSONException;

import android.content.Context;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.TileBitmap;
import com.nutiteq.db.MBTilesDbHelper;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.AbstractRasterDataSource;
import com.nutiteq.utils.UtfGridHelper;
import com.nutiteq.utils.UtfGridHelper.MBTileUTFGrid;

/**
 * A raster data source class for reading map tiles from local Sqlite database.
 * The database must contain table <i>tiles</i> with the following fields:
 * <p>
 * <p>
 * <i>zoom_level</i> (zoom level of the tile), <i>tile_column</i>,
 * <i>tile_row</i>, <i>tile_data</i> (compressed tile bitmap)
 */
public class MBTilesRasterDataSource extends AbstractRasterDataSource implements UTFGridDataSource {

    private MBTilesDbHelper db;
    private boolean tmsY;

    /**
     * Default constructor.
     * 
     * @param projection
     * 			  projection for the data source (practically always EPSG3857)
     * @param minZoom
     *            minimum zoom supported by the data source.
     * @param maxZoom
     *            maximum zoom supported by the data source.
     * @param path
     *            path to the local Sqlite database file. SQLiteException will
     *            be thrown if the database does not exist
     *            or can not be opened in read-only mode.
     * @param tmsY
     * 			  flag describing if tiles are vertically flipped or not
     * @param ctx
     *            Android application context.
     * @throws IOException
     *             exception if file not exists
     */
    public MBTilesRasterDataSource(Projection projection, int minZoom, int maxZoom, String path, boolean tmsY, Context ctx) throws IOException {
        super(projection, minZoom, maxZoom);
        reopen(path, tmsY, ctx);
    }

    /**
     * Reopen database file
     * 
     * @param path
     *            path to the local Sqlite database file. SQLiteException will
     *            be thrown if the database does not exist
     *            or can not be opened in read-only mode.
     * @param tmsY
     * 			  flag describing if tiles are vertically flipped or not
     * @param ctx
     *            Android application context.
     * @throws IOException
     *             exception if file not exists
     */
    public void reopen(String path, boolean tmsY, Context ctx) throws IOException {
        close();
        if (!(new File(path)).exists()) {
            throw new IOException("MBTiles file does not exist: " + path);
        }

        db = new MBTilesDbHelper(ctx, path);
        db.open();
        this.tmsY = tmsY;
    }

    /**
     * Close database file. After calling this, new tiles will not be read from
     * the database.
     */
    public void close() {
        if (db != null) {
            db.close();
            db = null;
        }
    }

    /**
     * Get database helper instance.
     * 
     * @return database helper instance if database is open, or null if closed.
     */
    public MBTilesDbHelper getDatabase() {
        return db;
    }

    @Override
    public TileBitmap loadTile(MapTile tile) {
        if (db == null) {
            return null;
        }

        int y = tmsY ? tile.y : (1 << tile.zoom) - 1 - tile.y;
        return new TileBitmap(db.getTileImg(tile.zoom, tile.x, y));
    }

    @Override
    public MBTileUTFGrid loadUTFGrid(MapTile tile) {
        if (db == null) {
            return null;
        }

        int y = tmsY ? tile.y : (1 << tile.zoom) - 1 - tile.y;
        byte[] gridBytes = db.getGrid(tile.zoom, tile.x, y);
        if (gridBytes == null) {
            return null;
        }
        try {
            return UtfGridHelper.decodeUtfGrid(gridBytes);
        } catch (JSONException e) {
            return null;
        } catch (IOException e) {
            return null;
        }
    }
}
