package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.R.drawable;
import com.nutiteq.advancedmap.R.id;
import com.nutiteq.advancedmap.R.layout;
import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.layers.vector.SpatialLiteDb;
import com.nutiteq.layers.vector.SpatialiteLayer;
import com.nutiteq.layers.vector.SpatialiteMarkerLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * Basic map, same as HelloMap
 * 
 * Just defines and configures map with useful settings.
 *
 * Used layer(s):
 *  TMSMapLayer for base map
 * 
 * @author jaak
 *
 */
public class MParkMapActivity extends Activity {

	private MapView mapView;
    private EPSG3857 proj;
    
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.main);

		Log.enableAll();
		Log.setTag("mpark");
		
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
		this.proj = new EPSG3857();
        TMSMapLayer mapLayer = new TMSMapLayer(proj, 0, 18, 0,
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
		
		addMParkLayers();

	}
     

    public MapView getMapView() {
        return mapView;
    }
    
    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

    private void addMParkLayers(){
        

        // some general constants
        int minZoom = 0;
        int color = Color.BLUE;
        int maxElements = 1000;
        
        // set styles for all 3 object types: point, line and polygon

        StyleSet<PointStyle> pointStyleSet = new StyleSet<PointStyle>();
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(
                getResources(), R.drawable.point);
        PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker)
                .setSize(0.05f).setColor(color).setPickingSize(0.2f).build();
        pointStyleSet.setZoomStyle(minZoom, pointStyle);

        StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>();
        LineStyle lineStyle = LineStyle.builder().setWidth(0.05f)
                .setColor(color).build();
        lineStyleSet.setZoomStyle(minZoom, lineStyle);

        int[] polyColors = {0xFFFFA500 /*orange*/, Color.RED, Color.GREEN, Color.BLUE};
        
        PolygonStyle polygonStyle1 = PolygonStyle.builder()
                .setColor(polyColors[0] & 0x80FFFFFF).setLineStyle(
                        LineStyle.builder().setWidth(0.05f).setColor(polyColors[0]).build()
                        ).build();

        PolygonStyle polygonStyle2 = PolygonStyle.builder()
                .setColor(polyColors[1] & 0x80FFFFFF).setLineStyle(
                        LineStyle.builder().setWidth(0.05f).setColor(polyColors[1]).build()
                        ).build();

        PolygonStyle polygonStyle3 = PolygonStyle.builder()
                .setColor(polyColors[2] & 0x80FFFFFF).setLineStyle(
                        LineStyle.builder().setWidth(0.05f).setColor(polyColors[2]).build()
                        ).build();
        
        PolygonStyle polygonStyle4 = PolygonStyle.builder()
                .setColor(polyColors[3] & 0x80FFFFFF).setLineStyle(
                        LineStyle.builder().setWidth(0.05f).setColor(polyColors[3]).build()
                        ).build();

        
        // polygons
        SpatialiteLayer spatialiteLayer1 = new SpatialiteLayer(proj, new SpatialLiteDb("/sdcard/mapxt/mparks.sqlite"), "mparks_p",
                "polygon", null, "id = 430", maxElements, pointStyleSet, lineStyleSet, new StyleSet<PolygonStyle>(polygonStyle1));
        
        mapView.getLayers().addLayer(spatialiteLayer1);

        SpatialiteLayer spatialiteLayer2 = new SpatialiteLayer(proj, new SpatialLiteDb("/sdcard/mapxt/mparks.sqlite"), "mparks_p",
                "polygon", null, "id = 434", maxElements, pointStyleSet, lineStyleSet, new StyleSet<PolygonStyle>(polygonStyle2));
        
        mapView.getLayers().addLayer(spatialiteLayer2);
        
        SpatialiteLayer spatialiteLayer3 = new SpatialiteLayer(proj, new SpatialLiteDb("/sdcard/mapxt/mparks.sqlite"), "mparks_p",
                "polygon", null, "id = 438", maxElements, pointStyleSet, lineStyleSet, new StyleSet<PolygonStyle>(polygonStyle3));
        
        mapView.getLayers().addLayer(spatialiteLayer3);
        
        SpatialiteLayer spatialiteLayer4 = new SpatialiteLayer(proj, new SpatialLiteDb("/sdcard/mapxt/mparks.sqlite"), "mparks_p",
                "polygon", null, "type = 2", maxElements, pointStyleSet, lineStyleSet, new StyleSet<PolygonStyle>(polygonStyle4));
        
        mapView.getLayers().addLayer(spatialiteLayer4);
        
        
        // points
        float pointSize = 0.4f;
        
        for(int i=1;i<=4;i++){
            Bitmap pointMarker1 = UnscaledBitmapLoader.decodeResource(
                    getResources(), getResources().getIdentifier("marker_"+i , "drawable", getPackageName()));
            StyleSet<MarkerStyle> markerStyleSet = new StyleSet<MarkerStyle>(MarkerStyle.builder().setBitmap(pointMarker1)
                    .setSize(pointSize).setPickingSize(0.2f).build());

            mapView.getLayers().addLayer(new SpatialiteMarkerLayer(proj, new SpatialLiteDb("/sdcard/mapxt/mparks.sqlite"), "mparks_pt",
                    "point", null, "type = "+i, maxElements, markerStyleSet));
       }

        Envelope extent = spatialiteLayer2.getDataExtent();
        
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);   
        int screenHeight = metrics.heightPixels;
        int screenWidth = metrics.widthPixels;

        double zoom = Math.log((screenWidth * (Math.PI * 6378137.0f * 2.0f)) 
                / ((extent.maxX-extent.minX) * 256.0)) / Math.log(2);
        
        MapPos centerPoint = new MapPos((extent.maxX+extent.minX)/2,(extent.maxY+extent.minY)/2);
        Log.debug("found extent "+extent+", zoom "+zoom+", centerPoint "+centerPoint);

        mapView.setFocusPoint(centerPoint);
        mapView.setZoom((float) zoom);
        // define pixels and screen width for automatic polygon/line simplification
        //spatialiteLayer.setAutoSimplify(1,screenWidth);
        
    }
     
}

