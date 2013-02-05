package com.nutiteq.hellomap;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.widget.Toast;

import com.nutiteq.MapView;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.MapListener;

public class MapEventListener extends MapListener {

    private Activity activity;
    private MapView mapView;
    private MyLocationCircle locationCircle;

    public void setLocationCircle(MyLocationCircle locationCircle) {
        this.locationCircle = locationCircle;
    }

    // activity is often useful to handle click events 
    public MapEventListener(Activity activity, MapView mapView) {
        this.activity = activity;
        this.mapView = mapView;
    }
    
    // Reset activity and map view
    public void reset(Activity activity, MapView mapView) {
        this.activity = activity;
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
        if(this.locationCircle != null){
            this.locationCircle.draw(gl, zoomPow2);
            
            // As we want to animate location circle, request new frame to be rendered.
            // This is really bad for power efficiency, as constant redrawing drains battery.
            mapView.requestRender();
        }
    }

    // Vector element (touch) handlers
    @Override
    public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
        Toast.makeText(activity, "onLabelClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
        Toast.makeText(activity, "onVectorElementClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();

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
