package com.nutiteq.services.routing;

import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.Vector;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import android.graphics.Bitmap;
import android.os.AsyncTask;

import com.nutiteq.advancedmap.RouteActivity;
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
import com.nutiteq.utils.NetUtils;

/**
 * Routing service using CloudMade routing, version 0.3.
 */
public class CloudMadeDirections  {
  private static final String ATTRIBUTE_LONGITUDE = "lon";
  private static final String ATTRIBUTE_LATITUDE = "lat";
  private static final String ROUTE_POINT_TAG = "rtept";
  private static final String WAYPOINT_TAG = "wpt";
  private static final String DESCRIPTION_TAG = "desc";
  private static final String TURN_TAG = "turn";
  
  private static final String ROUTE_EXTENSION_TAG = "extensions";
  private static final String TIME_TAG = "time";
  private static final String DISTANCE_TAG = "distance";
  private static final String OFFSET_TAG = "offset";


  public static final String ROUTE_TYPE_CAR = "car";
  public static final String ROUTE_TYPE_FOOT = "foot";
  public static final String ROUTE_TYPE_BICYCLE = "bicycle";

  public static final String ROUTE_TYPE_MODIFIER_SHORTEST = "shortest";
  public static final String ROUTE_TYPE_MODIFIER_FASTEST = "fastest";

  private static final String BASEURL = "http://routes.cloudmade.com/";

  private static final String RESPONSE_TYPE = "gpx";
  private static final String API_VERSION = "0.3";

  public static int IMAGE_ROUTE_START = 0;
  public static int IMAGE_ROUTE_RIGHT = 1;
  public static int IMAGE_ROUTE_LEFT = 2;
  public static int IMAGE_ROUTE_STRAIGHT = 3;
  public static int IMAGE_ROUTE_END = 4;

  private final MapPos start;
  private final MapPos end;
  private static String routeType;
  private static String routeTypeModifier;
  private static String apiKey;
  private RouteActivity routeActivity;
  private static Projection projection;

  /**
   * @param token CloudMade token
   * @param directionsWaiter listener for directions result (callback)
   * @param start start point, in Wgs84
   * @param end end point of route, in Wgs84
   * @param routeType route type: ROUTE_TYPE_CAR, ROUTE_TYPE_FOOT or ROUTE_TYPE_BICYCLE
   * @param routeTypeModifier ROUTE_TYPE_MODIFIER_SHORTEST (default is FASTEST)
   * @param apiKey your CloudMade HTTP API key, get it from www.cloudmade.com
   */
  public CloudMadeDirections(final RouteActivity routeActivity, final MapPos start,
      final MapPos end, final String routeType, final String routeTypeModifier, final String apiKey, 
      Projection projection
      ) {
    this.routeActivity = routeActivity;
    this.start = start;
    this.end = end;
    this.routeType = routeType;
    this.routeTypeModifier = routeTypeModifier;
    this.apiKey = apiKey;
    this.projection = projection;
  }
  
  public void route(){
      new CmRoutingTask(routeActivity).execute(this.start,this.end);
  }
  
  public static class CmRoutingTask extends AsyncTask<MapPos, Void, Route> {

      private RouteActivity routeActivity;
      private Bitmap[] routeImages;

      public CmRoutingTask(RouteActivity routeActivity){
          this.routeActivity = routeActivity;
      }
      
      protected Route doInBackground(MapPos... mapPos) {

          String url = createUrl(mapPos[0],mapPos[1]);
          
          String xml = NetUtils.downloadUrl(url, null, true, "UTF-8");
//          Log.debug("route response: "+xml);
          
          final Route route = readRoute(new StringReader(xml));
          return route;
      }

      protected void onPostExecute(Route route) {
          routeActivity.routeResult(route);
      }
  }
  

  public static String createUrl(MapPos start, MapPos end) {
    final StringBuffer url = new StringBuffer(BASEURL);
    url.append(apiKey).append("/api/").append(API_VERSION).append("/");
    url.append(start.y).append(",").append(start.x);
    url.append(",").append(end.y).append(",").append(end.x);
    url.append("/").append(routeType);
    if (routeTypeModifier != null && !"".equals(routeTypeModifier)) {
      url.append("/").append(routeTypeModifier);
    }
    url.append(".").append(RESPONSE_TYPE);

    return url.toString();
  }


