package com.nutiteq.layers.raster.deprecated;

import com.nutiteq.projections.Projection;

@Deprecated
public class TMSMapLayerNoCache extends TMSMapLayer {

    public TMSMapLayerNoCache(Projection projection, int minZoom, int maxZoom,
            int id, String baseUrl, String separator, String format) {
        super(projection, minZoom, maxZoom, id, baseUrl, separator, format);
        setPersistentCaching(false);
    }

}
