package com.nutiteq.advancedmap;

import com.nutiteq.components.MapPos;
import com.nutiteq.services.routing.Route;

/**
 * 
 * Interface for Activities and other classes which want to get routing (Grapphopper, MapQuest or CloudMade) responses 
 * 
 * @author jaak
 *
 */
public interface RouteActivity {

    /**
     * MapListener provides start and end locations of route, second click on  map
     * @param fromLat
     * @param fromLon
     * @param toLat
     * @param toLon
     */
    public void showRoute(final double fromLat, final double fromLon,
            final double toLat, final double toLon);

    /**
     * MapListener provides start location of route, first click on map
     * @param startPos
     */
    public void setStartMarker(MapPos startPos);
    public void setStopMarker(MapPos mapPos);
    
    /**
     * Routing service gives calculated route
     * @param route
     */
    public void routeResult(Route route);

    
}
