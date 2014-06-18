package com.nutiteq.advancedmap.activity;

import java.io.InputStream;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.hardware.SensorManager;
import android.location.Location;
import android.location.LocationManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.components.Vector3D;
import com.nutiteq.geometry.NMLModel;
import com.nutiteq.log.Log;
import com.nutiteq.nmlpackage.NMLPackage;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.OrientationManager;
import com.nutiteq.utils.OrientationManager.OnChangedListener;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.NMLModelLayer;
import com.nutiteq.vectorlayers.NMLModelOnlineLayer;

/**
 * Map Rotated and moved based on sensors: Compass and GPS. 
 * Shows user location as 3D model, as accurately as GPS allows
 */
public class CompassMapActivity extends Activity implements OnChangedListener {

    // start pos is shown until GPS fix is received
    private static final MapPos START_MAPPOS = new MapPos(-87.61866f, 41.88282f);
    private MapView mapView;
    private OrientationManager mOrientationManager;
    NMLModel locationMarkerModel;
    private StyleSet<ModelStyle> modelStyleSet;
    private NMLModelOnlineLayer modelLayer;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

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

        // add 3d map
        // define style for 3D to define minimum zoom = 14
        ModelStyle modelStyle = ModelStyle.builder().build();
        modelStyleSet = new StyleSet<ModelStyle>(null);
        modelStyleSet.setZoomStyle(14, modelStyle);

        online3DLayer("http://aws-lb.nutiteq.ee/nml/nmlserver3.php?data=chicago");
        
        // Location: Estonia
        MapPos mapPos = mapView.getLayers().getBaseLayer().getProjection().fromWgs84(START_MAPPOS.x,START_MAPPOS.y);
        mapView.setFocusPoint(mapPos);

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(15.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
        mapView.setTilt(30.0f);

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
        


        // 3d object
        // define style for 3D to define minimum zoom = 0
        ModelStyle modelMarkerStyle = ModelStyle.builder().build();
        StyleSet<ModelStyle> modelStyleSet = new StyleSet<ModelStyle>(modelMarkerStyle);

        // create layer and an model
        NMLModelLayer locationMarkerLayer = new NMLModelLayer(new EPSG3857());
        try {
            InputStream is = this.getResources().openRawResource(R.raw.man3d);
            NMLPackage.Model nmlModel = NMLPackage.Model.parseFrom(is);
            // set initial position for the milk truck
            locationMarkerModel = new NMLModel(mapPos, null, modelStyleSet, nmlModel, null);
            // set size, 10 is clear oversize, but this makes it visible
            locationMarkerModel.setScale(new Vector3D(50, 50, 50));
            locationMarkerLayer.add(locationMarkerModel);
            mapView.getLayers().addLayer(locationMarkerLayer);

        }
        catch (Exception e) {
            e.printStackTrace();
        }

        // use OrientationManager

        SensorManager sensorManager =
                (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        LocationManager locationManager =
                (LocationManager) getSystemService(Context.LOCATION_SERVICE);

        mOrientationManager = new OrientationManager(sensorManager, locationManager);

        mOrientationManager.addOnChangedListener(this);
        mOrientationManager.start();

        Toast.makeText(this, "Please wait for GPS fix...",Toast.LENGTH_LONG).show();

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

    // Switch online 3D Model layer with given URL
    private void online3DLayer(String dataset) {

        if(modelLayer != null)
            mapView.getLayers().removeLayer(modelLayer);

        modelLayer = new NMLModelOnlineLayer(new EPSG3857(),
                dataset, modelStyleSet);

        modelLayer.setMemoryLimit(40*1024*1024);

        modelLayer.setPersistentCacheSize(60*1024*1024);
        modelLayer.setPersistentCachePath(this.getDatabasePath("nmlcache_"+dataset.substring(dataset.lastIndexOf("="))).getPath());

        modelLayer.setLODResolutionFactor(0.3f);
        mapView.getLayers().addLayer(modelLayer);
    }


    @Override
    public void onOrientationChanged(OrientationManager orientationManager) {

        float azimut = orientationManager.getHeading(); // orientation contains: azimut, pitch and roll
        mapView.setMapRotation(360-azimut);
        locationMarkerModel.setRotation(new Vector3D(0, 0, 1), azimut-90);
    }

    @Override
    public void onLocationChanged(OrientationManager orientationManager) {
        Location location = orientationManager.getLocation();
        Log.debug("new location: "+location);
        MapPos mapPos = mapView.getLayers().getBaseProjection().fromWgs84(location.getLongitude(), location.getLatitude());
        mapView.setFocusPoint(mapPos);

        locationMarkerModel.setMapPos(mapPos);

    }

    @Override
    public void onAccuracyChanged(OrientationManager orientationManager) {
        Log.warning("interference!");
    }



}

