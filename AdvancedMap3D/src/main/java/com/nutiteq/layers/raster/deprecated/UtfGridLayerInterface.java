package com.nutiteq.layers.raster.deprecated;

import java.util.Map;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;

@Deprecated
public interface UtfGridLayerInterface {

    Map<String, String> getUtfGridTooltips(MapTile clickedTile, MutableMapPos tilePos, String template);

    boolean hasUtfGridTooltips();
}
