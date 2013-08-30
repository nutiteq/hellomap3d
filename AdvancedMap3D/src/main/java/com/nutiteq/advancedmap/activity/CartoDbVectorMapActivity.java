package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
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
import com.nutiteq.layers.vector.CartoDbVectorLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * 
 * Demonstrates usage of CartoDbVectorLayer, which is a online read-only vector map for cartodb.com server
 * 
 * Styles are defined for all possible graphics types: Point, Line and Polygons
 * 
 * For CartoDB editing see EditableCartoDbMapActivity.java
 * 
 * Used other layer(s):
 *  TMSMapLayer for base map
 * 
 * @author jaak
 *
 */
public class CartoDbVectorMapActivity extends Activity {

	private MapView mapView;

    
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.main);

		// enable logging for troubleshooting - optional
		Log.enableAll();
		Log.setTag("cartodb");

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
		TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 0, 18, 3,
//				"http://nutiteq.cartodb.com/tiles/tm_world_borders/", "/", ".png");
		        "http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");

		
		mapView.getLayers().setBaseLayer(mapLayer);

		// set initial map view camera - optional. "World view" is default
		// Location: Estonia
        //mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.5f, 58.3f));
        mapView.setFocusPoint(new MapPos(2745202.3f, 8269676.0f));
		// rotation - 0 = north-up
		mapView.setRotation(0f);
		// zoom - 0 = world, like on most web maps
		mapView.setZoom(9.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
		mapView.setTilt(90.0f);


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


		// 5. Add CartoDB vector layer to map

		//  5.1 Define styles for all possible geometry types

		int minZoom = 5;
		int color = Color.BLUE;
		
        StyleSet<PointStyle> pointStyleSet = new StyleSet<PointStyle>();
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker).setSize(0.05f).setColor(color).setPickingSize(0.2f).build();
        pointStyleSet.setZoomStyle(minZoom, pointStyle);

        StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>();
        LineStyle lineStyle = LineStyle.builder().setWidth(0.04f).setColor(Color.WHITE).build();
        lineStyleSet.setZoomStyle(minZoom, lineStyle);

        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(0xFFFF6600 & 0x80FFFFFF).setLineStyle(lineStyle).build();
        StyleSet<PolygonStyle> polygonStyleSet = new StyleSet<PolygonStyle>(null);
        polygonStyleSet.setZoomStyle(minZoom, polygonStyle);

        StyleSet<LineStyle> roadLineStyleSet = new StyleSet<LineStyle>(LineStyle.builder().setWidth(0.07f).setColor(0xFFAAAAAA).build());
        
        //  5.2 Define layer and add to map
        
        String account = "nutiteq";
        String table = "tm_world_borders"; // kihelkonnad_1897, maakond_20120701
        String columns = "name,iso2,pop2005,area,"+CartoDbVectorLayer.TAG_WEBMERCATOR; // NB! always include the_geom_webmercator
        int limit = 5000; // max number of objects
        String sql = "SELECT "+columns+" FROM "+table+" WHERE "+CartoDbVectorLayer.TAG_WEBMERCATOR +" && ST_SetSRID('BOX3D(!bbox!)'::box3d, 3857) LIMIT "+limit;
        
//      String sql2 = "SELECT name, type, oneway, osm_id, the_geom_webmercator FROM osm_roads WHERE type in ('trunk','primary') AND the_geom_webmercator && ST_SetSRID('BOX3D(!bbox!)'::box3d, 3857) LIMIT 500";
//      String sql2 = "SELECT name, type, oneway, osm_id, the_geom_webmercator FROM osm_roads WHERE the_geom_webmercator && ST_SetSRID('BOX3D(!bbox!)'::box3d, 3857) LIMIT 500";
        CartoDbVectorLayer cartoLayerTrunk = new CartoDbVectorLayer(mapView.getLayers().getBaseLayer().getProjection(), account, sql, pointStyleSet, lineStyleSet, polygonStyleSet);
        mapView.getLayers().addLayer(cartoLayerTrunk);

//		CartoDbVectorLayer cartoLayer = new CartoDbVectorLayer(mapView.getLayers().getBaseLayer().getProjection(), account, sql, pointStyleSet, lineStyleSet, polygonStyleSet);

//      OnlineVectorLayer vectorLayer = new OnlineVectorLayer(mapView.getLayers().getBaseLayer().getProjection(), pointStyleSet, lineStyleSet, polygonStyleSet,2000);
//		mapView.getLayers().addLayer(vectorLayer);
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
   
}

