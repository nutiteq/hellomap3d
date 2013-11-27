package com.nutiteq.datasources.vector;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.PriorityQueue;

import com.nutiteq.components.Bounds;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MutableVector;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.LongMap;
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
    List<LongMap.Entry<T>> elements;
    
    Cluster(LongMap.Entry<T> element) {
      elements = new ArrayList<LongMap.Entry<T>>();
      elements.add(element);
      mapPos = elementMerger.getMapPos(element.getValue());
    }
    
    void add(LongMap.Entry<T> element) {
      MapPos elementPos = elementMerger.getMapPos(element.getValue());
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
     *          list of input elements with their ids. This list is not empty.
     * @param mapPos
     *          position for the merged element.
     * @return merged element with new id. The id must uniquely identify the merged element. 
     */
    LongMap.Entry<T> mergeElements(List<LongMap.Entry<T>> elements, MapPos mapPos);
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
  public LongMap<T> loadElements(Envelope envelope, int zoom) {
    Bounds bounds = dataSource.getProjection().getBounds();
    double zoomTiles = 0.5 * (1 << zoom);
    double maxDistance = distance * Math.max(bounds.getWidth(), bounds.getHeight()) / zoomTiles;

    Envelope queryEnvelope = enlargeEnvelope(envelope, maxDistance * 2, bounds);
    LongMap<T> data = dataSource.loadElements(queryEnvelope, zoom);
    
    Envelope clusterEnvelope = enlargeEnvelope(envelope, maxDistance * 1, bounds);
    LongMap<T> clusteredData = clusterData(data, maxDistance, clusterEnvelope);

    return clusteredData;
  }
  
  protected Envelope enlargeEnvelope(Envelope envelope, double extra, Bounds bounds) {
    double minX = Math.max(envelope.minX - extra, bounds.left); 
    double maxX = Math.min(envelope.maxX + extra, bounds.right); 
    double minY = Math.max(envelope.minY - extra, bounds.bottom); 
    double maxY = Math.min(envelope.maxY + extra, bounds.top); 
    return new Envelope(minX, maxX, minY, maxY);
  }
  
  protected LongMap<T> clusterData(LongMap<T> elements, double maxDistance, Envelope envelope) {
    List<Cluster> clusters = new ArrayList<Cluster>();
    PriorityQueue<LongMap.Entry<T>> queue = new PriorityQueue<LongMap.Entry<T>>(elements.size() + 1, new Comparator<LongMap.Entry<T>>() {
      @Override
      public int compare(LongMap.Entry<T> elem1, LongMap.Entry<T> elem2) {
        return (int) (elem1.getKey() - elem2.getKey());
      }
    });
    for (Iterator<LongMap.Entry<T>> it = elements.entrySetIterator(); it.hasNext(); ) {
      LongMap.Entry<T> entry = it.next();
      queue.add(entry);
    }

    MutableVector diff = new MutableVector();
    while (!queue.isEmpty()) {
      LongMap.Entry<T> entry = queue.poll();
      boolean merged = false;
      MapPos elementPos = elementMerger.getMapPos(entry.getValue());
      for (Cluster cluster : clusters) {
        diff.setCoords(elementPos.x - cluster.mapPos.x, elementPos.y - cluster.mapPos.y);
        if (diff.getLength2D() <= maxDistance) {
          cluster.add(entry);
          merged = true;
          break;
        }
      }
      if (!merged) {
        Cluster cluster = new Cluster(entry);
        clusters.add(cluster);
      }
    }
    
    LongMap<T> clusteredElements = new LongHashMap<T>();
    for (Cluster cluster : clusters) {
      boolean inside = true;
      for (LongMap.Entry<T> entry : cluster.elements) { // if cluster contains points outside of the envelope, drop this cluster (too close to edge and may not contain all points in original data source)
        MapPos elementPos = elementMerger.getMapPos(entry.getValue());
        if (!envelope.contains(elementPos)) {
          inside = false;
          break;
        }
      }
      if (!inside) {
        continue;
      }
      if (cluster.elements.size() >= threshold) {
        LongMap.Entry<T> entry = elementMerger.mergeElements(cluster.elements, cluster.mapPos);
        clusteredElements.put(entry.getKey(), entry.getValue());
      } else {
        for (LongMap.Entry<T> entry : cluster.elements) {
          clusteredElements.put(entry.getKey(), entry.getValue());
        }
      }
    }
    
    return clusteredElements;
  }
  
}
