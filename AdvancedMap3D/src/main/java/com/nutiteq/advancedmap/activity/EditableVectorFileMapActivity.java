package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.graphics.Typeface;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.widget.Toast;

import com.nutiteq.components.Bounds;
import com.nutiteq.components.Color;
import com.nutiteq.components.Envelope;
import com.nutiteq.db.SpatialLiteDbHelper;
import com.nutiteq.editable.datasources.EditableOGRVectorDataSource;
import com.nutiteq.editable.datasources.EditableSpatialiteDataSource;
import com.nutiteq.editable.layers.EditableGeometryLayer;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.layers.Layer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
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

/**
 * 
 * Shows usage of EditableMapView with Spatialite database file or OGR shape file
 * 
 *  Enables offline editing of points, lines and polygons. 
 *  Supports both Spatialite 3.x and 4.x formats.
 *  
 *  See https://github.com/nutiteq/hellomap3d/wiki/Editable-MapView for details
 * 
 * Layers:
 *  RasterLayer with TMS data source- base map
 *  EditableGeometryLayer - layer with editable Spatialite data source / OGR data source (depending on file extension)
 * 
 * If Spatialite data source is detected, the Activity shows first list of tables in selected Spatialite database file,
 * and then opens for viewing and editing  selected one. It also creates toolbar for set of editing functions.
 * 
 * @author mtehver
 *
 */
public class EditableVectorFileMapActivity extends EditableMapActivityBase implements FilePickerActivity {
    // about 2000 lines/polygons for high-end devices is fine, for older devices <1000
    // for points 5000 would work fine with almost any device
    private static final int MAX_ELEMENTS = 500;

    // Internal dialog IDs
    private static final int DIALOG_TABLE_LIST = 1;
    private static final int DIALOG_NO_TABLES = 2;

    // Spatialite-specific data
    private SpatialLiteDbHelper spatialLite;
    private Map<String, SpatialLiteDbHelper.DbLayer> dbMetaData;
    private String[] tableList;

    // Style sets for elements
    private StyleSet<PointStyle> pointStyleSet;
    private StyleSet<LineStyle> lineStyleSet;
    private StyleSet<PolygonStyle> polygonStyleSet;
    private LabelStyle labelStyle;

