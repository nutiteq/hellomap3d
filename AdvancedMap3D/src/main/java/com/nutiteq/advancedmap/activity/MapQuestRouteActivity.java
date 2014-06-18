package com.nutiteq.advancedmap.activity;

import java.util.HashMap;
import java.util.Map;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.RouteMapEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.services.routing.MapQuestDirections;
import com.nutiteq.services.routing.Route;
import com.nutiteq.services.routing.RouteActivity;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * Online routing using MapQuest Open Directions API
 * 
 * Routing class MapQuestDirections is used to 
 * 1. Calculate route using online API, get it as Route object
 * 2. Create a Line which is stored to a routeLayer GeometryLayer
 * 3. Create Markers to each turn point, show them on markerLayer.
 *   Marker images are here loaded from URLs what are given in route results, using MqLoadInstructionImagesTask
 * 
 * Interfaces:
 *  RouteActivity - callback to enable Activity to get Routing results asynchronously
 * 
 * Classes:
 *  routing.MapQuestDirections implements MapQuestDirections Open routing API as described in
 * http://open.mapquestapi.com/directions/ 
 * 
 * 
 * Note: You have to use your own MapQuest API key if you use it in live application. 
 * 
 * @author jaak
 *
 */

public class MapQuestRouteActivity extends Activity implements RouteActivity{

    private static final float MARKER_SIZE = 0.4f;
    private static final String MAPQUEST_KEY = "Fmjtd%7Cluub2qu82q%2C70%3Do5-961w1w";
    private MapView mapView;
    private GeometryLayer routeLayer;
    private Marker startMarker;
    private Marker stopMarker;
    private MarkerLayer markerLayer;
    private MapQuestDirections directionsService;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        Log.enableAll();
        Log.setTag("mq_route");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);


        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            // add event listener
            RouteMapEventListener mapListener = new RouteMapEventListener(this);
            mapView.getOptions().setMapListener(mapListener);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            mapView.setComponents(components);
            // add event listener
            RouteMapEventListener mapListener = new RouteMapEventListener(this);
            mapView.getOptions().setMapListener(mapListener);
        }

        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 12);
        mapView.getLayers().setBaseLayer(mapLayer);

        // Location: London
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-0.1f, 51.51f));
        mapView.setZoom(14.0f);

        // routing layers
        routeLayer = new GeometryLayer(new EPSG3857());
        mapView.getLayers().addLayer(routeLayer);


        // create markers for start & end, and a layer for them
        Bitmap olMarker = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.olmarker);
        StyleSet<MarkerStyle> startMarkerStyleSet = new StyleSet<MarkerStyle>(
                MarkerStyle.builder().setBitmap(olMarker).setColor(Color.GREEN)
                .setSize(MARKER_SIZE).build());
        startMarker = new Marker(new MapPos(0, 0), new DefaultLabel("Start"),
                startMarkerStyleSet, null);

        StyleSet<MarkerStyle> stopMarkerStyleSet = new StyleSet<MarkerStyle>(
                MarkerStyle.builder().setBitmap(olMarker).setColor(Color.RED)
                .setSize(MARKER_SIZE).build());
        stopMarker = new Marker(new MapPos(0, 0), new DefaultLabel("Stop"),
                stopMarkerStyleSet, null);

        markerLayer = new MarkerLayer(new EPSG3857());
        mapView.getLayers().addLayer(markerLayer);

        // make markers invisible until we need them
//      startMarker.setVisible(false);
//      stopMarker.setVisible(false);
        markerLayer.add(startMarker);
        markerLayer.add(stopMarker);

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
        mapView.setTilt(90.0f);


        mapView.getOptions().setTileZoomLevelBias(-0.5f);

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

        // zoom buttons using Android widgets - optional
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

        Toast.makeText(getApplicationContext(), "Click on map to set route start and end", Toast.LENGTH_SHORT).show();
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

    @Override
    public void setStartMarker(MapPos startPos) {
        routeLayer.clear();
        markerLayer.clear();

        markerLayer.add(startMarker);
        startMarker.setMapPos(startPos);
        startMarker.setVisible(true);
    }

    @Override
    public void setStopMarker(MapPos pos) {
        markerLayer.add(stopMarker);
        stopMarker.setMapPos(pos);
        stopMarker.setVisible(true);
    }

    @Override
    public void showRoute(final double fromLat, final double fromLon,
            final double toLat, final double toLon) {

        Log.debug("calculating path " + fromLat + "," + fromLon + " to "
                + toLat + "," + toLon);

        Projection proj = mapView.getLayers().getBaseLayer().getProjection();

        StyleSet<LineStyle> routeLineStyle = new StyleSet<LineStyle>(LineStyle.builder().setWidth(0.05f).setColor(0xff9d7050).build());
        Map<String, String> routeOptions = new HashMap<String,String>();
        routeOptions.put("unit", "K"); // K - km, M - miles
        routeOptions.put("routeType", "fastest");
        // Add other route options here, see http://open.mapquestapi.com/directions/

        directionsService = new MapQuestDirections(this, new MapPos(fromLon, fromLat), new MapPos(toLon, toLat), routeOptions, MAPQUEST_KEY, proj, routeLineStyle);
        directionsService.route();
    }

    @Override
    public void routeResult(Route route) {

        if(route.getRouteResult() != Route.ROUTE_RESULT_OK){
            Toast.makeText(this, "Route error", Toast.LENGTH_LONG).show();
            return;
        }

        markerLayer.clear();
        routeLayer.clear();

        routeLayer.add(route.getRouteLine());
        Log.debug("route line points: "+route.getRouteLine().getVertexList().size());
//      Log.debug("route line: "+route.getRouteLine().toString());
        mapView.requestRender();
        Toast.makeText(this, "Route "+route.getRouteSummary(), Toast.LENGTH_LONG).show();
        directionsService.startRoutePointMarkerLoading(markerLayer, MARKER_SIZE);
    }
}

