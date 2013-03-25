package com.nutiteq.advancedmap.maplisteners;

import java.util.Map;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.webkit.WebView;
import android.widget.Toast;

import com.nutiteq.advancedmap.MBTilesMapActivity;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.raster.MBTilesMapLayer;
import com.nutiteq.layers.raster.db.MbTilesDatabaseHelper;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.MapListener;
import com.nutiteq.ui.ViewLabel;

import com.nutiteq.utils.UtfGridHelper;

public class MBTileMapEventListener extends MapListener {

	private Activity activity;
    private Marker clickMarker;

	// activity is often useful to handle click events
	public MBTileMapEventListener(Activity activity, Marker clickMarker) {
		this.activity = activity;
		this.clickMarker = clickMarker;
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
		Log.debug("onMapClicked " + (new EPSG3857()).toWgs84(x, y).x + " "
				+ (new EPSG3857()).toWgs84(x, y).y + " longClick: " + longClick);

		if(((MBTilesMapActivity) activity).getMapView().getLayers().getBaseLayer() instanceof MBTilesMapLayer){
		    MbTilesDatabaseHelper db = ((MBTilesMapLayer) ((MBTilesMapActivity) activity).getMapView().getLayers().getBaseLayer()).getDatabase();
		    Map<String, String> toolTips = db.getUtfGridTooltips(new MapPos(x,y), ((MBTilesMapActivity) activity).getMapView().getZoom());
		    if(toolTips == null){
		        return;
		    }
		    Log.debug("utfGrid tooltip values: "+toolTips.size());
		    if(toolTips.containsKey(UtfGridHelper.TEMPLATED_TEASER_KEY)){
	            String strippedTeaser = android.text.Html.fromHtml(toolTips.get(UtfGridHelper.TEMPLATED_TEASER_KEY).replaceAll("\\<.*?>","")).toString().replaceAll("\\p{C}", "").trim();
	            Toast.makeText(activity, strippedTeaser, Toast.LENGTH_SHORT).show();
	            updateMarker(new MapPos(x,y),toolTips.get(UtfGridHelper.TEMPLATED_TEASER_KEY));
		    }else{
		        // a static key, ADMIN is used in geography-class sample in TileMill
		        if(toolTips.containsKey("ADMIN")){
	                  Toast.makeText(activity, toolTips.get("ADMIN"), Toast.LENGTH_SHORT).show();
		        }
		    }
		    
		}
		
	}

	private void updateMarker(MapPos pos, String text) {
	    
	    if(clickMarker != null){
	        clickMarker.setMapPos(pos);
	        WebView webView = ((WebView)((ViewLabel)clickMarker.getLabel()).getView());
	       // clickMarker.setLabel(new DefaultLabel(text));
	        //webView.loadUrl("http://www.android.com");
	        Log.debug("showing html: "+text);
	        webView.loadDataWithBaseURL("file:///android_asset/",MBTilesMapActivity.HTML_HEAD+text+MBTilesMapActivity.HTML_FOOT, "text/html", "UTF-8",null);
	    }
    }

    @Override
	public void onMapMoved() {
	}

}
