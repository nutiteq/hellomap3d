package com.nutiteq.advancedmap.maplisteners;

import java.util.Map;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.webkit.WebView;

import com.nutiteq.MapView;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.raster.deprecated.UtfGridLayerInterface;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.MapListener;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UiUtils;
import com.nutiteq.utils.UtfGridHelper;

/**
 * 
 * This MapListener is used to listen map click events and update tooltip on map from UTFGrid data
 * The tooltip is technically a Marker which has real marker part (bitmap) hidden, and Label opened, so
 * it looks like just a Label on map.
 * 
 * WebView is created and used inside Label, this enables HTML for labels.
 * 
 * @author jaak
 *
 */
public class UtfGridLayerEventListener2 extends MapListener {

	private Activity activity;
    private UtfGridLayerInterface layer;
    private String template;
    private Marker clickMarker;
    private MapView mapView;

	// activity is often useful to handle click events
	public UtfGridLayerEventListener2(Activity activity, MapView mapView, UtfGridLayerInterface layer, Marker clickMarker) {
		this.activity = activity;
		this.layer = layer;
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
		
		MutableMapPos tilePos = new MutableMapPos();
		MapTile clickedTile = mapView.worldToMapTile(x, y, tilePos);

		Log.debug("clicked tile "+clickedTile+" pos:"+tilePos);
		
		if(layer instanceof UtfGridLayerInterface && layer.hasUtfGridTooltips()){

		    Map<String, String> toolTips =  layer.getUtfGridTooltips(clickedTile, tilePos, this.template);

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
                // strip HTML from the teaser, so it can be shown in normal 
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
            Log.debug("showing html: "+text);
            webView.loadDataWithBaseURL("file:///android_asset/",UiUtils.HTML_HEAD+text+UiUtils.HTML_FOOT, "text/html", "UTF-8",null);
            Log.debug("webView.getWidth() = "+webView.getWidth());
            Log.debug("webView.getHeight() = "+webView.getHeight());
            Log.debug("webView.getContentHeight() = "+webView.getContentHeight());
            // try to resize view based on real content, but this does not work properly
            //webView.layout(0, 0, webView.getWidth(), webView.getContentHeight());
            clickMarker.userData = toolTips;
            
        }
    }


	@Override
	public void onMapMoved() {
	}

    public void setTemplate(String template) {
        this.template = template;
    }
    
    public UtfGridLayerInterface getLayer() {
      return layer;
    }
    
    public Marker getClickMarker() {
      return clickMarker;
    }

}
