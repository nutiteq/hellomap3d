package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.View;
import android.webkit.WebView;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.WMSFeatureClickListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.datasources.raster.WMSRasterDataSource;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.Label;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * 
 * Demonstrates WmsLayer - online raster layer for WMS map sources.  
 * 
 * Note that WmsLayer works using map tile boundaries. Therefore it should work for WMSC sources also.
 * 
 * The sample loads one layer from a demo geoserver. In addition to map images it implements
 * GetFeatureInfo request which requests and renders object data if you click on map. Object data is shown
 * as Marker with Label (generated with WebView to support HTML), similar to UTFGridData in MBTiles and 
 * MapBox layers.
 * 
 * Clicks on map are detected using WmsLayerClickListener (a MapListener).
 * 
 * @author jaak
 *
 */
public class WmsMapActivity extends Activity {

    private MapView mapView;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        float dpi = metrics.density;

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
            // re-create listener
            WMSFeatureClickListener oldListener = (WMSFeatureClickListener) mapView.getOptions().getMapListener();
            WMSFeatureClickListener mapListener = new WMSFeatureClickListener(this, mapView, oldListener.getDataSource(), oldListener.getClickMarker());
            mapView.getOptions().setMapListener(mapListener);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            // set stereo view: works if you rotate to landscape and device has HTC 3D or LG Real3D
            mapView.setComponents(components);
        }


        // 3. Define map layer for basemap - mandatory.

        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 19, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);

        // add a layer and marker for click labels
        // define small invisible Marker, as Label requires some Marker 
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.01f).setColor(0).build();

        //  define label as WebView to show HTML
        WebView labelView = new WebView(this); 
        // It is important to set size, exception will come otherwise
        labelView.layout(0, 0, (int)(300 * dpi), (int)(150 * dpi));
        Label label = new ViewLabel("", labelView, new Handler());

        Marker clickMarker = new Marker(new MapPos(0,0), label, markerStyle, null);

        MarkerLayer clickMarkerLayer = new MarkerLayer(new EPSG3857());
        clickMarkerLayer.add(clickMarker);
        mapView.getLayers().addLayer(clickMarkerLayer);


        // add WMS layer as overlay

        String url = "http://kaart.maakaart.ee/geoserver/wms?transparent=true&";
        String layers = "topp:states";

        // note that data projection is different: WGS84 (EPSG:4326)
        WMSRasterDataSource wmsDataSource = new WMSRasterDataSource(new EPSG4326(), 0, 19, url, "", layers, "image/png");
        RasterLayer wmsLayer = new RasterLayer(wmsDataSource, 1012);
        wmsLayer.setFetchPriority(-5);
        mapView.getLayers().addLayer(wmsLayer);

        // add event listener for clicks on WMS map
        WMSFeatureClickListener mapListener = new WMSFeatureClickListener(this, mapView, wmsDataSource, clickMarker);
        mapView.getOptions().setMapListener(mapListener);



        // set initial map view camera - optional. "World view" is default
        // Location: USA
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-90f, 35f));

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(2.5f);


        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(true);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(false);
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
//      mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache_wms").getPath());
        // set persistent raster cache limit to 100MB
//      mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);

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

