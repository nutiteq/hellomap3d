package com.nutiteq.editable.layers.deprecated;

import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.content.Context;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.db.SpatialLiteDbHelper;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.LongHashMap;

/**
 * 
 * Local Spatialite database layer that supports editing. 
 * 
 * @author mtehver
 *
 */
@Deprecated
public class EditableSpatialiteLayer extends EditableGeometryDbLayer {
    private SpatialLiteDbHelper spatialLite;
    private SpatialLiteDbHelper.DbLayer dbLayer;

    private int maxObjects;
    private String[] userColumns;

    /**
     * Default constructor.
     * 
     * @param proj Layer projection
     * @param dbPath Spatialite file name full path 
     * @param tableName
     * @param geomColumnName column in tableName which has geometries
     * @param userColumns include values from these additional columns to userData
     * @param maxObjects maximum number of loaded objects, suggested <2000 or so
     * @param pointStyleSet required if layer has points
     * @param lineStyleSet required if layer has lines
     * @param polygonStyleSet required if layer has lines
     * @param context Activity who controls the layer
     * @throws IOException 
     */
    public EditableSpatialiteLayer(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, int maxObjects,
            StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet, Context context) throws IOException {
        super(proj, pointStyleSet, lineStyleSet, polygonStyleSet, context);
        this.userColumns = userColumns;
        this.maxObjects = maxObjects;

        spatialLite = new SpatialLiteDbHelper(dbPath);
        Map<String, SpatialLiteDbHelper.DbLayer> dbLayers = spatialLite.qrySpatialLayerMetadata();
        for (String layerKey : dbLayers.keySet()) {
            SpatialLiteDbHelper.DbLayer layer = dbLayers.get(layerKey);
            if (layer.table.compareTo(tableName) == 0
                    && layer.geomColumn.compareTo(geomColumnName) == 0) {
                this.dbLayer = layer;
                break;
            }
        }


        if (this.dbLayer == null) {
            Log.error("EditableSpatialiteLayer: Could not find a matching layer " + tableName + "." + geomColumnName);
        }

        // fix/add SDK SRID definition for conversions
        spatialLite.defineEPSG3857();
    }

    public String[] getUserColumns() {
        return userColumns;
    }

    @Override
    protected LongHashMap<Geometry> queryElements(Envelope envelope, int zoom) {
        if (dbLayer == null) {
            return new LongHashMap<Geometry>();
        }

        // TODO: use fromInternal(envelope) here
        MapPos bottomLeft = projection.fromInternal(envelope.getMinX(), envelope.getMinY());
        MapPos topRight = projection.fromInternal(envelope.getMaxX(), envelope.getMaxY());
        List<Geometry> objectTemp = spatialLite.qrySpatiaLiteGeom(new Envelope(bottomLeft.x, topRight.x,bottomLeft.y, topRight.y), maxObjects, dbLayer, userColumns, 0, 0);

        LongHashMap<Geometry> elementMap = new LongHashMap<Geometry>(); 
        for (Geometry object : objectTemp){
            elementMap.put(object.getId(), object);
        }
        return elementMap;
    }

    @Override
    protected long insertElement(Geometry element) {
        return spatialLite.insertSpatiaLiteGeom(dbLayer, element);		
    }

    @Override
    protected void updateElement(long id, Geometry element) {
        spatialLite.updateSpatiaLiteGeom(dbLayer, id, element);
    }

    @Override
    protected void deleteElement(long id) {
        spatialLite.deleteSpatiaLiteGeom(dbLayer, id);
    }

    @SuppressWarnings("unchecked")
    @Override
    protected Label createLabel(Object userData) {
        StringBuffer labelTxt = new StringBuffer();
        for(Map.Entry<String, String> entry : ((Map<String, String>) userData).entrySet()){
            labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
        }

        return new DefaultLabel("Data:", labelTxt.toString());
    }

    @SuppressWarnings("unchecked")
    @Override
    protected Object cloneUserData(Object userData) {
        return new HashMap<String, String>((Map<String, String>) userData);
    }

    @Override
    public Envelope getDataExtent() {
        return spatialLite.qryDataExtent(dbLayer);
    }
}
