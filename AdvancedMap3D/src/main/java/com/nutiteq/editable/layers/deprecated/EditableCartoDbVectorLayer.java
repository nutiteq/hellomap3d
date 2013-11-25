package com.nutiteq.editable.layers.deprecated;

import java.io.ByteArrayInputStream;
import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.net.ParseException;
import android.net.Uri;

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
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.Utils;
import com.nutiteq.utils.WkbRead;
import com.nutiteq.utils.WktWriter;

/**
 * 
 * CartoDb layer that supports editing. 
 * 
 * @author mtehver
 *
 */
@Deprecated
public class EditableCartoDbVectorLayer extends EditableGeometryDbLayer {
	public static final String TAG_GEOM = "the_geom";
	public static final String TAG_GEOM_WEBMERCATOR = "the_geom_webmercator";
	public static final String TAG_CARTODB_ID = "cartodb_id";
	private static final String TAG_ROWS = "rows";

	private static final CharSequence PLACEHOLDER_BBOX = "!bbox!";
	private static final CharSequence PLACEHOLDER_ID = "!id!";
	private static final CharSequence PLACEHOLDER_GEOM = "!geom!";

	private final String account;
	private final String apiKey;
	private final String querySql;
	private final String insertSql;
	private final String updateSql;
	private final String deleteSql;
	private final boolean multiGeometry; 

	/**
	 * Default constructor.
	 * 
	 * @param proj Layer projection
	 * @param account Your CartoDB Account
	 * @param apiKey Your CartoDB API Key, get it from CartoDB account settings page 
	 * @param querySql SQL template to query data. 
	 *     First returned columns must be: cartodb_id (unique id), the_geom_webmercator (geometry), name (a string). 
	 *     You can add more columns, these will go to userData. 
	 *     Query parameter: !bbox!
	 * @param insertSql SQL template to insert data. Query parameter: !geom!
	 * @param updateSql SQL template to update data. Query parameters: !geom!, !name! and !id!
	 * @param deleteSql SQL template for deleting. Query parameter: !id!
	 * @param multiGeometry true if object must be saved as MULTIgeometry
     * @param pointStyleSet required if layer has points
     * @param lineStyleSet required if layer has lines
     * @param polygonStyleSet required if layer has lines
     * @param context Activity who controls the layer
	 */
	public EditableCartoDbVectorLayer(Projection proj, String account, String apiKey,
			String querySql, String insertSql, String updateSql, String deleteSql, boolean multiGeometry,
			StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet, Context context) {
		super(proj, pointStyleSet, lineStyleSet, polygonStyleSet, context);
		this.account = account;
		this.apiKey = apiKey;
		this.querySql = querySql;
		this.insertSql = insertSql;
		this.updateSql = updateSql;
		this.deleteSql = deleteSql;
		this.multiGeometry = multiGeometry;
	}

	@Override
	protected LongHashMap<Geometry> queryElements(Envelope envelope, int zoom) {
		long timeStart = System.currentTimeMillis();

		// TODO: use fromInternal(envelope) here
		MapPos minPos = projection.fromInternal(envelope.getMinX(), envelope.getMinY());
		MapPos maxPos = projection.fromInternal(envelope.getMaxX(), envelope.getMaxY());

		String sql = this.querySql.replace(PLACEHOLDER_BBOX, ""+minPos.x+" "+minPos.y+","+maxPos.x+" "+maxPos.y);
		Log.debug("EditableCartoDbVectorLayer: query sql: " + sql);

		// load geometries
		LongHashMap<Geometry> newElementMap = new LongHashMap<Geometry>();

		try {
			Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
			uri.appendQueryParameter("q", sql);
			Log.debug("EditableCartoDbVectorLayer: query url:" + uri.build().toString());

			JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

			if (jsonData == null){
				Log.debug("EditableCartoDbVectorLayer: No CartoDB data");
				return newElementMap;
			}

			JSONArray rows = jsonData.getJSONArray(TAG_ROWS);

			for (int i = 0; i < rows.length(); i++) {
				JSONObject row = rows.getJSONObject(i);

				// copy all fields to userData object
				final Map<String, String> userData = new HashMap<String, String>();
				for (@SuppressWarnings("unchecked")Iterator<String> iter = row.keys(); iter.hasNext(); ) {
					String key = iter.next();
					// exclude geom and id fields
					if (!key.equals(TAG_GEOM_WEBMERCATOR) && !key.equals(TAG_GEOM) && !key.equals(TAG_CARTODB_ID)) {
						try {
							Object value = row.get(key);
							userData.put(key, value.toString());
						} catch (JSONException e) { 
							Log.error("EditableCartoDbVectorLayer: Error parsing JSON keys " + e.toString());
						}
					}
				}

				long id = row.getLong(TAG_CARTODB_ID);
				String the_geom_webmercator = row.getString(TAG_GEOM_WEBMERCATOR);
				byte[] wkb = Utils.hexStringToByteArray(the_geom_webmercator);

				Geometry[] geoms = WkbRead.readWkb(new ByteArrayInputStream(wkb), userData);
				if (geoms.length > 1) {
					Log.warning("EditableCartoDbLayer: multigeometry, ignoring");
					continue;
				}
				for (Geometry geom : geoms) {
					newElementMap.put(id, geom);
				}
			}
		}
		catch (ParseException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing data " + e.toString());
		} catch (JSONException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing JSON data " + e.toString());
		}

		long timeEnd = System.currentTimeMillis();
		Log.debug("EditableCartoDbVectorLayer: loaded N:" + newElementMap.size()+" time ms:" + (timeEnd-timeStart));
		return newElementMap;
	}
	
