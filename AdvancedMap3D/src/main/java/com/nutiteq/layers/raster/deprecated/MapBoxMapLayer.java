package com.nutiteq.layers.raster.deprecated;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.os.AsyncTask;
import android.widget.RelativeLayout;

import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.UTFGridLayerEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.layers.raster.deprecated.TMSMapLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.tasks.deprecated.NetFetchTileTask;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.UiUtils;
import com.nutiteq.utils.UtfGridHelper;
import com.nutiteq.utils.UtfGridHelper.MBTileUTFGrid;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

/**
 * A raster layer class for reading map tiles from MapBox online tile storage.
 * Reads also UTFGrids for interactivity
 */
@Deprecated
public class MapBoxMapLayer extends TMSMapLayer implements UtfGridLayerInterface{

    private String account;
    private String map;
    private Map<MapPos, MBTileUTFGrid> utfGrids;

    /**
     * Requests UTFGrid for all loaded tiles
     * 
     * @author jaak
     *
     */
    private class NetFetchUtgGridTileTask extends NetFetchTileTask{

        private MapTile tile;

        public NetFetchUtgGridTileTask(MapTile tile, Components components,
                long tileIdOffset, String url, boolean memoryCaching, boolean persistentCaching) {
            super(tile, components, tileIdOffset, url);
            this.tile = tile;
        }

        @Override
        protected void finished(byte[] data) {
            try {
                if(data == null){
                    Log.debug("no grid for "+tile.zoom+"/"+tile.x+"/"+tile.y);
                    return;
                }

                MBTileUTFGrid grid = UtfGridHelper.decodeUtfGrid(data);
                utfGrids.put(new MapPos(tile.x,tile.y,tile.zoom), grid);

            } catch (UnsupportedEncodingException e) {
                e.printStackTrace();
            } catch (IOException e) {
                Log.error("cannot inflate utfgrid data " + e.getMessage());
                e.printStackTrace();
            } catch (JSONException e) {
                Log.error("JSON parser exception " + e.getMessage());
                e.printStackTrace();
            }
        }
    }

    /**
     * Default constructor.
     * 
     * @param projection
     *            projection for the layer.
     * @param minZoom
     *            minimum zoom supported by the layer.
     * @param maxZoom
     *            maximum zoom supported by the layer.
     * @param id
     *            unique layer id. Id for the layer must be depend ONLY on the
     *            layer source, otherwise tile caching will not work properly.
     * @param account
     *            MapBox :account id
     * @param map
     *            MapBox :map id
     */
    public MapBoxMapLayer(Projection projection, int minZoom, int maxZoom,
            int id, String account, String map){
        super(projection, minZoom, maxZoom, id, "http://api.tiles.mapbox.com/v3/"+account+"."+map+"/", "/", ".png");
        this.account = account;
        this.map = map;
        this.utfGrids = new HashMap<MapPos, MBTileUTFGrid>();
        setPersistentCaching(true);
    }

    @Override
    public void fetchTile(MapTile tile) {
        // fetch raster tile
        super.fetchTile(tile);

        if (tile.zoom < minZoom || tile.zoom > maxZoom) {
            return;
        }

        // preload UTFGrid tiles 

        String url = "http://api.tiles.mapbox.com/v3/" + account + "." + map + 
                "/" + tile.zoom + "/" + tile.x + "/" + tile.y + ".grid.json?callback=grid";

        executeFetchTask(new NetFetchUtgGridTileTask(tile, components, tileIdOffset, url, memoryCaching, persistentCaching));
    }

    @Override
    public Map<String, String> getUtfGridTooltips(MapTile clickedTile, MutableMapPos tilePos, String template) {
        Map<String, String> data = new HashMap<String, String>();

        int zoom = clickedTile.zoom;

        // what was clicked tile?
        int tileX = clickedTile.x;
        int tileY = clickedTile.y;

        // point on clicked tile
        int clickedX = (int) (tilePos.x * 256);
        int clickedY = 256 - (int) (tilePos.y * 256);

        Log.debug("clicked on tile "+zoom+"/"+tileX+"/"+tileY+" point:"+clickedX+":"+clickedY);

        // get UTFGrid data for the tile
        MBTileUTFGrid grid = utfGrids.get(new MapPos(tileX, tileY, (int)zoom));

        if(grid == null){ // no grid found
            Log.debug("no UTFgrid loaded for "+(int)zoom+"/"+tileX+"/"+tileY);
            return null;
        }

        int id = UtfGridHelper.utfGridCode(256, clickedX, clickedY, grid, 0);
        if(grid.keys[id].equals("")){
            Log.debug("no utfGrid data here");
            return null;
        }

        int gridKey = Integer.parseInt(grid.keys[id]);
        JSONObject clickedData = grid.data.optJSONObject(String.valueOf(gridKey));

        if(clickedData == null){
            Log.debug("no gridDataJson value for "+id+ " in "+Arrays.toString(grid.keys)+ " tile:"+zoom+ "/"+ tileX +"/"+ tileY);
            return null;
        }
        try {
            // convert JSON to a HashMap

            JSONArray names = clickedData.names();
            for(int i = 0; i < names.length(); i++){
                String name = names.getString(i);
                String value = clickedData.getString(name);
                data.put(name,value);
            }


            // resolve Mustache template using https://github.com/samskivert/jmustache lib

            if(template != null){

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
            Log.error("UTFGrid JSON parsing error "+e.getMessage());
        }
        return data;
    }


    public static class LoadMetadataTask extends AsyncTask<Void, Void, JSONObject> {

        private Activity activity;
        private UTFGridLayerEventListener mapListener;
        private String account;
        private String map;

        public LoadMetadataTask(Activity activity, UTFGridLayerEventListener mapListener, String account, String map){
            this.activity=activity;
            this.mapListener = mapListener;
            this.account = account;
            this.map = map;
        }

        protected JSONObject doInBackground(Void... v) {
            JSONObject metaData = downloadMetadata(account, map);

            return metaData;
        }

        protected void onPostExecute(JSONObject metaData) {
            if(metaData == null){
                Log.error("no metadata found");
                return;
            }
            String template = metaData.optString("template");
            this.mapListener.setTemplate(template);

            String legend = metaData.optString("legend");
            if(legend != null && !legend.equals("")){
                Log.debug("legend: "+legend);
                UiUtils.addWebView((RelativeLayout) this.activity.findViewById(R.id.mainView), this.activity, legend, 320, 300);
            }else{
                Log.debug("no legend found");
            }

        }
    }

    /**
     * Load metadata as JSON. Suggested to be called from non-UI thread.
     * @return 
     */
    public static JSONObject downloadMetadata(String account, String map) {
        try {
            String url = "http://api.tiles.mapbox.com/v3/"+account+"."+map+".json";
            String json = NetUtils.downloadUrl(url, null, true, "UTF-8");
            if(json == null)
                return null;
            JSONObject metaData = new JSONObject(json);
            Log.debug("metadata loaded: "+metaData.toString());
            return metaData;

        } catch (JSONException e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public boolean hasUtfGridTooltips() {
        return true;
    }


}
