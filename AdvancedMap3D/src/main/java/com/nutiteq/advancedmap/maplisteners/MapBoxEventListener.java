package com.nutiteq.advancedmap.maplisteners;

import java.util.Map;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.util.Log;
import android.widget.Toast;

import com.nutiteq.advancedmap.MapBoxMapActivity;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.raster.MapBoxMapLayer;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.MapListener;
import com.nutiteq.utils.UtfGridHelper;

public class MapBoxEventListener extends MapListener {

	private Activity activity;
    private MapBoxMapLayer layer;
    private String template;

	// activity is often useful to handle click events
	public MapBoxEventListener(Activity activity, MapBoxMapLayer mapLayer) {
		this.activity = activity;
		this.layer = mapLayer;
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
	}

	@Override
	public void onVectorElementClicked(VectorElement vectorElement, double x,
			double y, boolean longClick) {

	}

	// Map View manipulation handlers
	@Override
	public void onMapClicked(final double x, final double y,
			final boolean longClick) {
		// x and y are in base map projection, we convert them to the familiar
		// WGS84
		Log.d("nm", "onMapClicked " + (new EPSG3857()).toWgs84(x, y).x + " "
				+ (new EPSG3857()).toWgs84(x, y).y + " longClick: " + longClick);

		if(layer instanceof MapBoxMapLayer){
		    
		    
		    Map<String, String> toolTips =  layer.getUtfGridTooltips(new MapPos(x,y), ((MapBoxMapActivity) activity).getMapView().getZoom(), this.template);

		    if(toolTips == null){
		        return;
		    }
		    Log.d("nm","utfGrid tooltip values: "+toolTips.size());
		    if(toolTips.containsKey(UtfGridHelper.TEMPLATED_TEASER_KEY)){
	            String strippedTeaser = android.text.Html.fromHtml(toolTips.get(UtfGridHelper.TEMPLATED_TEASER_KEY).replaceAll("\\<.*?>","")).toString().replaceAll("\\p{C}", "").trim();
	            Toast.makeText(activity, strippedTeaser, Toast.LENGTH_SHORT).show();
		    }else{
		        // a static key, ADMIN is used in geography-class sample in TileMill
		        if(toolTips.containsKey("ADMIN")){
	                  Toast.makeText(activity, toolTips.get("ADMIN"), Toast.LENGTH_SHORT).show();
		        }
		    }
		    
		}
		
	}

	@Override
	public void onMapMoved() {
	}

    public void setTemplate(String template) {
        this.template = template;
    }

}