	private String getGeometryType(Geometry element) {
		String multi = (multiGeometry ? "MULTI" : "");
		if (element instanceof Point) {
			return multi + "POINT";
		} else if (element instanceof Line) {
			return multi + "LINE";
		} else if (element instanceof Polygon) {
			return multi + "POLYGON";
		}
		return null;
	}
	
	private static String replaceSqlMap(String sql, Map<String, String> tokens) {
		for (Iterator<Map.Entry<String, String>> it = tokens.entrySet().iterator(); it.hasNext(); ) {
			Map.Entry<String, String> entry = it.next();
			String key = entry.getKey();
			String value = entry.getValue();
			String encValue;
			if (value == null) {
				encValue = "NULL";
			} else {
				String hexValue;
				try {
					hexValue = String.format("%x", new BigInteger(1, value.getBytes("UTF-8")));
				} catch (UnsupportedEncodingException e) {
					throw new RuntimeException("Unsupported character");
				}
				encValue = "convert_from('\\x" + hexValue + "', 'UTF8')";
			}
			sql = sql.replace("!" + key + "!", encValue);
		}
		return sql;
	}
	
	@Override
	protected long insertElement(Geometry element) {
		String wktGeom = WktWriter.writeWkt(element, getGeometryType(element));
		String sql = this.insertSql.replace(PLACEHOLDER_GEOM, "GeometryFromText('" + wktGeom + "',3857)");
		if (element.userData instanceof Map<?, ?>) {
			@SuppressWarnings("unchecked")
			Map<String, String> tokens = (Map<String, String>) element.userData;
			sql = replaceSqlMap(sql, tokens); 
		}
		Log.debug("EditableCartoDbVectorLayer: insert sql: " + sql);

		try {
			Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
			Map<String, String> params = new HashMap<String, String>();
			params.put("q", sql);
			params.put("api_key", apiKey);
			Log.debug("EditableCartoDbVectorLayer: insert url:" + uri.build().toString());
			JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString(), params);

			if (jsonData == null) {
				Log.debug("EditableCartoDbVectorLayer: No CartoDB data");
				throw new RuntimeException("Could not insert data to CartoDB table");
			}

			JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
			if (rows.length() != 1) {
				Log.debug("EditableCartoDbVectorLayer: Illegal insert result (must be single row)");
				throw new RuntimeException("Could not insert data to CartoDB table");
			}
			JSONObject row = rows.getJSONObject(0);
			long id = row.getLong(TAG_CARTODB_ID);
			return id;
		}
		catch (ParseException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing data " + e.toString());
			throw new RuntimeException("Illegal result from CartoDB server");
		}
		catch (JSONException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing JSON data " + e.toString());
			throw new RuntimeException("Illegal result from CartoDB server");
		}
	}
	
	@Override
	protected void updateElement(long id, Geometry element) {
		String wktGeom = WktWriter.writeWkt(element, getGeometryType(element));
		String sql = this.updateSql.replace(PLACEHOLDER_GEOM, "GeometryFromText('" + wktGeom + "',3857)").replace(PLACEHOLDER_ID, Long.toString(id));
		if (element.userData instanceof Map<?, ?>) {
			@SuppressWarnings("unchecked")
			Map<String, String> tokens = (Map<String, String>) element.userData;
			sql = replaceSqlMap(sql, tokens); 
		}
		Log.debug("EditableCartoDbVectorLayer: update sql: " + sql);

		try {
			Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
			Map<String, String> params = new HashMap<String, String>();
			params.put("q", sql);
			params.put("api_key", apiKey);
			Log.debug("EditableCartoDbVectorLayer: update url:" + uri.build().toString());
			JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString(), params);

			if (jsonData == null) {
				Log.debug("EditableCartoDbVectorLayer: No CartoDB data");
				throw new RuntimeException("Could not update data in CartoDB table");
			}

			JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
			if (rows.length() != 0) {
				Log.debug("EditableCartoDbVectorLayer: Illegal insert result");
				throw new RuntimeException("Could not update data in CartoDB table");
			}
		}
		catch (ParseException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing data " + e.toString());
			throw new RuntimeException("Illegal result from CartoDB server");
		}
		catch (JSONException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing JSON data " + e.toString());
			throw new RuntimeException("Illegal result from CartoDB server");
		}
	}
	
	@Override
	protected void deleteElement(long id) {
		String sql = this.deleteSql.replace(PLACEHOLDER_ID, Long.toString(id));
		Log.debug("EditableCartoDbVectorLayer: delete sql: " + sql);

		try {
			Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
			uri.appendQueryParameter("q", sql);
			uri.appendQueryParameter("api_key", apiKey);
			Log.debug("EditableCartoDbVectorLayer: delete url:" + uri.build().toString());
			JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

			if (jsonData == null) {
				Log.debug("EditableCartoDbVectorLayer: No CartoDB data");
				throw new RuntimeException("Could not delete data from CartoDB table");
			}

			JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
			if (rows.length() != 0) {
				Log.debug("EditableCartoDbVectorLayer: Illegal insert result");
				throw new RuntimeException("Could not delete data from CartoDB table");
			}
		}
		catch (ParseException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing data " + e.toString());
			throw new RuntimeException("Illegal result from CartoDB server");
		} catch (JSONException e) {
			Log.error("EditableCartoDbVectorLayer: Error parsing JSON data " + e.toString());
			throw new RuntimeException("Illegal result from CartoDB server");
		}
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

}
