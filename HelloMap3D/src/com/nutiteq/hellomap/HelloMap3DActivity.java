package com.nutiteq.hellomap;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.util.DisplayMetrics;

import com.nutiteq.MapView;
import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.TMSMapLayer;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * This is minimal example of Nutiteq 3D map app.
 * Also some useful extra configurations are added
 * @author jaak
 *
 */
public class HelloMap3DActivity extends Activity {

    private MapView mapView;
    private LocationListener locationListener;

    @SuppressLint("NewApi")
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
            // just restore configuration and update listener, skip other initializations
            mapView.setComponents(retainObject);
            MyLocationMapEventListener mapListener = (MyLocationMapEventListener) mapView.getOptions().getMapListener();
            mapListener.reset(this, mapView);
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

        adjustMapDpi();
        
//        mapView.getOptions().setFPSIndicator(true);
//        mapView.getOptions().setRasterTaskPoolSize(4);
        
        // set initial map view camera - optional. "World view" is default
        // Location: San Francisco 
        // NB! it must be in base layer projection (EPSG3857), so we convert it from lat and long
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 37.76666666666f));
        // rotation - 0 = north-up
        mapView.setRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(16.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
        mapView.setTilt(65.0f);


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
        //mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
        // set persistent raster cache limit to 100MB
        mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);


        // 5. Add simple marker to map. 
        // define marker style (image, size, color)
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.5f).setColor(Color.WHITE).build();
        // define label what is shown when you click on marker
        Label markerLabel = new DefaultLabel("San Francisco", "Here is a marker");
        
        // define location of the marker, it must be converted to base map coordinate system
        MapPos markerLocation = mapLayer.getProjection().fromWgs84(-122.416667f, 37.766667f);

        // create layer and add object to the layer, finally add layer to the map. 
        // All overlay layers must be same projection as base layer, so we reuse it
        MarkerLayer markerLayer = new MarkerLayer(mapLayer.getProjection());
        markerLayer.add(new Marker(markerLocation, markerLabel, markerStyle, null));
        mapView.getLayers().addLayer(markerLayer);

        // add event listener
        MyLocationMapEventListener mapListener = new MyLocationMapEventListener(this, mapView);
        mapView.getOptions().setMapListener(mapListener);
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        Log.debug("onRetainNonConfigurationInstance");
        return this.mapView.getComponents();
    }
    
    
    @Override
    protected void onStart() {
        super.onStart();
        // 4. Start the map - mandatory.
        mapView.startMapping();

        // add GPS My Location functionality 
        MyLocationCircle locationCircle = new MyLocationCircle();
        MyLocationMapEventListener mapListener = (MyLocationMapEventListener) mapView.getOptions().getMapListener();
        mapListener.setLocationCircle(locationCircle);
        initGps(locationCircle);
    }

    @Override
    protected void onStop() {
        // remove GPS support, otherwise we will leak memory
        deinitGps();

        super.onStop();
        // Note: it is recommended to move startMapping() call to onStart method and implement onStop method (call MapView.stopMapping() from onStop). 
        mapView.stopMapping();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
    
    protected void initGps(final MyLocationCircle locationCircle) {
        final Projection proj = mapView.getLayers().getBaseLayer().getProjection();
        
        locationListener = new LocationListener() {
            @Override
            public void onLocationChanged(Location location) {
                 if (locationCircle != null) {
                     locationCircle.setLocation(proj, location);
                     locationCircle.setVisible(true);
                     
                     // recenter automatically to GPS point
                     // TODO in real app it can be annoying this way, add extra control that it is done only once
                     mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(location.getLongitude(), location.getLatitude()));
                 }
            }

            @Override
            public void onStatusChanged(String provider, int status, Bundle extras) {
                Log.debug("GPS onStatusChanged "+provider+" to "+status);
            }

            @Override
            public void onProviderEnabled(String provider) {
                Log.debug("GPS onProviderEnabled");
            }

            @Override
            public void onProviderDisabled(String provider) {
                Log.debug("GPS onProviderDisabled");
            }
        };
        
        LocationManager locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);
        
        locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 10000, 100, locationListener);
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000, 0, locationListener);

    }
    
    protected void deinitGps() {
        // remove listeners from location manager - otherwise we will leak memory
        LocationManager locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);
        locationManager.removeUpdates(locationListener);    
    }

    // adjust zooming to DPI, so texts on rasters will be not too small
    // useful for non-retina rasters, they would look like "digitally zoomed"
    private void adjustMapDpi() {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        float dpi = metrics.densityDpi;
        // following is equal to  -log2(dpi / DEFAULT_DPI)
        float adjustment = (float) - (Math.log(dpi / DisplayMetrics.DENSITY_HIGH) / Math.log(2));
        Log.debug("adjust DPI = "+dpi+" as zoom adjustment = "+adjustment);
        mapView.getOptions().setTileZoomLevelBias(adjustment / 2.0f);
    }
}