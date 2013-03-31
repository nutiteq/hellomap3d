package com.nutiteq.layers.raster;

import java.util.Map;

import com.nutiteq.components.MapPos;

public interface UtfGridLayerInterface {

    Map<String, String> getUtfGridTooltips(MapPos p, float zoom, String template);

}
