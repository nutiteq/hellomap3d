package com.nutiteq.layers.vector;

import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Vector;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.db.DBLayer;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.geometry.Polygon3D;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.vectorlayers.Polygon3DLayer;

public class Polygon3DDbLayer extends Polygon3DLayer {
  private static final float DEFAULT_HEIGHT = 2.0f;
  private SpatialLiteDb spatialLite;
  private DBLayer dbLayer;

  private StyleSet<Polygon3DStyle> styleSet;

  private int minZoom;
  private String heightColumnName;
  private float heightFactor;
  private int maxObjects;

  /**
   * Simple layer of 3D Polygons, reads data from Spatialite table with 2D polygons
 * @param dbPath Spatialite format database
 * @param tableName table with data
 * @param geomColumnName column in table with geometry
 * @param heightColumnName column in table with heigth in meters
 * @param heightFactor multiply height with this number to make it visible
 * @param maxObjects limit number of loaded objects, set to 500 e.g. to avoid out of memory
 * @param styleSet Polygon3DStyle styleset for visualisation
 */
public Polygon3DDbLayer(String dbPath, String tableName, String geomColumnName, String heightColumnName,
      float heightFactor, int maxObjects, StyleSet<Polygon3DStyle> styleSet) {
    super(new EPSG3857());
    this.styleSet = styleSet;
    this.heightColumnName = heightColumnName;
    this.heightFactor = heightFactor;
    this.maxObjects = maxObjects;
    minZoom = styleSet.getFirstNonNullZoomStyleZoom();
    spatialLite = new SpatialLiteDb(dbPath);
    Vector<DBLayer> dbLayers = spatialLite.qrySpatialLayerMetadata();
    for (DBLayer dbLayer : dbLayers) {
      if (dbLayer.table.compareTo(tableName) == 0 && dbLayer.geomColumn.compareTo(geomColumnName) == 0) {
        this.dbLayer = dbLayer;
        break;
      }
    }

    if (this.dbLayer == null) {
      Log.error("Polygon3DDbLayer: Could not find a matching DBLayer!");
    }
  }

  public void add(Polygon3D element) {
    throw new UnsupportedOperationException();
  }

  public void remove(Polygon3D element) {
    throw new UnsupportedOperationException();
  }

  @SuppressWarnings("unchecked")
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

    List<Geometry> visibleElementslist = spatialLite.qrySpatiaLiteGeom(new Envelope(bottomLeft.x, topRight.x,
        bottomLeft.y, topRight.y), maxObjects, dbLayer, new String[] { heightColumnName });

    long start = System.currentTimeMillis();
    List<Polygon3D> newVisibleElementsList = new LinkedList<Polygon3D>();
    for (Geometry geometry : visibleElementslist) {

      float height;
      if (heightColumnName == null) {
        height = DEFAULT_HEIGHT;
      } else {
        height = Float.parseFloat(((Map<String, String>) geometry.userData).get(heightColumnName)) * heightFactor;
      }
      Polygon3D polygon3D = new Polygon3D(((Polygon) geometry).getVertexList(), ((Polygon) geometry).getHolePolygonList(), height, null, styleSet, null);
      polygon3D.attachToLayer(this);
      polygon3D.setActiveStyle(zoom);
      newVisibleElementsList.add(polygon3D);
    }
    Log.debug("Triangulation time: " + (System.currentTimeMillis() - start));

    setVisibleElementsList(newVisibleElementsList);
  }
}
