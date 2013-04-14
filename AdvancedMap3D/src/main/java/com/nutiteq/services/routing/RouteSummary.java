package com.nutiteq.services.routing;

import com.nutiteq.components.Envelope;

/**
 * Summary of the found route int directions service.
 */
public class RouteSummary {

  private final DurationTime totalTime;
  private final Distance distance;
  private final Envelope boundingBox;

  /**
   * @param totalTime
   *          total time for the route
   * @param distance
   *          distance of the route
   * @param boundingBox
   *          bounding box for best view of full route
   */
  public RouteSummary(final DurationTime totalTime, final Distance distance,
      final Envelope boundingBox) {
    this.totalTime = totalTime;
    this.distance = distance;
    this.boundingBox = boundingBox;
  }

  /**
   * Total time for the route
   * 
   * @return route total time
   */
  public DurationTime getTotalTime() {
    return totalTime;
  }

  /**
   * Distance of this route
   * 
   * @return route distance
   */
  public Distance getDistance() {
    return distance;
  }

  /**
   * Best view for full route
   * 
   * @return bounding box for best view of the route
   */
  public Envelope getBoundingBox() {
    return boundingBox;
  }
  
  @Override
  public String toString(){
      return "Distance: "+distance+" Time: "+totalTime;
  }
}
