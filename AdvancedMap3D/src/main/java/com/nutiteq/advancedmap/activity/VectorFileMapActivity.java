package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.Window;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Bounds;
import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.Options;
import com.nutiteq.datasources.vector.OGRVectorDataSource;
import com.nutiteq.datasources.vector.SpatialiteDataSource;
import com.nutiteq.db.SpatialLiteDbHelper;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.LabelStyle;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;

/**
 * 
 * Demonstrates usage of file based data sources: supports SpatiaLite and OGR data sources
 *
 * It requires OGR native library with JNI wrappers.
 * 
 * To use the sample an OGR-supported datasource, e.g. Shapefile must be in SDCard
 * 
 * In case of SpatiaLite file, the application shows first list of tables in selected Spatialite database file, then opens file. 
 *  
 * The sampple requires custom compiled jsqlite (with spatialite) and proj.4 libraries via JNI
 *  
 * You need a Spatialite table file in SDCard. Up to version 4.x tables are supported.
 * See https://github.com/nutiteq/hellomap3d/wiki/Ogr-layer for details
 * 
 * @author jaak
 *
 */
public class VectorFileMapActivity extends Activity implements FilePickerActivity {

    // Internal dialog ids
    private static final int DIALOG_TABLE_LIST = 1;
    private static final int DIALOG_NO_TABLES = 2;
    
    // Limit for the number of vector elements that are loaded
    private static final int MAX_ELEMENTS = 500;

    private MapView mapView;

    // Spatialite-specific members
    private String[] tableList = new String[1];
    private SpatialLiteDbHelper spatialLite;
    private Map<String, SpatialLiteDbHelper.DbLayer> dbMetaData;

