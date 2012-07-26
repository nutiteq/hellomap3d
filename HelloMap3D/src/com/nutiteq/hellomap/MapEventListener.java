package com.nutiteq.hellomap;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.widget.Toast;

import com.nutiteq.geometry.VectorElement;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.MapListener;

public class MapEventListener extends MapListener {

    private Activity activity;

    // activity is often useful to handle click events 
    public MapEventListener(Activity activity) {
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
    public void onDrawFrameBefore3D(GL10 arg0, float zoomPow2) {
    }

    // Vector element (touch) handlers
    @Override
    public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
        Toast.makeText(activity, "onLabelClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onVectorElementClicked(VectorElement vectorElement, boolean longClick) {
        Toast.makeText(activity, "onVectorElementClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();

    }

    // Map View manipulation handlers
    @Override
    public void onMapClicked(final float x, final float y, final boolean longClick) {
        // NB! this event is not called from UI thread, so we need following trick to show Toast
        // x and y are in base map projection, we convert them to the familiar WGS84 
        activity.runOnUiThread(new Runnable() {
            public void run() {
                Toast.makeText(activity, "onMapClicked "+(new EPSG3857()).toWgs84(x, y).x+" "+(new EPSG3857()).toWgs84(x, y).y+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
            }
        });
    }

    @Override
    public void onMapMoved() {
        // this method is also called from non-UI thread
        activity.runOnUiThread(new Runnable() {
            public void run() {
                Toast.makeText(activity, "onMapMoved", Toast.LENGTH_SHORT).show();
            }
        });
    }
}
