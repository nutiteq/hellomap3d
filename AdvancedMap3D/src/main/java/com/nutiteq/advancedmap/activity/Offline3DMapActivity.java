package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.Window;
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
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.NMLModelDbLayer;

/**
 * 
 * Demonstrates NMLModelDbLayer - 3D model layer which loads data fom a .nmldb file
 * 
 * After file loading the map is recentered to content coverage area.
 * 
 * To use this sample a .nmldb file must be loaded to SDCard file.
 * See https://github.com/nutiteq/hellomap3d/wiki/Nml-3d-models-map-layer for details and sample data download
 * 
 * @author jaak
 *
 */
public class Offline3DMapActivity extends Activity implements FilePickerActivity {

	private MapView mapView;

    
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

		setContentView(R.layout.main);

		// enable logging for troubleshooting - optional
		Log.enableAll();
		Log.setTag("nml3d");

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
		

        // define style for 3D to define minimum zoom = 14
        ModelStyle modelStyle = ModelStyle.builder().build();
        StyleSet<ModelStyle> modelStyleSet = new StyleSet<ModelStyle>(null);
        modelStyleSet.setZoomStyle(14, modelStyle);

        // ** 3D Model layer
        try {
            Bundle b = getIntent().getExtras();
            String mapFile = b.getString("selectedFile");
            
            NMLModelDbLayer modelLayer = new NMLModelDbLayer(new EPSG3857(),
                    mapFile, modelStyleSet);
            modelLayer.setMemoryLimit(20*1024*1024);
            mapView.getLayers().addLayer(modelLayer);
            

            // set initial map view camera from database
            Envelope extent = modelLayer.getDataExtent();
            
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
            e.printStackTrace();
            return;
        }
        

		// rotation - 0 = north-up
		mapView.setRotation(0f);
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

    @Override
    public FileFilter getFileFilter() {
        return new FileFilter() {
            @Override
            public boolean accept(File file) {
                // accept only readable files
                if (file.canRead()) {
                    if (file.isDirectory()) {
                        // accept all directories
                        return true;
                    } else if (file.isFile()
                            && (file.getName().endsWith(".db") ||
                                    file.getName().endsWith(".nml") ||
                                    file.getName().endsWith(".nmldb")||
                                    file.getName().endsWith(".sqlite"))) {
                        // accept files with given extension
                        return true;
                    }
                }
                return false;
            };
        };
    }

	public MapView getMapView() {
        return mapView;
    }

    @Override
    public String getFileSelectMessage() {
        return "Select 3D file (.nmldb)";
    }

}

