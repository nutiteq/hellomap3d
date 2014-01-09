package com.nutiteq.datasources.vector;

import java.io.IOException;
import java.util.Collection;
import java.util.List;
import java.util.Map;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.db.SpatialLiteDbHelper;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.WkbRead.GeometryFactory;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

/**
 * Abstract data source for local Spatialite databases.
 * Instances need to define factory methods for creating style sets based on element metadata.
 *  
 * @author mark
 *
 */
public abstract class SpatialiteDataSource extends AbstractVectorDataSource<Geometry> {
    protected final SpatialLiteDbHelper spatialLite;
    protected SpatialLiteDbHelper.DbLayer dbLayer;

    private float autoSimplifyPixels = 0;
    private int screenWidth = 0;
    private int maxElements = Integer.MAX_VALUE;
    private String[] userColumns;
    private String filter;

    /**
     * Default constructor.
     * 
     * @param proj data source projection
     * @param dbPath path to Spatialite file
     * @param tableName table from the database
     * @param geomColumnName geometry column from the table
     * @param userColumns load data from these columns as userData
     * @param filter filter expression for queries
     * @throws IOException 
     */
    public SpatialiteDataSource(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, String filter) throws IOException {
        this(proj, new SpatialLiteDbHelper(dbPath), tableName, geomColumnName, userColumns, filter);
    }

    /**
     * Construct data source with the SpatialLiteDb already opened, and filters
     * 
     * @param proj data source projection
     * @param spatialLiteDb Spatialite database
     * @param tableName table from the database
     * @param geomColumnName geometry column from the table
     * @param userColumns load data from these columns as userData
     * @param filter SQL filter to select some objects, used for WHERE
     */
    public SpatialiteDataSource(Projection proj, SpatialLiteDbHelper spatialLiteDb,
            String tableName, String geomColumnName, String[] userColumns, String filter) {

        super(proj);
        this.userColumns = userColumns;
        this.spatialLite = spatialLiteDb;
        this.filter = filter;

        Map<String, SpatialLiteDbHelper.DbLayer> dbLayers = spatialLite.qrySpatialLayerMetadata();
        for (String layerKey : dbLayers.keySet()) {
            SpatialLiteDbHelper.DbLayer layer = dbLayers.get(layerKey);
            if (layer.table.compareTo(tableName) == 0 && layer.geomColumn.compareTo(geomColumnName) == 0) {
                this.dbLayer = layer;
                break;
            }
        }

        if (this.dbLayer == null) {
            Log.error("SpatialiteDataSource: Could not find a matching layer " + tableName + "." + geomColumnName);
        }

        // get all columns as user columns
        if (this.userColumns == null){
            this.userColumns = spatialLite.qryColumns(dbLayer);
        }

        // fix/add SDK SRID definition for conversions
        spatialLite.defineEPSG3857();
    }

    /**
     * Limit maximum objects returned by each query.
     * 
     * @param maxElements maximum objects
     */
    public void setMaxElements(int maxElements) {
        this.maxElements = maxElements;

        notifyElementsChanged();
    }

    /**
     * Set auto-simplification parameters for queries.
     * 
     * @param autoSimplifyPixels
     *          maximum allowed error resulting from simplification
     * @param screenWidth
     *          target screen width in pixels
     */
    public void setAutoSimplify(float autoSimplifyPixels, int screenWidth) {
        this.autoSimplifyPixels = autoSimplifyPixels;
        this.screenWidth = screenWidth;

        notifyElementsChanged();
    }

    @Override
    public Envelope getDataExtent() {
        return spatialLite.qryDataExtent(dbLayer);
    }

    @Override
    public Collection<Geometry> loadElements(final CullState cullState) {
        if (dbLayer == null) {
            return null;
        }

        Envelope envelope = projection.fromInternal(cullState.envelope);

        // Create WKB geometry factory
        GeometryFactory geomFactory = new GeometryFactory() {

            @SuppressWarnings("unchecked")
            @Override
            public Point createPoint(MapPos mapPos, Object userData) {
                Label label = createLabel((Map<String, String>) userData);
                return new Point(mapPos, label, createPointStyleSet((Map<String, String>) userData, cullState.zoom), userData);
            }

            @SuppressWarnings("unchecked")
            @Override
            public Line createLine(List<MapPos> points, Object userData) {
                Label label = createLabel((Map<String, String>) userData);
                return new Line(points, label, createLineStyleSet((Map<String, String>) userData, cullState.zoom), userData);
            }

            @SuppressWarnings("unchecked")
            @Override
            public Polygon createPolygon(List<MapPos> outerRing, List<List<MapPos>> innerRings, Object userData) {
                Label label = createLabel((Map<String, String>) userData);
                return new Polygon(outerRing, innerRings, label, createPolygonStyleSet((Map<String, String>) userData, cullState.zoom), userData);
            }

            @Override
            public Geometry[] createMultigeometry(List<Geometry> geomList) {
                return geomList.toArray(new Geometry[geomList.size()]);
            }

        };
        
        // Perform the query
        List<Geometry> elements = spatialLite.qrySpatiaLiteGeom(envelope, maxElements, dbLayer, userColumns, filter, autoSimplifyPixels, screenWidth, geomFactory);
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
