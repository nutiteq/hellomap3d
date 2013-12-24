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
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.Utils;
import com.nutiteq.utils.WkbRead;
import com.nutiteq.utils.WkbRead.GeometryFactory;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

public class CartoDbDataSource extends AbstractVectorDataSource<Geometry> {
  protected static final Projection CARTODB_PROJECTION = new EPSG3857();

  protected static final String TAG_CARTODB_ID = "cartodb_id";
  protected static final String TAG_GEOM = "the_geom";
  protected static final String TAG_GEOM_WEBMERCATOR = "the_geom_webmercator";
  protected static final String TAG_ROWS = "rows";

  protected static final CharSequence PLACEHOLDER_BBOX = "!bbox!";
  protected static final CharSequence PLACEHOLDER_ENVELOPE = "!envelope!";

  protected final String account;
  protected final String sql;

  protected StyleSet<PointStyle> pointStyleSet;
  protected StyleSet<LineStyle> lineStyleSet;
  protected StyleSet<PolygonStyle> polygonStyleSet;
  protected int minZoom;

  public CartoDbDataSource(String account, String sql, StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet) {
    super(CARTODB_PROJECTION);
    this.account = account;
    this.sql = sql;

    this.pointStyleSet = pointStyleSet;
    this.lineStyleSet = lineStyleSet;
    this.polygonStyleSet = polygonStyleSet;

    if (pointStyleSet != null) {
      minZoom = Math.min(minZoom, pointStyleSet.getFirstNonNullZoomStyleZoom());
    }
    if (lineStyleSet != null) {
      minZoom = Math.min(minZoom, lineStyleSet.getFirstNonNullZoomStyleZoom());
    }
    if (polygonStyleSet != null) {
      minZoom = Math.min(minZoom, polygonStyleSet.getFirstNonNullZoomStyleZoom());
    }
  }

  @Override
  public Envelope getDataExtent() {
    // TODO: implement
    return null;
  }

  @Override
  public Collection<Geometry> loadElements(CullState cullState) {
    if (cullState.zoom < minZoom) {
      return null;
    }
    
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
    Log.debug("CartoDB sql: " + sql);

    final List<Geometry> elements = new LinkedList<Geometry>();
    try {
      Uri.Builder uri = Uri.parse("http://" + this.account + ".cartodb.com/api/v2/sql").buildUpon();
      uri.appendQueryParameter("q", sql);
      Log.debug("CartoDB url: " + uri.build().toString());

      JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

      if (jsonData == null){
        Log.debug("No CartoDB data");
        return elements;
      }

      JSONArray rows = jsonData.getJSONArray(TAG_ROWS);

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
              Log.error("Error parsing JSON keys " + e.toString());
            }
          }
        }

        final Label label = createLabel(userData);
        final long id = row.getLong(TAG_CARTODB_ID);
        final String geomString = row.getString(TAG_GEOM_WEBMERCATOR);
        final byte[] wkb = Utils.hexStringToByteArray(geomString);

        Geometry[] geoms = WkbRead.readWkb(new ByteArrayInputStream(wkb), new GeometryFactory() {

          @Override
          public Point createPoint(MapPos mapPos, int srid) {
            return new Point(mapPos, label, pointStyleSet, userData);
          }

          @Override
          public Line createLine(List<MapPos> points, int srid) {
            return new Line(points, label, lineStyleSet, userData);
          }

          @Override
          public Polygon createPolygon(List<MapPos> outerRing, List<List<MapPos>> innerRings, int srid) {
            return new Polygon(outerRing, innerRings, label, polygonStyleSet, userData);
          }

          @Override
          public Geometry[] createMultigeometry(List<Geometry> geomList) {
            return geomList.toArray(new Geometry[geomList.size()]);
          }

        });

        for (int j = 0; j < geoms.length; j++) {
          geoms[j].setId(id);
          elements.add(geoms[j]);
        }
      }
    }
    catch (ParseException e) {
      Log.error("Error parsing data " + e.toString());
    } catch (JSONException e) {
      Log.error("Error parsing JSON data " + e.toString());
    }

    long timeEnd = System.currentTimeMillis();
    Log.debug("CartoDbLayer loaded N:"+ elements.size()+" time ms:"+(timeEnd-timeStart));
    return elements;
  }

  protected Label createLabel(Map<String, String> userData) {
    StringBuffer labelTxt = new StringBuffer();
    for (Map.Entry<String, String> entry : userData.entrySet()){
      labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
    }
    return new DefaultLabel("Data:",labelTxt.toString());
  }

}
