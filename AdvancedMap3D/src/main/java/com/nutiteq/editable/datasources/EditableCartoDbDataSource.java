package com.nutiteq.editable.datasources;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.net.ParseException;
import android.net.Uri;

import com.nutiteq.datasources.vector.CartoDbDataSource;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.WktWriter;

/**
 * 
 * Editable CartoDb data source. 
 * 
 * @author mtehver
 *
 */
public abstract class EditableCartoDbDataSource extends CartoDbDataSource implements EditableVectorDataSource<Geometry> {
    private static final CharSequence PLACEHOLDER_ID = "!id!";
    private static final CharSequence PLACEHOLDER_GEOM = "!geom!";

    private final String apiKey;
    private final String insertSql;
    private final String updateSql;
    private final String deleteSql;
    private final boolean multiGeometry;

    /**
     * Default constructor.
     * 
     * @param projection data source projection
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
     * @param context Activity who controls the layer
     */
    public EditableCartoDbDataSource(Projection projection, String account, String apiKey,
            String querySql, String insertSql, String updateSql, String deleteSql, boolean multiGeometry) {
        super(projection, account, querySql);

        this.apiKey = apiKey;
        this.insertSql = insertSql;
        this.updateSql = updateSql;
        this.deleteSql = deleteSql;
        this.multiGeometry = multiGeometry;
    }

    @Override
    public long insertElement(Geometry element) {
        String wktGeom = WktWriter.writeWkt(element, getGeometryType(element));
        String sql = this.insertSql.replace(PLACEHOLDER_GEOM, "GeometryFromText('" + wktGeom + "',3857)");
        if (element.userData instanceof Map<?, ?>) {
            @SuppressWarnings("unchecked")
            Map<String, String> tokens = (Map<String, String>) element.userData;
            sql = replaceSqlMap(sql, tokens); 
        }
        Log.debug("EditableCartoDbDataSource: insert sql: " + sql);

        try {
            Uri.Builder uri = Uri.parse("http://" + this.account + ".cartodb.com/api/v2/sql").buildUpon();
            Map<String, String> params = new HashMap<String, String>();
            params.put("q", sql);
            params.put("api_key", apiKey);
            Log.debug("EditableCartoDbDataSource: insert url:" + uri.build().toString());
            JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString(), params);

            if (jsonData == null) {
                Log.debug("EditableCartoDbDataSource: No CartoDB data");
                throw new RuntimeException("Could not insert data to CartoDB table");
            }

            JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
            if (rows.length() != 1) {
                Log.debug("EditableCartoDbDataSource: Illegal insert result (must be single row)");
                throw new RuntimeException("Could not insert data to CartoDB table");
            }
            JSONObject row = rows.getJSONObject(0);
            long id = row.getLong(TAG_CARTODB_ID);
            return id;
        } catch (ParseException e) {
            Log.error("EditableCartoDbDataSource: Error parsing data " + e.toString());
            throw new RuntimeException("Illegal result from CartoDB server");
        } catch (JSONException e) {
            Log.error("EditableCartoDbDataSource: Error parsing JSON data " + e.toString());
            throw new RuntimeException("Illegal result from CartoDB server");
        }
    }

    @Override
    public void updateElement(long id, Geometry element) {
        String wktGeom = WktWriter.writeWkt(element, getGeometryType(element));
        String sql = this.updateSql.replace(PLACEHOLDER_GEOM, "GeometryFromText('" + wktGeom + "',3857)").replace(PLACEHOLDER_ID, Long.toString(id));
        if (element.userData instanceof Map<?, ?>) {
            @SuppressWarnings("unchecked")
            Map<String, String> tokens = (Map<String, String>) element.userData;
            sql = replaceSqlMap(sql, tokens); 
        }
        Log.debug("EditableCartoDbDataSource: update sql: " + sql);

        try {
            Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
            Map<String, String> params = new HashMap<String, String>();
            params.put("q", sql);
            params.put("api_key", apiKey);
            Log.debug("EditableCartoDbDataSource: update url:" + uri.build().toString());
            JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString(), params);

            if (jsonData == null) {
                Log.debug("EditableCartoDbDataSource: No CartoDB data");
                throw new RuntimeException("Could not update data in CartoDB table");
            }

            JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
            if (rows.length() != 0) {
                Log.debug("EditableCartoDbDataSource: Illegal insert result");
                throw new RuntimeException("Could not update data in CartoDB table");
            }
        } catch (ParseException e) {
            Log.error("EditableCartoDbDataSource: Error parsing data " + e.toString());
            throw new RuntimeException("Illegal result from CartoDB server");
        } catch (JSONException e) {
            Log.error("EditableCartoDbDataSource: Error parsing JSON data " + e.toString());
            throw new RuntimeException("Illegal result from CartoDB server");
        }
    }

    @Override
    public void deleteElement(long id) {
        String sql = this.deleteSql.replace(PLACEHOLDER_ID, Long.toString(id));
        Log.debug("EditableCartoDbDataSource: delete sql: " + sql);

        try {
            Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
            uri.appendQueryParameter("q", sql);
            uri.appendQueryParameter("api_key", apiKey);
            Log.debug("EditableCartoDbDataSource: delete url:" + uri.build().toString());
            JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

            if (jsonData == null) {
                Log.debug("EditableCartoDbDataSource: No CartoDB data");
                throw new RuntimeException("Could not delete data from CartoDB table");
            }

            JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
            if (rows.length() != 0) {
                Log.debug("EditableCartoDbDataSource: Illegal insert result");
                throw new RuntimeException("Could not delete data from CartoDB table");
            }
        } catch (ParseException e) {
            Log.error("EditableCartoDbDataSource: Error parsing data " + e.toString());
            throw new RuntimeException("Illegal result from CartoDB server");
        } catch (JSONException e) {
            Log.error("EditableCartoDbDataSource: Error parsing JSON data " + e.toString());
            throw new RuntimeException("Illegal result from CartoDB server");
        }
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

}
