package com.nutiteq.cachestores;

import com.nutiteq.cache.PersistentCache;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.TileBitmap;
import com.nutiteq.log.Log;
import com.nutiteq.rasterdatasources.CacheRasterDataSource.CacheStore;

/**
 * A simple persistent cache store example that uses PersistentCache class in the SDK as back end.
 * Note: this implementation only works with one data source: tile ids of multiple data sources are probably overlapping and would cause issues.
 * 
 * @author mark
 *
 */
public class PersistentCacheStore implements CacheStore {
  private final String fileName;
  private final int maxSize;
  private final PersistentCache persistentCache;

  public PersistentCacheStore(String fileName, int maxSize) {
    this.fileName = fileName;
    this.maxSize = maxSize;
    this.persistentCache = new PersistentCache();
  }
  
  @Override
  public void open() {
    persistentCache.setSize(maxSize);
    persistentCache.setPath(fileName);
    persistentCache.reopenDb();
  }

  @Override
  public void close() {
    persistentCache.closeDb();
  }

  @Override
  public TileBitmap get(MapTile tile) {
    try {
      byte[] compressed = persistentCache.get(tile.id);
      return new TileBitmap(compressed);
    } catch (Exception e) {
      Log.error("PersistentCacheStore: exception while loading tile: " + e);
      return null;
    }
  }

  @Override
  public void clear() {
    try {
      persistentCache.clear();
    } catch (Exception e) {
      Log.error("PersistentCacheStore: exception while clearing: " + e);
    }
  }

  @Override
  public void put(MapTile tile, TileBitmap tileBitmap) {
    try {
      byte[] compressed = tileBitmap.getCompressed();
      persistentCache.add(tile.id, compressed);
    } catch (Exception e) {
      Log.error("PersistentCacheStore: exception while storing: " + e);
    }
  }

  @Override
  public void remove(MapTile tile) {
    try {
      persistentCache.remove(tile.id);
    } catch (Exception e) {
      Log.error("PersistentCacheStore: exception while removing: " + e);
    }
  }

}
