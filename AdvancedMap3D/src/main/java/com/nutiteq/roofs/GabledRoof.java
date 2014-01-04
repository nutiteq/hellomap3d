package com.nutiteq.roofs;

import com.nutiteq.components.Vector;
import com.vividsolutions.jts.geom.Coordinate;
import com.vividsolutions.jts.geom.Geometry;
import com.vividsolutions.jts.geom.LineString;
import com.vividsolutions.jts.geom.LinearRing;
import com.vividsolutions.jts.geom.Polygon;
import com.vividsolutions.jts.geom.impl.CoordinateArraySequence;

public class GabledRoof extends Roof {
  
  private Vector c1Point;
  private Vector c2Point;
  private Vector c3Point;
  private Vector c4Point;
 
  private Vector r1Point;
  private Vector r2Point;
  private double rLength;
  private double halfCrossLength; 

  public GabledRoof(float roofHeight, boolean alongLongSide) {
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
 
    // Calculate the ridge parameters
    r1Point = c1Point.getPointBetween2D(c2Point, 0.5f);
    r2Point = c3Point.getPointBetween2D(c4Point, 0.5f);
    rLength = r1Point.getDistanceFromPoint2D(r2Point);
    halfCrossLength = c1Point.getDistanceFromPoint2D(c2Point) / 2;
    
    // Add the ridge as an intersection line
    lines = new LineString[1];
    Coordinate[] lineCoords = new Coordinate[2];
    lineCoords[0] = new Coordinate(r1Point.x, r1Point.y);
    lineCoords[1] = new Coordinate(r2Point.x, r2Point.y);
    lines[0] = new LineString(new CoordinateArraySequence(lineCoords), geoFac);
    
    // Gabled roof for a rectangle has 2 parts, calculate the first part
    polygons = new Polygon[2];
    Coordinate[] polyCoords = new Coordinate[5];
    polyCoords[0] = new Coordinate(c1Point.x, c1Point.y);
    polyCoords[1] = new Coordinate(r1Point.x, r1Point.y);
    polyCoords[2] = new Coordinate(r2Point.x, r2Point.y);
    polyCoords[3] = new Coordinate(c4Point.x, c4Point.y);
    polyCoords[4] = new Coordinate(polyCoords[0]);
    polygons[0] = new Polygon(new LinearRing(new CoordinateArraySequence(polyCoords), geoFac), null, geoFac);
    
    // Calculate the second part of the gabled roof
    polyCoords = new Coordinate[5];
    polyCoords[0] = new Coordinate(r1Point.x, r1Point.y);
    polyCoords[1] = new Coordinate(c2Point.x, c2Point.y);
    polyCoords[2] = new Coordinate(c3Point.x, c3Point.y);
    polyCoords[3] = new Coordinate(r2Point.x, r2Point.y);
    polyCoords[4] = new Coordinate(polyCoords[0]);
    polygons[1] = new Polygon(new LinearRing(new CoordinateArraySequence(polyCoords), geoFac), null, geoFac);
  }

  @Override
  public double calculateRoofPointHeight(double x, double y) {
    // Calculate point's height based on it's distance from the roof ridge
    double distanceFromRidge = Math.abs((x - r1Point.x) * (r2Point.y - r1Point.y) - 
        (y - r1Point.y) * (r2Point.x - r1Point.x)) / rLength;
    return (1 - distanceFromRidge / halfCrossLength) * roofHeight;
  }
}
