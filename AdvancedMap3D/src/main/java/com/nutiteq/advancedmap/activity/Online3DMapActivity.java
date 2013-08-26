package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.R.drawable;
import com.nutiteq.advancedmap.R.id;
import com.nutiteq.advancedmap.R.layout;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.NMLModelOnlineLayer;

/**
 * Demonstrates NMLModelOnlineLayer - online 3D model layer which loads data from Nutiteq NML online API.
 * 
 * The demo server has data of few cities: Tallinn, Barcelona, San Francisco. This content is from Google 3D Warehouse
 *
 * Loaded data is partly cached locally with special cache.
 *
 * @author jaak
 *
 */
public class Online3DMapActivity extends Activity {

	private static final String DATASET = "demo"; // demo
    private MapView mapView;
    private StyleSet<ModelStyle> modelStyleSet;

    
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.main);

		// enable logging for troubleshooting - optional
		Log.enableAll();
		Log.setTag("online3d");

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
		      // set stereo view: works if you rotate to landscape and device has HTC 3D or LG Real3D
		      mapView.setComponents(components);
		      }


		// 3. Define map layer for basemap - mandatory.

		TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 0, 18, 2,
                "http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");
        mapView.getLayers().setBaseLayer(mapLayer);
		

        // define style for 3D to define minimum zoom = 14
        ModelStyle modelStyle = ModelStyle.builder().build();
        modelStyleSet = new StyleSet<ModelStyle>(null);
        modelStyleSet.setZoomStyle(14, modelStyle);

        // Tallinn
        //mapView.setFocusPoint(new MapPos(2753845.7830863246f, 8275045.674995658f));
        
        // San Francisco
        //mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 37.76666666666f));
        getMapView().setFocusPoint(new MapPos(-1.3625947E7f, 4550716.0f));
        
        // Rotterdam
        //mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(4.480727f, 51.921098f));
        mapView.setZoom(17.0f);

        // set initial layer
        online3DLayer(DATASET);
        
		// rotation - 0 = north-up
		//mapView.setRotation(-96.140175f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
		//mapView.setTilt(30.0f);

		// Activate some mapview options to make it smoother - optional
		mapView.getOptions().setPreloading(false);
		mapView.getOptions().setSeamlessHorizontalPan(true);
		mapView.getOptions().setTileFading(false);
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

	}



	public MapView getMapView() {
        return mapView;
    }



    @Override
    protected void onStop() {
        super.onStop();
        Log.debug("x " + getMapView().getFocusPoint().x);
        Log.debug("y " + getMapView().getFocusPoint().y);
        Log.debug("tilt " + getMapView().getTilt());
        Log.debug("rotation " + getMapView().getRotation());
        Log.debug("zoom " + getMapView().getZoom());
        
        mapView.stopMapping();
    }

    @Override
    public boolean onCreateOptionsMenu(final Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.online3d, menu);
        return true;
    }
    
    @Override
    public boolean onMenuItemSelected(final int featureId, final MenuItem item) {

        item.setChecked(true);

        switch (item.getItemId()) {

        // map types
        case R.id.menu3d_demo:
            online3DLayer("demo");
            // San Francisco
            getMapView().setFocusPoint(new MapPos(-1.3625947E7f, 4550716.0f), 1000);
            break;
        case R.id.menu3d_hover:
            online3DLayer("hover");
            // San Francisco
            getMapView().setFocusPoint(new MapPos(-1.3625947E7f, 4550716.0f), 1000);
            break;
        case R.id.menu3d_tomtomlod3:
            online3DLayer("tomtom");
            // San Francisco
            getMapView().setFocusPoint(new MapPos(-1.3625947E7f, 4550716.0f), 1000);
            break;
        case R.id.menu3d_blom:
            online3DLayer("blom");
            // London
            mapView.setFocusPoint(mapView.getLayers().getBaseProjection().fromWgs84(-0.109015f, 51.516584f), 1000);
            break;

        }
        return false;
    }



    private void online3DLayer(String dataset) {
        // TODO Auto-generated method stub
        // ** Online 3D Model layer
        
        NMLModelOnlineLayer modelLayer = new NMLModelOnlineLayer(new EPSG3857(),
                "http://aws-lb.nutiteq.ee/nml/nmlserver3.php?data="+dataset+"&", modelStyleSet);

        modelLayer.setMemoryLimit(40*1024*1024);
        
        modelLayer.setPersistentCacheSize(60*1024*1024);
        modelLayer.setPersistentCachePath(this.getDatabasePath("nmlcache_"+dataset).getPath());
        
        modelLayer.setLODResolutionFactor(0.3f);
        getMapView().getLayers().addLayer(modelLayer);
    }
     
}

