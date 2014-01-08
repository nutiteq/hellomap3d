package com.nutiteq.advancedmap.activity;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.webkit.WebView;
import android.widget.RelativeLayout;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.UTFGridLayerEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.datasources.raster.MBOnlineRasterDataSource;
import com.nutiteq.geometry.Marker;
import com.nutiteq.layers.raster.UTFGridRasterLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.Label;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.UiUtils;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * Demonstrates usage of MapBoxMapLayer - online tile-based map source
 * 
 * It is almost like basic tile-based source, with some important additions:
 * a) load metadata as json, here map legend HTML and template for map popup is loaded from this
 * b) load UTFGrid tiles as json in addition to map graphics. This enables tooltip on map click,
 *  even through it is raster data source
 *  Note that from SDK point of view the grid tooltip is an invisible Marker in MarkerLayer, which has open Label.
 *
 *  jMoustache library is required to resolve popup templates.
 * 
 * @author jaak
 *
 */
public class MapBoxMapActivity extends Activity {

    // Please configure your ID and account here
    private static final String MAPBOX_MAPID = "geography-class";
    private static final String MAPBOX_ACCOUNT = "nutiteq";

    /**
     * Asynchronous task for loading metadata.
     */
    public class LoadMetadataTask extends AsyncTask<Void, Void, JSONObject> {

        private UTFGridLayerEventListener mapListener;
        private String account;
        private String map;

        public LoadMetadataTask(UTFGridLayerEventListener mapListener, String account, String map){
            this.mapListener = mapListener;
            this.account = account;
            this.map = map;
        }

        protected JSONObject doInBackground(Void... v) {
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
                UiUtils.addWebView((RelativeLayout) findViewById(R.id.mainView), MapBoxMapActivity.this, legend, 320, 300);
            }else{
                Log.debug("no legend found");
            }

        }
    }

    private MapView mapView;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("mapbox");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            // recreate listener
            UTFGridLayerEventListener oldListener = (UTFGridLayerEventListener ) mapView.getOptions().getMapListener();
            UTFGridLayerEventListener mapListener = new UTFGridLayerEventListener(this, mapView, oldListener.getLayer(), oldListener.getClickMarker());
            mapView.getOptions().setMapListener(mapListener);
            mapView.getOptions().setMapListener(null);

            LoadMetadataTask task = new LoadMetadataTask(mapListener, MAPBOX_ACCOUNT, MAPBOX_MAPID);
            task.execute();
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            mapView.setComponents(components);
        }


        // 3. Define map layer for basemap - mandatory.

        // MapBox Streets
        //		final MapBoxMapLayer mapLayer = new MapBoxMapLayer(new EPSG3857(), 0, 19, 333,
        //				MAPBOX_ACCOUNT, "map-j6a1wkx0");

        // MapBox Satellite
        MBOnlineRasterDataSource dataSource = new MBOnlineRasterDataSource(new EPSG3857(), 0, 19, MAPBOX_ACCOUNT, MAPBOX_MAPID);
        UTFGridRasterLayer mapLayer = new UTFGridRasterLayer(dataSource, dataSource, 334);

        mapView.getLayers().setBaseLayer(mapLayer);

        // add a layer and marker for click labels
        // define small invisible Marker, as Label requires some Marker 
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.01f).setColor(0).build();

        //  define label as WebView to show HTML
        WebView labelView = new WebView(this); 
        // It is important to set size, exception will come otherwise
        labelView.layout(0, 0, 150, 150);
        Label label = new ViewLabel("", labelView, new Handler());

        Marker clickMarker = new Marker(new MapPos(0,0), label, markerStyle, null);

        MarkerLayer clickMarkerLayer = new MarkerLayer(new EPSG3857());
        clickMarkerLayer.add(clickMarker);
        mapView.getLayers().addLayer(clickMarkerLayer);

        // add event listener
        UTFGridLayerEventListener mapListener = new UTFGridLayerEventListener(this, mapView, mapLayer, clickMarker);
        mapView.getOptions().setMapListener(mapListener);

        // download Metadata, add legend and tooltip listener hooks
        LoadMetadataTask task = new LoadMetadataTask(mapListener, MAPBOX_ACCOUNT, MAPBOX_MAPID);
        task.execute();

        // set initial map view camera - optional. "World view" is default
        // Location: Estonia
        //mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.5f, 58.3f));
        mapView.setFocusPoint(new MapPos(2745202.3f, 8269676.0f));
        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(5.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
        mapView.setTilt(90.0f);


        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(false);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(true);
        mapView.getOptions().setKineticPanning(true);
        mapView.getOptions().setDoubleClickZoomIn(true);
        mapView.getOptions().setDualClickZoomOut(true);

        // mapView.getConstraints().setTiltRange(new Range(90,90));

        // set sky bitmap - optional, default - white
        mapView.getOptions().setSkyDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setSkyOffset(4.86f);
        mapView.getOptions().setSkyBitmap(
                UnscaledBitmapLoader.decodeResource(getResources(),
                        R.drawable.sky_small));

        // Map background, visible if no map tiles loaded - optional, default - white
        mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setBackgroundPlaneBitmap(
                UnscaledBitmapLoader.decodeResource(getResources(),
                        R.drawable.background_plane));
        mapView.getOptions().setClearColor(Color.WHITE);

        // configure texture caching - optional, suggested
        mapView.getOptions().setTextureMemoryCacheSize(20 * 1024 * 1024);
        mapView.getOptions().setCompressedMemoryCacheSize(8 * 1024 * 1024);

        // define online map persistent caching - optional, suggested. Default - no caching
        // FIXME jaakl: currently disabled caching to enforce loading utfgrid. there is no utfgrid cache yet
        //        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache_mapbox").getPath());
        // set persistent raster cache limit to 100MB
        mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);

        // 4. zoom buttons using Android widgets - optional
        // get the zoomcontrols that was defined in main.xml
        ZoomControls zoomControls = (ZoomControls) findViewById(R.id.zoomcontrols);
        // set zoomcontrols listeners to enable zooming
        zoomControls.setOnZoomInClickListener(new View.OnClickListener() {
            public void onClick(final View v) {
                mapView.zoomIn();
            }
        });
        zoomControls.setOnZoomOutClickListener(new View.OnClickListener() {
            public void onClick(final View v) {
                mapView.zoomOut();
            }
        });

    }

    @Override
    protected void onStart() {
        mapView.startMapping();
        super.onStart();
    }

    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

    public MapView getMapView() {
        return mapView;
    }

}

