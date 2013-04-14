package com.nutiteq.services.routing;

import java.util.ArrayList;
import java.util.Vector;

import android.graphics.Bitmap;

import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Marker;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.DefaultLabel;

/**
 * Data object containing route information from directions service.
 */
public class Route {
  private final RouteSummary summary;
  private final Line routeLine;
  private final RouteInstruction[] instructions;
  private final int routeResult;
  public final static int ROUTE_RESULT_OK = 0;
  public final static int ROUTE_RESULT_NETWORK_ERRROR = 1;
  public final static int ROUTE_RESULT_NO_ROUTE = 2;

  public final static int IMAGE_ROUTE_START = 0;
  public final static int IMAGE_ROUTE_RIGHT = 1;
  public final static int IMAGE_ROUTE_LEFT = 2;
  public final static int IMAGE_ROUTE_STRAIGHT = 3;
  public final static int IMAGE_ROUTE_END = 4;
  
  /**
   * Route from directions service
   * 
   * @param summary
   *          summary of the route
   * @param routeLine
   *          line containing route points
   * @param instructions
   *          route instructions
   */
  public Route(final RouteSummary summary, final Line routeLine,
      final RouteInstruction[] instructions, int routeResult) {
    this.summary = summary;
    this.routeLine = routeLine;
    this.instructions = instructions;
    this.routeResult = routeResult;
  }

  /**
   * Get route summary
   * 
   * @return summary of route
   */
  public RouteSummary getRouteSummary() {
    return summary;
  }

  /**
   * Get line for this route
   * 
   * @return route lines
   */
  public Line getRouteLine() {
    return routeLine;
  }

  /**
   * Get instructios for this route
   * 
   * @return instruction points
   */
  public RouteInstruction[] getInstructions() {
    return instructions;
  }
  
  
  /**
   * Routing result code
   * @return see Route constants
   */
  public int getRouteResult(){
      return routeResult;
  }

  /**
   * Get route markers, that can be shown on map. Images order see IMAGES_ROUTE constants
   * 
   * @param routeImages
   *          images to be used in direction instructions
   * @return instructions that can be shown on map
   */
  public ArrayList<Marker> getRoutePointMarkers(final Bitmap[] routeImages, float imageSize) {
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
