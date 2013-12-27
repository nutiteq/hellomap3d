package com.nutiteq.datasources.vector;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.projections.Projection;
import com.nutiteq.utils.Const;
import com.nutiteq.utils.GeomUtils;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.LongMap;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;
import com.nutiteq.vectordatasources.VectorDataSource;

/**
 * Data source for simplifying original geometric data.
 * For points, uses simple bucketing algorithm (points are associated with integer coordinates and only one point with unique coordinates is kept).
 * For lines and polygons, Douglas-Peucker algorithm is used for ring simplification.
 */
public class SimplifierVectorDataSource extends AbstractVectorDataSource<Geometry> {
  public static final int DOUGLAS_PEUCKER = 0;
  public static final int VERTEX_SNAP = 1;
  
  private final VectorDataSource<Geometry> dataSource;
  private float tolerance;
  private int lineSimplifyAlgorithm = DOUGLAS_PEUCKER;
  private int polygonSimplifyAlgorithm = VERTEX_SNAP;
  
  /**
   * Default constructor
   * 
   * @param dataSource
   *          original data source
   * @param tolerance
   *          error tolerance parameter for simplification. 0.1f can be used for rough simplification, 0.01f for finer simplification.
   */
  public SimplifierVectorDataSource(VectorDataSource<Geometry> dataSource, float tolerance) {
    super(dataSource.getProjection());
    this.dataSource = dataSource;
    this.tolerance = tolerance;
  }
  
  /**
   * Get current simplification error tolerance value.
   * 
   * @return current tolerance value
   */
  public float getTolerance() {
    return tolerance;
  }
  
  /**
   * Set the current error tolerance value
   * 
   * @param tolerance
   *          new value for error tolerance
   */
  public void setTolerance(float tolerance) {
    this.tolerance = tolerance;
  }
  
  /**
   * Get current line simplification algorithm.
   * 
   * @return current simplification algorithm. Either DOUGLAS_PEUCKER or VERTEX_SNAP.
   */
  public int getLineSimplificationAlgorithm() {
    return lineSimplifyAlgorithm;
  }
  
  /**
   * Set current line simplification algorithm.
   * 
   * @param algorithm
   *          algorithm to use. Either DOUGLAS_PEUCKER or VERTEX_SNAP. Default is DOUGLAS_PEUCKER.
   */
  public void setLineSimplificationAlgorithm(int algorithm) {
    this.lineSimplifyAlgorithm = algorithm;
  }
  
  /**
   * Get current polygon simplification algorithm.
   * 
   * @return current simplification algorithm. Either DOUGLAS_PEUCKER or VERTEX_SNAP.
   */
  public int getPolygonSimplificationAlgorithm() {
    return polygonSimplifyAlgorithm;
  }
  
  /**
   * Set current polygon simplification algorithm.
   * 
   * @param algorithm
   *          algorithm to use. Either DOUGLAS_PEUCKER or VERTEX_SNAP. Default is VERTEX_SNAP.
   */
  public void setPolygonSimplificationAlgorithm(int algorithm) {
    this.polygonSimplifyAlgorithm = algorithm;
  }
  
  @Override
  public Envelope getDataExtent() {
    return dataSource.getDataExtent();
  }
  
  @Override
  public Collection<Geometry> loadElements(CullState cullState) {
    // Load original data
    Collection<Geometry> data = dataSource.loadElements(cullState);
    if (tolerance <= 0) {
      return data;
    }
    
    // Calculate zoom-based tolerance. Assume coordinates are in internal coordinate system
    double tolerance = Const.UNIT_SIZE * this.tolerance / (1 << cullState.zoom);
    
    // Create map of simplified elements
    Collection<Geometry> simplifiedData = new ArrayList<Geometry>(data.size() + 1);
    LongMap<Geometry> pointBucketMap = new LongHashMap<Geometry>();
    for (Iterator<Geometry> it = data.iterator(); it.hasNext(); ) {
      Geometry element = it.next();
      
      // Process element based on type
      if (element instanceof Point) {
        Point point = (Point) element;
        long bucketId = calculatePointBucket(point, tolerance);
        pointBucketMap.put(bucketId, element);
      } else if (element instanceof Line) {
        Line line = (Line) element;
        List<MapPos> simplifiedVertexList = simplifyRing(line.getVertexList(), tolerance, lineSimplifyAlgorithm);
        if (simplifiedVertexList.size() >= 2) {
          Line simplifiedLine = new Line(simplifiedVertexList, line.getLabel(), line.getStyleSet(), line.userData);
          simplifiedData.add(simplifiedLine);
        }
      } else if (element instanceof Polygon) {
        Polygon polygon = (Polygon) element;
        List<MapPos> simplifiedVertexList = simplifyRing(polygon.getVertexList(), tolerance, polygonSimplifyAlgorithm);
        if (simplifiedVertexList.size() >= 3) {
          List<List<MapPos>> holePolygonList = null;
          if (polygon.getHolePolygonList() != null) {
            holePolygonList = new LinkedList<List<MapPos>>();
            for (List<MapPos> holeVertexList : polygon.getHolePolygonList()) {
              List<MapPos> simplifiedHoleVertexList = simplifyRing(holeVertexList, tolerance, polygonSimplifyAlgorithm);
              if (simplifiedHoleVertexList.size() >= 3) {
                holePolygonList.add(simplifiedHoleVertexList);
              }
            }
          }
          Polygon simplifiedPolygon = new Polygon(simplifiedVertexList, holePolygonList, polygon.getLabel(), polygon.getStyleSet(), polygon.userData);
          simplifiedData.add(simplifiedPolygon);
        }
      }
    }
    
    // Add bucketed points
    simplifiedData.addAll(pointBucketMap.values());

    return simplifiedData;
  }
  
