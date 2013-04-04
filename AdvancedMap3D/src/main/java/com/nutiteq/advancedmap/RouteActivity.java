package com.nutiteq.advancedmap;

import com.nutiteq.components.MapPos;

public interface RouteActivity {

    public void showRoute(final double fromLat, final double fromLon,
            final double toLat, final double toLon);

    public void setStartmarker(MapPos startPos);
    
}
