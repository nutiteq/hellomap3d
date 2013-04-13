package com.nutiteq.advancedmap;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.view.View;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.maplisteners.MyLocationMapEventListener;
import com.nutiteq.advancedmap.maplisteners.MyLocationMapEventListener.MyLocationCircle;
import com.nutiteq.components.Components;
import com.nutiteq.components.Options;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * Basic map, same as HelloMap
 * 
 * @author jaak
 *
 */
public class AnimatedLocationActivity extends Activity {

	private MapView mapView;
    
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
			mapView.startMapping();
			return;
		} else {
			// 2. create and set MapView components - mandatory
		      Components components = new Components();
		      mapView.setComponents(components);
		      }


        // 3. Define map layer for basemap - mandatory.
        // Here we use MapQuest open tiles
        // Almost all online tiled maps use EPSG3857 projection.
        TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 5, 18, 0,
                "http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");

        mapView.getLayers().setBaseLayer(mapLayer);
        
        // Location: Estonia
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.5f, 58.3f));

        // rotation - 0 = north-up
        mapView.setRotation(0f);
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

		// 4. Start the map - mandatory
		mapView.startMapping();

        
		// 5. zoom buttons using Android widgets - optional
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
		
        // add event listener to redraw map
        MyLocationMapEventListener mapListener = new MyLocationMapEventListener(this, mapView);
        mapView.getOptions().setMapListener(mapListener);
   
        // add GPS My Location functionality 
        initGps(mapListener.getLocationCircle());

	}
     

    public MapView getMapView() {
        return mapView;
    }
    
    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

    protected void initGps(final MyLocationMapEventListener.MyLocationCircle locationCircle) {
        final Projection proj = mapView.getLayers().getBaseLayer().getProjection();
        
        LocationListener locationListener = new LocationListener() 
        {
            public void onLocationChanged(Location location) {
                Log.debug("GPS onLocationChanged "+location);
                 if (locationCircle != null) {
                     locationCircle.setLocation(proj, location);
                     locationCircle.setVisible(true);
                     mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(location.getLongitude(), location.getLatitude()));
                 }
            }

            public void onStatusChanged(String provider, int status, Bundle extras) {
                Log.debug("GPS onStatusChanged "+provider+" to "+status);
            }

            public void onProviderEnabled(String provider) {
                Log.debug("GPS onProviderEnabled");
            }

            public void onProviderDisabled(String provider) {
                Log.debug("GPS onProviderDisabled");
            }
        };
        
        LocationManager locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);
        
        locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 10000, 100, locationListener);
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000, 0, locationListener);

    } 
    
}

