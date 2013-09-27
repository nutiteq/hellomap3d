package com.nutiteq.advancedmap.maplisteners;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.os.Handler;
import android.webkit.WebView;

import com.nutiteq.MapView;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.raster.UtfGridLayerInterface;
import com.nutiteq.layers.raster.WmsLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.MapListener;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UiUtils;

/**
 * 
 * Used for click detection on WMS map, where GetFeatureInfo can be requested. Click initiates
 * HTTP request to server to get additional metadata as HTML. Then WebView is embed to a Label and shown
 * on map.
 * 
 * @author jaak
 *
 */
public class WmsLayerClickListener extends MapListener {

    private WmsLayer layer;
    private Marker clickMarker;
    private MapView mapView;
    private Handler handler = new Handler();

	// activity is often useful to handle click events
	public WmsLayerClickListener(Activity activity, MapView mapView, WmsLayer layer, Marker clickMarker) {
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
		
		final MutableMapPos tilePos = new MutableMapPos();
		final MapTile clickedTile = mapView.worldToMapTile(x, y, tilePos);

		Log.debug("clicked tile "+clickedTile+" pos:"+tilePos);

		// perform network query to get feature info. This must be done in separate thread!
		new Thread(new Runnable() {
		  @Override
		  public void run() {
	          final String featureInfo = layer.getFeatureInfo(clickedTile, tilePos);

	          if (featureInfo == null) {
	              return;
	          } else {
	              // update marker in UI thread 
	              handler.post(new Runnable() {
                      @Override
                      public void run() {
                          updateMarker(new MapPos(x, y), featureInfo);
                      }
	              });
	          }
		  }
		}).start();
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

    public WmsLayer getLayer() {
      return layer;
    }
    
    public Marker getClickMarker() {
      return clickMarker;
    }

}
