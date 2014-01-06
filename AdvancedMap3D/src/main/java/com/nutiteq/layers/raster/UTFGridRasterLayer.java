package com.nutiteq.layers.raster;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.datasources.raster.UTFGridDataSource;
import com.nutiteq.log.Log;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.Task;
import com.nutiteq.utils.UtfGridHelper;
import com.nutiteq.utils.UtfGridHelper.MBTileUTFGrid;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

/**
 * Specialized raster layer that gives information via UTF grid about objects.
 * 
 * @author mark
 *
 */
public class UTFGridRasterLayer extends RasterLayer {

    /**
     * Requests UTFGrid for all loaded tiles
     */
    private class UTFGridDataSourceFetchTask implements Task {
        final MapTile tile;

        public UTFGridDataSourceFetchTask(MapTile tile) {
            this.tile = tile;
        }

        @Override
        public void run() {
            try {
                MBTileUTFGrid utfGrid = utfGridDataSource.loadUTFGrid(tile);
                if (utfGrid == null) {
                    Log.error("UTFGridDataSourceFetchTask: no grid element data");
                } else {
                    synchronized (utfGrids) {
                        utfGrids.put(new MapTile(tile.x, tile.y, tile.zoom, 0), utfGrid);
                    }
                }
            } catch (Exception e) {
                Log.error("UTFGridDataSourceFetchTask: failed to fetch grid element: " + e.getMessage());
            }
        }

        @Override
        public boolean isCancelable() {
            return true;
        }

        @Override
        public void cancel() {
        }
    }

    private final UTFGridDataSource utfGridDataSource;
    private Map<MapTile, MBTileUTFGrid> utfGrids = new HashMap<MapTile, MBTileUTFGrid>();

    /**
     * Default constructor.
     * 
     * @param dataSource
     * 			data source for raster tiles
     * @param utfGridDataSource
     * 			data source for grid
     * @param id
     * 			layer id
     */
    public UTFGridRasterLayer(RasterDataSource dataSource, UTFGridDataSource utfGridDataSource, int id) {
        super(dataSource, id);
        this.utfGridDataSource = utfGridDataSource;
    }

    /**
     * Get attached UTF grid data source.
     * 
     * @return attached data source
     */
    public UTFGridDataSource getUTFGridDataSource() {
        return utfGridDataSource;		
    }

    /**
     * Get tile meta data at given position.
     * 
     * @param tile
     *          tile to use
     * @param tilePos
     *          position within tile
     * @param template
     *          template to use for formatting metadata. Can be null.
     * @return meta data key-value map
     */
    public Map<String, String> getUTFGridTooltips(MapTile tile, MutableMapPos tilePos, String template) {
        Map<String, String> data = new HashMap<String, String>();

        // point on clicked tile
        int clickedX = (int) (tilePos.x * 256);
        int clickedY = 256 - (int) (tilePos.y * 256);

        // get UTFGrid data for the tile
        MBTileUTFGrid grid;
        synchronized (utfGrids) {
            grid = utfGrids.get(new MapTile(tile.x, tile.y, tile.zoom, 0));
        }
        if (grid == null){ // no grid found
            Log.debug("UTFGridRasterLayer: no UTFgrid loaded for " + tile.zoom + "/" + tile.x + "/" + tile.y);
            return null;
        }

        int id = UtfGridHelper.utfGridCode(256, clickedX, clickedY, grid, 0);
        if (grid.keys[id].equals("")){
            Log.debug("UTFGridRasterLayer: no UTFgrid data here");
            return null;
        }

        int gridKey = Integer.parseInt(grid.keys[id]);
        JSONObject clickedData = grid.data.optJSONObject(String.valueOf(gridKey));

        if (clickedData == null){
            Log.debug("UTFGridRasterLayer: no gridDataJson value for " + id + " in " + Arrays.toString(grid.keys) + " tile:" + tile.zoom + "/" + tile.x + "/" + tile.y);
            return null;
        }
        try {
            // convert JSON to a HashMap
            JSONArray names = clickedData.names();
            for (int i = 0; i < names.length(); i++){
                String name = names.getString(i);
                String value = clickedData.getString(name);
                data.put(name,value);
            }

            // resolve Mustache template using https://github.com/samskivert/jmustache lib
            if (template != null){

                // use jMustache templating
                Template tmpl = Mustache.compiler().compile(template);

                // add one more element to activate "teaser" section
                // options: __teaser__, __full__, __location__
                data.put("__teaser__", "1");

                String teaser = (tmpl.execute(data));

                // replace it with "full" to get full HTML also
                data.remove("__teaser__");
                data.put("__full__", "1");

                String fullToolTip = (tmpl.execute(data));             
                data.remove("__full__");

                data.put("__location__", "1");

                String location = (tmpl.execute(data));             
                data.remove("__location__");

                Log.debug("teaser:"+teaser);
                Log.debug("fullToolTip:"+fullToolTip);
                Log.debug("location:"+location);


                data.put(UtfGridHelper.TEMPLATED_TEASER_KEY,teaser);
                data.put(UtfGridHelper.TEMPLATED_FULL_KEY,fullToolTip);
                data.put(UtfGridHelper.TEMPLATED_LOCATION_KEY,location);
            }

        } catch (JSONException e) {
            Log.error("UTFGridRasterLayer: JSON parsing error " + e.getMessage());
        }
        return data;
    }

    @Override
    public void fetchTile(MapTile tile) {
        // Fetch raster tile
        super.fetchTile(tile);

        // Fetch UTFGrid element
        if (tile.zoom >= minZoom && tile.zoom <= maxZoom) {
            executeFetchTask(new UTFGridDataSourceFetchTask(tile));
        }
    }

    @Override
    public void setPersistentCaching(boolean caching) {
        throw new UnsupportedOperationException("Persistent caching is not supported for UTFGridRasterLayer");
    }
}
