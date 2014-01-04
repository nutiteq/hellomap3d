package com.nutiteq.roofs;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

import com.nutiteq.components.Color;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MutableEnvelope;
import com.nutiteq.components.Point3D;
import com.nutiteq.components.Vector;
import com.nutiteq.components.Vector3D;
import com.nutiteq.geometry.Polygon3D;
import com.nutiteq.projections.Projection;
import com.nutiteq.renderprojections.RenderProjection;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.Const;
import com.nutiteq.utils.PolygonTriangulation;
import com.vividsolutions.jts.algorithm.MinimumDiameter;
import com.vividsolutions.jts.geom.Coordinate;
import com.vividsolutions.jts.geom.Geometry;
import com.vividsolutions.jts.geom.GeometryFactory;
import com.vividsolutions.jts.geom.LineString;
import com.vividsolutions.jts.geom.LinearRing;
import com.vividsolutions.jts.geom.MultiPolygon;
import com.vividsolutions.jts.geom.Polygon;
import com.vividsolutions.jts.geom.impl.CoordinateArraySequence;
import com.vividsolutions.jts.geom.util.PolygonExtracter;

/**
 * Geometric prism (defined by base polygon and height) on the map. Base polygons can be non-convex and may contain holes. 
 */
public class Polygon3DRoof extends Polygon3D {
  private static final float MIN_RECTANGLE_SCALE = 1.01f;

  private float minHeight;
  private Roof roof;
  
  private Color color;
  private Color roofColor;
  
  private GeometryFactory geoFac = new GeometryFactory();

  /**
   * Default constructor (can be used if single style is sufficient for all zoom levels).
   * 
   * @param mapPoses
   *        vertices of the base polygon. Vertices are defined in the coordinate system of the layer this element is attached to.
   * @param mapPosesHoles
   *        list of hole polygons. Can be null.
   * @param roofType
   *        type of the roof.     
   * @param height
   *        height of the prism. 
   * @param roofHeight
   *        height of the roof.
   * @param label
   *        the label for the polygon. Label is shown when the polygon is selected. Can be null.
   * @param polygonStyle
   *        style for displaying the polygon (color, texture, etc). 
   * @param userData
   *        custom user data associated with the element.
   */
  public Polygon3DRoof(List<MapPos> mapPoses, List<List<MapPos>> mapPosesHoles, float height, float minHeight,
      Roof roof, int color, int roofColor, Label label, Polygon3DStyle polygonStyle, Object userData) {
    this(mapPoses, mapPosesHoles, height, minHeight, roof, color, roofColor, label, new StyleSet<Polygon3DStyle>(polygonStyle), userData);
  }

  /**
   * Constructor for the case when style depends on zoom level.
   * 
   * @param mapPoses
   *        vertices of the base polygon. Vertices are defined in the coordinate system of the layer this element is attached to.
   * @param mapPosesHoles
   *        list of hole polygons. Can be null.        
    * @param roofType
   *        type of the roof.     
   * @param height
   *        height of the prism. 
   * @param roofHeight
   *        height of the roof.
   * @param label
   *        the label for the polygon. Label is shown when the polygon is selected. Can be null.
   * @param styles
   *        style set defining how to display the polygon. 
   * @param userData
   *        custom user data associated with the element.
   */
  public Polygon3DRoof(List<MapPos> mapPoses, List<List<MapPos>> mapPosesHoles, float height, float minHeight,
      Roof roof, int color, int roofColor, Label label,  StyleSet<Polygon3DStyle> styles, Object userData) {
    super(mapPoses, mapPosesHoles, height, label, styles, userData);
    this.minHeight = minHeight;
    this.roof = roof;
    
    if (roof != null) {
    	this.height -= roof.getRoofHeight();
    }
    
    this.color = new Color(color);
    this.roofColor = new Color(roofColor);
  }
  
  /**
   * Set the roof.
   * 
   * @param roof new roof.
   */
  public void setRoof(Roof roof) {
    if (roof != this.roof) {
      this.roof = roof;
      notifyElementChanged();
    }
  }

