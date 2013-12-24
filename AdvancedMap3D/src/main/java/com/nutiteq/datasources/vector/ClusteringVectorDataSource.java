package com.nutiteq.datasources.vector;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.PriorityQueue;

import com.nutiteq.components.Bounds;
import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MutableVector;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;
import com.nutiteq.vectordatasources.VectorDataSource;

/**
 * Virtual clustering data source. Take elements from input data source and merges elements that are closer than given
 * minimum distance value into a cluster. Element merging can be customized.   
 *
 * @param <T>
 *          Vector element data type (usually Marker or Point)
 */
public class ClusteringVectorDataSource<T extends VectorElement> extends AbstractVectorDataSource<T> {
  private VectorDataSource<T> dataSource;
  private ElementMerger<T> elementMerger;
  private float distance;
  private int threshold;
  
  private class Cluster {
    MapPos mapPos;
    List<T> elements;
    
    Cluster(T element) {
      elements = new ArrayList<T>();
      elements.add(element);
      mapPos = elementMerger.getMapPos(element);
    }
    
    void add(T element) {
      MapPos elementPos = elementMerger.getMapPos(element);
      double cx = (mapPos.x * elements.size() + elementPos.x);
      double cy = (mapPos.y * elements.size() + elementPos.y);
      elements.add(element);
      mapPos = new MapPos(cx / elements.size(), cy / elements.size());
    }
  }

  /**
   * Interface for customizing element merging.
   *
   * @param <T>
   *          Vector element data type (usually Marker or Point)
   */
  public interface ElementMerger<T extends VectorElement> {
    /**
     * Extract element position.
     * 
     * @param element
     *          input element
     * @return input element position
     */
    MapPos getMapPos(T element);
    
    /**
     * Merge list of elements into a single element.
     * 
     * @param elements
     *          list of input elements. This list is not empty.
     * @param mapPos
     *          position for the merged element.
     * @return merged element 
     */
    T mergeElements(List<T> elements, MapPos mapPos);
  }
  
  /**
   * Default constructor.
   * 
   * @param dataSource
   *          input data source
   * @param distance
   *          minimum allowed distance between elements. Distance is in relative screen units - 1.0f covers whole screen. Good value to start with is usually between 0.05f and 0.1f.
   * @param threshold
   *          minimum number of elements in a cluster. When cluster contains less elements, elements are added individually. Must be larger than 1, usually 2.
   * @param elementMerger
   *          element merger interface for customizing clustering.
   */
  public ClusteringVectorDataSource(VectorDataSource<T> dataSource, float distance, int threshold, ElementMerger<T> elementMerger) {
    super(dataSource.getProjection());
    this.dataSource = dataSource;
    this.distance = distance;
    this.threshold = threshold;
    this.elementMerger = elementMerger;
  }
  
  @Override
  public Envelope getDataExtent() {
    return dataSource.getDataExtent();
  }

  @Override
  public Collection<T> loadElements(CullState cullState) {
    int zoom = cullState.zoom;
    Envelope envelope = projection.fromInternal(cullState.envelope);
    Bounds bounds = dataSource.getProjection().getBounds();
    double zoomTiles = 0.5 * (1 << zoom);
    double maxDistance = distance * Math.max(bounds.getWidth(), bounds.getHeight()) / zoomTiles;

    Envelope queryEnvelope = enlargeEnvelope(envelope, maxDistance * 2, bounds);
    CullState queryCullState = new CullState(queryEnvelope, cullState.camera, cullState.renderProjection);
    Collection<T> data = dataSource.loadElements(queryCullState);
    
    Envelope clusterEnvelope = enlargeEnvelope(envelope, maxDistance * 1, bounds);
    Collection<T> clusteredData = clusterData(data, maxDistance, clusterEnvelope);

    return clusteredData;
  }
  
  protected Envelope enlargeEnvelope(Envelope envelope, double extra, Bounds bounds) {
    double minX = Math.max(envelope.minX - extra, bounds.left); 
    double maxX = Math.min(envelope.maxX + extra, bounds.right); 
    double minY = Math.max(envelope.minY - extra, bounds.bottom); 
    double maxY = Math.min(envelope.maxY + extra, bounds.top); 
    return new Envelope(minX, maxX, minY, maxY);
  }
  
  protected Collection<T> clusterData(Collection<T> elements, double maxDistance, Envelope envelope) {
    List<Cluster> clusters = new ArrayList<Cluster>();
    PriorityQueue<T> queue = new PriorityQueue<T>(elements.size() + 1, new Comparator<T>() {
      @Override
      public int compare(T elem1, T elem2) {
        if (elem1.getId() != elem2.getId()) {
          return elem1.getId() < elem2.getId() ? -1 : 1;
        }
        return elem1.hashCode() - elem2.hashCode();
      }
    });
    queue.addAll(elements);

    MutableVector diff = new MutableVector();
    while (!queue.isEmpty()) {
      T element = queue.poll();
      boolean merged = false;
      MapPos elementPos = elementMerger.getMapPos(element);
      for (Cluster cluster : clusters) {
        diff.setCoords(elementPos.x - cluster.mapPos.x, elementPos.y - cluster.mapPos.y);
        if (diff.getLength2D() <= maxDistance) {
          cluster.add(element);
          merged = true;
          break;
        }
      }
      if (!merged) {
        Cluster cluster = new Cluster(element);
        clusters.add(cluster);
      }
    }
    
    List<T> clusteredElements = new ArrayList<T>();
    for (Cluster cluster : clusters) {
      boolean inside = true;
      for (T element : cluster.elements) { // if cluster contains points outside of the envelope, drop this cluster (too close to edge and may not contain all points in original data source)
        MapPos elementPos = elementMerger.getMapPos(element);
        if (!envelope.contains(elementPos)) {
          inside = false;
          break;
        }
      }
      if (!inside) {
        continue;
      }
      if (cluster.elements.size() >= threshold) {
        T element = elementMerger.mergeElements(cluster.elements, cluster.mapPos);
        clusteredElements.add(element);
      } else {
        clusteredElements.addAll(cluster.elements);
      }
    }
    
    return clusteredElements;
  }
  
}
