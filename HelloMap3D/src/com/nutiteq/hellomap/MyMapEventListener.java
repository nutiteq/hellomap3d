package com.nutiteq.hellomap;

import android.app.Activity;
import android.widget.Toast;

import com.nutiteq.MapView;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.MapListener;

public class MyMapEventListener extends MapListener {

    private Activity activity;

    // activity is often useful to handle click events 
    public MyMapEventListener(Activity activity, MapView mapView) {
        this.activity = activity;
    }
    
    // Vector element (touch) handlers
    @Override
    public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
        if (vectorElement.getLabel() != null) {
            Toast.makeText(activity, "onLabelClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
        if (vectorElement.getLabel() != null) {
            Toast.makeText(activity, "onVectorElementClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
        }
    }

    // Map View manipulation handlers
    @Override
    public void onMapClicked(final double x, final double y, final boolean longClick) {
        // x and y are in base map projection, we convert them to the familiar WGS84 
        Toast.makeText(activity, "onMapClicked "+(new EPSG3857()).toWgs84(x, y).x+" "+(new EPSG3857()).toWgs84(x, y).y+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onMapMoved() {
        // this method is also called from non-UI thread
    }
}
