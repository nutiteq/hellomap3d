package com.nutiteq.advancedmap.maplisteners;

import javax.microedition.khronos.opengles.GL10;

import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.services.routing.RouteActivity;
import com.nutiteq.ui.MapListener;

/**
 * 
 * This MapListener waits for two clicks on map - first to set routing start point, and then
 * second to mark end point and start routing service.
 * 
 * @author jaak
 *
 */
public class RouteMapEventListener extends MapListener {

	private RouteActivity activity;
    private MapPos startPos;
    private MapPos stopPos;

	// activity is often useful to handle click events
	public RouteMapEventListener(RouteActivity activity) {
		this.activity = activity;
	}

	// Map drawing callbacks for OpenGL manipulations
	@Override
	public void onSurfaceChanged(GL10 gl, int width, int height) {
	}

	@Override
	public void onDrawFrameAfter3D(GL10 gl, float zoomPow2) {
	}

	@Override
	public void onDrawFrameBefore3D(GL10 gl, float zoomPow2) {
	}

	// Vector element (touch) handlers
	@Override
	public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
		// Toast.makeText(activity, "onLabelClicked "+((DefaultLabel)
		// vectorElement.getLabel()).getTitle()+" longClick: "+longClick,
		// Toast.LENGTH_SHORT).show();
	}

	@Override
	public void onVectorElementClicked(VectorElement vectorElement, double x,
			double y, boolean longClick) {
		// Toast.makeText(activity, "onVectorElementClicked "+((DefaultLabel)
		// vectorElement.getLabel()).getTitle()+" longClick: "+longClick,
		// Toast.LENGTH_SHORT).show();

	}

	// Map View manipulation handlers
	@Override
	public void onMapClicked(final double x, final double y,
			final boolean longClick) {
		// x and y are in base map projection, we convert them to the familiar
		// WGS84
		Log.debug("onMapClicked " + (new EPSG3857()).toWgs84(x, y).x + " "
				+ (new EPSG3857()).toWgs84(x, y).y + " longClick: " + longClick);
		
		if(startPos == null){
		    // set start, or start again
		    startPos = (new EPSG3857()).toWgs84(x,y);
		    activity.setStartMarker(new MapPos(x,y));
		}else if(stopPos == null){
		    // set stop and calculate
		    stopPos = (new EPSG3857()).toWgs84(x,y);
		    activity.setStopMarker(new MapPos(x,y));
	        activity.showRoute(startPos.y, startPos.x, stopPos.y, stopPos.x);
		 
	        // restart to force new route next time
	        startPos = null;
	        stopPos = null;
		}
		
		
		

	}

	@Override
	public void onMapMoved() {
	}

}
