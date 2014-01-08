package com.nutiteq.datasources.vector;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Type;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;

import android.net.Uri;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonParseException;
import com.google.gson.JsonSyntaxException;
import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.Label;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

/**
 * Reads WFS using json format 
 *
 * @author jaak
 *
 */
public abstract class WFSVectorDataSource extends AbstractVectorDataSource<com.nutiteq.geometry.Geometry> {

    private final String baseUrl;

    private Envelope loadedEnvelope = new Envelope(0, 0, 0, 0);
    private List<com.nutiteq.geometry.Geometry> loadedGeometryList = new ArrayList<com.nutiteq.geometry.Geometry>();

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

    /**
     * CartoDB datasource connector, based on general query 
     * 
     * @param proj layer projection. NB! data must be in the same projection
     * @param baseUrl - WMS URL
     * @throws IOException file not found or other problem opening OGR datasource
     */
    public WFSVectorDataSource(Projection proj, String baseUrl) {
        super(proj);
        this.baseUrl = baseUrl;
    }
    
    public void reloadElements() {
        synchronized (this) {
            loadedEnvelope = new Envelope(0, 0, 0, 0);
            loadedGeometryList = new ArrayList<com.nutiteq.geometry.Geometry>();
        }
        notifyElementsChanged();
    }

    @Override
    public Envelope getDataExtent() {
        return null;
    }

    @Override
    public Collection<com.nutiteq.geometry.Geometry> loadElements(CullState cullState) {
        synchronized (this) {
            if (loadedEnvelope.equals(cullState.envelope)) {
                return loadedGeometryList;
            }

            FeatureCollection features = downloadFeatureCollection(cullState.envelope);

            List<com.nutiteq.geometry.Geometry> geometryList = new LinkedList<com.nutiteq.geometry.Geometry>();
            for (Feature feature : features.features){
                com.nutiteq.geometry.Geometry geometry = null;
                Label label = createLabel(feature);

                if (feature.geometry.type.equals("LineString")) {
                    List<MapPos> linePos = new ArrayList<MapPos>();
                    double[][] lineCoords = feature.geometry.lineCoordinates;
                    for(double[] lineCoord : lineCoords) {
                        linePos.add(new MapPos(lineCoord[0],lineCoord[1]));
                    }
                    geometry = new Line(linePos, label, createLineFeatureStyleSet(feature, cullState.zoom), feature);
                } else if (feature.geometry.type.equals("Point")) {
                    MapPos mapPos = new MapPos(feature.geometry.pointCoordinates[0], feature.geometry.pointCoordinates[1]);
                    geometry = new Point(mapPos, label, createPointFeatureStyleSet(feature, cullState.zoom), feature);
                } else {
                    Log.warning("WFSVectorDataSource: skipping geometry type " + feature.geometry.type);
                    continue;
                }

                geometry.attachToDataSource(this);
                geometryList.add(geometry);
            }

            loadedEnvelope = cullState.envelope;
            loadedGeometryList = geometryList;
            return geometryList;
        }
    }

    protected abstract Label createLabel(Feature feature);
    
    protected abstract StyleSet<PointStyle> createPointFeatureStyleSet(Feature feature, int zoom);

    protected abstract StyleSet<LineStyle> createLineFeatureStyleSet(Feature feature, int zoom);

    private FeatureCollection downloadFeatureCollection(Envelope envInternal) {
        // Download JSON for given envelope
        StringBuilder json = new StringBuilder();
        long startTime = System.currentTimeMillis();
        Envelope env = projection.fromInternal(envInternal);

        try {
            Uri.Builder uri = Uri.parse(baseUrl).buildUpon();
            uri.appendQueryParameter("outputFormat", "application/json");
            uri.appendQueryParameter("BBOX", "" + env.minX + "," + env.minY + "," + env.maxX + "," + env.maxY);
            Log.debug("WFSVectorDataSource: url " + uri.build().toString());
            HttpURLConnection conn = (HttpURLConnection) new URL(uri.toString()).openConnection();
            DataInputStream is = new DataInputStream(conn.getInputStream());
            BufferedReader reader = new BufferedReader(new InputStreamReader(is, "UTF-8"));
            while (true) {
                String line = reader.readLine();
                if (line == null) {
                    break;
                }
                json.append(line);
            }
        } catch (Exception e) {
            Log.error("WFSVectorDataSource: exception: " + e);
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
        FeatureCollection features = new FeatureCollection();
        try {
            GsonBuilder builder = new GsonBuilder();
            builder.registerTypeAdapter(Geometry.class, deserializer);
            Gson gson = builder.create();
            features = gson.fromJson(json.toString(), FeatureCollection.class);
        } catch (JsonSyntaxException e) {
            e.printStackTrace();
        }

        long time = System.currentTimeMillis() - startTime;
        Log.debug("WFSVectorDataSource: received " + features.features.length + " elements, time ms " + time);
        return features;
    }

}
