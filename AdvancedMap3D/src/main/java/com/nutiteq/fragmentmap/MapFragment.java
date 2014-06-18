package com.nutiteq.fragmentmap;

import java.util.ArrayList;
import java.util.List;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.Layer;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.MapListener;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

/**
 * 
 * This fragment is a demonstration how to embed MapView in a Fragment class and how to serialize the state.
 * The state serialization needs to be customized for each use case separately. Note: serialization of layer elements is not needed when
 * the elements are loaded from external data source, in that case the list of layers has to be simply rebuild in OnCreateView.
 * When elements are added dynamically, then custom serialization is needed. In this example we have a custom marker layer
 * and to serialize/restore each marker, we keep its state in MarkerState class. Actual markers are created in onActivityCreated method.
 *  
 * @author mtehver
 *
 */
public class MapFragment extends Fragment {

  /**
   * Interface for marker selection listener 
   */
  public interface OnMarkerSelectedListener {
    void onMarkerSelected(Marker marker, String info);
  }
  
  /**
   * Custom map event listener
   */
  private class MapEventListener extends MapListener {
    @Override
    public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
      if (vectorElement instanceof Marker) {
        selectMarker((Marker) vectorElement, true);
      }
    }

    @Override
    public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
    }

    @Override
    public void onMapClicked(final double x, final double y, final boolean longClick) {
    }

    @Override
    public void onMapMoved() {
    }
  }

  /**
   * State class for each marker, supports serialization to Bundle class
   */
  private class MarkerState {
    MapPos mapPos;
    String info;
    boolean selected;
    
    MarkerState(MapPos mapPos, String info) {
      this.mapPos = mapPos;
      this.info = info;
      this.selected = false;
    }
    
    MarkerState(Bundle bundle) {
      mapPos = new MapPos(bundle.getDoubleArray("mapPos"));
      info = bundle.getString("info");
      selected = bundle.getBoolean("selected");
    }
    
    Bundle saveState() {
      Bundle bundle = new Bundle();
      bundle.putDoubleArray("mapPos", mapPos.toArray());
      bundle.putString("info", info);
      bundle.putBoolean("selected", selected);
      return bundle;
    }
  }

  private MapView mapView;
  private Layer baseLayer;
  private MarkerLayer markerLayer;
  private Marker selectedMarker;
  private MarkerStyle normalMarkerStyle;
  private MarkerStyle selectedMarkerStyle;
  private List<MarkerState> markerStates = new ArrayList<MarkerState>();

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {	
    // MapView initialization
    mapView = new MapView(getActivity());
    mapView.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    mapView.setComponents(new Components());

    // Create base layer
    RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
    baseLayer = new RasterLayer(dataSource, 0);
    mapView.getLayers().setBaseLayer(baseLayer);
    
    // Create marker layer
    markerLayer = new MarkerLayer(baseLayer.getProjection());
    mapView.getLayers().addLayer(markerLayer);

    // Styles for markers
    normalMarkerStyle = MarkerStyle.builder().setSize(0.5f).setBitmap(
        UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker)
      ).build();
    selectedMarkerStyle = MarkerStyle.builder().setSize(0.65f).setColor(Color.RED).setBitmap(
        UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker)
      ).build();

    // Add event listener
    MapEventListener mapListener = new MapEventListener();
    mapView.getOptions().setMapListener(mapListener);

    // Activate some mapview options to make it smoother - optional
    mapView.getOptions().setPreloading(true);
    mapView.getOptions().setSeamlessHorizontalPan(true);
    mapView.getOptions().setTileFading(true);
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

    return mapView;
  }
  
  @Override
  public void onDestroyView() {
  	super.onDestroyView();
    
  	mapView.setComponents(null);
  	selectedMarker = null;
    normalMarkerStyle = null;
    selectedMarkerStyle = null;
  	baseLayer = null;
  	markerLayer = null;
  	mapView = null;
  }

  @Override
  public void onStart() {
    super.onStart();
    mapView.startMapping();
  }

  @Override
  public void onStop() {
    mapView.stopMapping();
    super.onStop();
  }
  
  @Override
  public void onSaveInstanceState(Bundle state) {
    super.onSaveInstanceState(state);

    // Save camera/viewpoint state
    state.putDoubleArray("focusPoint", mapView.getFocusPoint().toArray());
    state.putFloat("zoom", mapView.getZoom());
    state.putFloat("rotation", mapView.getMapRotation());
    state.putFloat("tilt", mapView.getTilt());

    // Save markers
    Bundle markersBundle = new Bundle();
    for (int i = 0; i < markerStates.size(); i++) {
      MarkerState markerState = markerStates.get(i);
      markersBundle.putBundle("" + i, markerState.saveState());
    }
    state.putBundle("markers", markersBundle);
  }
  
  @Override
  public void onActivityCreated(Bundle savedInstanceState) {
    super.onActivityCreated(savedInstanceState);

    if (savedInstanceState != null) {
      // Restore camera state
      mapView.setFocusPoint(new MapPos(savedInstanceState.getDoubleArray("focusPoint")));
      mapView.setZoom(savedInstanceState.getFloat("zoom"));
      mapView.setMapRotation(savedInstanceState.getFloat("rotation"));
      mapView.setTilt(savedInstanceState.getFloat("tilt"));

      // Restore markers
      Bundle markersBundle = savedInstanceState.getBundle("markers");
      for (int i = 0; i < markersBundle.size(); i++) {
        MarkerState markerState = new MarkerState(markersBundle.getBundle("" + i));
        markerStates.add(markerState);
      }
    } else {
      // Initialize camera state 
      mapView.setFocusPoint(baseLayer.getProjection().fromWgs84(25.4426f, 42.7026f));
      mapView.setZoom(8.0f);
      mapView.setMapRotation(0f);
      mapView.setTilt(90.0f);

      // Create random markers, centered around focus point
      MapPos focusPoint = mapView.getFocusPoint();
      for (int i = 0; i < 20; i++) {
        MapPos mapPos = new MapPos(focusPoint.x + (Math.random() - 0.5f) * 100000, focusPoint.y + (Math.random() - 0.5f) * 100000);
        MapPos wgs84 = mapView.getLayers().getBaseProjection().toWgs84(mapPos.x, mapPos.y);
        String info = "Marker " + i + "\nWGS84 " + String.format("%.4f, %.4f", wgs84.x, wgs84.y);
        markerStates.add(new MarkerState(mapPos, info));
      }
    }

    // Initialize marker layer based on markerStates list
    for (MarkerState markerState : markerStates) {
      Marker marker = new Marker(markerState.mapPos, null, normalMarkerStyle, markerState);
      markerLayer.add(marker);
      if (markerState.selected) {
        selectMarker(marker, false);
      }
    }
  }

  protected void selectMarker(Marker marker, boolean updateListener) {
    if (selectedMarker == marker) {
      return;
    }
    if (selectedMarker != null) {
      MarkerState selectedMarkerState = (MarkerState) selectedMarker.userData;
      selectedMarkerState.selected = false;
      selectedMarker.setStyle(normalMarkerStyle);
    }
    MarkerState markerState = (MarkerState) marker.userData; 
    markerState.selected = true;
    marker.setStyle(selectedMarkerStyle);
    selectedMarker = marker;
    mapView.selectVectorElement(marker);
    OnMarkerSelectedListener listener = (OnMarkerSelectedListener) getActivity();
    if (updateListener && listener != null) {
      listener.onMarkerSelected(marker, markerState.info);
    }
  }
}
