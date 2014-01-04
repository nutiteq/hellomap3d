package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.Map;
import java.util.Vector;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.layers.raster.GdalDatasetInfo;
import com.nutiteq.layers.raster.GdalMapLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * 
 * Demonstrates GdalMapLayer layer, whic uses GDAL native library
 * 
 * Requires GDAL native library with JNI wrappers, and raster data file (e.g. GeoTIFF) in SDCard
 * 
 * GDAL is used to load map tiles, and tiles are stored to persistent cache for faster loading later.
 * 
 * See https://github.com/nutiteq/hellomap3d/wiki/Gdal-layer for details
 * 
 * @author jaak
 *
 */
public class RasterFileMapActivity extends Activity implements FilePickerActivity {

    private MapView mapView;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

        setContentView(R.layout.main);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("gdal");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            // set stereo view: works if you rotate to landscape and device has HTC 3D or LG Real3D
            mapView.setComponents(components);
        }


        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);

        // read filename from extras
        Bundle b = getIntent().getExtras();
        String file = b.getString("selectedFile");

        try {
            GdalMapLayer gdalLayer = new GdalMapLayer(new EPSG3857(), 0, 18, 9, file, mapView, true);
            gdalLayer.setShowAlways(true);
            mapView.getLayers().addLayer(gdalLayer);
            Map<Envelope, GdalDatasetInfo> dataSets = gdalLayer.getDatasets();
            if(!dataSets.isEmpty()){
                GdalDatasetInfo firstDataSet = (GdalDatasetInfo) dataSets.values().toArray()[0];

                MapPos centerPoint = new MapPos((firstDataSet.envelope.maxX+firstDataSet.envelope.minX)/2,
                        (firstDataSet.envelope.maxY+firstDataSet.envelope.minY)/2);


                Log.debug("found extent "+firstDataSet.envelope+", zoom "+firstDataSet.bestZoom+", centerPoint "+centerPoint);

                mapView.setFocusPoint(centerPoint);
                mapView.setZoom((float) firstDataSet.bestZoom);
            }else{
                Log.debug("no dataset info");
                Toast.makeText(this, "No dataset info", Toast.LENGTH_LONG).show();

                mapView.setFocusPoint(new MapPos(0,0));
                mapView.setZoom(1.0f);

            }

            // rotation - 0 = north-up
            mapView.setMapRotation(0f);
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
            // mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
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

        } catch (IOException e) {
            Toast.makeText(this, "ERROR "+e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
        }

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

    @Override
    public String getFileSelectMessage() {
        return "Select a raster file (.tif etc)";
    }

    @Override
    public FileFilter getFileFilter() {
        return new FileFilter() {
            @Override
            public boolean accept(File file) {
                String fileExtension = file.getName().substring(file.getName().lastIndexOf(".")+1).toLowerCase();
                Vector<String> exts = GdalMapLayer.getExtensions();
                return (file.isDirectory() || exts.contains(fileExtension));
            }
        };
    }

}

