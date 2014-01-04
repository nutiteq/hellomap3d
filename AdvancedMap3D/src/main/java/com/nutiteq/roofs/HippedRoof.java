package com.nutiteq.roofs;

import com.nutiteq.components.Vector;
import com.vividsolutions.jts.geom.Coordinate;
import com.vividsolutions.jts.geom.Geometry;
import com.vividsolutions.jts.geom.LineString;
import com.vividsolutions.jts.geom.LinearRing;
import com.vividsolutions.jts.geom.Point;
import com.vividsolutions.jts.geom.Polygon;
import com.vividsolutions.jts.geom.impl.CoordinateArraySequence;

public class HippedRoof extends Roof {
  // Ratio between the desired ridge length and the length of the whole bounding rectangle
  private static final float HIP_RATIO = 0.5f;
  
  // Corners
  private Vector c1Point;
  private Vector c2Point;
  private Vector c3Point;
  private Vector c4Point;
  
  // Edges
  private Vector e1Vec;
  private Vector e3Vec;
  private double e1Length;
 
  // Ridge parameters
  private Vector r1Point;
  private Vector r2Point;
  private Vector rVec;
  private double rLength;
  private double halfCrossLength; 
  
  // Triangle parameters
  private Polygon tri1;
  private Polygon tri2;
  private double triLength;

  public HippedRoof(float roofHeight, boolean alongLongSide) {
    super(roofHeight, alongLongSide);
  }

