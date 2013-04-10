package com.nutiteq.layers.raster;

import java.util.Map;

import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;

public interface UtfGridLayerInterface {

    Map<String, String> getUtfGridTooltips(MapTile clickedTile, MutableMapPos tilePos, String template);

}