  protected static Route readRoute(final Reader reader) {
    final Vector wayPoints = new Vector();
    final Vector instructionPoints = new Vector();
    RouteSummary summary = null;
    
    try {
        XmlPullParserFactory factory = XmlPullParserFactory.newInstance();
        factory.setNamespaceAware(true);
        XmlPullParser parser = factory.newPullParser();

      parser.setInput(reader);
      int eventType = parser.getEventType();
      while (eventType != XmlPullParser.END_DOCUMENT) {
        if (eventType == XmlPullParser.START_TAG) {
          final String tagName = parser.getName();
          if (ROUTE_EXTENSION_TAG.equals(tagName) && summary == null) {
              summary = readRouteSummary(parser);
          }else if (WAYPOINT_TAG.equals(tagName)) {
            final String lat = parser.getAttributeValue(null, ATTRIBUTE_LATITUDE);
            final String lon = parser.getAttributeValue(null, ATTRIBUTE_LONGITUDE);
            wayPoints.addElement(projection.fromWgs84(Double.parseDouble(lon), Double.parseDouble(lat)));
          }else if (ROUTE_POINT_TAG.equals(tagName)) {
            instructionPoints.addElement(readInstruction(parser, instructionPoints.size()));
          }
        }
        eventType = parser.next();
      }
    } catch (final Exception e) {
      Log.error("Route: read " + e.getMessage());
    }

    final RouteInstruction[] instructions = new RouteInstruction[instructionPoints.size()];
    instructionPoints.copyInto(instructions);

    StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>(LineStyle.builder().setWidth(0.05f).setColor(0xff9d7050).build());
    
    if(wayPoints == null || wayPoints.size()<2){
        Log.error("no route line");
        return new Route(null, null, instructions, Route.ROUTE_RESULT_NO_ROUTE);
    }
    
    return new Route(summary, new Line(wayPoints, new DefaultLabel("Route", summary.toString()),lineStyleSet, summary), instructions, Route.ROUTE_RESULT_OK);
  }

  private static RouteSummary readRouteSummary(final XmlPullParser parser) throws IOException {
      DurationTime totalTime = null;
      Distance distance = null;
      Envelope boundingBox = null;
      try {
        int eventType = parser.next();
        while (!ROUTE_EXTENSION_TAG.equals(parser.getName())) {
          if (XmlPullParser.START_TAG == eventType) {
            final String tagName = parser.getName();
            if (TIME_TAG.equals(tagName)) {
              totalTime = new DurationTime(Long.parseLong(parser.nextText()));
            } else if (DISTANCE_TAG.equals(tagName)) {
              distance = readDistance(parser);
            } 
          }
          eventType = parser.next();
        }
      } catch (final XmlPullParserException e) {
        Log.error(e.getMessage());
      }
      return new RouteSummary(totalTime, distance, boundingBox);
    }
  
  private static Distance readDistance(final XmlPullParser parser) throws IOException {
    String distanceString;
    try {
      distanceString = parser.nextText();

      try {
        return new Distance(Float.parseFloat(distanceString), "m");
      } catch (final NumberFormatException e) {
        Log.error("NumberFormatException in readDistance");
        return new Distance(0, "");
      }

    } catch (XmlPullParserException e1) {
      Log.error("XML parsing exception in readDistance");
      e1.printStackTrace();
      return new Distance(0, "");
    }
  }
  
  private static RouteInstruction readInstruction(final XmlPullParser parser, final int count)
      throws Exception {
    final String lat = parser.getAttributeValue(null, ATTRIBUTE_LATITUDE);
    final String lon = parser.getAttributeValue(null, ATTRIBUTE_LONGITUDE);
    final MapPos location = projection.fromWgs84(Double.parseDouble(lon), Double.parseDouble(lat));

    String description = null;
    int eventType = parser.next();
    if (DESCRIPTION_TAG.equals(parser.getName())) {
      description = parser.nextText();
    }
    eventType = parser.next();
    eventType = parser.next();
    
    DurationTime time = null;
    Distance distance = null;
    int turn = IMAGE_ROUTE_START;
    while (!ROUTE_EXTENSION_TAG.equals(parser.getName())) {
        if (XmlPullParser.START_TAG == eventType) {
                final String tagName = parser.getName();
                if (TIME_TAG.equals(tagName)) {
                    time = new DurationTime(Long.parseLong(parser.nextText()));
                } else if (DISTANCE_TAG.equals(tagName)) {
                    distance = readDistance(parser);
                } else if (TURN_TAG.equals(tagName)) {
                    turn = parseTurn(parser.nextText());
                }else if (OFFSET_TAG.equals(tagName)) {
                    String offset = parser.nextText();
                    if(offset.equals("0")){
                        turn=IMAGE_ROUTE_START;
                    }
                }
            }
        eventType = parser.next();
      }
    
    return new RouteInstruction(count, turn, time, description, distance, location);
  }

  private static int parseTurn(String turn) {
        if (turn.equals("TSLR") || turn.equals("TR")) {
            return IMAGE_ROUTE_RIGHT;
        }
        if (turn.equals("TSLL") || turn.equals("TL")) {
            return IMAGE_ROUTE_LEFT;
        }

        // all other cases (C, EXITn ... are taken as Stright/Continue
        return IMAGE_ROUTE_STRAIGHT;
    }

  /**
   * Get route markers, that can be shown on map. Images order see IMAGES_ROUTE constants
   * 
   * @param routeImages
   *          images to be used in direction instructions
   * @return markers that can be shown on map
   */
  public static ArrayList<Marker> getRoutePointMarkers(final Bitmap[] routeImages, float imageSize, final RouteInstruction[] instructions) {
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
  
}
