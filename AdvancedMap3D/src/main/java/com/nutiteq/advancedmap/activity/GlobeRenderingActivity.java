package com.nutiteq.advancedmap.activity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ToggleButton;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.geometry.Polygon3D;
import com.nutiteq.geometry.Text;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.TextStyle;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.MarkerLayer;
import com.nutiteq.vectorlayers.Polygon3DLayer;
import com.nutiteq.vectorlayers.TextLayer;

/**
 * This is an example of Nutiteq 3D globe rendering
 *  
 * @author mark
 * 
 */
public class GlobeRenderingActivity extends Activity {

  public MapView mapView;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.globe);

    // enable logging for troubleshooting - optional
    Log.enableAll();
    Log.setTag("globerendering");

    // 1. Get the MapView from the Layout xml - mandatory
    mapView = (MapView) findViewById(R.id.mapView);

    // Optional, but very useful: restore map state during device rotation,
    // it is saved in onRetainNonConfigurationInstance() below
    Components retainObject = (Components) getLastNonConfigurationInstance();
    if (retainObject != null) {
      // just restore configuration and update listener, skip other
      // initializations
      mapView.setComponents(retainObject);

      setBackdropImage();
      setButtonListener();

      mapView.startMapping();
      return;
    } else {
      // 2. create and set MapView components - mandatory
      mapView.setComponents(new Components());
    }

    // 3. Define map layer for basemap - mandatory.
    RasterDataSource rasterDataSource = new HTTPRasterDataSource(new EPSG4326(), 0, 19, "http://www.staremapy.cz/naturalearth/{zoom}/{x}/{yflipped}.png");
    RasterLayer mapLayer = new RasterLayer(rasterDataSource, 1508);
    mapView.getLayers().setBaseLayer(mapLayer);

    // set initial map view camera - optional. "World view" is default
    // Location: San Francisco
    // NB! it must be in base layer projection (EPSG3857), so we convert it
    // from lat and long
    mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 0));
    mapView.setMapRotation(0f);
    mapView.setZoom(2.0f);

    // Activate some mapview options to make it smoother - optional
    mapView.getOptions().setPreloading(true);
    mapView.getOptions().setSeamlessHorizontalPan(true);
    mapView.getOptions().setTileFading(true);
    mapView.getOptions().setKineticPanning(true);
    mapView.getOptions().setDoubleClickZoomIn(true);
    mapView.getOptions().setDualClickZoomOut(true);
    mapView.getOptions().setRasterTaskPoolSize(2);
    //mapView.getOptions().setTileZoomLevelBias(0.3f);

    // configure texture caching - optional, suggested
    mapView.getOptions().setTextureMemoryCacheSize(20 * 1024 * 1024);
    mapView.getOptions().setCompressedMemoryCacheSize(4 * 1024 * 1024);

    // define online map persistent caching - optional, suggested.
    mapView.getOptions().setPersistentCacheSize(20 * 1024 * 1024);

    // Set backdrop image
    setBackdropImage();
    
    // Set up button listener for plane/globe mode switching
    setButtonListener();

    // Add custom layers 
    addMarkerLayer();
    addTextLayer();
    addPointLayer();
    addLineLayer();
    addPolyLayer();
    addPoly3DLayer();
  }

  private void setBackdropImage() {
    // Create starry image with (approximately) same resolution as view
    DisplayMetrics displaymetrics = new DisplayMetrics();
    getWindowManager().getDefaultDisplay().getMetrics(displaymetrics);
    int height = displaymetrics.heightPixels / 2;
    int width = displaymetrics.widthPixels / 2;
    Bitmap backgroundBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(backgroundBitmap);
    canvas.drawRGB(0, 0, 0);
    Paint paint = new Paint();
    paint.setColor(Color.WHITE);
    for (int i = 0; i < 200; i++) {
      int x = (int) (Math.random() * canvas.getWidth());
      int y = (int) (Math.random() * canvas.getHeight());
      canvas.drawCircle(x, y, (float) Math.random() * 1.25f, paint);
    }
    
    // Set the image as backdrop
    mapView.getOptions().setBackgroundImageDrawMode(Options.DRAW_BACKDROP_BITMAP);
    mapView.getOptions().setBackgroundImageBitmap(backgroundBitmap);
  }

  private void setButtonListener() {
    ToggleButton toggle = (ToggleButton) findViewById(R.id.toggleProjection);
    mapView.getOptions().setRenderProjection(toggle.isChecked() ? Options.SPHERICAL_RENDERPROJECTION : Options.PLANAR_RENDERPROJECTION);

    toggle.setOnCheckedChangeListener(new OnCheckedChangeListener() {
      @Override
      public void onCheckedChanged(CompoundButton button, boolean set) {
        mapView.getOptions().setRenderProjection(set ? Options.SPHERICAL_RENDERPROJECTION : Options.PLANAR_RENDERPROJECTION);
      }
    });
  }

  private void addPointLayer() {
    GeometryLayer geoLayer = new GeometryLayer(new EPSG3857());
    mapView.getComponents().layers.addLayer(geoLayer);
    Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(
        getResources(), R.drawable.point);
    Label pointLabel = new DefaultLabel("Point","Here is a point");
    PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker).setColor(Color.GREEN).build();
    MapPos pointLocation = mapView.getComponents().layers.getBaseProjection().fromWgs84(-149.416667f, 37.766667f);
    Point point = new Point(pointLocation, pointLabel, pointStyle, null);
    geoLayer.add(point);
  }

  private void addLineLayer() {
    GeometryLayer geoLayer = new GeometryLayer(new EPSG4326());
    mapView.getComponents().layers.addLayer(geoLayer);
    LineStyle lineStyle = LineStyle.builder().setLineJoinMode(LineStyle.ROUND_LINEJOIN).build();
    Label label = new DefaultLabel("Line", "Here is a line");
    List<MapPos> posList = new ArrayList<MapPos>(Arrays.asList(
        new MapPos(-90, 30),
        new MapPos(90, 30)
        ));

    Line line = new Line(posList, label, lineStyle, null);
    geoLayer.add(line);
  }

  private void addPolyLayer() {
    GeometryLayer geoLayer = new GeometryLayer(new EPSG4326());
    mapView.getComponents().layers.addLayer(geoLayer);
    PolygonStyle polyStyle = PolygonStyle.builder().build();
    MapPos origin = new MapPos(0, -70);
    List<MapPos> posList = new ArrayList<MapPos>(Arrays.asList(
        new MapPos(origin.x - 80, origin.y + 30),
        new MapPos(origin.x, origin.y),
        new MapPos(origin.x + 90, origin.y + 30)
        ));
    List<MapPos> holePosList = new ArrayList<MapPos>(Arrays.asList(
        new MapPos(origin.x, origin.y + 15000),
        new MapPos(origin.x - 150000, origin.y + 210000),
        new MapPos(origin.x + 150000, origin.y + 210000)
        ));
    List<List<MapPos>> holes = new ArrayList<List<MapPos>>();
    holes.add(holePosList);
    Label label = new DefaultLabel("Poly", "Here is a polygon");
    Polygon poly = new Polygon(posList, null, label, polyStyle, null);
    geoLayer.add(poly);
  }

  private void addPoly3DLayer() {
    Polygon3DLayer geoLayer = new Polygon3DLayer(new EPSG3857());
    mapView.getComponents().layers.addLayer(geoLayer);
    Polygon3DStyle polyStyle = Polygon3DStyle.builder().build();
    MapPos origin = mapView.getComponents().layers.getBaseProjection().fromWgs84(129.416667f, 37.766667f);
    List<MapPos> posList = new ArrayList<MapPos>(Arrays.asList(
        new MapPos(origin.x, origin.y),
        new MapPos(origin.x - 3000000, origin.y + 1500000),
        new MapPos(origin.x + 3000000, origin.y + 1500000)
        ));
    List<MapPos> holePosList = new ArrayList<MapPos>(Arrays.asList(
        new MapPos(origin.x, origin.y + 15000),
        new MapPos(origin.x - 150000, origin.y + 210000),
        new MapPos(origin.x + 150000, origin.y + 210000)
        ));
    List<List<MapPos>> holes = new ArrayList<List<MapPos>>();
    holes.add(holePosList);
    Label label = new DefaultLabel("Poly3D", "Here is a polygon3D");
    Polygon3D poly = new Polygon3D(posList, holes, 5000, label, polyStyle, null);
    geoLayer.add(poly);
  }

  private void addMarkerLayer() {
    Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
    MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.5f).setAllowOverlap(false).setColor(Color.WHITE).build();
    Label markerLabel = new DefaultLabel("San Francisco", "Here is a marker");
    MapPos markerLocation = mapView.getComponents().layers.getBaseProjection().fromWgs84(-122.416667f, 80.766667f);
    MarkerLayer markerLayer = new MarkerLayer(mapView.getComponents().layers.getBaseProjection());
    Marker marker = new Marker(markerLocation, markerLabel, markerStyle, null);
    markerLayer.add(marker);
    mapView.getLayers().addLayer(markerLayer);
  }

  private void addTextLayer() {
    TextLayer textLayer = new TextLayer(mapView.getComponents().layers.getBaseProjection());
    mapView.getComponents().layers.addLayer(textLayer);
    TextStyle textStyle = TextStyle.builder().setOrientation(MarkerStyle.GROUND_ORIENTATION).setAllowOverlap(false).setSize(30).build();
    MapPos origin = mapView.getComponents().layers.getBaseProjection().fromWgs84(-129.416667f, 17.766667f);
    Text text = new Text(origin, "Text", textStyle, null);
    textLayer.add(text);
  }

  @Override
  public Object onRetainNonConfigurationInstance() {
    Log.debug("onRetainNonConfigurationInstance");
    return this.mapView.getComponents();
  }

  @Override
  protected void onStart() {
    super.onStart();
    // 4. Start the map - mandatory.
    mapView.startMapping();
  }

  @Override
  protected void onStop() {
    super.onStop();
    mapView.stopMapping();
  }

  @Override
  protected void onDestroy() {
    super.onDestroy();
  }

}
