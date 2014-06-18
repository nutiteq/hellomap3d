package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * Address search / Geocoding sample.
 * 
 * Sample uses Android searchable interface, which is linked to Activity via AndroidManifest file.
 * 
 * Classes:
 * 1. geocode.MapQuestGeocoder.java Geocoder implementation, uses MapQuest Open API REST API Map
 * 
 * 2. mapquest.SearchQueryResults.java - ListView which initiates real search, and shows results as ListView
 * 
 * 3. mapquest.SearchRecentSuggestionsProvider.java - stores last search terms to memory
 * 
 * 4. AddressSearchActivity.java opens Android default search UI. Search result comes from resuming 
 *      from search results activity, this is shown on map, and map is re-centered to found result.
 * 
 * 5. Resources: values/strings.xml, layout/search_query_results.xml and layout/searchrow.xml define ListView.
 *      xml/searchable.xml - needed for Android searchable interface
 *
 * Used layer(s):
 *  RasterLayer with TMS tile source for base map
 *        
 * @author jaak
 *
 */
public class AddressSearchActivity extends Activity {

    private static Marker searchResult;
    private MapView mapView;
    private MarkerLayer searchMarkerLayer;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        Log.enableAll();
        Log.setTag("addresssearch");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            mapView.setComponents(components);
        }


        // 3. Define map layer for basemap - mandatory.
        // Here we use MapQuest open tiles
        // Almost all online tiled maps use EPSG3857 projection.
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);

        // Location: Estonia
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.5f, 58.3f));

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(5.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
        mapView.setTilt(90.0f);

        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(true);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(true);
        mapView.getOptions().setKineticPanning(true);
        mapView.getOptions().setDoubleClickZoomIn(true);
        mapView.getOptions().setDualClickZoomOut(true);

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
        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
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

        // create layer for search result 
        searchMarkerLayer = new MarkerLayer(mapView.getLayers().getBaseLayer().getProjection());
//      searchResult = new Marker(new MapPos(0,0), null, (MarkerStyle) null, null);
//      searchResult.setVisible(false);
        mapView.getLayers().addLayer(searchMarkerLayer);

        // open search right away
        // search class is defined in AndroidManifest.xml as android.intent.action.SEARCH
        onSearchRequested();
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

    @Override 
    protected void onResume() {

        super.onResume();
        Log.debug("onResume");

        if (searchResult != null && searchResult.getMapPos().x != 0) {
            // recenter to searchResult
            Log.debug("Add search result and recenter to it: ");
            if (searchResult.getLayer() == null) {
                searchMarkerLayer.add(searchResult);
            }
            mapView.setFocusPoint(searchResult.getMapPos());
            //searchResult.setVisible(true);
            mapView.selectVectorElement(searchResult);
        }
    }

    public static void setSearchResult(Marker marker) {
        Log.debug("Search result selected: " + marker.getMapPos());
        searchResult = marker;
    }

    public MapView getMapView() {
        return mapView;
    }

}

