package com.nutiteq.datasources.raster;

import com.nutiteq.components.MapTile;
import com.nutiteq.utils.UtfGridHelper.MBTileUTFGrid;

/**
 * Interface for UTF grid overlays.
 *
 */
public interface UTFGridDataSource {

    /**
     * Load UTF grid data for given tile.
     * 
     * @param tile
     * 			input tile
     * @return UTF grid data for the tile or null if not available.
     */
    MBTileUTFGrid loadUTFGrid(MapTile tile);
}