  private long calculatePointBucket(Point point, double tolerance) {
    int x = (int) Math.floor(point.getMapPos().x / tolerance);
    int y = (int) Math.floor(point.getMapPos().y / tolerance);
    return x ^ ((long) y << 32); 
  }
  
  private List<MapPos> simplifyRing(List<MapPos> ring, double tolerance, int algorithm) {
    switch (algorithm) {
    case DOUGLAS_PEUCKER:
      return simplifyRingDouglasPeucker(ring, tolerance);
    case VERTEX_SNAP:
      return simplifyRingVertexSnap(ring, tolerance);
    }
    return ring;
  }
  
  private List<MapPos> simplifyRingVertexSnap(List<MapPos> ring, double tolerance) {
    if (tolerance <= 0) {
      return ring;
    }
    Projection projection = dataSource.getProjection();
    List<MapPos> result = new ArrayList<MapPos>();
    for (int i = 0; i < ring.size(); i++) {
      MapPos pos = projection.toInternal(ring.get(i).x, ring.get(i).y);
      double x = Math.round(pos.x / tolerance) * tolerance;
      double y = Math.round(pos.y / tolerance) * tolerance;
      MapPos current = projection.fromInternal(x, y);
      if (result.size() > 0) {
        MapPos last = result.get(result.size() - 1);
        if (last.equals(current)) {
          continue;
        }
      }
      result.add(current);
    }
    return result;
  }

private List<MapPos> simplifyRingDouglasPeucker(List<MapPos> ring, double tolerance) {
    if (ring.size() <= 2) {
      return ring;
    }
    Projection projection = dataSource.getProjection();
    MapPos posA = projection.toInternal(ring.get(0).x, ring.get(0).y);
    MapPos posB = projection.toInternal(ring.get(ring.size() - 1).x, ring.get(ring.size() - 1).y);
    int index = -1;
    double distmax = tolerance * tolerance;
    for (int i = 1; i < ring.size() - 1; i++) {
      MapPos pos = projection.toInternal(ring.get(i).x, ring.get(i).y);
      double dist = distanceLinePointSqr(posA.x, posA.y, posB.x, posB.y, pos.x, pos.y);
      if (dist > distmax) {
        index = i;
        distmax = dist;
      }
    }
    if (index == -1) {
      List<MapPos> result = new ArrayList<MapPos>();
      result.add(ring.get(0));
      result.add(ring.get(ring.size() - 1));
      return result;
    }
    List<MapPos> ring0 = simplifyRingDouglasPeucker(ring.subList(0, index + 1), tolerance);
    List<MapPos> ring1 = simplifyRingDouglasPeucker(ring.subList(index, ring.size()), tolerance);
    List<MapPos> result = new ArrayList<MapPos>(ring0);
    result.remove(result.size() - 1); // also first vertex in ring1, thus duplicate
    result.addAll(ring1);
    return result;
  }
  
  private static double distanceLinePointSqr(double ax, double ay, double bx, double by, double px, double py) {
    double apx = px - ax;
    double apy = py - ay;
    double abx = bx - ax;
    double aby = by - ay;

    double ab2 = abx * abx + aby * aby;
    double ap_ab = apx * abx + apy * aby;
    double t = Math.min(1, Math.max(0, ap_ab / ab2));

    double qx = (float) (ax + abx * t);
    double qy = (float) (ay + aby * t);
    double distX = px - qx;
    double distY = py - qy;
    return distX * distX + distY * distY;
  }
}