  /**
   * Not part of public API.
   * @pad.exclude
   */
  @Override
  public void calculateInternalState() {
    Projection projection = layer.getProjection();
   
    // Create polygon shell coordinates
    MutableEnvelope envInternal = new MutableEnvelope();
    Coordinate[] shellCoords = new Coordinate[mapPoses.size() + 1];
    int shellCoordCount = 0;
    for (MapPos mapPosOrig : mapPoses) {
      MapPos mapPos = projection.toInternal(mapPosOrig.x, mapPosOrig.y);
      envInternal.add(mapPos.x, mapPos.y);
      shellCoords[shellCoordCount] = new Coordinate(mapPos.x, mapPos.y, 0);
      shellCoordCount++;
    }
    shellCoords[shellCoords.length - 1] = new Coordinate(shellCoords[0]); 
    LinearRing shellInternal = new LinearRing(new CoordinateArraySequence(shellCoords), geoFac);
    
    LinearRing[] holesInternal = new LinearRing[0];
    if (mapPosesHoles != null) {
      holesInternal = new LinearRing[mapPosesHoles.size()];
      int holeCount = 0;
      for (List<MapPos> mapPosesHoleOrig : mapPosesHoles) {
        Coordinate[] holeCoords = new Coordinate[mapPosesHoleOrig.size() + 1];
        int holeCoordCount = 0;
        for (MapPos mapPosOrig : mapPosesHoleOrig) {
          MapPos mapPos = projection.toInternal(mapPosOrig.x, mapPosOrig.y);
          holeCoords[holeCoordCount] = new Coordinate(mapPos.x, mapPos.y, 0);
          holeCoordCount++;
        }
        holeCoords[holeCoords.length - 1] = new Coordinate(holeCoords[0]); 
        holesInternal[holeCount] = new LinearRing(new CoordinateArraySequence(holeCoords), geoFac);
        holeCount++;
      }
    }
    com.vividsolutions.jts.geom.Polygon roofPolygon = 
        new com.vividsolutions.jts.geom.Polygon(shellInternal, holesInternal, geoFac);
    com.vividsolutions.jts.geom.Point centroid = roofPolygon.getCentroid();
    MapPos originMapPosInternal = new MapPos(centroid.getX(), centroid.getY());
   
    
    
    
    
    // Calculate roof
    ArrayList<MapPos> roofVertices = new ArrayList<MapPos>();
    // Undefined roof will default to flat
    if (roof == null || (roof instanceof FlatRoof)) {
      // Prepare polygon exterior
      LineString polygonExt = roofPolygon.getExteriorRing();
      ArrayList<MapPos> polygonExtArray = new ArrayList<MapPos>();
      for (Coordinate coord:polygonExt.getCoordinates()) {
        polygonExtArray.add(new MapPos(coord.x, coord.y, coord.z));
      }
      
      // Prepare polygon holes
      ArrayList<ArrayList<MapPos>> polygonIntArrayArray = new ArrayList<ArrayList<MapPos>>();
      for (int tsj2 = 0; tsj2 < roofPolygon.getNumInteriorRing(); tsj2++) {
        ArrayList<MapPos> polygonIntArray = new ArrayList<MapPos>();
        for (Coordinate coord:roofPolygon.getInteriorRingN(tsj2).getCoordinates()) {
          polygonIntArray.add(new MapPos(coord.x, coord.y, coord.z));
        }
        polygonIntArrayArray.add(polygonIntArray);
      }
      
      // Triangulate roof part polygon, add triangles to list
      roofVertices.addAll(PolygonTriangulation.triangulate(polygonExtArray, polygonIntArrayArray));
    } else {
      // Calculate the polygon and it's minimum enclosing rectangle
      MinimumDiameter minDiameter = new MinimumDiameter(roofPolygon);
      com.vividsolutions.jts.geom.Geometry minRectangle = minDiameter.getMinimumRectangle();
      scaleGeometry(minRectangle, MIN_RECTANGLE_SCALE);
     
      // Get all the roof lines, add their collision points with the roof polygon to polygon coordinates
      roof.calculateRoof(minRectangle);
      for (LineString line : roof.getLines()) {
        roofPolygon = (Polygon) PolygonExtracter.getPolygons(roofPolygon.union(line)).get(0);
      }
      
      // Get all the roof part polygons without the z coordinate, intersect them with the roof polygon
      com.vividsolutions.jts.geom.Polygon[] roofPartPolygon = roof.getPolygons();
      for (int tsj = 0; tsj < roofPartPolygon.length; tsj++) {
        com.vividsolutions.jts.geom.Geometry polygonal = roofPolygon.intersection(roofPartPolygon[tsj]);
        
        // PolygonExtracter can return Polygon or MultiPolygon
        List<com.vividsolutions.jts.geom.Polygon> intersectedPolygons = 
            new LinkedList<com.vividsolutions.jts.geom.Polygon>();
        if (polygonal instanceof com.vividsolutions.jts.geom.Polygon) {
          intersectedPolygons.add((Polygon) polygonal);
        } else {
          MultiPolygon multiPolygon = (MultiPolygon) polygonal;
          for (int tsj2 = 0; tsj2 < multiPolygon.getNumGeometries(); tsj2++) {
            intersectedPolygons.add((Polygon) multiPolygon.getGeometryN(tsj2));
          }
        }
        
        for (com.vividsolutions.jts.geom.Polygon intersected : intersectedPolygons) {
          // Calculate heights for intersected roof parts
          roof.calculateRoofPartHeights(intersected);
          
          // Prepare polygon exterior
          LineString polygonExt = intersected.getExteriorRing();
          ArrayList<MapPos> polygonExtArray = new ArrayList<MapPos>();
          for (Coordinate coord:polygonExt.getCoordinates()) {
            polygonExtArray.add(new MapPos(coord.x, coord.y, coord.z));
          }
          
          // Prepare polygon holes
          ArrayList<ArrayList<MapPos>> polygonIntArrayArray = new ArrayList<ArrayList<MapPos>>();
          for (int tsj2 = 0; tsj2 < intersected.getNumInteriorRing(); tsj2++) {
            ArrayList<MapPos> polygonIntArray = new ArrayList<MapPos>();
            for (Coordinate coord:intersected.getInteriorRingN(tsj2).getCoordinates()) {
              polygonIntArray.add(new MapPos(coord.x, coord.y, coord.z));
            }
            polygonIntArrayArray.add(polygonIntArray);
          }
          
          // Triangulate roof part polygon, add triangles to list
          roofVertices.addAll(PolygonTriangulation.triangulate(polygonExtArray, polygonIntArrayArray));
        }
      }
    }
    
    
    
    
    // Allocate buffers for vertices/colors
    int sideVerts = roofPolygon.getExteriorRing().getCoordinates().length - 1;
    for (int tsj = 0; tsj < roofPolygon.getNumInteriorRing(); tsj++) {
      sideVerts += roofPolygon.getInteriorRingN(tsj).getCoordinates().length - 1;
    }
    float[] verts = new float[(roofVertices.size() + sideVerts * VERTICES_PER_SIDE) * 3];
    float[] colors = new float[(roofVertices.size() + sideVerts * VERTICES_PER_SIDE) * 3];

    // Calculate side vertices/colors
    int sideIndex = roofVertices.size();
    for (int n = -1; n < holesInternal.length; n++) {
      // n = -1 is for base polygon, n=0.. are for holes
      Coordinate[] coords = (n < 0 ? roofPolygon.getExteriorRing().getCoordinates() 
          : roofPolygon.getInteriorRingN(n).getCoordinates());

      // Calculate vertex orientation of the polygon
      double area = 0;
      for (int tsj = 0; tsj < coords.length - 1; tsj++) {
        Coordinate p1 = coords[tsj];
        Coordinate p2 = coords[(tsj + 1) % (coords.length - 1)];
        area += p1.x * p2.y - p1.y * p2.x;
      }
      boolean clockwise = (area * (n >= 0 ? -1 : 1) < 0);

      Coordinate firstMapPos = null, prevMapPos = null;
      for (int tsj = 0; tsj <= coords.length - 1; tsj++) {
        Coordinate mapPos;
        if (tsj < coords.length - 1) {
          mapPos = coords[tsj];
          if (firstMapPos == null) {
            firstMapPos = mapPos;
          }
        } else {
          mapPos = firstMapPos;
        }

        if (prevMapPos != null) {
          int index = sideIndex * 3;
          Coordinate mapPos0 = clockwise ? prevMapPos : mapPos;
          Coordinate mapPos1 = clockwise ? mapPos : prevMapPos;

          // Calculate how high walls need to connect with the roof
          float roofHeight1 = (float) roof.calculateRoofPointHeight(mapPos0.x, mapPos0.y) + height;
          float roofHeight2 = (float) roof.calculateRoofPointHeight(mapPos1.x, mapPos1.y) + height;
          
          // Calculate the vertex coordinates
          verts[index + 0] = (float) (mapPos0.x - originMapPosInternal.x);
          verts[index + 1] = (float) (mapPos0.y - originMapPosInternal.y);
          verts[index + 2] = roofHeight1;
          verts[index + 6] = (float) (mapPos0.x - originMapPosInternal.x);
          verts[index + 7] = (float) (mapPos0.y - originMapPosInternal.y);
          verts[index + 8] = minHeight;
          verts[index + 3] = (float) (mapPos1.x - originMapPosInternal.x);
          verts[index + 4] = (float) (mapPos1.y - originMapPosInternal.y);
          verts[index + 5] = minHeight;

          verts[index + 9]  = (float) (mapPos1.x - originMapPosInternal.x);
          verts[index + 10] = (float) (mapPos1.y - originMapPosInternal.y);
          verts[index + 11] = roofHeight2;
          verts[index + 15] = (float) (mapPos0.x - originMapPosInternal.x);
          verts[index + 16] = (float) (mapPos0.y - originMapPosInternal.y);
          verts[index + 17] = roofHeight1;
          verts[index + 12] = (float) (mapPos1.x - originMapPosInternal.x);
          verts[index + 13] = (float) (mapPos1.y - originMapPosInternal.y);
          verts[index + 14] = minHeight;

          // Calculate lighting intensity for the wall
          Vector surface = new Vector(mapPos1.x - mapPos0.x, mapPos1.y - mapPos0.y, 0).getNormalized2D();
          float intensity = calculateIntensity(new Vector(-surface.y, surface.x, 0));
          index = sideIndex * 3;
          for (int tsj2 = 0; tsj2 < 6; tsj2++) {
        	int index2 = index + tsj2 * 3;
            colors[index2] = color.r * intensity;
            colors[index2 + 1] = color.g * intensity;
            colors[index2 + 2] = color.b * intensity;
          }
          
          sideIndex += VERTICES_PER_SIDE;
        }
        prevMapPos = mapPos;
      }
    }
    
    // Calculate roof polygon vertices/colors
    for (int i = 0; i < roofVertices.size(); i += 3) {
      int index = i * 3;
      verts[index + 0] = (float) (roofVertices.get(i).x - originMapPosInternal.x);
      verts[index + 1] = (float) (roofVertices.get(i).y - originMapPosInternal.y);
      verts[index + 2] = (float) (roofVertices.get(i).z - originMapPosInternal.z) + height;
      verts[index + 3] = (float) (roofVertices.get(i + 1).x - originMapPosInternal.x);
      verts[index + 4] = (float) (roofVertices.get(i + 1).y - originMapPosInternal.y);
      verts[index + 5] = (float) (roofVertices.get(i + 1).z - originMapPosInternal.z) + height;
      verts[index + 6] = (float) (roofVertices.get(i + 2).x - originMapPosInternal.x);
      verts[index + 7] = (float) (roofVertices.get(i + 2).y - originMapPosInternal.y);
      verts[index + 8] = (float) (roofVertices.get(i + 2).z - originMapPosInternal.z) + height;

      // Calculate normal for the roof triangle, flat roof always has the same norma
      Vector normal;
      if (roof == null || (roof instanceof FlatRoof)) {       
        normal = new Vector(0, 0, 1);
      } else {
        Vector edge1 = new Vector(
            roofVertices.get(i + 1).x - roofVertices.get(i).x,
            roofVertices.get(i + 1).y - roofVertices.get(i).y,
            roofVertices.get(i + 1).z - roofVertices.get(i).z);
        Vector edge2 = new Vector(
            roofVertices.get(i + 2).x - roofVertices.get(i).x,
            roofVertices.get(i + 2).y - roofVertices.get(i).y,
            roofVertices.get(i + 2).z - roofVertices.get(i).z);
        normal = new Vector(
            edge1.y * edge2.z - edge1.z * edge2.y, 
            edge1.z * edge2.x - edge1.x * edge2.z, 
            edge1.x * edge2.y - edge1.y * edge2.x).getNormalized3D();
      }
      
      float intensity = calculateIntensity(normal); 
      index = i * 3;
      for (int tsj2 = 0; tsj2 < 3; tsj2++) {
    	int index2 = index + tsj2 * 3;
        colors[index2] = roofColor.r * intensity;
        colors[index2 + 1] = roofColor.g * intensity;
        colors[index2 + 2] = roofColor.b * intensity;
      }
    }

    RenderProjection renderProjection = layer.getRenderProjection();
    Point3D originPoint = renderProjection.project(originMapPosInternal);
    for (int i = 0; i < verts.length; i += 3) {
      MapPos mapPosInternal = new MapPos(verts[i + 0] + originMapPosInternal.x, verts[i + 1] + originMapPosInternal.y, verts[i + 2] + originMapPosInternal.z);
      Point3D point = renderProjection.project(mapPosInternal);
      verts[i + 0] = (float) (point.x - originPoint.x);
      verts[i + 1] = (float) (point.y - originPoint.y);
      verts[i + 2] = (float) (point.z - originPoint.z); 
    }
    setInternalState(new Polygon3DInternalState(originPoint, 
        verts, colors, new Envelope(envInternal)));
  }
  
  protected float calculateIntensity(Vector norm) {
    Vector3D lightDir = Const.LIGHT_DIR;
    float intensity = (float) -Vector3D.dotProduct(new Vector3D(norm.x, norm.y, norm.z), lightDir) * 0.5f + 0.5f;
    return (1 - Const.AMBIENT_FACTOR) * intensity + Const.AMBIENT_FACTOR;
  }
 
  private void scaleGeometry(Geometry geometry, float scale) {
    com.vividsolutions.jts.geom.Point centroid = geometry.getCentroid();
    Coordinate[] coords = geometry.getCoordinates();
    for (int tsj = 0; tsj < coords.length - 1; tsj++) {
      Coordinate coord = coords[tsj];
      coord.x = centroid.getX() + (coord.x - centroid.getX()) * scale;
      coord.y = centroid.getY() + (coord.y - centroid.getY()) * scale;
    }
  }
}
