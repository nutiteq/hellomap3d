package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;

import org.mapsforge.map.reader.MapDatabase;
import org.mapsforge.map.reader.header.FileOpenResult;
import org.mapsforge.map.reader.header.MapFileInfo;
import org.mapsforge.map.rendertheme.InternalRenderTheme;
import org.mapsforge.map.rendertheme.XmlRenderTheme;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.Window;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Bounds;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.datasources.raster.MapsforgeRasterDataSource;

/**
 * 
 * Demonstrates usage of MapsforgeMapLayer offline raster layer
 * 
 * It uses external MapsForge library which generates raster map tiles from
 * vector database.
 * 
 * You need to preload .map file to SDCard for using this layer. See
 * https://github.com/nutiteq/hellomap3d/wiki/Mapsforge-layer for details
 * 
 * @author jaak
 * 
 */
public class MapsForgeMapActivity extends Activity implements FilePickerActivity {

    private MapView mapView;
    private float dpi;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

        setContentView(R.layout.main);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("mapsforge");

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        dpi = metrics.density;

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
            // set stereo view: works if you rotate to landscape and device has
            // HTC 3D or LG Real3D
            mapView.setComponents(components);
        }

        // 3. Define map layer for basemap - mandatory.
        // read filename from extras
        Bundle b = getIntent().getExtras();
        String mapFilePath = b.getString("selectedFile");

        XmlRenderTheme renderTheme = InternalRenderTheme.OSMARENDER;
        // XmlRenderTheme renderTheme = new AssetsRenderTheme(this, "",
        // "renderthemes/assets_noname.xml");

        MapDatabase mapDatabase = new MapDatabase();
        mapDatabase.closeFile();
        File mapFile = new File("/" + mapFilePath);
        FileOpenResult fileOpenResult = mapDatabase.openFile(mapFile);
        if (fileOpenResult.isSuccess()) {
            Log.debug("MapsforgeRasterDataSource: MapDatabase opened ok: " + mapFilePath);
        }

        MapsforgeRasterDataSource dataSource = new MapsforgeRasterDataSource(new EPSG3857(), 0, 20, mapFile, mapDatabase, renderTheme, this.getApplication());
        RasterLayer mapLayer = new RasterLayer(dataSource, 1044);
        mapView.getLayers().setBaseLayer(mapLayer);

        // set initial map view camera from database
        MapFileInfo mapFileInfo = dataSource.getMapDatabase().getMapFileInfo();
        if (mapFileInfo != null) {
            if (mapFileInfo.startPosition != null && mapFileInfo.startZoomLevel != null) {
                // start position is defined
                MapPos mapCenter = new MapPos(mapFileInfo.startPosition.longitude, mapFileInfo.startPosition.latitude, mapFileInfo.startZoomLevel);
                Log.debug("center: " + mapCenter);
                mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(mapCenter.x, mapCenter.y));
                mapView.setZoom((float) mapCenter.z);
            } else if (mapFileInfo.boundingBox != null) {
                // start position not defined, but boundingbox is defined
                MapPos boxMin = mapView.getLayers().getBaseLayer().getProjection()
                        .fromWgs84(mapFileInfo.boundingBox.minLongitude, mapFileInfo.boundingBox.minLatitude);
                MapPos boxMax = mapView.getLayers().getBaseLayer().getProjection()
                        .fromWgs84(mapFileInfo.boundingBox.maxLongitude, mapFileInfo.boundingBox.maxLatitude);
                mapView.setBoundingBox(new Bounds(boxMin.x, boxMin.y, boxMax.x, boxMax.y), true);
            }
        }

        // if no fileinfo, startPosition or boundingBox, then remain to default
        // world view

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D
        // map view, minimum allowed is 30 degrees.
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
        mapView.getOptions().setSkyBitmap(UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.sky_small));

        // Map background, visible if no map tiles loaded - optional, default -
        // white
        mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setBackgroundPlaneBitmap(UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.background_plane));
        mapView.getOptions().setClearColor(Color.WHITE);

        // configure texture caching - optional, suggested
        mapView.getOptions().setTextureMemoryCacheSize(40 * 1024 * 1024);
        mapView.getOptions().setCompressedMemoryCacheSize(16 * 1024 * 1024);

        // define online map persistent caching - optional, suggested. Default -
        // no caching
        // mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
        // set persistent raster cache limit to 100MB
        mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);

        mapView.getOptions().setRasterTaskPoolSize(1);

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
                    } else if (file.isFile() && (file.getName().endsWith(".map"))) {
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
        return "Select MapsForge .map file";
    }

}
