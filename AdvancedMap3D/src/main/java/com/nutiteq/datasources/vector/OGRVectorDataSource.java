package com.nutiteq.datasources.vector;

import java.io.IOException;
import java.util.Collection;
import java.util.List;
import java.util.Map;

import org.gdal.ogr.ogr;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.db.OGRFileHelper;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.Label;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

/**
 * Datasource for reading OGR file data sources.
 * Instances need to define factory methods for creating style sets based on element metadata.
 *  
 * @author jaak
 *
 */
public abstract class OGRVectorDataSource extends AbstractVectorDataSource<Geometry> {

    protected final OGRFileHelper ogrHelper;

    static {
        ogr.RegisterAll();
    }

    static {
        try {
            // force Java to load PROJ.4 library. Needed as we don't call it directly, but 
            // OGR datasource reading may need it.
            System.loadLibrary("proj");

            // OGR stdout redirect
            new Thread() {
                @Override
                public void run() {
                    ogr.nativePipeSTDERRToLogcat();
                }
            }.start();
        } catch (Throwable t) {
            System.err.println("OGRVectorDataSource: Unable to load proj: " + t);
        }
    }

    /**
     * Open OGR datasource. Datasource properties depend on particular data type, e.g. for Shapefile just give file name
     * This sample tries to read whole layer, you probably need adjustments to optimize reading depending on data specifics
     * 
     * @param proj layer projection. NB! data must be in the same projection
     * @param fileName datasource name: file or connection string
     * @param tableName table (OGR layer) name, needed for multi-layer datasets. If null, takes the first layer from dataset
     * @throws IOException file not found or other problem opening OGR datasource
     */
    public OGRVectorDataSource(Projection proj, String fileName, String tableName) throws IOException {
        this(proj, fileName, tableName, false);
    }

    /**
     * Open OGR datasource. Datasource properties depend on particular data type, e.g. for Shapefile just give file name
     * This sample tries to read whole layer, you probably need adjustments to optimize reading depending on data specifics
     * 
     * @param proj layer projection. NB! data must be in the same projection
     * @param fileName datasource name: file or connection string
     * @param tableName table (OGR layer) name, needed for multi-layer datasets. If null, takes the first layer from dataset
     * @param update if true, file will be opened in read-write mode. If false, file is only readable
     * @throws IOException file not found or other problem opening OGR datasource
     */
    public OGRVectorDataSource(Projection proj, String fileName, String tableName, boolean update) throws IOException {
        super(proj);

        this.ogrHelper = new OGRFileHelper(fileName, tableName, update) {
            @Override
            protected Label createLabel(Map<String, String> userData) {
                return OGRVectorDataSource.this.createLabel(userData);
            }

            @Override
            protected StyleSet<PointStyle> createPointStyleSet(Map<String, String> userData, int zoom) {
                return OGRVectorDataSource.this.createPointStyleSet(userData, zoom);
            }

            @Override
            protected StyleSet<LineStyle> createLineStyleSet(Map<String, String> userData, int zoom) {
                return OGRVectorDataSource.this.createLineStyleSet(userData, zoom);
            }

            @Override
            protected StyleSet<PolygonStyle> createPolygonStyleSet(Map<String, String> userData, int zoom) {
                return OGRVectorDataSource.this.createPolygonStyleSet(userData, zoom);
            }
        };
    }

    /**
     * Limit maximum objects returned by each query.
     * 
     * @param maxElements maximum objects
     */
    public void setMaxElements(int maxElements) {
        ogrHelper.setMaxElements(maxElements);

        notifyElementsChanged();
    }

    @Override
    public Envelope getDataExtent() {
        return ogrHelper.getDataExtent();
    }

    @Override
    public Collection<Geometry> loadElements(CullState cullState) {
        Envelope envelope = projection.fromInternal(cullState.envelope);

        List<Geometry> elements = ogrHelper.loadData(envelope, cullState.zoom);
        for (Geometry element : elements) {
            element.attachToDataSource(this);
        }
        return elements;
    }

    protected abstract Label createLabel(Map<String, String> userData);

    protected abstract StyleSet<PointStyle> createPointStyleSet(Map<String, String> userData, int zoom);

    protected abstract StyleSet<LineStyle> createLineStyleSet(Map<String, String> userData, int zoom);

    protected abstract StyleSet<PolygonStyle> createPolygonStyleSet(Map<String, String> userData, int zoom);
}
