package com.nutiteq.layers.vector;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Type;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import android.net.Uri;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonParseException;
import com.google.gson.JsonSyntaxException;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Text;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.tasks.Task;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.VectorLayer;

/**
 * Reads WFS using json format 
 * @author jaak
 *
 */
public class WfsLayer extends GeometryLayer {
    
	private StyleSet<PointStyle> pointStyleSet;
	private StyleSet<LineStyle> lineStyleSet;
	private StyleSet<PolygonStyle> polygonStyleSet;
    private float minZoom = Float.MAX_VALUE;
    private String baseUrl;
    public FeatureCollection features;


    // classes for GSON
    public static class Properties {
        public String type;
        public String name;
        public String osm_id;
        
        public Properties() { }
    }
    
    public static class Geometry {
        public String type;
        public double[] pointCoordinates;
        public double[][] lineCoordinates;
        
        public Geometry() { }
    }
    
    public static class Feature {
        public String type;
        public String id;
        public Geometry geometry;
        public Properties properties;
        
        public Feature() { }
    }
    
    public static class FeatureCollection {
        public String type;
        public Feature[] features;
        
        public FeatureCollection() { }
    }
    
    // data loader task
    
    protected class LoadWfsDataTask implements Task {
        final Envelope envelope;
        final int zoom;
        private VectorLayer<?> layer;
        
        LoadWfsDataTask(Envelope envelope, int zoom, VectorLayer<?> layer) {
          this.envelope = envelope;
          this.zoom = zoom;
          this.layer = layer;
        }
        
        @Override
        public void run() {
            features = downloadFeatureCollection(envelope);
            
            List<com.nutiteq.geometry.Geometry> newVisibleElementsList = new LinkedList<com.nutiteq.geometry.Geometry>();
            
            for(Feature feature : features.features){
                com.nutiteq.geometry.Geometry newObject = null;
                DefaultLabel label = new DefaultLabel(feature.properties.name, "OSM Id: "+feature.properties.osm_id+" type:"+feature.properties.type);
                
                if (feature.geometry.type.equals("LineString")) {
                    List<MapPos> linePos = new ArrayList<MapPos>();
                    double[][] lineCoords = feature.geometry.lineCoordinates;
                    for(double[] lineCoord : lineCoords){
                        linePos.add(new MapPos(lineCoord[0],lineCoord[1]));
                    }
                    newObject = new Line(linePos, label, lineStyleSet, null);

                } else if (feature.geometry.type.equals("Point")){
                    MapPos mapPos = new MapPos(feature.geometry.pointCoordinates[0], feature.geometry.pointCoordinates[1]);
                    newObject = new Point(mapPos, label, pointStyleSet, null);
                } else {
                    Log.warning("skipping geometry type "+feature.geometry.type);
                }                

                
                newObject.attachToLayer(layer);
                newObject.setActiveStyle(zoom);
                newVisibleElementsList.add(newObject);
            }
            setVisibleElementsList(newVisibleElementsList);
            
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
	 * @param baseUrl - WMS URL
	 * @param pointStyleSet styleset for point objects
	 * @param lineStyleSet styleset for line objects
	 * @param polygonStyleSet styleset for polygon objects
	 * @throws IOException file not found or other problem opening OGR datasource
	 */
	public WfsLayer(Projection proj, String baseUrl,
			StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet) {
		super(proj);
		this.baseUrl = baseUrl;
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
        Log.debug("WfsLayer minZoom = "+minZoom);
	}



    @Override
	public void calculateVisibleElements(Envelope envelope, int zoom) {
        if (zoom < minZoom) {
            return;
        }
        executeVisibilityCalculationTask(new LoadWfsDataTask(envelope,zoom, this));
	}

    private FeatureCollection downloadFeatureCollection(Envelope env) {
	        // Download JSON for given envelope
	        String json = "";
	        long startTime = System.currentTimeMillis();

	        MapPos envMin = projection.fromInternal(env.minX, env.minY);
	        MapPos envMax = projection.fromInternal(env.maxX, env.maxY);
	        
	    try {
	      Uri.Builder uri = Uri.parse(baseUrl).buildUpon();
	      uri.appendQueryParameter("outputFormat", "application/json");
	      uri.appendQueryParameter("BBOX", "" + envMin.x + "," + envMin.y + "," + envMax.x + "," + envMax.y);
	      Log.debug("WfsLayer: url " + uri.build().toString());
	        HttpURLConnection conn = (HttpURLConnection) new URL(uri.toString()).openConnection();
	      DataInputStream is = new DataInputStream(conn.getInputStream());
	        BufferedReader reader = new BufferedReader(new InputStreamReader(is, "UTF-8"));
	        while (true) {
	            String line = reader.readLine();
	            if (line == null) {
	                break;
	            }
	            json += line;
	        }
	    } catch (Exception e) {
	        Log.error("WfsLayer: exception: " + e);
	        return null;
	    }
	    
	    // Create custom deserializer for geometry data. Actual type depends whether the geometry is a point or a linestring.
	    // FIXME jaakl: double conversion -> coordinate list to array[][], then to MapPos
	    
	        JsonDeserializer<Geometry> deserializer = new JsonDeserializer<Geometry>() {
	            @Override
	            public Geometry deserialize(JsonElement json, Type typeOfT, JsonDeserializationContext context) throws JsonParseException {
	                Geometry geom = new Geometry();
	                geom.type = json.getAsJsonObject().get("type").getAsString();
	                JsonArray jsonCoordinates = json.getAsJsonObject().get("coordinates").getAsJsonArray();
	                if (geom.type.equals("LineString")) {
	                    geom.lineCoordinates = new double[jsonCoordinates.size()][];
	                    for (int i = 0; i < geom.lineCoordinates.length; i++) {
	                        JsonArray jsonCoordinatePair = jsonCoordinates.get(i).getAsJsonArray();
	                        geom.lineCoordinates[i] = new double[] { jsonCoordinatePair.get(0).getAsDouble(), jsonCoordinatePair.get(1).getAsDouble() };
	                    }
	                }
	                if (geom.type.equals("Point")) {
	                    if (jsonCoordinates.size() == 2) {
	                        geom.pointCoordinates = new double[2];
	                        geom.pointCoordinates[0] = jsonCoordinates.get(0).getAsDouble();
	                        geom.pointCoordinates[1] = jsonCoordinates.get(1).getAsDouble();
	                    }
	                }
	                return geom;
	            }
	        };
	        
	        // Deserialize JSON
	        FeatureCollection features = null;
            try {
                GsonBuilder builder = new GsonBuilder();
                builder.registerTypeAdapter(Geometry.class, deserializer);
                Gson gson = builder.create();
                features = gson.fromJson(json, FeatureCollection.class);
            } catch (JsonSyntaxException e) {
                e.printStackTrace();
            }
	        long time = System.currentTimeMillis() - startTime;
	        Log.debug("WfsLayer: received " + features.features.length + " elements time ms:"+time);
	        return features;
	    }
    
}
