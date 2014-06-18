package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.datasources.vector.WFSTextVectorDataSource;
import com.nutiteq.datasources.vector.WFSVectorDataSource;
import com.nutiteq.datasources.vector.WFSVectorDataSource.Feature;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.style.TextStyle;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.TextLayer;

/**
 * 
 * Demonstrates usage of two layers/data sources: 
 *      Geometry layer - uses data source with online vector API to WFS server
 *      Text Layer     - uses same data source as geometry layer, reads element description (text) from vector element meta data
 * 
 * It uses one predefined URL and code-defined style for labels. Road labels are rotated to match with roads,
 * amenity labels are horizontal. The texts are configured to resolve automatically overlapping.
 * 
 * @author jaak
 *
 */
public class WfsMapActivity extends Activity {

    private MapView mapView;
    private float dpi;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        dpi = metrics.density;

        setContentView(R.layout.main);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("wfs");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (MapView) findViewById(R.id.mapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            // set stereo view: works if you rotate to landscape and device has HTC 3D or LG Real3D
            mapView.setComponents(components);
        }

        // 3. Define map layer for basemap - mandatory.
        
        // create styles
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);

        final StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>(
                LineStyle.builder().setWidth(0.04f)
                    .setColor(0xFFAAAAAA).build());

        final StyleSet<PointStyle> pointStyleSet = new StyleSet<PointStyle>(
                PointStyle.builder().setBitmap(pointMarker)
                    .setSize(0.05f).setColor(Color.BLUE).setPickingSize(0.2f).build());

        // add WFS layer as base layer
        String layers = "osm:osm_mainroads_gen1,osm:osm_amenities,osm:osm_roads";
        String wfsUrl = "http://kaart.maakaart.ee/geoserver/osm/ows?service=WFS&version=1.0.0&request=GetFeature&typeName=" + layers + "&maxFeatures=1500";

        WFSVectorDataSource wfsDataSource = new WFSVectorDataSource(new EPSG3857(), wfsUrl) {

            @Override
            protected Label createLabel(Feature feature) {
                return new DefaultLabel(feature.properties.name, "OSM Id: " + feature.properties.osm_id + " type:" + feature.properties.type);
            }

            @Override
            protected StyleSet<PointStyle> createPointFeatureStyleSet(Feature feature, int zoom) {
                return pointStyleSet;
            }

            @Override
            protected StyleSet<LineStyle> createLineFeatureStyleSet(Feature feature, int zoom) {
                return lineStyleSet;
            }

        };

        GeometryLayer wfsLayer = new GeometryLayer(wfsDataSource);
        mapView.getLayers().setBaseLayer(wfsLayer);

        // add label layer for WFS streets

        // 1. define style callback for labels
        WFSTextVectorDataSource wfsTextDataSource = new WFSTextVectorDataSource(wfsDataSource) {

            private StyleSet<TextStyle> styleSetRoad = new StyleSet<TextStyle>(
                    TextStyle.builder().setAllowOverlap(false)
                        .setOrientation(TextStyle.GROUND_ORIENTATION)
                        .setAnchorY(TextStyle.CENTER)
                        .setSize((int) (25 * dpi)).build());

            private StyleSet<TextStyle> styleSetAmenity = new StyleSet<TextStyle>(
                    TextStyle.builder().setAllowOverlap(false)
                        .setOrientation(TextStyle.CAMERA_BILLBOARD_ORIENTATION)
                        .setSize((int) (30 * dpi))
                        .setColor(Color.argb(255, 100, 100, 100))
                        .setPlacementPriority(5).build());

            @Override
            protected StyleSet<TextStyle> createFeatureStyleSet(Feature feature, int zoom) {
                if (feature.geometry.type.equals("Point")) {
                    return styleSetAmenity;
                }
                return styleSetRoad;
            }

        };
        
        TextLayer wfsTextLayer = new TextLayer(wfsTextDataSource);

        // 2. set properties for texts
        wfsTextDataSource.setMaxElements(30);
        wfsTextLayer.setZOrdered(false);
        wfsTextLayer.setTextFading(true);

        // 3. add layer
        mapView.getLayers().addLayer(wfsTextLayer);

        // Location: San Francisco
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.416667f, 37.766667f));
        mapView.setFocusPoint(new MapPos(2753791.3f, 8275296.0f)); // Tallinn

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(16.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
        mapView.setTilt(40.0f);


        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(false);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(false);
        mapView.getOptions().setKineticPanning(true);
        mapView.getOptions().setDoubleClickZoomIn(true);
        mapView.getOptions().setDualClickZoomOut(true);

        // set sky bitmap - optional, default - white
        mapView.getOptions().setSkyDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setSkyOffset(4.86f);
        mapView.getOptions().setSkyBitmap(
                UnscaledBitmapLoader.decodeResource(getResources(),
                        R.drawable.sky_small));

        // Map background, visible if no map tiles loaded - optional, default - white
        mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_COLOR);
        mapView.getOptions().setBackgroundPlaneColor(Color.WHITE);
        mapView.getOptions().setClearColor(Color.WHITE);

        // configure texture caching - optional, suggested
        mapView.getOptions().setTextureMemoryCacheSize(20 * 1024 * 1024);
        mapView.getOptions().setCompressedMemoryCacheSize(8 * 1024 * 1024);

        // define online map persistent caching - optional, suggested. Default - no caching
        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache_wms").getPath());
        // set persistent raster cache limit to 100MB
        mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);

        // 4. zoom buttons using Android widgets - optional
        // get the zoomcontrols that was defined in main.xml
        ZoomControls zoomControls = (ZoomControls) findViewById(R.id.zoomcontrols);

        // set zoomcontrols listeners to enable zooming
        zoomControls.setOnZoomInClickListener(new View.OnClickListener() {
            public void onClick(final View v) {
                mapView.zoomIn();
            }
        });
        zoomControls.setOnZoomOutClickListener(new View.OnClickListener() {
            public void onClick(final View v) {
                mapView.zoomOut();
            }
        });

    }

    @Override
    protected void onStart() {
        mapView.startMapping();
        super.onStart();
    }

    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        Log.debug("onRetainNonConfigurationInstance");
        return this.mapView.getComponents();
    }

    public MapView getMapView() {
        return mapView;
    }

}
