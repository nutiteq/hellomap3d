package com.nutiteq.advancedmap.maplisteners;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.location.Location;

import com.nutiteq.MapView;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Point3D;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.projections.Projection;
import com.nutiteq.renderprojections.RenderProjection;
import com.nutiteq.ui.MapListener;
import com.nutiteq.utils.Const;

/**
 * 
 * This MapListener demonstrates usage of OpenGL-level API to show animations on map
 * 
 * @author jaak
 *
 */
public class MyLocationMapEventListener extends MapListener {

    private Activity activity;
    private MapView mapView;
    private MyLocationCircle locationCircle;

    // activity is often useful to handle click events 
    public MyLocationMapEventListener(Activity activity, MapView mapView) {
        this.activity = activity;
        this.mapView = mapView;
        
        this.locationCircle = new MyLocationCircle();
        
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
    //    Toast.makeText(activity, "onLabelClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
     //   Toast.makeText(activity, "onVectorElementClicked "+((DefaultLabel) vectorElement.getLabel()).getTitle()+" longClick: "+longClick, Toast.LENGTH_SHORT).show();

    }

    // Map View manipulation handlers
    @Override
    public void onMapClicked(final double x, final double y, final boolean longClick) {
        // x and y are in base map projection, we convert them to the familiar WGS84 
    //     Toast.makeText(activity, "onMapClicked "+(new EPSG3857()).toWgs84(x, y).x+" "+(new EPSG3857()).toWgs84(x, y).y+" longClick: "+longClick, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onMapMoved() {
        // this method is also called from non-UI thread
    }

    public static class MyLocationCircle {
        private static final int NR_OF_CIRCLE_VERTS = 24;
        private FloatBuffer circleVertBuf;
        private float[] circleLocalFrameMatrix = new float[16];
        private Point3D circlePos = new Point3D(0, 0, 0);
        private float circleRadius;
        private float circleScale;
        private float circleAlpha = 1.0f;
        private boolean visible = false;
        
        MyLocationCircle() {
            // Create circle vertex array for later use in drawing
            ByteBuffer byteBuffer = ByteBuffer
                    .allocateDirect((NR_OF_CIRCLE_VERTS + 2) * 3 * Float.SIZE / 8);
            byteBuffer.order(ByteOrder.nativeOrder());
            circleVertBuf = byteBuffer.asFloatBuffer();
            float degreesPerVert = 360.0f / NR_OF_CIRCLE_VERTS;
            circleVertBuf.put(0);
            circleVertBuf.put(0);
            circleVertBuf.put(0);
            for (float tsj = 0; tsj < 360; tsj += degreesPerVert) {
                circleVertBuf.put((float) Math.cos(tsj * Const.DEG_TO_RAD));
                circleVertBuf.put((float) Math.sin(tsj * Const.DEG_TO_RAD));
                circleVertBuf.put(0);
            }
            circleVertBuf.put(1);
            circleVertBuf.put(0);
            circleVertBuf.put(0);
            circleVertBuf.position(0);
        }
        
        /**
         * Draw circle, called for each frame
         * @param gl OpenGL context
         * @param zoomPow2 Zoom level in power of 2, to calculate easily fixed size on map
         */
        public void draw(GL10 gl, float zoomPow2){
            if(!visible){
                return;
            }

            // circle max radius 
            // make sure that it is at least minimum radius, otherwise is too small in general zoom
            float circleScaleMax = Math.max(
                    Const.UNIT_SIZE * circleRadius / 7500000f, // based on GPS accuracy. This constant depends on latitude
                    Const.UNIT_SIZE / zoomPow2 * 0.2f); // minimum, fixed value
            float circleScaleStep = circleScaleMax / 100.0f;
            float circleAlphaStep = -circleScaleStep
                    / (circleScaleMax - circleScaleStep);
            
            gl.glBindTexture(GL10.GL_TEXTURE_2D, 0);
            
            // Colour is yellow (R=1,G=1,B=0)
            gl.glColor4f(1, 1, 0, circleAlpha);

            gl.glVertexPointer(3, GL10.GL_FLOAT, 0, circleVertBuf);

            gl.glPushMatrix();
            gl.glTranslatef((float) circlePos.x, (float) circlePos.y, (float) circlePos.z);
            gl.glMultMatrixf(circleLocalFrameMatrix, 0);
            
            gl.glScalef(circleScale , circleScale , 1);
            gl.glDrawArrays(GL10.GL_TRIANGLE_FAN, 0, NR_OF_CIRCLE_VERTS + 2);
            gl.glPopMatrix();

            // circleScale is size of circle for current frame,
            // in range of 1...circleScaleMax, step circleScaleStep
            // also drawing transparency (alpha channel) is reduced for larger circles
            circleScale += circleScaleStep;
            circleAlpha += circleAlphaStep;
            if (circleScale > circleScaleMax) {
                circleScale = 0.0f;
                circleAlpha = 1.0f;
            }       
        }

        public void setVisible(boolean visible) {
            this.visible = visible;
        }

        public void setLocation(Projection proj, RenderProjection renderProj, Location location) {
            MapPos mapPos = proj.fromWgs84(location.getLongitude(), location.getLatitude());
            MapPos mapPosInternal = proj.toInternal(mapPos.x, mapPos.y);
            circlePos = renderProj.project(mapPosInternal);
            circleRadius = location.getAccuracy();
            double[] localFrameMatrix = renderProj.getLocalFrameMatrix(circlePos);
            for (int i = 0; i < 16; i++) {
              circleLocalFrameMatrix[i] = (float) localFrameMatrix[i];
            }
        }
    }

    public MyLocationCircle getLocationCircle() {
        return locationCircle;
    }
}


