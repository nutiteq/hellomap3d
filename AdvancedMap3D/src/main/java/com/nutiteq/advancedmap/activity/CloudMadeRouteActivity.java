package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.util.DisplayMetrics;
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
import com.nutiteq.services.routing.CloudMadeDirections;
import com.nutiteq.services.routing.Route;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * Online routing using CloudMade routing http API
 * 
 * Routing class CloudMadeDirections is used to 
 * 1. Calculate route using online API, get it as Route object
 * 2. Create a Line which is stored to a routeLayer GeometryLayer
 * 3. Create Markers to each turn point, show them on markerLayer.
 *   Marker images with arrows are bundled with this application
 * 
 * Interfaces:
 *  RouteActivity - callback to enable Activity to get Routing results asynchronously
 * 
 * Classes:
 *  routing.CloudMadeDirections implements CloudMade routing API as described in
 * http://developers.cloudmade.com/projects/show/routing-http-api
 * 
 * Resources:
 *  drawable/direction_[down, up, upthenleft, upthenright].png - instruction markers
 * 
 * Note: You have to use your own CloudMade API key if you use it in live application. 
 * 
 * @author jaak
 *
 */
public class CloudMadeRouteActivity extends Activity implements RouteActivity{

    private static final float MARKER_SIZE = 0.3f;
    private static final String CLOUDMADE_KEY = "e12f720d5f2b5499946d2e975088dc89";
    private MapView mapView;
    private GeometryLayer routeLayer;
    private Marker startMarker;
    private Marker stopMarker;
    private Bitmap[] routeImages = new Bitmap[5];
    private MarkerLayer markerLayer;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        Log.enableAll();
        Log.setTag("cloudmade");

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

        // use special style for high-density devices
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        String cloudMadeStyle = "997";

        if(metrics.densityDpi >= DisplayMetrics.DENSITY_HIGH){
            cloudMadeStyle  = "997@2x";
        }

        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://b.tile.cloudmade.com/"+CLOUDMADE_KEY+"/"+cloudMadeStyle+"/256/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
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

        // define images for turns
        // source: http://mapicons.nicolasmollet.com/markers/transportation/directions/directions/
        // TODO: use better structure than plain array for this
        routeImages[CloudMadeDirections.IMAGE_ROUTE_START] = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.direction_up);
        routeImages[CloudMadeDirections.IMAGE_ROUTE_RIGHT] = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.direction_upthenright);
        routeImages[CloudMadeDirections.IMAGE_ROUTE_LEFT] = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.direction_upthenleft);
        routeImages[CloudMadeDirections.IMAGE_ROUTE_STRAIGHT] = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.direction_up);
        routeImages[CloudMadeDirections.IMAGE_ROUTE_END] = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.direction_down);

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
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
        stopMarker.setMapPos(proj.fromWgs84(toLon, toLat));

        CloudMadeDirections directionsService = new CloudMadeDirections(this, new MapPos(fromLon, fromLat), new MapPos(toLon, toLat), CloudMadeDirections.ROUTE_TYPE_CAR, CloudMadeDirections.ROUTE_TYPE_MODIFIER_FASTEST, CLOUDMADE_KEY, proj);
        directionsService.route();
    }

    @Override
    public void routeResult(Route route) {

        if(route.getRouteResult() != Route.ROUTE_RESULT_OK){
            Toast.makeText(this, "Route error", Toast.LENGTH_LONG).show();
            return;
        }

        routeLayer.clear();
        routeLayer.add(route.getRouteLine());
        Log.debug("route line points: "+route.getRouteLine().getVertexList().size());
//      Log.debug("route line: "+route.getRouteLine().toString());
        markerLayer.addAll(CloudMadeDirections.getRoutePointMarkers(routeImages, MARKER_SIZE, route.getInstructions()));
        mapView.requestRender();
        Toast.makeText(this, "Route "+route.getRouteSummary(), Toast.LENGTH_LONG).show();
    }
}

