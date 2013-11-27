package com.nutiteq.datasources.vector;

import java.util.Iterator;

import com.nutiteq.components.Bounds;
import com.nutiteq.components.Envelope;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.utils.Const;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.LongMap;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;
import com.nutiteq.vectordatasources.VectorDataSource;

/**
 * Tiling vector data source. Splits incoming requests for data into tiles.
 * Tile size is uniform across the request and depends on current zoom level.
 * Also handles trivial caching - if new requests touches same tile as previous request, the tile from previous request is reused.
 * 
 * @param <T>
 *          Vector element data type 
 */
public class TileVectorDataSource<T extends VectorElement> extends AbstractVectorDataSource<T> {
  private final VectorDataSource<T> dataSource;
  private LongMap<LongMap<T>> currentTileMap = new LongHashMap<LongMap<T>>(); 
  
  /**
   * Default constructor
   * 
   * @param dataSource
   *          original data source
   */
  public TileVectorDataSource(VectorDataSource<T> dataSource) {
    super(dataSource.getProjection());
    this.dataSource = dataSource;
  }

  @Override
  public Envelope getDataExtent() {
    return dataSource.getDataExtent();
  }

  @Override
  public LongMap<T> loadElements(Envelope envelope, int zoom) {
    Bounds bounds = dataSource.getProjection().getBounds();
    int zoomTiles = 1 * (1 << zoom);
    double tileWidth  = bounds.getWidth()  / zoomTiles;
    double tileHeight = bounds.getHeight() / zoomTiles;
    
    // Calculate tile extents
    int tileX0 = (int) Math.floor(envelope.minX / tileWidth);
    int tileY0 = (int) Math.floor(envelope.minY / tileHeight);
    int tileX1 = (int) Math.ceil(envelope.maxX  / tileWidth);
    int tileY1 = (int) Math.ceil(envelope.maxY  / tileHeight);
    
    // Build final data map from individual tiles
    LongMap<T> data = new LongHashMap<T>();
    LongMap<LongMap<T>> newTileMap = new LongHashMap<LongMap<T>>();
    for (int y = tileY0; y < tileY1; y++) {
      for (int x = tileX0; x < tileX1; x++) {
        // Precise tile containment test
        Envelope tileEnvelope = new Envelope(x * tileWidth, x * tileWidth + tileWidth, y * tileHeight, y * tileHeight + tileHeight);
        if (!envelope.intersects(tileEnvelope)) {
          continue;
        }
        
        // Create tile id based on zoom level and tile coordinates. Try to reuse already existing tile
        long tileId = zoom + (Const.MAX_SUPPORTED_ZOOM_LEVEL + 1) * ((long) y * zoomTiles + x);
        LongMap<T> tileData = currentTileMap.get(tileId);
        if (tileData == null) {
          tileData = dataSource.loadElements(tileEnvelope, zoom);
          if (tileData == null) {
            continue;
          }
        }
        
        // Merge data
        newTileMap.put(tileId, tileData);
        for (Iterator<LongMap.Entry<T>> it = tileData.entrySetIterator(); it.hasNext(); ) {
          LongMap.Entry<T> entry = it.next();
          data.put(entry.getKey(), entry.getValue());
        }
      }
    }
    currentTileMap = newTileMap;
    return data;
  }

}
