package com.nutiteq.datasources.raster;

import java.io.File;
import java.io.InputStream;

import org.mapsforge.android.maps.DebugSettings;
import org.mapsforge.android.maps.mapgenerator.JobParameters;
import org.mapsforge.android.maps.mapgenerator.JobTheme;
import org.mapsforge.android.maps.mapgenerator.MapGenerator;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorFactory;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorInternal;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorJob;
import org.mapsforge.android.maps.mapgenerator.databaserenderer.DatabaseRenderer;
import org.mapsforge.core.Tile;
import org.mapsforge.map.reader.MapDatabase;
import org.mapsforge.map.reader.header.FileOpenResult;

import android.graphics.Bitmap;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.TileBitmap;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.AbstractRasterDataSource;

/**
 * Data source for Mapsforge raster tiles. 
 * 
 * @author jaak
 *
 */
public class MapsforgeRasterDataSource extends AbstractRasterDataSource {
    private static final int TILE_SIZE = 256;
    private static final float DEFAULT_TEXT_SCALE = 1;

    private MapGenerator mapGenerator;
    private JobTheme theme;
    private MapDatabase mapDatabase;

    /**
     * 
     * Modified InternalRenderTheme, as mapsforge bundled does not find theme path
     * @author jaak
     *
     */
    public enum InternalRenderTheme implements JobTheme {
        /**
         * A render-theme similar to the OpenStreetMap Osmarender style.
         * 
         * @see <a href="http://wiki.openstreetmap.org/wiki/Osmarender">Osmarender</a>
         */
        OSMARENDER("/org/mapsforge/android/maps/rendertheme/osmarender/osmarender.xml");

        private final String path;

        private InternalRenderTheme(String path) {
            this.path = path;
        }

        @Override
        public InputStream getRenderThemeAsStream() {
            return getClass().getResourceAsStream(this.path);
        }
    }

    public MapsforgeRasterDataSource(Projection projection, int minZoom, int maxZoom, String path, JobTheme theme) {
        super(projection, minZoom, maxZoom);

        mapGenerator = MapGeneratorFactory.createMapGenerator(MapGeneratorInternal.DATABASE_RENDERER);

        mapDatabase = new MapDatabase();
        mapDatabase.closeFile();
        FileOpenResult fileOpenResult = mapDatabase.openFile(new File("/" + path));
        if (fileOpenResult.isSuccess()) {
            Log.debug("MapsforgeRasterDataSource: MapDatabase opened ok: " + path);
        }

        ((DatabaseRenderer) mapGenerator).setMapDatabase(mapDatabase);
        this.theme = theme;
    }

    public MapDatabase getMapDatabase() {
        return mapDatabase;
    }

    @Override
    public TileBitmap loadTile(MapTile tile) {
        long startTime = System.currentTimeMillis();
        MapGeneratorJob mapGeneratorJob = new MapGeneratorJob(new Tile(tile.x, tile.y, (byte) tile.zoom), "1", new JobParameters(theme, DEFAULT_TEXT_SCALE),
                new DebugSettings(false, false, false));
        Bitmap bitmap = Bitmap.createBitmap(TILE_SIZE, TILE_SIZE, Bitmap.Config.RGB_565);
        boolean success = this.mapGenerator.executeJob(mapGeneratorJob, bitmap);

        long endTime = System.currentTimeMillis();
        Log.debug("MapsforgeRasterDataSource: run success=" + success + "time: " + (endTime-startTime) + " ms");
        
        return new TileBitmap(bitmap);
    }

}
