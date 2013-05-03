package com.nutiteq.layers.vector;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.net.ParseException;
import android.net.Uri;
import android.net.http.AndroidHttpClient;

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
import com.nutiteq.tasks.Task;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.Utils;
import com.nutiteq.utils.WkbRead;
import com.nutiteq.vectorlayers.GeometryLayer;

/**
 * Layer which requests vector data from CartoDB API: 
 * http://developers.cartodb.com/documentation/cartodb-apis.html
 * @author jaak
 *
 */
public class CartoDbVectorLayer extends GeometryLayer {
	public static final String TAG_WEBMERCATOR = "the_geom_webmercator";
    private static final CharSequence BBOX_PLACEHODER = "!bbox!";
    private static final String TAG_ROWS = "rows";
    
    private String sql;
	private StyleSet<PointStyle> pointStyleSet;
	private StyleSet<LineStyle> lineStyleSet;
	private StyleSet<PolygonStyle> polygonStyleSet;
    private String[] fieldNames;
    private float minZoom = Float.MAX_VALUE;
    private String account;


    protected class LoadCartoDataTask implements Task {
        final Envelope envelope;
        final int zoom;
        
        LoadCartoDataTask(Envelope envelope, int zoom) {
          this.envelope = envelope;
          this.zoom = zoom;
        }
        
        @Override
        public void run() {
          loadData(envelope, zoom);
        }

        @Override
        public boolean isCancelable() {
          return true;
        }

        @Override
        public void cancel() {
        }
      }

    
	/**
	 * CartoDB datasource connector, based on general query 
	 * 
	 * @param proj layer projection. NB! data must be in the same projection
	 * @param account your CartoDB account name
	 * @param sql SQL which requests data. Include " && ST_SetSRID('BOX3D(!bbox!)'::box3d, 3857)" for bounding box filter.
	 *          Suggested to include have 'LIMIT 1000' statement to limit number of objects
	 * @param pointStyleSet styleset for point objects
	 * @param lineStyleSet styleset for line objects
	 * @param polygonStyleSet styleset for polygon objects
	 * @throws IOException file not found or other problem opening OGR datasource
	 */
	public CartoDbVectorLayer(Projection proj, String account, String sql,
			StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet) {
		super(proj);
		this.sql = sql;
		this.account = account;
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
        Log.debug("CartoDbLayer minZoom = "+minZoom);
	}



    @Override
	public void calculateVisibleElements(Envelope envelope, int zoom) {
        if (zoom < minZoom) {
            return;
        }
        executeVisibilityCalculationTask(new LoadCartoDataTask(envelope,zoom));
	}

	protected Label createLabel(Map<String, String> userData) {
	    StringBuffer labelTxt = new StringBuffer();
	    for(Map.Entry<String, String> entry : userData.entrySet()){
	        labelTxt.append(entry.getKey() + ": " + entry.getValue()+"\n");
	    }
	    
		return new DefaultLabel("Data:",labelTxt.toString());
	}
	
	
 
    
    // TODO: check if map data and layer projections are same. If not, need to convert spatial filter rect (from map->data projection) and data objects (data->map projection).

    public void loadData(Envelope envelope, int zoom) {

        long timeStart = System.currentTimeMillis();
        
        MapPos minPos = projection.fromInternal(envelope.getMinX(), envelope.getMinY());
        MapPos maxPos = projection.fromInternal(envelope.getMaxX(), envelope.getMaxY());
        
        String sqlBbox = this.sql.replace(BBOX_PLACEHODER, ""+minPos.x+" "+minPos.y+","+maxPos.x+" "+maxPos.y);
        Log.debug("CartoDB sql: " + sqlBbox);
        
        // load geometries
        List<Geometry> newVisibleElementsList = new LinkedList<Geometry>();

           try {
                Uri.Builder uri = Uri.parse("http://"+this.account+".cartodb.com/api/v2/sql").buildUpon();
                uri.appendQueryParameter("q", sqlBbox);
                Log.debug("CartoDB url:" + uri.build().toString());

                JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

                if(jsonData == null){
                    Log.debug("No CartoDB data");
                    return;
                }
                
                JSONArray rows = jsonData.getJSONArray(TAG_ROWS);
                
                
                for (int i = 0; i < rows.length(); i++) {
                    JSONObject row = rows.getJSONObject(i);
                    
                    // copy all fields to userData object
                    final Map<String, String> userData = new HashMap<String, String>();
                    Iterator<String> iter = row.keys();
                    while (iter.hasNext()) {
                        String key = iter.next();
                        // exclude geom fields
                        if(!key.equals("the_geom_webmercator") && !key.equals("the_geom")){
                            try {
                                Object value =  row.get(key);
                                userData.put(key, value.toString());
                            } catch (JSONException e) { 
                                Log.error( "Error parsing JSON keys " + e.toString());
                            }
                        }
                    }
                            
                    String the_geom_webmercator = row.getString(TAG_WEBMERCATOR);
                    byte[] wkb = Utils.hexStringToByteArray(the_geom_webmercator);
                    
                    Geometry[] geoms = WkbRead.readWkb(new ByteArrayInputStream(wkb), userData);
                    for(Geometry geom : geoms){
                        Label label = createLabel(userData);
                        Geometry newObject = null;
                        
                        if(geom instanceof Point){
                            newObject = new Point(((Point) geom).getMapPos(), label, pointStyleSet, userData);
                        }else if(geom instanceof Line){
                            newObject = new Line(((Line) geom).getVertexList(), label, lineStyleSet, userData);
                        }else if(geom instanceof Polygon){
                            newObject = new Polygon(((Polygon) geom).getVertexList(), ((Polygon) geom).getHolePolygonList(), label, polygonStyleSet, userData);
                        }
                        
                        newObject.attachToLayer(this);
                        newObject.setActiveStyle(zoom);
                        newVisibleElementsList.add(newObject);
                    }
                }
            }
            catch (ParseException e) {
                Log.error( "Error parsing data " + e.toString());
            } catch (JSONException e) {
                Log.error( "Error parsing JSON data " + e.toString());
            }
        
        
        long timeEnd = System.currentTimeMillis();
        Log.debug("CartoDbLayer loaded N:"+ newVisibleElementsList.size()+" time ms:"+(timeEnd-timeStart));
        setVisibleElementsList(newVisibleElementsList);
        
        
    }
    
}
