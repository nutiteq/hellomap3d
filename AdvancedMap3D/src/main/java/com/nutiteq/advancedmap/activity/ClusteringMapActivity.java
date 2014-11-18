package com.nutiteq.advancedmap.activity;

import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.os.Bundle;

import com.nutiteq.advancedmap.R;
import com.nutiteq.MapView;
import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.datasources.vector.ClusteringVectorDataSource;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.ui.MapListener;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectordatasources.QuadTreeVectorDataSource;
import com.nutiteq.vectorlayers.MarkerLayer;

/**
 * This is sample of marker clustering.
 * AdvancedLayers project contains virtual data source that is used in this sample
 */
public class ClusteringMapActivity extends Activity {

  private MapView mapView;


  @Override
  public void onCreate(Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);

      setContentView(R.layout.main);

      // enable logging for troubleshooting - optional
      Log.enableAll();
      Log.setTag("online3d");

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

      RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
      RasterLayer mapLayer = new RasterLayer(dataSource, 2);
      mapView.getLayers().setBaseLayer(mapLayer);


      // Rotterdam
      mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(4.480727f, 51.921098f));
      mapView.setZoom(4.0f);

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
      mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_BITMAP);
      mapView.getOptions().setBackgroundPlaneBitmap(
              UnscaledBitmapLoader.decodeResource(getResources(),
                      R.drawable.background_plane));
      mapView.getOptions().setClearColor(Color.WHITE);

      // configure texture caching - optional, suggested
      mapView.getOptions().setTextureMemoryCacheSize(20 * 1024 * 1024);
      mapView.getOptions().setCompressedMemoryCacheSize(8 * 1024 * 1024);

      // define online map persistent caching - optional, suggested. Default - no caching
      mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
      // set persistent raster cache limit to 100MB
      mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);

      addClusterLayer(new EPSG3857());
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

  void addClusterLayer(Projection proj) {
    // Create random markers
    QuadTreeVectorDataSource<Marker> markerSource = new QuadTreeVectorDataSource<Marker>(proj);
    
    MarkerStyle[] markerStyles = new MarkerStyle[] {
        MarkerStyle.builder().setBitmap(UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.marker_red)).setSize(0.5f).build(),
        MarkerStyle.builder().setBitmap(UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.marker_green)).setSize(0.5f).build(),
        MarkerStyle.builder().setBitmap(UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.marker_blue)).setSize(0.5f).build(),
    };

    for (int i = 0; i < 200; i++) {
      MapPos mapPos = new MapPos(Math.random() * proj.getBounds().getWidth() + proj.getBounds().left, Math.random() * proj.getBounds().getHeight() + proj.getBounds().bottom);
      MarkerStyle markerStyle = markerStyles[(int) Math.floor(Math.random() * 0.99 * markerStyles.length)];
      Label label = new DefaultLabel("Marker " + i);
      Marker marker = new Marker(mapPos, label, markerStyle, null);
      markerSource.add(marker);
    }
    
    // Create element merger for clustering.
    // Merger interface contains 2 methods:
    // a) for extracting element coordinate
    // b) for merging N elements and placing the resulting cluster element to specified position
    ClusteringVectorDataSource.ElementMerger<Marker> merger = new ClusteringVectorDataSource.ElementMerger<Marker>() {
      private Map<Integer, MarkerStyle> clusterStyles = new TreeMap<Integer, MarkerStyle>();

      @Override
      public MapPos getMapPos(Marker element) {
        // For marker cluster, the only sensible way to calculate element position is to use marker's position 
        return element.getMapPos();
      }

      @Override
      public synchronized Marker mergeElements(List<Marker> elements, MapPos mapPos) {
        // Create marker style, based on cluster size. Cache created styles.
        MarkerStyle clusterStyle = clusterStyles.get(elements.size());
        if (clusterStyle == null) {
          Bitmap markerBitmap = Bitmap.createBitmap(UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.marker_black));
          Bitmap canvasBitmap = markerBitmap.copy(Bitmap.Config.ARGB_8888, true);
          Canvas canvas = new Canvas(canvasBitmap); 
          Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
          paint.setTextAlign(Align.CENTER);
          paint.setTextSize(15);
          paint.setColor(0xff000000);
          paint.setStyle(android.graphics.Paint.Style.FILL);
          canvas.drawText(Integer.toString(elements.size()), markerBitmap.getWidth() / 2, markerBitmap.getHeight() / 2 - 5, paint);
          clusterStyle = MarkerStyle.builder().setBitmap(canvasBitmap).setSize(0.75f).build();
          clusterStyles.put(elements.size(), clusterStyle);
        }

        // Create marker for the cluster
        Marker marker = new Marker(mapPos, null, clusterStyle, null);
        return marker;
      }
    };
    
    // Create clustering data source. Use 0.1 for distance metrics (markers that are closer that 0.1 of the screen size are grouped) and start clustering from 2 elements.
    ClusteringVectorDataSource<Marker> clusterSource = new ClusteringVectorDataSource<Marker>(markerSource, 0.1f, 2, merger);
    
    // Create marker layer, use clustering data source
    MarkerLayer markerLayer = new MarkerLayer(clusterSource);
    mapView.getLayers().addLayer(markerLayer);
    
    // Create custom map listener for handling clicks on cluster markers
    mapView.getOptions().setMapListener(new MapListener() {
      @Override
      public void onMapMoved() {
      }

      @Override
      public void onMapClicked(double x, double y, boolean longClick) {
      }

      @Override
      public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
        if (vectorElement instanceof Marker && vectorElement.getLabel() == null) { // zoom-in if no label (cluster marker)
          // Animate, set focus point to the cluster center and zoom in
          mapView.setFocusPoint(((Marker) vectorElement).getMapPos(), 300);
          mapView.zoom(1.0f, 300);
        }
      }

      @Override
      public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
      }
    });
  }
}
