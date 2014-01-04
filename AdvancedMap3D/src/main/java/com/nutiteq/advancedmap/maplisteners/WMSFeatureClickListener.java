package com.nutiteq.advancedmap.maplisteners;

import android.app.Activity;
import android.os.Handler;
import android.webkit.WebView;

import com.nutiteq.MapView;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.datasources.raster.WMSRasterDataSource;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
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
public class WMSFeatureClickListener extends MapListener {

    private WMSRasterDataSource dataSource;
    private Marker clickMarker;
    private MapView mapView;
    private Handler handler = new Handler();

    // activity is often useful to handle click events
    public WMSFeatureClickListener(Activity activity, MapView mapView, WMSRasterDataSource dataSource, Marker clickMarker) {
        this.dataSource = dataSource;
        this.clickMarker = clickMarker;
        this.mapView = mapView;
    }

    // Vector element (touch) handlers
    @Override
    public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
        Log.debug("clicked on label");
    }

    @Override
    public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
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
                final String featureInfo = dataSource.getFeatureInfo(clickedTile, tilePos);

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
            webView.loadDataWithBaseURL("file:///android_asset/", text, "text/html", "UTF-8", null);
        }
    }


    @Override
    public void onMapMoved() {
    }

    public Marker getClickMarker() {
        return clickMarker;
    }

    public WMSRasterDataSource getDataSource() {
        return dataSource;
    }
}
