package com.nutiteq.services.routing;

import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Vector;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.AsyncTask;
import android.util.SparseArray;

import com.nutiteq.advancedmap.activity.RouteActivity;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.utils.GeoUtils;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * Routing service using MapQuest Open Directions Service, v1
 *  See http://open.mapquestapi.com/directions/ for 
 */
public class MapQuestDirections  {

  private static final String BASEURL = "http://open.mapquestapi.com/directions/v1/route?";

  private final MapPos start;
  private final MapPos end;
  private final String apiKey;
  private RouteActivity routeActivity;

  private Map<String, String> routeParameters;
  private SparseArray<String> iconUrls;
  private final Projection projection;

  private StyleSet<LineStyle> lineStyleSet;

  private RouteInstruction[] instructions;

  /**
   * @param routeActivity listener for directions result (callback)
   * @param start start point, in Wgs84
   * @param end end point of route, in Wgs84
   * @param apiKey your MapQuest API key
   * @param lineStyleSet StyleSet for route line, e.g.  new StyleSet<LineStyle>(LineStyle.builder().setWidth(0.05f).setColor(0xff9d7050).build())
   */
  public MapQuestDirections(final RouteActivity routeActivity, final MapPos start,
      final MapPos end, final Map<String,String> routeParameters, final String apiKey, 
      Projection projection, StyleSet<LineStyle> lineStyleSet) {
    this.routeActivity = routeActivity;
    this.start = start;
    this.end = end;
    this.routeParameters = routeParameters;
    this.apiKey = apiKey;
    this.projection = projection;
    this.lineStyleSet = lineStyleSet;
    
    this.iconUrls = new SparseArray<String>();
    
    // fixed parameters
    this.routeParameters.put("from", start.y+","+start.x);
    this.routeParameters.put("to", end.y+","+end.x);
    this.routeParameters.put("outFormat", "json");
    this.routeParameters.put("shapeFormat", "cmp");
    if(!this.routeParameters.containsKey("generalize")){
        this.routeParameters.put("generalize", "0");
    }
    
  }
  
  public void route(){
      new MqRoutingTask(routeActivity, routeParameters).execute(this.start,this.end);
  }
  
  public class MqRoutingTask extends AsyncTask<MapPos, Void, Route> {

      private RouteActivity routeActivity;
      private Map<String, String> routeParameters;

      public MqRoutingTask(RouteActivity routeActivity, Map<String, String> routeParameters){
          this.routeActivity = routeActivity;
          this.routeParameters = routeParameters;
          
      }
      
      protected Route doInBackground(MapPos... mapPos) {

          String url = createUrl(mapPos[0],mapPos[1],routeParameters, apiKey);
          Log.debug("route url: " + url);
          String json = NetUtils.downloadUrl(url, null, true, "UTF-8");
          Log.debug("route response: "+json);
          
          final Route route = readRoute(json);
          return route;
      }

      protected void onPostExecute(Route route) {
          routeActivity.routeResult(route);
      }
  }
  
  public class MqLoadInstructionImagesTask extends AsyncTask<Void, Void, ArrayList<Marker>> {

      private MarkerLayer markerLayer;
      private float markerSize;

      public MqLoadInstructionImagesTask(MarkerLayer markerLayer, float markerSize){
          this.markerLayer = markerLayer;
          this.markerSize = markerSize;
      }
      
      protected ArrayList<Marker> doInBackground(Void... mapPos) {
          if (instructions == null || instructions.length == 0) {
              return null;
          }

            final ArrayList<Marker> routePointMarkers = new ArrayList<Marker>(instructions.length);
            
            for (int i = 0; i < instructions.length; i++) {
              final RouteInstruction current = instructions[i];
              String url = iconUrls.get(current.getInstructionType());
              Bitmap bitMap = NetUtils.getBitmapFromURL(url);
              
              MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(bitMap).setSize(markerSize).build();
              
              routePointMarkers.add(new Marker(current.getPoint(), new DefaultLabel(current.getInstructionNumber()+"."+current
                  .getInstruction(),"Distance: "+current.getDistance()+" time: "+current.getDuration()), markerStyle, current));
            }
         return routePointMarkers;
      }
      
      protected void onPostExecute(ArrayList<Marker> routePointMarkers) {
        markerLayer.addAll(routePointMarkers);
      }

  }

