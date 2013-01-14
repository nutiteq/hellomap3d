package com.nutiteq.layers.vector;

import java.util.Map;
import java.util.Vector;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.db.DBLayer;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.vectorlayers.GeometryLayer;


public class SpatialiteLayer extends GeometryLayer {
  private SpatialLiteDb spatialLite;
  private DBLayer dbLayer;

  private StyleSet<PointStyle> pointStyleSet;
  private StyleSet<LineStyle> lineStyleSet;
  private StyleSet<PolygonStyle> polygonStyle;

  private int minZoom;
  private int maxObjects;
  private String[] userColumns;

  public SpatialiteLayer(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, int maxObjects,
      StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet) {
    super(proj);
    this.userColumns = userColumns;
    this.pointStyleSet = pointStyleSet;
    this.lineStyleSet = lineStyleSet;
    this.polygonStyle = polygonStyleSet;
    this.maxObjects = maxObjects;
    if (pointStyleSet != null) {
      minZoom = pointStyleSet.getFirstNonNullZoomStyleZoom();
    }
    if (lineStyleSet != null) {
      minZoom = lineStyleSet.getFirstNonNullZoomStyleZoom();
    }
    if (polygonStyleSet != null) {
      minZoom = polygonStyleSet.getFirstNonNullZoomStyleZoom();
    }

    spatialLite = new SpatialLiteDb(dbPath);
    Vector<DBLayer> dbLayers = spatialLite.qrySpatialLayerMetadata();
    for (DBLayer dbLayer : dbLayers) {
      if (dbLayer.table.compareTo(tableName) == 0 && dbLayer.geomColumn.compareTo(geomColumnName) == 0) {
        this.dbLayer = dbLayer;
        break;
      }
    }

    if (this.dbLayer == null) {
      Log.error("SpatialiteLayer: Could not find a matching layer " + tableName + "." + geomColumnName);
    }
  }

  public void add(Geometry element) {
    throw new UnsupportedOperationException();
  }

  public void remove(Geometry element) {
    throw new UnsupportedOperationException();
  }

  @Override
  public void calculateVisibleElements(Envelope envelope, int zoom) {
    if (dbLayer == null) {
      return;
    }

    if (zoom < minZoom) {
        setVisibleElementsList(null);
      return;
    }

    MapPos bottomLeft = projection.fromInternal(envelope.getMinX(), envelope.getMinY());
    MapPos topRight = projection.fromInternal(envelope.getMaxX(), envelope.getMaxY());
    Vector<Geometry> objectTemp = spatialLite.qrySpatiaLiteGeom(new Envelope(bottomLeft.x, topRight.x,
        bottomLeft.y, topRight.y), maxObjects, dbLayer, userColumns);
    
    Vector<Geometry> objects = new Vector<Geometry>();
    // apply styles, create new objects for these
    for(Geometry object: objectTemp){
        
        final Map<String, String> userData = (Map<String, String>) object.userData;
        String name = userData.get(userColumns[0]);
        DefaultLabel label = null;
        if (name != null){
            label = new DefaultLabel(name);
        }
        
        Geometry newObject = null;
        
        if(object instanceof Point){
            newObject = new Point(((Point) object).getMapPos(), label, pointStyleSet, object.userData);
        }else if(object instanceof Line){
            newObject = new Line(((Line) object).getVertexList(), label, lineStyleSet, object.userData);
        }else if(object instanceof Polygon){
            newObject = new Polygon(((Polygon) object).getVertexList(), ((Polygon) object).getHolePolygonList(), label, polygonStyle, object.userData);
        }
        
        newObject.attachToLayer(this);
        newObject.setActiveStyle(zoom);
        
        objects.add(newObject);
    }
    
    setVisibleElementsList(objects);

  }
}
