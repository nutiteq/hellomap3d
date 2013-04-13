package com.nutiteq.advancedmap;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.webkit.WebView;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.maplisteners.UtfGridLayerEventListener;
import com.nutiteq.advancedmap.maplisteners.WmsLayerClickListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.geometry.Marker;
import com.nutiteq.layers.raster.MapBoxMapLayer;
import com.nutiteq.layers.raster.MapBoxMapLayer.LoadMetadataTask;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.layers.raster.WmsLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.Label;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

public class WmsMapActivity extends Activity {

	private MapView mapView;

    
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.main);

		// enable logging for troubleshooting - optional
		Log.enableAll();
		Log.setTag("mapbox");

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

        TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 5, 18, 0,
                "http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");
        mapView.getLayers().setBaseLayer(mapLayer);

        // add a layer and marker for click labels
        // define small invisible Marker, as Label requires some Marker 
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.01f).setColor(0).build();

        //  define label as WebView to show HTML
        WebView labelView = new WebView(this); 
        // It is important to set size, exception will come otherwise
        labelView.layout(0, 0, 300, 150);
        Label label = new ViewLabel("", labelView, new Handler());
        
        Marker clickMarker = new Marker(new MapPos(0,0), label, markerStyle, null);
        
        MarkerLayer clickMarkerLayer = new MarkerLayer(new EPSG3857());
        clickMarkerLayer.add(clickMarker);
        mapView.getLayers().addLayer(clickMarkerLayer);
		

        // add WMS layer as overlay
        
        String url = "http://kaart.maakaart.ee/geoserver/wms?transparent=true&";
        String layers = "topp:states";
        
        // note that data projection is different: WGS84 (EPSG:4326)
        WmsLayer wmsLayer = new WmsLayer(new EPSG3857(), 0, 19, 1012, url, "", layers, "image/png", new EPSG4326());
        wmsLayer.setFetchPriority(-5);
        mapView.getLayers().addLayer(wmsLayer);
        
        // add event listener for clicks on WMS map
		final WmsLayerClickListener mapListener = new WmsLayerClickListener(this, mapView, wmsLayer, clickMarker);
        mapView.getOptions().setMapListener(mapListener);


        
		// set initial map view camera - optional. "World view" is default
		// Location: USA
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-90f, 35f));
        
		// rotation - 0 = north-up
		mapView.setRotation(0f);
		// zoom - 0 = world, like on most web maps
		mapView.setZoom(5.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
		mapView.setTilt(40.0f);


		// Activate some mapview options to make it smoother - optional
		mapView.getOptions().setPreloading(true);
		mapView.getOptions().setSeamlessHorizontalPan(true);
		mapView.getOptions().setTileFading(false);
		mapView.getOptions().setKineticPanning(true);
		mapView.getOptions().setDoubleClickZoomIn(true);
		mapView.getOptions().setDualClickZoomOut(true);

		// mapView.getConstraints().setTiltRange(new Range(90,90));
		
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
        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache_wms").getPath());
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
        mapView.stopMapping();
    }

}

