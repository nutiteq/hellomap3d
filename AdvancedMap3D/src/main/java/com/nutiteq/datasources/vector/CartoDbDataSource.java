package com.nutiteq.datasources.vector;

import java.io.ByteArrayInputStream;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.net.ParseException;
import android.net.Uri;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
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
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.Utils;
import com.nutiteq.utils.WkbRead;
import com.nutiteq.utils.WkbRead.GeometryFactory;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

/**
 * Abstract data source for online CartoDB geometry.
 * Instances need to define factory methods for creating style sets based on element metadata.
 * 
 * @author mark
 *
 */
public abstract class CartoDbDataSource extends AbstractVectorDataSource<Geometry> {
    protected static final String TAG_CARTODB_ID = "cartodb_id";
    protected static final String TAG_GEOM = "the_geom";
    protected static final String TAG_GEOM_WEBMERCATOR = "the_geom_webmercator";
    protected static final String TAG_ROWS = "rows";

    protected static final CharSequence PLACEHOLDER_BBOX = "!bbox!";
    protected static final CharSequence PLACEHOLDER_ENVELOPE = "!envelope!";

    protected final String account;
    protected final String sql;

    private int maxElements = Integer.MAX_VALUE;

    /**
     * Default constructor.
     * 
     * @param projection
     *        projection for the data source (almost always EPSG3857)
     * @param account
     *        CartoDB account id
     * @param sql
     *        SQL sentence for loading data. Should contain placeholders (!bbox!, !envelope!) and tags 
     */
    public CartoDbDataSource(Projection projection, String account, String sql) {
        super(projection);
        this.account = account;
        this.sql = sql;
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

    @Override
    public Envelope getDataExtent() {
        return null;
    }

    @Override
    public Collection<Geometry> loadElements(final CullState cullState) {
        Envelope envelope = projection.fromInternal(cullState.envelope);

        long timeStart = System.currentTimeMillis();

        String bboxString = "" + envelope.getMinX() + " " + envelope.getMinY() + "," + envelope.getMaxX() + " " + envelope.getMaxY();
        String envString = "";
        MapPos[] convexHull = envelope.getConvexHull();
        if (convexHull.length > 0) {
            for (int i = 0; i <= convexHull.length; i++) {
                MapPos point = convexHull[i % convexHull.length];
                if (envString.length() > 0) envString += ",";
                envString += point.x + " " + point.y;
            }
        }
        String sql = this.sql.replace(PLACEHOLDER_BBOX, bboxString).replace(PLACEHOLDER_ENVELOPE, envString);
        Log.debug("CartoDbDataSource: sql: " + sql);

        final List<Geometry> elements = new LinkedList<Geometry>();
        try {
            Uri.Builder uri = Uri.parse("http://" + this.account + ".cartodb.com/api/v2/sql").buildUpon();
            uri.appendQueryParameter("q", sql);
            Log.debug("CartoDbDataSource: url: " + uri.build().toString());

            JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

            if (jsonData == null){
                Log.debug("CartoDbDataSource: no data");
                return elements;
            }

            JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
            
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

            for (int i = 0; i < rows.length(); i++) {
                JSONObject row = rows.getJSONObject(i);

                final Map<String, String> userData = new HashMap<String, String>();
                for (Iterator<?> it = row.keys(); it.hasNext(); ) {
                    String key = (String) it.next();
                    if (!key.equals(TAG_GEOM_WEBMERCATOR) && !key.equals(TAG_GEOM) && !key.equals(TAG_CARTODB_ID)) {
                        try {
                            Object value =  row.get(key);
                            userData.put(key, value.toString());
                        } catch (JSONException e) { 
                            Log.error("CartoDbDataSource: error parsing JSON keys " + e.toString());
                        }
                    }
                }

                final long id = row.getLong(TAG_CARTODB_ID);
                final String geomString = row.getString(TAG_GEOM_WEBMERCATOR);
                final byte[] wkb = Utils.hexStringToByteArray(geomString);
                Geometry[] geoms = WkbRead.readWkb(new ByteArrayInputStream(wkb), geomFactory, userData);

                for (Geometry geom : geoms) {
                    if (elements.size() < maxElements) {
                        geom.setId(id);
                        geom.attachToDataSource(this);
                        elements.add(geom);
                    }
                }
            }

        } catch (ParseException e) {
            Log.error("CartoDbDataSource: error parsing data " + e.toString());
        } catch (JSONException e) {
            Log.error("CartoDbDataSource: error parsing JSON data " + e.toString());
        }

        long timeEnd = System.currentTimeMillis();
        Log.debug("CartoDbDataSource: loaded N: " + elements.size() + " time ms:" + (timeEnd - timeStart));
        return elements;
    }

    protected abstract Label createLabel(Map<String, String> userData);

    protected abstract StyleSet<PointStyle> createPointStyleSet(Map<String, String> userData, int zoom);

    protected abstract StyleSet<LineStyle> createLineStyleSet(Map<String, String> userData, int zoom);

    protected abstract StyleSet<PolygonStyle> createPolygonStyleSet(Map<String, String> userData, int zoom);
}
