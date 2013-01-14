package com.nutiteq.advancedmap;

import java.util.Vector;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;

import com.nutiteq.MapView;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.db.DBLayer;
import com.nutiteq.geometry.Marker;
import com.nutiteq.layers.vector.SpatialLiteDb;
import com.nutiteq.layers.vector.SpatialiteLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.TMSMapLayer;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;
import com.nutiteq.vectorlayers.NMLModelDbLayer;

public class AdvancedMapActivity extends Activity {

    private MapView mapView;

    
    // force to load proj library for spatialite
    static {
        try {
          System.loadLibrary("proj");
        } catch (Throwable t) {
            System.err.println("Unable to load proj: " + t);
        }
        }

    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("hellomap");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            mapView.startMapping();
            return;
        } else {
            // 2. create and set MapView components - mandatory
            mapView.setComponents(new Components());
        }

        // 3. Define map layer for basemap - mandatory.
        // Here we use MapQuest open tiles
        // Almost all online tiled maps use EPSG3857 projection.
        TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 0, 18, 0,
                "http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");

        mapView.getLayers().setBaseLayer(mapLayer);

        // set initial map view camera - optional. "World view" is default
        // Location: San Francisco 
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 37.76666666666f));

        // Romania
        mapView.setFocusPoint(2901450,5528971);
        // rotation - 0 = north-up
        mapView.setRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(14.0f);
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
        mapView.getOptions().setTextureMemoryCacheSize(40 * 1024 * 1024);
        mapView.getOptions().setCompressedMemoryCacheSize(8 * 1024 * 1024);
        
        // define online map persistent caching - optional, suggested. Default - no caching
        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
        // set persistent raster cache limit to 100MB
        mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);

        // 4. Start the map - mandatory
        mapView.startMapping();

        addLayers(mapLayer.getProjection());
        
    }


    private void addLayers(Projection proj) {
        
        // ** Add simple marker to map. 
        // define marker style (image, size, color)
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.5f).setColor(Color.WHITE).build();
        // define label what is shown when you click on marker
        Label markerLabel = new DefaultLabel("San Francisco", "Here is a marker");
        
        // define location of the marker, it must be converted to base map coordinate system
        MapPos markerLocation = proj.fromWgs84(-122.416667f, 37.766667f);

        // create layer and add object to the layer, finally add layer to the map. 
        // All overlay layers must be same projection as base layer, so we reuse it
        MarkerLayer markerLayer = new MarkerLayer(proj);
        markerLayer.add(new Marker(markerLocation, markerLabel, markerStyle, null));
        mapView.getLayers().addLayer(markerLayer);

        // ** OSM 3D building layer, visible from zoom 15 
        Polygon3DStyle polygon3DStyle = Polygon3DStyle.builder().setColor(Color.BLACK | 0x40ffffff).build();
        StyleSet<Polygon3DStyle> polygon3DStyleSet = new StyleSet<Polygon3DStyle>(null);
        polygon3DStyleSet.setZoomStyle(15, polygon3DStyle);

        // ** 3D OpenStreetMap house "shoebox" layer
//        Polygon3DOSMLayer osm3dLayer = new Polygon3DOSMLayer(new EPSG3857(), 0.500f, 200, polygon3DStyleSet);
//        mapView.getLayers().addLayer(osm3dLayer);

        // define style for 3D to define minimum zoom = 0 
        ModelStyle modelStyle = ModelStyle.builder().build();
        StyleSet<ModelStyle> modelStyleSet = new StyleSet<ModelStyle>(modelStyle);
        
        // ** 3D Model layer
        NMLModelDbLayer modelLayer = new NMLModelDbLayer(new EPSG3857(),
                "/sdcard/buildings.sqlite", modelStyleSet);
        
        mapView.getLayers().addLayer(modelLayer);
        
        
        // ** Spatialite
        // pick just first geotable from the database and add as layer
        // load metadata for picking first table
        String dbPath = Environment.getExternalStorageDirectory().getPath()+"/mapxt/romania_sp3857.sqlite";
        int minZoom = 10;
        
        SpatialLiteDb spatialLite = new SpatialLiteDb(dbPath);
        Vector<DBLayer> dbMetaData = spatialLite.qrySpatialLayerMetadata();

        for(DBLayer dbLayer : dbMetaData){
            Log.debug("layer: "+dbLayer.table+" "+dbLayer.type+" geom:"+dbLayer.geomColumn);
        }
        
        // set styles for all 3 object types: point, line and polygon
        StyleSet<PointStyle> pointStyleSet = new StyleSet<PointStyle>();
        PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker).setSize(0.2f).setColor(Color.GREEN).build();
        pointStyleSet.setZoomStyle(minZoom,pointStyle);

        StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>();
        lineStyleSet.setZoomStyle(minZoom, LineStyle.builder().setWidth(0.1f).setColor(Color.GREEN).build());

        StyleSet<LineStyle> lineStyleSetHw = new StyleSet<LineStyle>();
        lineStyleSetHw.setZoomStyle(minZoom, LineStyle.builder().setWidth(0.07f).setColor(Color.GRAY).build());
        
        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(Color.BLUE).build();
        StyleSet<PolygonStyle> polygonStyleSet = new StyleSet<PolygonStyle>(null);
        polygonStyleSet.setZoomStyle(minZoom, polygonStyle);
        
        SpatialiteLayer spatialiteLayerPt = new SpatialiteLayer(proj, dbPath, "pt_tourism",
                "GEOMETRY", new String[]{"name"}, 500, pointStyleSet, null, null);
        
        mapView.getLayers().addLayer(spatialiteLayerPt);

        SpatialiteLayer spatialiteLayerLn = new SpatialiteLayer(proj, dbPath, "ln_railway",
                "GEOMETRY", new String[]{"sub_type"}, 500, null, lineStyleSet, null);
        
        mapView.getLayers().addLayer(spatialiteLayerLn);

        SpatialiteLayer spatialiteLayerHw = new SpatialiteLayer(proj, dbPath, "ln_highway",
                "GEOMETRY", new String[]{"name"}, 500, null, lineStyleSetHw, null);
        
        mapView.getLayers().addLayer(spatialiteLayerHw);
        
        SpatialiteLayer spatialiteLayerPoly = new SpatialiteLayer(proj, dbPath, "pg_boundary",
                "GEOMETRY", new String[]{"name"}, 500, null, null, polygonStyleSet);
        
        mapView.getLayers().addLayer(spatialiteLayerPoly);
        
        
    }

}

