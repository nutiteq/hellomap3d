package com.nutiteq.roofs;

import com.vividsolutions.jts.geom.Coordinate;
import com.vividsolutions.jts.geom.Geometry;
import com.vividsolutions.jts.geom.GeometryFactory;
import com.vividsolutions.jts.geom.LineString;
import com.vividsolutions.jts.geom.Polygon;

/**
 * Roofs can be defined for 3D buildings
 * 
 * They require JTS library to be included with app package
 * 
 * @author ats
 *
 */
public abstract class Roof {
  
  protected GeometryFactory geoFac = new GeometryFactory();
  
  protected Polygon[] polygons;
  protected LineString[] lines = new LineString[0];
  
  protected float roofHeight;
  protected boolean alongLongSide;
  
  protected Roof(float roofHeight, boolean alongLongSide) {
    this.roofHeight = roofHeight;
    this.alongLongSide = alongLongSide;
  }
  
  public abstract void calculateRoof(Geometry minRectangle);
  
  public abstract double calculateRoofPointHeight(double x, double y);
  
  public void calculateRoofPartHeights(Geometry roofPart) {
    for (Coordinate coord : roofPart.getCoordinates()) {
      coord.z = calculateRoofPointHeight(coord.x, coord.y);
    }
  }
  
  public Polygon[] getPolygons() {
    return polygons;
  }
  
  public LineString[] getLines() {
    return lines;
  }
  
  public float getRoofHeight() {
    return roofHeight;
  }
  
  public boolean getAlongLongSide() {
    return alongLongSide;
  }
}