  public static String createUrl(MapPos start, MapPos end, Map<String, String> routeParameters, String apiKey) {
      
      Uri.Builder uri = Uri.parse(BASEURL).buildUpon();
      Iterator<Entry<String, String>> it = routeParameters.entrySet().iterator();
      while (it.hasNext()) {
          Map.Entry<String, String> pairs = (Map.Entry<String, String>)it.next();
          uri.appendQueryParameter((String)pairs.getKey(),(String)pairs.getValue());
      }
      String url = uri.build().toString();
      
      //avoid urlencoding of key
      if(apiKey != null){
          url += "&key="+apiKey;
      }
              
    return url;
  }


  protected  Route readRoute(final String json) {

    // parse response
    try {
        JSONObject route = new JSONObject(json).getJSONObject("route");
        // 1. summary
        float distance = (float) route.getDouble("distance");
        JSONObject options = route.getJSONObject("options");
        String unit = options.getString("unit");
        DurationTime totalTime = new DurationTime(route.getLong("time")); 
        Envelope boundingBox = new Envelope(
                route.getJSONObject("boundingBox").getJSONObject("ul").getDouble("lng"),
                route.getJSONObject("boundingBox").getJSONObject("lr").getDouble("lng"),
                route.getJSONObject("boundingBox").getJSONObject("lr").getDouble("lat"),
                route.getJSONObject("boundingBox").getJSONObject("ul").getDouble("lat")
             );
        
        RouteSummary summary = new RouteSummary(totalTime, new Distance(distance, unit), boundingBox);
    
        // 2. instructions. Note that many extra parameters are ignored here
        final Vector<RouteInstruction> instructionPoints = new Vector<RouteInstruction>();
        
        JSONArray maneuvers = route.getJSONArray("legs").getJSONObject(0).getJSONArray("maneuvers");
        
        for (int i = 0; i < maneuvers.length(); i++) {
            JSONObject maneuver = maneuvers.getJSONObject(i);
            int turnType = maneuver.getInt("turnType");
            DurationTime time = new DurationTime(maneuver.getLong("time"));
            Distance dist = new Distance((float) maneuver.getDouble("distance"), unit);
            MapPos location = projection.fromWgs84(maneuver.getJSONObject("startPoint").getDouble("lng"), maneuver.getJSONObject("startPoint").getDouble("lat"));
            String narrative = maneuver.getString("narrative");
            
            RouteInstruction instruction = new RouteInstruction(i, turnType, time, narrative, dist, location);
            instructionPoints.add(instruction);
            
            this.iconUrls.put(turnType, maneuver.getString("iconUrl"));
            
        }

        // 3. route shape
        
        String shapePoints = route.getJSONObject("shape").getString("shapePoints");
        Vector<MapPos> wayPoints = GeoUtils.decompress(shapePoints, 5, projection); // default precision is 5
                
        // convert route to needed formats
        instructions = new RouteInstruction[instructionPoints.size()];
        instructionPoints.copyInto(instructions);
    
        if(wayPoints == null || wayPoints.size()<2){
            Log.error("no route line");
            return new Route(null, null, null, Route.ROUTE_RESULT_NO_ROUTE);
        }
        
        return new Route(summary, new Line(wayPoints, new DefaultLabel("Route", summary.toString()), lineStyleSet, summary), instructions, Route.ROUTE_RESULT_OK);

    } catch (JSONException e) {
        Log.error("JSON parsing error "+e.getMessage());
  }
    
    return new Route(null, null, null, Route.ROUTE_RESULT_INTERNAL_ERROR);

  }
  
  /**
   * Get route markers, that can be shown on map. Convert them from instructions
   * 
   * @param routeImages
   *          images to be used in direction instructions
   * @return markers that can be shown on map
   */
  public ArrayList<Marker> getRoutePointMarkers(final Bitmap[] routeImages, float imageSize, final RouteInstruction[] instructions) {
    if (instructions == null || instructions.length == 0) {
      return new ArrayList<Marker>();
    }

    
    
    final ArrayList<Marker> routePointMarkers = new ArrayList<Marker>(instructions.length);
    
    for (int i = 0; i < instructions.length; i++) {
      final RouteInstruction current = instructions[i];
      MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(routeImages[current.getInstructionType()]).setSize(imageSize).build();
      routePointMarkers.add(new Marker(current.getPoint(), new DefaultLabel("Step "+current.getInstructionNumber(),current
          .getInstruction()), markerStyle, current));
    }

    return routePointMarkers;
  }
  
  public void startRoutePointMarkerLoading(MarkerLayer markerLayer,
        float markerSize) {
    new MqLoadInstructionImagesTask(markerLayer, markerSize).execute();
    
  }
  
}