    @Override
    protected void createBaseLayer() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://kaart.maakaart.ee/osm/tiles/1.0.0/osm_noname_EPSG900913/{zoom}/{x}/{yflipped}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);
        mapView.setFocusPoint(2970894.6988791,8045087.2280313);
        mapView.setZoom(4.0f);
    }

    @Override
    protected void createEditableLayers() {
        // read filename from extras
        Bundle b = getIntent().getExtras();
        String file = b.getString("selectedFile");

        createStyleSets();
        if (file.endsWith(".db") || file.endsWith(".sqlite") || file.endsWith(".spatialite")) {
            showSpatialiteTableList(file);
        } else {
            createEditableOGRLayers(file);
        }
    }

    @Override
    protected List<EditableGeometryLayer> getEditableLayers() {
        List<EditableGeometryLayer> layers = new ArrayList<EditableGeometryLayer>();
        for (Layer layer : mapView.getComponents().layers.getLayers()) {
            if (layer instanceof EditableGeometryLayer) {
                layers.add((EditableGeometryLayer) layer);
            }
        }
        return layers;
    }

    @Override
    protected void createEditableElement() {
        AlertDialog.Builder typeBuilder = new AlertDialog.Builder(this);
        typeBuilder.setTitle("Choose type");
        final String[] items = new String[] { "Point", "Line", "Polygon" };
        typeBuilder.setItems(items, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int item) {
                Map<String, String> userData = new HashMap<String, String>();
                switch (item) {
                case 0:
                    mapView.createElement(Point.class, userData);
                    break;
                case 1:
                    mapView.createElement(Line.class, userData);
                    break;
                case 2:
                    mapView.createElement(Polygon.class, userData);
                    break;
                }
            }
        });
        AlertDialog typeDialog = typeBuilder.create();
        typeDialog.show();
    }

    @Override
    protected void attachEditableElementToLayer(Geometry element) {
        List<EditableGeometryLayer> layers = getEditableLayers();
        if (layers.isEmpty()) {
            return;
        }
        if (element instanceof Point) {
            Point point = (Point) element;
            point.setStyleSet(pointStyleSet);
            layers.get(0).add(point); 
        } else if (element instanceof Line) {
            Line line = (Line) element;
            line.setStyleSet(lineStyleSet);
            layers.get(0).add(line); 
        } else if (element instanceof Polygon) {
            Polygon polygon = (Polygon) element;
            polygon.setStyleSet(polygonStyleSet);
            layers.get(0).add(polygon);
        }
    }

    @Override
    protected List<String> getEditableElementUserColumns(Geometry element) {
        @SuppressWarnings("unchecked")
        Map<String, String> userData = (Map<String, String>) element.userData;
        return new ArrayList<String>(userData.keySet());
    }

    @Override
    protected Dialog onCreateDialog(int id) {
        switch(id) {
        case DIALOG_TABLE_LIST:
            return new AlertDialog.Builder(this)
            .setTitle("Select table:")
            .setSingleChoiceItems(tableList, 0, null)
            .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int whichButton) {
                    dialog.dismiss();
                    int selectedPosition = ((AlertDialog) dialog)
                            .getListView().getCheckedItemPosition();
                    createEditableSpatialiteLayers(selectedPosition);
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

        if (tables.size() > 0) {
            tableList = (String[]) tables.toArray(new String[0]);
            showDialog(DIALOG_TABLE_LIST);
        } else {
            showDialog(DIALOG_NO_TABLES);
        }
    }

    private void createStyleSets() {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        float dpi = metrics.density;

        pointStyleSet = new StyleSet<PointStyle>();
        PointStyle pointStyle = PointStyle.builder().setColor(Color.GREEN).setSize(0.2f).build();
        pointStyleSet.setZoomStyle(0, pointStyle);

        lineStyleSet = new StyleSet<LineStyle>();
        LineStyle lineStyle = LineStyle.builder().setWidth(0.1f).setColor(Color.BLUE).build();
        lineStyleSet.setZoomStyle(0, lineStyle);

        polygonStyleSet = new StyleSet<PolygonStyle>();
        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(Color.BLUE | Color.GREEN).build();
        polygonStyleSet.setZoomStyle(0, polygonStyle);

        labelStyle = 
                LabelStyle.builder()
                .setEdgePadding((int) (12 * dpi))
                .setLinePadding((int) (6 * dpi))
                .setTitleFont(Typeface.create("Arial", Typeface.BOLD), (int) (16 * dpi))
                .setDescriptionFont(Typeface.create("Arial", Typeface.NORMAL), (int) (13 * dpi))
                .build();
    }

    private void createEditableOGRLayers(String dbPath) {
        // create editable data source and layer
        EditableOGRVectorDataSource dataSource;
        try {
            dataSource = new EditableOGRVectorDataSource(new EPSG3857(), dbPath, null) {

                @Override
                protected Label createLabel(Map<String, String> userData) {
                    return EditableVectorFileMapActivity.this.createLabel(userData);
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
        EditableGeometryLayer dbEditableLayer = new EditableGeometryLayer(dataSource);
        mapView.getLayers().addLayer(dbEditableLayer);

        // zoom map to data extent
        Envelope extent = dbEditableLayer.getDataExtent();
        mapView.setBoundingBox(new Bounds(extent.minX, extent.maxY, extent.maxX, extent.minY), false);
    }

    private void createEditableSpatialiteLayers(int selectedPosition) {
        // find out which table and geometry column to be opened - here it comes from earlier UI interaction steps
        String[] tableKey = tableList[selectedPosition].split("\\.");
        String tableName = tableKey[0];
        String geomColumn = tableKey[1];

        // create editable data source and layer
        EditableSpatialiteDataSource dataSource = new EditableSpatialiteDataSource(new EPSG3857(), spatialLite, tableName, geomColumn, new String[]{"name"}, null) {

            @Override
            protected Label createLabel(Map<String, String> userData) {
                return EditableVectorFileMapActivity.this.createLabel(userData);
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
        EditableGeometryLayer dbEditableLayer = new EditableGeometryLayer(dataSource);
        mapView.getLayers().addLayer(dbEditableLayer);

        // zoom map to data extent
        Envelope extent = dbEditableLayer.getDataExtent();
        mapView.setBoundingBox(new Bounds(extent.minX, extent.maxY, extent.maxX, extent.minY), false);
    }

    private Label createLabel(Map<String, String> userData) {
        StringBuffer labelTxt = new StringBuffer();
        for(Map.Entry<String, String> entry : userData.entrySet()){
            labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
        }
        return new DefaultLabel("Data:", labelTxt.toString(), labelStyle);
    }

    // Methods for FilePicker

    @Override
    public String getFileSelectMessage() {
        return "Select Spatialite database file (.spatialite, .db, .sqlite) or OGR shape file (.shp)";
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
                    } else if (file.isFile() && (file.getName().endsWith(".db") || file.getName().endsWith(".sqlite") || file.getName().endsWith(".spatialite") || file.getName().endsWith(".shp"))) {
                        // accept files with given extension
                        return true;
                    }
                }
                return false;
            };
        };
    }

}
