package com.nutiteq.advancedmap;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.Window;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.layers.vector.OgrLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * 
 * Demonstrates usage of OgrLayer - offline vector layer which uses OGR library
 *
 * 
 * It requries OGR native library with JNI wrappers.
 * 
 * To use the sample an OGR-supported datasource, e.g. Shapefile must be in SDCard
 * 
 * See https://github.com/nutiteq/hellomap3d/wiki/Ogr-layer for details
 * 
 * @author jaak
 *
 */
public class VectorFileMapActivity extends Activity implements FilePickerActivity {

	private MapView mapView;

    
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

		setContentView(R.layout.main);

		// enable logging for troubleshooting - optional
		Log.enableAll();
		Log.setTag("ogractivity");

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
		// Here we use MapQuest open tiles
		// Almost all online tiled maps use EPSG3857 projection.
		TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 0, 18, 0,
				"http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");

		mapView.getLayers().setBaseLayer(mapLayer);

		// set initial map view camera - optional. "World view" is default
		// Location: Estonia
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.5f, 58.3f));

		// rotation - 0 = north-up
		mapView.setRotation(0f);
		// zoom - 0 = world, like on most web maps
		mapView.setZoom(10.0f);
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
		
		 // read filename from extras
        Bundle b = getIntent().getExtras();
        String file = b.getString("selectedFile");
        
        addOgrLayer(mapLayer.getProjection(), file, null, Color.BLUE);
        
     // 5. Add set of static OGR vector layers to map
//        addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/buildings.shp","buildings", Color.DKGRAY);
//        addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/points.shp", "points",Color.CYAN);
//        addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/places.shp", "places",Color.BLACK);
//        addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/roads.shp","roads",Color.YELLOW);
//        addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/railways.shp","railways",Color.GRAY);
//        addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/waterways.shp","waterways",Color.BLUE);
        
	}

	   private void addOgrLayer(Projection proj, String dbPath, String table, int color) {

	        // set styles for all 3 object types: point, line and polygon
	       int minZoom = 5;
	       
	        StyleSet<PointStyle> pointStyleSet = new StyleSet<PointStyle>();
	        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
	        PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker).setSize(0.05f).setColor(color).setPickingSize(0.2f).build();
	        pointStyleSet.setZoomStyle(minZoom, pointStyle);

	        StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>();
	        LineStyle lineStyle = LineStyle.builder().setWidth(0.05f).setColor(color).build();
	        lineStyleSet.setZoomStyle(minZoom, lineStyle);

	        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(color & 0x80FFFFFF).setLineStyle(lineStyle).build();
	        StyleSet<PolygonStyle> polygonStyleSet = new StyleSet<PolygonStyle>(null);
	        polygonStyleSet.setZoomStyle(minZoom, polygonStyle);

	        OgrLayer ogrLayer;
            try {
                ogrLayer = new OgrLayer(proj, dbPath, table,
                        500, pointStyleSet, lineStyleSet, polygonStyleSet);
                ogrLayer.printSupportedDrivers();
                mapView.getLayers().addLayer(ogrLayer);
 
                Envelope extent = ogrLayer.getDataExtent();
                DisplayMetrics metrics = new DisplayMetrics();
                getWindowManager().getDefaultDisplay().getMetrics(metrics);   
                int screenHeight = metrics.heightPixels;
                int screenWidth = metrics.widthPixels;

                double zoom = Math.log((screenWidth * (Math.PI * 6378137.0f * 2.0f)) 
                        / ((extent.maxX-extent.minX) * 256.0)) / Math.log(2);
                
                MapPos centerPoint = new MapPos((extent.maxX+extent.minX)/2,(extent.maxY+extent.minY)/2);
                Log.debug("found extent "+extent+", zoom "+zoom+", centerPoint "+centerPoint);
                
                mapView.setZoom((float) zoom);
                mapView.setFocusPoint(centerPoint); 
                
            } catch (IOException e) {
                Log.error(e.getLocalizedMessage());
                Toast.makeText(this, "ERROR "+e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
            }

	    }
	
    public MapView getMapView() {
        return mapView;
    }

    @Override
    public String getFileSelectMessage() {
        return "Select vector data file (.shp, .kml etc)";
    }

    @Override
    public FileFilter getFileFilter() {
        return new FileFilter() {
            @Override
            public boolean accept(File file) {                
                return true;
            }
        };
    }
    
    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

     
}

