package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;
import java.util.ArrayList;
import java.util.List;

import org.mapsforge.android.maps.mapgenerator.JobTheme;
import org.mapsforge.core.GeoPoint;
import org.mapsforge.map.reader.header.MapFileInfo;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Path;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.graphhopper.GHRequest;
import com.graphhopper.GHResponse;
import com.graphhopper.GraphHopper;
import com.graphhopper.util.PointList;
import com.graphhopper.util.StopWatch;
import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.RouteMapEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.datasources.raster.MapsforgeRasterDataSource;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.services.routing.Route;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * 
 * 
 * Uses Graphhopper library to calculate offline routes
 * 
 * Requires that user has downloaded Graphhopper data package to SDCARD. 
 * 
 * See https://github.com/nutiteq/hellomap3d/wiki/Offline-routing for details and downloads
 * 
 * @author jaak
 *
 */
public class GraphhopperRouteActivity extends Activity implements FilePickerActivity, RouteActivity{

    private MapView mapView;
    private GraphHopper gh;
    protected boolean errorLoading;
    protected boolean graphLoaded;
    protected boolean shortestPathRunning;
    private GeometryLayer routeLayer;
    private Marker startMarker;
    private Marker stopMarker;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        Log.enableAll();
        Log.setTag("graphhopper");

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


        // read filename from extras
        Bundle b = getIntent().getExtras();
        String mapFile = b.getString("selectedFile");

        // open graph
        openGraph(mapFile.substring(0, mapFile.lastIndexOf("-gh/")));
        //openGraph("/sdcard/mapxt/graphhopper/new-york");

        //  use mapsforge map as offline base
        JobTheme renderTheme = MapsforgeRasterDataSource.InternalRenderTheme.OSMARENDER;
        MapsforgeRasterDataSource dataSource = new MapsforgeRasterDataSource(new EPSG3857(), 0, 20, mapFile, renderTheme);
        RasterLayer mapLayer = new RasterLayer(dataSource, 1044);

        mapView.getLayers().setBaseLayer(mapLayer);

