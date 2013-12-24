package com.nutiteq.datasources.vector;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import com.nutiteq.components.Bounds;
import com.nutiteq.components.CullState;
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
  private LongMap<Collection<T>> currentTileMap = new LongHashMap<Collection<T>>(); 
  
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
  public Collection<T> loadElements(CullState cullState) {
    int zoom = cullState.zoom;
    Envelope envelope = projection.fromInternal(cullState.envelope);
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
    List<T> data = new ArrayList<T>();
    LongMap<Collection<T>> newTileMap = new LongHashMap<Collection<T>>();
    for (int y = tileY0; y < tileY1; y++) {
      for (int x = tileX0; x < tileX1; x++) {
        // Precise tile containment test
        Envelope tileEnvelope = new Envelope(x * tileWidth, x * tileWidth + tileWidth, y * tileHeight, y * tileHeight + tileHeight);
        if (!envelope.intersects(tileEnvelope)) {
          continue;
        }
        
        // Create tile id based on zoom level and tile coordinates. Try to reuse already existing tile
        long tileId = zoom + (Const.MAX_SUPPORTED_ZOOM_LEVEL + 1) * ((long) y * zoomTiles + x);
        Collection<T> tileData = currentTileMap.get(tileId);
        if (tileData == null) {
          CullState tileCullState = new CullState(tileEnvelope, cullState.camera, cullState.renderProjection);
          tileData = dataSource.loadElements(tileCullState);
          if (tileData == null) {
            continue;
          }
        }
        
        // Merge data
        newTileMap.put(tileId, tileData);
        data.addAll(tileData);
      }
    }
    currentTileMap = newTileMap;
    return data;
  }

}
