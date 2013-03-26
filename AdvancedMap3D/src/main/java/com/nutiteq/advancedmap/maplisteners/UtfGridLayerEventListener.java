package com.nutiteq.advancedmap.maplisteners;

import java.util.Map;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.webkit.WebView;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.MBTilesMapActivity;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.raster.UtfGridLayerInterface;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.MapListener;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UiUtils;
import com.nutiteq.utils.UtfGridHelper;

public class UtfGridLayerEventListener extends MapListener {

	private Activity activity;
    private UtfGridLayerInterface layer;
    private String template;
    private Marker clickMarker;
    private MapView mapView;

	// activity is often useful to handle click events
	public UtfGridLayerEventListener(Activity activity, MapView mapView, UtfGridLayerInterface mapLayer, Marker clickMarker) {
		this.activity = activity;
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
        if(vectorElement.userData != null){
            Map<String, String> toolTipData = (Map<String, String>) vectorElement.userData;
            if(toolTipData.containsKey(UtfGridHelper.TEMPLATED_LOCATION_KEY)){
//              String strippedTeaser = android.text.Html.fromHtml(toolTips.get(UtfGridHelper.TEMPLATED_TEASER_KEY).replaceAll("\\<.*?>","")).toString().replaceAll("\\p{C}", "").trim();
//              Toast.makeText(activity, strippedTeaser, Toast.LENGTH_SHORT).show();
//                Log.debug("show label ")
                String url = toolTipData.get(UtfGridHelper.TEMPLATED_LOCATION_KEY);
                Log.debug("open url "+url);
                if(!url.equals("")){
                    Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                    activity.startActivity(i);
                }else{
                    Log.debug("empty url/location");    
                }
                
            }
        }
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

		if(layer instanceof UtfGridLayerInterface){
		    
		    Map<String, String> toolTips =  layer.getUtfGridTooltips(new MapPos(x,y), mapView.getZoom(), this.template);

		    if(toolTips == null){
		        return;
		    }
		    Log.debug("utfGrid tooltip values: "+toolTips.size());
		    updateMarker(new MapPos(x,y), toolTips);
		    
		}
		
	}
	

    private void updateMarker(MapPos pos, Map<String, String> toolTips) {
        
        if(clickMarker != null){
            
            String text = "";
            if(toolTips.containsKey(UtfGridHelper.TEMPLATED_TEASER_KEY)){
//              String strippedTeaser = android.text.Html.fromHtml(toolTips.get(UtfGridHelper.TEMPLATED_TEASER_KEY).replaceAll("\\<.*?>","")).toString().replaceAll("\\p{C}", "").trim();
//              Toast.makeText(activity, strippedTeaser, Toast.LENGTH_SHORT).show();
//                Log.debug("show label ")
                text  = toolTips.get(UtfGridHelper.TEMPLATED_TEASER_KEY);
            }else if(toolTips.containsKey("ADMIN")){
                text = toolTips.get("ADMIN");
            }
            
            clickMarker.setMapPos(pos);
            mapView.selectVectorElement(clickMarker);
            WebView webView = ((WebView)((ViewLabel)clickMarker.getLabel()).getView());
           // clickMarker.setLabel(new DefaultLabel(text));
            //webView.loadUrl("http://www.android.com");
            Log.debug("showing html: "+text);
            webView.loadDataWithBaseURL("file:///android_asset/",UiUtils.HTML_HEAD+text+UiUtils.HTML_FOOT, "text/html", "UTF-8",null);
            
            clickMarker.userData = toolTips;
            
        }
    }


	@Override
	public void onMapMoved() {
	}

    public void setTemplate(String template) {
        this.template = template;
    }

}