        // set initial map view camera from database
        MapFileInfo fileInfo = dataSource.getMapDatabase().getMapFileInfo();
        GeoPoint center = fileInfo.startPosition;
        if(center != null){
            MapPos mapCenter = new MapPos(center.getLongitude(), center.getLatitude(),0);
            Log.debug("center: "+mapCenter);
            mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(mapCenter.x,mapCenter.y));
        }

        if(fileInfo.startZoomLevel != null){
            mapView.setZoom(fileInfo.startZoomLevel);
        }else{
            mapView.setZoom(10.0f);
        }

        // routing layers
        routeLayer = new GeometryLayer(new EPSG3857());
        mapView.getLayers().addLayer(routeLayer);


        // create markers for start & end, and a layer for them
        Bitmap olMarker = UnscaledBitmapLoader.decodeResource(getResources(),
                R.drawable.olmarker);
        StyleSet<MarkerStyle> startMarkerStyleSet = new StyleSet<MarkerStyle>(
                MarkerStyle.builder().setBitmap(olMarker).setColor(Color.GREEN)
                .setSize(0.2f).build());
        startMarker = new Marker(new MapPos(0, 0), new DefaultLabel("Start"),
                startMarkerStyleSet, null);

        StyleSet<MarkerStyle> stopMarkerStyleSet = new StyleSet<MarkerStyle>(
                MarkerStyle.builder().setBitmap(olMarker).setColor(Color.RED)
                .setSize(0.2f).build());
        stopMarker = new Marker(new MapPos(0, 0), new DefaultLabel("Stop"),
                stopMarkerStyleSet, null);

        MarkerLayer markerLayer = new MarkerLayer(new EPSG3857());
        mapView.getLayers().addLayer(markerLayer);

        markerLayer.add(startMarker);
        markerLayer.add(stopMarker);

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
    public void showRoute(final double fromLat, final double fromLon,
            final double toLat, final double toLon) {

        Log.debug("calculating path "+fromLat+","+fromLon+" to "+toLat+","+toLon);
        if(!graphLoaded){
            Log.error("graph not loaded yet");
            Toast.makeText(getApplicationContext(), "graph not loaded yet, cannot route", Toast.LENGTH_LONG).show();
            return;
        }

        Projection proj = mapView.getLayers().getBaseLayer().getProjection();
        stopMarker.setMapPos(proj.fromWgs84(toLon, toLat));


        new AsyncTask<Void, Void, GHResponse>() {
            float time;

            protected GHResponse doInBackground(Void... v) {
                StopWatch sw = new StopWatch().start();
                GHRequest req = new GHRequest(fromLat, fromLon, toLat, toLon)
                .setAlgorithm("dijkstrabi");
                GHResponse resp = gh.route(req);
                time = sw.stop().getSeconds();
                return resp;
            }

            protected void onPostExecute(GHResponse res) {
                Log.debug("from:" + fromLat + "," + fromLon + " to:" + toLat + ","
                        + toLon + " found path with distance:" + res.getDistance()
                        / 1000f + ", nodes:" + res.getPoints().getSize() + ", time:"
                        + time + " " + res.getDebugInfo());
                Toast.makeText(getApplicationContext(), "the route is " + (int) (res.getDistance() / 100) / 10f
                        + "km long, time:" + res.getTime() / 60f + "min, calculation time:" + time, Toast.LENGTH_LONG).show();

                routeLayer.clear();
                routeLayer.add(createPolyline(startMarker.getMapPos(), stopMarker.getMapPos(), res));


                shortestPathRunning = false;
            }
        }.execute();
    }


    // creates Nutiteq line from GraphHopper response
    protected Line createPolyline(MapPos start, MapPos end, GHResponse response) {

        StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>(LineStyle.builder().setWidth(0.05f).setColor(Color.BLUE).build());

        Projection proj = mapView.getLayers().getBaseLayer().getProjection();
        int points = response.getPoints().getSize();
        List<MapPos> geoPoints = new ArrayList<MapPos>(points+2);
        PointList tmp = response.getPoints();
        geoPoints.add(start);
        for (int i = 0; i < points; i++) {
            geoPoints.add(proj.fromWgs84(tmp.getLongitude(i), tmp.getLatitude(i)));
        }
        geoPoints.add(end);

        String labelText = "" + (int) (response.getDistance() / 100) / 10f
                + "km, time:" + response.getTime() / 60f + "min";

        return new Line(geoPoints, new DefaultLabel("Route", labelText), lineStyleSet, null);
    }

    // opens GraphHopper graph file
    void openGraph(final String graphFile) {
        Log.debug("loading graph (" + graphFile
                + ") ... ");
        new AsyncTask<Void, Void, Path>() {
            protected Path doInBackground(Void... v) {
                try {
                    GraphHopper tmpHopp = new GraphHopper().forMobile();
                    tmpHopp.setCHShortcuts(true, true);
                    tmpHopp.load(graphFile);
                    Log.debug("found graph with " + tmpHopp.getGraph().getNodes() + " nodes");
                    gh = tmpHopp;
                    graphLoaded = true;
                } catch (Throwable t) {
                    Log.error(t.getMessage());
                    errorLoading = true;
                    return null;
                }
                return null;
            }

            protected void onPostExecute(Path o) {
                if(graphLoaded)
                    Toast.makeText(getApplicationContext(), "graph loaded, click on map to set route start and end", Toast.LENGTH_SHORT).show();
                else
                    Toast.makeText(getApplicationContext(), "graph loading problem", Toast.LENGTH_SHORT).show();
            }
        }.execute();
    }


    public MapView getMapView() {
        return mapView;
    }

    @Override
    public String getFileSelectMessage() {
        return "Select .map file in graphhopper graph (<mapname>_gh folder)";
    }

    @Override
    public FileFilter getFileFilter() {
        return new FileFilter() {
            @Override
            public boolean accept(File file) {
                // accept only readable files
                if (file.canRead()) {
                    if (file.isDirectory()) {
                        // allow to select any directory
                        return true;
                    } else if (file.isFile()
                            && (file.getName().endsWith(".ghz") || file.getName().endsWith(".map"))) {
                        // accept files with given extension
                        return true;
                    }
                }
                return false;
            };
        };
    }

    @Override
    public void setStartMarker(MapPos startPos) {
        routeLayer.clear();
        stopMarker.setVisible(false);
        startMarker.setMapPos(startPos);
        startMarker.setVisible(true);
    }

    @Override
    public void setStopMarker(MapPos pos) {
        stopMarker.setMapPos(pos);
        stopMarker.setVisible(true);
    }


    @Override
    public void routeResult(Route route) {
        // not used here, as Graphhopper routing is called from same application, 
        // without using Route object and separate service class
    }

}