    private StyleSet<PointStyle> pointStyleSet;
    private StyleSet<LineStyle> lineStyleSet;
    private StyleSet<PolygonStyle> polygonStyleSet;
    private LabelStyle labelStyle;

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
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);

        // set initial map view camera - optional. "World view" is default
        // Location: Estonia
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.5f, 58.3f));

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
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

        // read filename from extras
        Bundle b = getIntent().getExtras();
        String file = b.getString("selectedFile");

        createStyleSets();
        if (file.endsWith(".db") || file.endsWith(".sqlite") || file.endsWith(".spatialite")) {
            showSpatialiteTableList(file);
        } else {
            addOgrLayer(mapLayer.getProjection(), file, null, Color.BLUE);
        }

        // 5. Add set of static OGR vector layers to map
        //      addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/buildings.shp","buildings", Color.DKGRAY);
        //      addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/points.shp", "points",Color.CYAN);
        //      addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/places.shp", "places",Color.BLACK);
        //      addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/roads.shp","roads",Color.YELLOW);
        //      addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/railways.shp","railways",Color.GRAY);
        //      addOgrLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory()+"/mapxt/eesti/waterways.shp","waterways",Color.BLUE);

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

    private void createStyleSets() {
        // set styles for all 3 object types: point, line and polygon
        int minZoom = 5;
        int color = Color.BLUE;

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        float dpi = metrics.density;

        pointStyleSet = new StyleSet<PointStyle>();
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker).setSize(0.05f).setColor(color).setPickingSize(0.2f).build();
        pointStyleSet.setZoomStyle(minZoom, pointStyle);

        lineStyleSet = new StyleSet<LineStyle>();
        LineStyle lineStyle = LineStyle.builder().setWidth(0.05f).setColor(color).build();
        lineStyleSet.setZoomStyle(minZoom, lineStyle);

        polygonStyleSet = new StyleSet<PolygonStyle>(null);
        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(color & 0x80FFFFFF).setLineStyle(lineStyle).build();
        polygonStyleSet.setZoomStyle(minZoom, polygonStyle);

        labelStyle = 
                LabelStyle.builder()
                .setEdgePadding((int) (12 * dpi))
                .setLinePadding((int) (6 * dpi))
                .setTitleFont(Typeface.create("Arial", Typeface.BOLD), (int) (16 * dpi))
                .setDescriptionFont(Typeface.create("Arial", Typeface.NORMAL), (int) (13 * dpi))
                .build();
    }

    private void addOgrLayer(Projection proj, String dbPath, String table, int color) {
        OGRVectorDataSource dataSource;
        try {
            dataSource = new OGRVectorDataSource(proj, dbPath, table) {
                @Override
                protected Label createLabel(Map<String, String> userData) {
                    return VectorFileMapActivity.this.createLabel(userData);
                }

                @Override
                protected StyleSet<PointStyle> createPointStyleSet(Map<String, String> userData, int zoom) {
                    return pointStyleSet;
                }

                @Override
                protected StyleSet<LineStyle> createLineStyleSet(Map<String, String> userData, int zoom) {
                    return lineStyleSet;
                }

                @Override
                protected StyleSet<PolygonStyle> createPolygonStyleSet(Map<String, String> userData, int zoom) {
                    return polygonStyleSet;
                }

            };
        } catch (IOException e) {
            Log.error(e.getLocalizedMessage());
            Toast.makeText(this, "ERROR "+e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
            return;
        }

        dataSource.setMaxElements(MAX_ELEMENTS);

        GeometryLayer ogrLayer = new GeometryLayer(dataSource);
        mapView.getLayers().addLayer(ogrLayer);

        Envelope extent = ogrLayer.getDataExtent();
        mapView.setBoundingBox(new Bounds(extent.minX, extent.maxY, extent.maxX, extent.minY), false);
    }

    @Override
    protected Dialog onCreateDialog(int id) {
        switch(id){
        case DIALOG_TABLE_LIST:
            return new AlertDialog.Builder(this)
                .setTitle("Select table:")
                .setSingleChoiceItems(tableList, 0, null)
                .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        dialog.dismiss();
                        int selectedPosition = ((AlertDialog) dialog).getListView().getCheckedItemPosition();
                        addSpatiaLiteTable(selectedPosition);
                    }
                }).create();

        case DIALOG_NO_TABLES:
            return new AlertDialog.Builder(this)
                .setMessage("No geometry_columns or spatial_ref_sys metadata found. Check logcat for more details.")
                .setPositiveButton("Back", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        dialog.dismiss();
                        finish();
                    }
                }).create();

        }
        return null;
    }

    private void showSpatialiteTableList(String dbPath) {
        try {
            spatialLite = new SpatialLiteDbHelper(dbPath);
        } catch (IOException e) {
            Log.error(e.getLocalizedMessage());
            Toast.makeText(this, "ERROR "+e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
            return;
        }
        dbMetaData = spatialLite.qrySpatialLayerMetadata();

        ArrayList<String> tables = new ArrayList<String>();

        for (String layerKey : dbMetaData.keySet()) {
            SpatialLiteDbHelper.DbLayer layer = dbMetaData.get(layerKey);
            Log.debug("layer: " + layer.table + " " + layer.type + " geom:"
                    + layer.geomColumn+ " SRID: "+layer.srid);
            tables.add(layerKey);
        }

        Collections.sort(tables);

        if(tables.size() > 0){
            tableList = (String[]) tables.toArray(new String[0]);
            showDialog(DIALOG_TABLE_LIST);

        }else{
            showDialog(DIALOG_NO_TABLES);
        }

    }

    public void addSpatiaLiteTable(int selectedPosition){
        String[] tableKey = tableList[selectedPosition].split("\\.");

        SpatialiteDataSource dataSource = new SpatialiteDataSource(new EPSG3857(), spatialLite, tableKey[0], tableKey[1], null, null) {
            @Override
            protected Label createLabel(Map<String, String> userData) {
                return VectorFileMapActivity.this.createLabel(userData);
            }

            @Override
            protected StyleSet<PointStyle> createPointStyleSet(Map<String, String> userData, int zoom) {
                return pointStyleSet;
            }

            @Override
            protected StyleSet<LineStyle> createLineStyleSet(Map<String, String> userData, int zoom) {
                return lineStyleSet;
            }

            @Override
            protected StyleSet<PolygonStyle> createPolygonStyleSet(Map<String, String> userData, int zoom) {
                return polygonStyleSet;
            }
        };
        dataSource.setMaxElements(MAX_ELEMENTS);

        // define pixels and screen width for automatic polygon/line simplification
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);   
        dataSource.setAutoSimplify(2, metrics.widthPixels);

        GeometryLayer spatialiteLayer = new GeometryLayer(dataSource);

        mapView.getLayers().addLayer(spatialiteLayer);

        Envelope extent = spatialiteLayer.getDataExtent();
        mapView.setBoundingBox(new Bounds(extent.minX, extent.maxY, extent.maxX, extent.minY), false);
    }
    
    private Label createLabel(Map<String, String> userData) {
        StringBuffer labelTxt = new StringBuffer();
        for(Map.Entry<String, String> entry : userData.entrySet()){
            labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
        }
        return new DefaultLabel("Data:", labelTxt.toString(), labelStyle);
    }

    public MapView getMapView() {
        return mapView;
    }

    // Methods for FilePicker

    @Override
    public String getFileSelectMessage() {
        return "Select Spatialite database file (.spatialite, .db, .sqlite) or vector data file (.shp, .kml etc)";
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

}

