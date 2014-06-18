package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.MyLocationMapEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.layers.vector.DriveTimeRegionLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.renderprojections.RenderProjection;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * Shows animated location on map, and fixed drivetime region around user location
 * 
 * Animated location uses Android location API to get coordinates, and accuracy. 
 * Animation is painted in MyLocationMapEventListener.onDrawFrameBefore3D() method using OpenGL.
 * 
 * Additional classes:
 *  maplisteners.MyLocationMapEventListener - called for map updates
 *   MyLocationMapEventListener.MyLocationCircle - knows user location and can do OpenGL drawing for specific frame
 *  
 * Used layer(s):
 *  RasterLayer with TMS tile source for base map
 *  DriveTimeRegionLayer - Vector layer which requests drivetime from Nutiteq demo server, provided coordinates and drive time value
 *      Drive time is shown as polygon on map. Provided coordinates are user GPS location coordinates, 
 *      Drive time value is selected by user with a Seekbar control on map
 *      During data loading progress indicator is shown in application header, using FEATURE_INDETERMINATE_PROGRESS
 * 
 * @author jaak
 *
 */
public class AnimatedLocationActivity extends Activity {

    private MapView mapView;
    private DriveTimeRegionLayer driveTimeLayer;
    private LocationListener locationListener;

    int[] timeValues = new int[] { 1, 5, 10, 15, 30, 60, 90, 120, 240, 480 };

    String[] timeLabels = new String[] { "1 min", "5 min", "10 min", "15 min",
            "30 min", "1 h", "1:30 h", "2 h", "4 h", "8 h" };
    private TextView textView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);


        // spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);


        setContentView(R.layout.main);
        Log.enableAll();
        Log.setTag("gpsmap");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);

            // add map event listener
            MyLocationMapEventListener mapListener = new MyLocationMapEventListener(this, mapView);
            mapView.getOptions().setMapListener(mapListener);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            mapView.setComponents(components);

            // add map event listener
            MyLocationMapEventListener mapListener = new MyLocationMapEventListener(this, mapView);
            mapView.getOptions().setMapListener(mapListener);
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

        // add SeekBar to control DrivingDistance
        SeekBar seekBar = (SeekBar) findViewById(R.id.seekBar);
        seekBar.setVisibility(View.VISIBLE);

        textView = (TextView) findViewById(R.id.textView);
        textView.setVisibility(View.VISIBLE);

        // configure SeekBar
        seekBar.setMax(timeValues.length-1);
        seekBar.setProgress(4);

        seekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                Log.debug("progress to "+progress+" time to "+timeValues[progress]);
                textView.setText(timeLabels[progress]);
                driveTimeLayer.setDistance((float)timeValues[progress]/60.0f);
            }
        });     

        // drivetime region layer
        StyleSet<PolygonStyle> polygonStyleSet = new StyleSet<PolygonStyle>(PolygonStyle.builder().setColor(Color.GREEN & 0x80FFFFFF).build());

        driveTimeLayer = new DriveTimeRegionLayer(mapView.getLayers().getBaseLayer().getProjection(), polygonStyleSet) {
            @Override
            protected void setRunningState(final boolean flag) {
                runOnUiThread(new Runnable() {
                    public void run() {
                        setProgressBarIndeterminateVisibility(flag);
                    }
                });
            }
        };

        // initial values
        driveTimeLayer.setDistance((float)timeValues[seekBar.getProgress()]/60.0f);
        textView.setText(timeLabels[seekBar.getProgress()]);

        mapView.getLayers().addLayer(driveTimeLayer);
    }


    @Override
    protected void onStart() {
        mapView.startMapping();
        super.onStart();

        // add GPS My Location functionality 
        initGps(((MyLocationMapEventListener) mapView.getOptions().getMapListener()).getLocationCircle());
    }

    @Override
    protected void onStop() {
        // remove GPS listener, otherwise we will leak memory
        deinitGps();

        super.onStop();
        mapView.stopMapping();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    protected void initGps(final MyLocationMapEventListener.MyLocationCircle locationCircle) {
        final Projection proj = mapView.getLayers().getBaseLayer().getProjection();
        final RenderProjection renderProj = mapView.getLayers().getBaseLayer().getRenderProjection();

        // create location listener
        locationListener = new LocationListener() {
            @Override
            public void onLocationChanged(Location location) {
                Log.debug("GPS onLocationChanged "+location);
                if (locationCircle != null) {
                    locationCircle.setLocation(proj, renderProj, location);
                    locationCircle.setVisible(true);
                    mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(location.getLongitude(), location.getLatitude()));
                    driveTimeLayer.setMapPos(new MapPos(location.getLongitude(), location.getLatitude()));
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

        // dynamic: just fire all providers with same parameters
        for(String provider : locationManager.getProviders(true)){
            Log.debug("adding location provider "+provider);
            locationManager.requestLocationUpdates(provider, 10000, 500, locationListener);    
        }

        // fixed providers - may not work on some devices
//      locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 10000, 500, locationListener);
//      locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000, 500, locationListener);

    }

    protected void deinitGps() {
        // remove listeners from location manager - otherwise we will leak memory
        LocationManager locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);
        locationManager.removeUpdates(locationListener);    
    }

    public MapView getMapView() {
        return mapView;
    }

}

