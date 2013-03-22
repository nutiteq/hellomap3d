package com.nutiteq.advancedmap;

import java.io.File;
import java.io.FileFilter;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Vector;

import org.mapsforge.android.maps.mapgenerator.JobTheme;
import org.mapsforge.android.maps.mapgenerator.databaserenderer.ExternalRenderTheme;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.Window;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.maplisteners.MBTileMapEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.db.DBLayer;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.geometry.Marker;
import com.nutiteq.layers.raster.GdalMapLayer;
import com.nutiteq.layers.raster.MBTilesMapLayer;
import com.nutiteq.layers.raster.MapsforgeMapLayer;
import com.nutiteq.layers.raster.PackagedMapLayer;
import com.nutiteq.layers.raster.QuadKeyLayer;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.layers.raster.WmsLayer;
import com.nutiteq.layers.vector.OgrLayer;
import com.nutiteq.layers.vector.Polygon3DOSMLayer;
import com.nutiteq.layers.vector.SpatialLiteDb;
import com.nutiteq.layers.vector.SpatialiteLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.StoredMapLayer;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;
import com.nutiteq.vectorlayers.NMLModelDbLayer;

public class MBTilesMapActivity extends Activity implements FilePickerActivity{

	private MapView mapView;

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
			// just restore configuration, skip other initializations
			mapView.setComponents(retainObject);
			mapView.startMapping();
			return;
		} else {
			// 2. create and set MapView components - mandatory
		      Components components = new Components();
		      mapView.setComponents(components);
		      }

		// add event listener
		MBTileMapEventListener mapListener = new MBTileMapEventListener(this);
		mapView.getOptions().setMapListener(mapListener);

		// 3. Define map layer for basemap - mandatory
		// MBTiles supports only EPSG3857 projection

        try {
            
            // read filename from extras
            Bundle b = getIntent().getExtras();
            String file = b.getString("selectedFile");
            MBTilesMapLayer dbLayer = new MBTilesMapLayer(new EPSG3857(), 0, 19, file.hashCode(), file, this);
            mapView.getLayers().setBaseLayer(dbLayer);
        } catch (IOException e) {
            Log.error(e.getLocalizedMessage());
            e.printStackTrace();
        }

		// set initial map view camera - optional. "World view" is default
        
        // bulgaria
      //  mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(25.295818066955075f, 42.606913041613375f));
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(26.483230800000037, 42.550218000000044));

		// rotation - 0 = north-up
		mapView.setRotation(0f);
		// zoom - 0 = world, like on most web maps
		mapView.setZoom(5.0f);
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

	}
     

    public MapView getMapView() {
        return mapView;
    }


    @Override
    public String getFileSelectMessage() {
        return "Select a MBTiles (.db, .sqlite or .mbtiles) file";
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
                            && (file.getName().endsWith(".db")
                                    || file.getName().endsWith(".sqlite") || file
                                    .getName().endsWith(".mbtiles"))) {
                        // accept files with given extension
                        return true;
                    }
                }
                return false;
            };
        };
    }
     
}

