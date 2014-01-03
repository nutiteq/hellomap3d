package com.nutiteq.hellomap;

import java.util.ArrayList;
import java.util.List;

import android.location.Location;

import com.nutiteq.components.Color;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Line;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.utils.Const;
import com.nutiteq.vectorlayers.GeometryLayer;

public class MyLocationCircle {
    private static final int NR_OF_CIRCLE_VERTS = 18;

    private final GeometryLayer layer;
    private Line circle = null;
    private List<MapPos> circleVerts = new ArrayList<MapPos>(NR_OF_CIRCLE_VERTS);
    private MapPos circlePos = new MapPos(0, 0);
    private float circleScale = 0;
    private float circleRadius = 1;
    private float projectionScale = 0;
    private boolean visible = false;
    
    MyLocationCircle(GeometryLayer layer) {
        this.layer = layer;
    }
    
    public void update(float zoom) {
        // circle max radius 
        // make sure that it is at least minimum radius, otherwise is too small in general zoom
        float zoomPow2 = (float) Math.pow(2, zoom);
        float circleScaleMax = Math.max(
            circleRadius / 7500000f, // based on GPS accuracy. This constant depends on latitude
            1.0f / zoomPow2 * 0.2f) * projectionScale; // minimum, fixed value
        float circleScaleStep = circleScaleMax / 50.0f;

        circleScale += circleScaleStep;
        if (circleScale > circleScaleMax) {
            circleScale = 0.0f;
        }

        // Build closed circle
        circleVerts.clear();
        for (float tsj = 0; tsj <= 360; tsj += 360 / NR_OF_CIRCLE_VERTS) {
            MapPos mapPos = new MapPos(circleScale * Math.cos(tsj * Const.DEG_TO_RAD) + circlePos.x, circleScale * Math.sin(tsj * Const.DEG_TO_RAD) + circlePos.y);
            circleVerts.add(mapPos);
        }

        // Create/update line
        if (circle == null) {
            LineStyle style = LineStyle.builder().setWidth(0.1f).setColor(Color.argb(192, 255, 255, 0)).build();
            circle = new Line(circleVerts, null, style, null);
            layer.add(circle);
        } else {
            circle.setVertexList(circleVerts);
        }
        circle.setVisible(visible);
    }

    public void setVisible(boolean visible) {
        this.visible = visible;
    }

    public void setLocation(Projection proj, Location location) {
        circlePos = proj.fromWgs84(location.getLongitude(), location.getLatitude());
        projectionScale = (float) proj.getBounds().getWidth();
        circleRadius = location.getAccuracy();
    }
}