  @Override
  public void calculateRoof(Geometry minRectangle) {
    // Get the 4 corner points of the minimum enclosing rectangle
    Coordinate[] coords = minRectangle.getCoordinates();
    c1Point = new Vector(coords[0].x, coords[0].y);
    c2Point = new Vector(coords[1].x, coords[1].y);
    c3Point = new Vector(coords[2].x, coords[2].y);
    c4Point = new Vector(coords[3].x, coords[3].y);
    
    // Rotate the roof by 90 degrees to get the requested roof gable location
    double side1Length = c1Point.getDistanceFromPoint2D(c2Point);
    double side2Length = c2Point.getDistanceFromPoint2D(c3Point);
    if ((side1Length > side2Length && alongLongSide) || (side1Length < side2Length && !alongLongSide)) {
      Vector temp = c4Point;
      c4Point = c3Point;
      c3Point = c2Point;
      c2Point = c1Point;
      c1Point = temp;
      double temp2 = side1Length;
      side1Length = side2Length;
      side2Length = temp2;
    }
    
    e1Vec = c2Point.getSubtracted2D(c1Point);
    e3Vec = c4Point.getSubtracted2D(c3Point);
    e1Length = side1Length;
 
    // Calculate the ridge parameters
    Vector e1MidPoint = c1Point.getPointBetween2D(c2Point, 0.5f);
    Vector e3MidPoint = c3Point.getPointBetween2D(c4Point, 0.5f);
    Vector rMidPoint = e1MidPoint.getPointBetween2D(e3MidPoint, 0.5f);
    r1Point = rMidPoint.getPointBetween2D(e1MidPoint, HIP_RATIO);
    r2Point = rMidPoint.getPointBetween2D(e3MidPoint, HIP_RATIO);
    rVec = r2Point.getSubtracted2D(r1Point);
    rLength = r1Point.getDistanceFromPoint2D(r2Point);
    triLength = (side2Length - rLength) / 2;
    halfCrossLength = e1Length / 2;
    
    // Add the ridge as an intersection line
    lines = new LineString[5];
    Coordinate[] lineCoords = new Coordinate[2];
    lineCoords[0] = new Coordinate(r1Point.x, r1Point.y);
    lineCoords[1] = new Coordinate(r2Point.x, r2Point.y);
    lines[0] = new LineString(new CoordinateArraySequence(lineCoords), geoFac);
    lineCoords = new Coordinate[2];
    lineCoords[0] = new Coordinate(r1Point.x, r1Point.y);
    lineCoords[1] = new Coordinate(c1Point.x, c1Point.y);
    lines[1] = new LineString(new CoordinateArraySequence(lineCoords), geoFac);
    lineCoords = new Coordinate[2];
    lineCoords[0] = new Coordinate(r1Point.x, r1Point.y);
    lineCoords[1] = new Coordinate(c2Point.x, c2Point.y);
    lines[2] = new LineString(new CoordinateArraySequence(lineCoords), geoFac);
    lineCoords = new Coordinate[2];
    lineCoords[0] = new Coordinate(r2Point.x, r2Point.y);
    lineCoords[1] = new Coordinate(c3Point.x, c3Point.y);
    lines[3] = new LineString(new CoordinateArraySequence(lineCoords), geoFac);
    lineCoords = new Coordinate[2];
    lineCoords[0] = new Coordinate(r2Point.x, r2Point.y);
    lineCoords[1] = new Coordinate(c4Point.x, c4Point.y);
    lines[4] = new LineString(new CoordinateArraySequence(lineCoords), geoFac);
    
    // Hipped roof for a rectangle has 4 parts, calculate the first rectangle
    polygons = new Polygon[4];
    Coordinate[] polyCoords = new Coordinate[5];
    polyCoords[0] = new Coordinate(c1Point.x, c1Point.y);
    polyCoords[1] = new Coordinate(r1Point.x, r1Point.y);
    polyCoords[2] = new Coordinate(r2Point.x, r2Point.y);
    polyCoords[3] = new Coordinate(c4Point.x, c4Point.y);
    polyCoords[4] = new Coordinate(polyCoords[0]);
    polygons[0] = new Polygon(new LinearRing(new CoordinateArraySequence(polyCoords), geoFac), null, geoFac);
    
    // Calculate the second rectangle
    polyCoords = new Coordinate[5];
    polyCoords[0] = new Coordinate(r1Point.x, r1Point.y);
    polyCoords[1] = new Coordinate(c2Point.x, c2Point.y);
    polyCoords[2] = new Coordinate(c3Point.x, c3Point.y);
    polyCoords[3] = new Coordinate(r2Point.x, r2Point.y);
    polyCoords[4] = new Coordinate(polyCoords[0]);
    polygons[1] = new Polygon(new LinearRing(new CoordinateArraySequence(polyCoords), geoFac), null, geoFac);
    
    // Calculate the first triangle
    polyCoords = new Coordinate[4];
    polyCoords[0] = new Coordinate(c1Point.x, c1Point.y);
    polyCoords[1] = new Coordinate(r1Point.x, r1Point.y);
    polyCoords[2] = new Coordinate(c2Point.x, c2Point.y);
    polyCoords[3] = new Coordinate(polyCoords[0]);
    polygons[2] = new Polygon(new LinearRing(new CoordinateArraySequence(polyCoords), geoFac), null, geoFac);
    tri1 = (Polygon) polygons[2].clone();
    
    // Calculate the second triangle
    polyCoords = new Coordinate[4];
    polyCoords[0] = new Coordinate(c3Point.x, c3Point.y);
    polyCoords[1] = new Coordinate(r2Point.x, r2Point.y);
    polyCoords[2] = new Coordinate(c4Point.x, c4Point.y);
    polyCoords[3] = new Coordinate(polyCoords[0]);
    polygons[3] = new Polygon(new LinearRing(new CoordinateArraySequence(polyCoords), geoFac), null, geoFac);
    tri2 = (Polygon) polygons[3].clone();
  }

  @Override
  public double calculateRoofPointHeight(double x, double y) {
    Point p = new Point(new CoordinateArraySequence(new Coordinate[] {new Coordinate(x, y)}), geoFac);
    // If point lies in the triangles, calculate their height based on their distance from the edge
    if (tri1.contains(p)) {
      double distanceFromEdge = Math.abs((x - c1Point.x) * (e1Vec.y) - 
          (y - c1Point.y) * (e1Vec.x)) / e1Length;
      return  (distanceFromEdge / triLength) * roofHeight;
    } else if (tri2.contains(p)) {
      double distanceFromEdge = Math.abs((x - c3Point.x) * (e3Vec.y) - 
          (y - c3Point.y) * (e3Vec.x)) / e1Length;
      return  (distanceFromEdge / triLength) * roofHeight;
    }
    
    // Calculate point's height based on it's distance from the roof ridge
    double distanceFromRidge = Math.abs((x - r1Point.x) * (rVec.y) - 
        (y - r1Point.y) * (rVec.x)) / rLength;
    return (1 - distanceFromRidge / halfCrossLength) * roofHeight;
  }
}
