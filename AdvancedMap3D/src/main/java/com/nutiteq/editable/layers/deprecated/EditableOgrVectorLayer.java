package com.nutiteq.editable.layers.deprecated;

import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.gdal.ogr.ogr;

import android.content.Context;
import android.net.ParseException;

import com.nutiteq.components.Envelope;
import com.nutiteq.db.OGRFileHelper;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LabelStyle;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.LongHashMap;

/**
 * 
 * OGR layer that supports editing. 
 * 
 * @author mtehver
 *
 */
@Deprecated
public class EditableOgrVectorLayer extends EditableGeometryDbLayer {

    private OGRFileHelper ogrHelper;
    private LabelStyle labelStyle;

    static {
        ogr.RegisterAll();
    }

    static {
        try {
            // force Java to load PROJ.4 library. Needed as we don't call it directly, but 
            // OGR datasource reading may need it.
            System.loadLibrary("proj");        
        } catch (Throwable t) {
            System.err.println("Unable to load proj: " + t);
        }
    }

    /**
     * Default constructor.
     * 
     * @param proj Layer projection
     * @param fileName datasource name: file or connection string
     * @param tableName table (OGR layer) name, needed for multi-layer datasets. If null, takes the first layer from dataset
     * @param multiGeometry true if object must be saved as MULTIgeometry
     * @param userColumns include values from these additional columns to userData
     * @param maxObjects maximum number of loaded objects, suggested <2000 or so
     * @param pointStyleSet required if layer has points
     * @param lineStyleSet required if layer has lines
     * @param polygonStyleSet required if layer has lines
     * @param labelStyle 
     * @param context Activity who controls the layer
     * @throws IOException 
     */
    public EditableOgrVectorLayer(Projection proj, String fileName, String tableName, boolean multiGeometry, int maxObjects, 
            final StyleSet<PointStyle> pointStyleSet, final StyleSet<LineStyle> lineStyleSet, final StyleSet<PolygonStyle> polygonStyleSet, LabelStyle labelStyle, Context context) throws IOException {
        super(proj, pointStyleSet, lineStyleSet, polygonStyleSet, context);

        this.ogrHelper = new OGRFileHelper(fileName, tableName, true) {
            @Override
            protected Label createLabel(Map<String, String> userData) {
                return EditableOgrVectorLayer.this.createLabel(userData);
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
        this.ogrHelper.setMaxElements(maxObjects);
        this.labelStyle = labelStyle;

        if (pointStyleSet != null) {
            minZoom = Math.min(minZoom, pointStyleSet.getFirstNonNullZoomStyleZoom());
        }
        if (lineStyleSet != null) {
            minZoom = Math.min(minZoom, lineStyleSet.getFirstNonNullZoomStyleZoom());
        }
        if (polygonStyleSet != null) {
            minZoom = Math.min(minZoom, polygonStyleSet.getFirstNonNullZoomStyleZoom());
        }
        Log.debug("ogrLayer style minZoom = "+minZoom);

    }

    @Override
    protected LongHashMap<Geometry> queryElements(Envelope envelope, int zoom) {
        // load geometries
        LongHashMap<Geometry> newElementMap = new LongHashMap<Geometry>();

        try {
            List<Geometry> data = ogrHelper.loadData(projection.fromInternal(envelope), zoom);
            for (Geometry geom : data) {
                geom.attachToLayer(this);
                geom.setActiveStyle(zoom);
                newElementMap.put(geom.getId(), geom);
            }
        }
        catch (ParseException e) {
            Log.error("EditableOgrVectorLayer: Error parsing data " + e.toString());
        }

        return newElementMap;
    }

    @Override
    protected long insertElement(Geometry element) {

        long id = ogrHelper.insertElement(element);
        Log.debug("inserted feature to OGR with id "+id);

        return id;
    }

    @Override
    protected void updateElement(long id, Geometry element) {
        ogrHelper.updateElement(id, element);
    }

    @Override
    protected void deleteElement(long id) {
        ogrHelper.deleteElement(id);
    }

    @SuppressWarnings("unchecked")
    @Override
    protected Label createLabel(Object userData) {
        StringBuffer labelTxt = new StringBuffer();
        for(Map.Entry<String, String> entry : ((Map<String, String>) userData).entrySet()){
            labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
        }

        return new DefaultLabel("Data:", labelTxt.toString(), labelStyle);
    }

    @SuppressWarnings("unchecked")
    @Override
    protected Object cloneUserData(Object userData) {
        return new HashMap<String, String>((Map<String, String>) userData);
    }

    @Override
    public Envelope getDataExtent() {
        return ogrHelper.getDataExtent();
    }

}
