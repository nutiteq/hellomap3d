package com.nutiteq.advancedmap.maplisteners;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.webkit.WebView;

import com.nutiteq.MapView;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.raster.WmsLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.MapListener;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UiUtils;

public class WmsLayerClickListener extends MapListener {

    private WmsLayer layer;
    private Marker clickMarker;
    private MapView mapView;

	// activity is often useful to handle click events
	public WmsLayerClickListener(Activity activity, MapView mapView, WmsLayer mapLayer, Marker clickMarker) {
		this.layer = mapLayer;
		this.clickMarker = clickMarker;
		this.mapView = mapView;
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
	    Log.debug("clicked on label");
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
		Log.debug("onMapClicked " + (new EPSG3857()).toWgs84(x, y).x + " "
				+ (new EPSG3857()).toWgs84(x, y).y + " longClick: " + longClick);
		
		MutableMapPos tilePos = new MutableMapPos();
		MapTile clickedTile = mapView.worldToMapTile(x, y, tilePos);

		Log.debug("clicked tile "+clickedTile+" pos:"+tilePos);
		
        // TODO: getFeatureInfo does network request, so call it from another thread

        String featureInfo = layer.getFeatureInfo(clickedTile, tilePos);

        if (featureInfo == null) {
            return;
        } else {
            updateMarker(new MapPos(x, y), featureInfo);
        }
		
	}
	

    private void updateMarker(MapPos pos, String text) {
        
        if(clickMarker != null){
            clickMarker.setMapPos(pos);
            mapView.selectVectorElement(clickMarker);
            WebView webView = ((WebView)((ViewLabel)clickMarker.getLabel()).getView());
            Log.debug("showing html: "+text);
            webView.loadDataWithBaseURL("file:///android_asset/",UiUtils.HTML_HEAD+text+UiUtils.HTML_FOOT, "text/html", "UTF-8",null);
            
        }
    }


	@Override
	public void onMapMoved() {
	}

}
