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
import java.util.Vector;

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
import com.nutiteq.utils.GeoUtils;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.Utils;
import com.nutiteq.utils.WkbRead;
import com.nutiteq.vectorlayers.GeometryLayer;

/**
 * Layer uses nutiteq custom compressed vector API
 * @author jaak
 *
 */
public class OnlineVectorLayer extends GeometryLayer {

    private StyleSet<PointStyle> pointStyleSet;
	private StyleSet<LineStyle> lineStyleSet;
	private StyleSet<PolygonStyle> polygonStyleSet;
    private float minZoom = Float.MAX_VALUE;
    private String account;
    private int maxObjects;


    protected class LoadVectorDataTask implements Task {
        final Envelope envelope;
        final int zoom;
        
        LoadVectorDataTask(Envelope envelope, int zoom) {
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
	 * Vector datasource connector, based on general query 
	 * 
	 * @param proj layer projection. NB! data must be in the same projection
	 * @param pointStyleSet styleset for point objects
	 * @param lineStyleSet styleset for line objects
	 * @param polygonStyleSet styleset for polygon objects
	 * @throws IOException file not found or other problem opening OGR datasource
	 */
	public OnlineVectorLayer(Projection proj, StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet, int maxObjects) {
		super(proj);
		this.pointStyleSet = pointStyleSet;
		this.lineStyleSet = lineStyleSet;
		this.polygonStyleSet = polygonStyleSet;
		this.maxObjects = maxObjects;

        if (pointStyleSet != null) {
            minZoom = Math.min(minZoom, pointStyleSet.getFirstNonNullZoomStyleZoom());
        }
        if (lineStyleSet != null) {
            minZoom = Math.min(minZoom, lineStyleSet.getFirstNonNullZoomStyleZoom());
        }
        if (polygonStyleSet != null) {
            minZoom = Math.min(minZoom, polygonStyleSet.getFirstNonNullZoomStyleZoom());
        }
        Log.debug("OnlineVectorLayer minZoom = "+minZoom);
	}



    @Override
	public void calculateVisibleElements(Envelope envelope, int zoom) {
        if (zoom < minZoom) {
            return;
        }
        executeVisibilityCalculationTask(new LoadVectorDataTask(envelope,zoom));
	}

    
    // TODO: check if map data and layer projections are same. If not, need to convert spatial filter rect (from map->data projection) and data objects (data->map projection).

    public void loadData(Envelope envelope, int zoom) {

        long timeStart = System.currentTimeMillis();
        
        MapPos minPos = projection.fromInternal(envelope.getMinX(), envelope.getMinY());
        MapPos maxPos = projection.fromInternal(envelope.getMaxX(), envelope.getMaxY());
        
        String sqlBbox = ""+minPos.x+","+minPos.y+","+maxPos.x+","+maxPos.y;
        
        // load geometries
        List<Geometry> newVisibleElementsList = new LinkedList<Geometry>();

           try {
                Uri.Builder uri = Uri.parse("http://kaart.maakaart.ee/poiexport/roads.php?output=gpoly&").buildUpon();
                uri.appendQueryParameter("bbox", sqlBbox);
                uri.appendQueryParameter("max", String.valueOf(maxObjects));
                Log.debug("Vector API url:" + uri.build().toString());

                JSONObject jsonData = NetUtils.getJSONFromUrl(uri.build().toString());

                if(jsonData == null){
                    Log.debug("No CartoDB data");
                    return;
                }
                
                JSONArray rows = jsonData.getJSONArray("res");
                
                for (int i = 0; i < rows.length(); i++) {
                    JSONObject row = rows.getJSONObject(i);
                    
                    String the_geom_encoded = row.getString("g");
                    String name = row.getString("n");
                    String type = row.getString("t");
                    
                    Vector<MapPos> mapPos = GeoUtils.decompress(the_geom_encoded, 5, projection);
                    
                    Geometry newObject = new Line(mapPos, new DefaultLabel(name,type), lineStyleSet, null);
                    
                    newObject.attachToLayer(this);
                    newObject.setActiveStyle(zoom);
                    newVisibleElementsList.add(newObject);
                  }
            }
            catch (ParseException e) {
                Log.error( "Error parsing data " + e.toString());
            } catch (JSONException e) {
                Log.error( "Error parsing JSON data " + e.toString());
            }
        
        
        long timeEnd = System.currentTimeMillis();
        Log.debug("OnlineVector loaded N:"+ newVisibleElementsList.size()+" time ms:"+(timeEnd-timeStart));
        setVisibleElementsList(newVisibleElementsList);
        
        
    }
    
}
