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
import com.nutiteq.components.Range;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.Text;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.style.TextStyle;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.GeometryLayer;
import com.nutiteq.vectorlayers.MarkerLayer;
import com.nutiteq.vectorlayers.TextLayer;

/**
 * This is an example of Nutiteq 3D globe rendering.
 * Used layers:
 *  raster with TMS data source (EPSG4326 projection as EPSG3857 does not define pole area)
 *  marker, text, geometry
 *  
 * @author mark
 * 
 */
public class GlobeRenderingActivity extends Activity {

    private static final int MINOR_ZOOM = 4;
    public MapView mapView;
    private StyleSet<TextStyle> textStyleGeneral;
    private StyleSet<TextStyle> textStyleMinor;
    private TextLayer textLayer;
    private GeometryLayer gridLayer;

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
            // Add custom layers 
            addMarkerLayer();
            initTextLayer();
            addLineGridLayer();
            
            setBackdropImage();
            setButtonListener();

            mapView.startMapping();
            return;
        } else {
            // 2. create and set MapView components - mandatory
            mapView.setComponents(new Components());
        }

        // 3. Define map layer for basemap - mandatory.
//      RasterDataSource rasterDataSource = new HTTPRasterDataSource(new EPSG4326(), 0, 5, "http://www.staremapy.cz/naturalearth/{zoom}/{x}/{yflipped}.png");
        RasterDataSource rasterDataSource = new HTTPRasterDataSource(new EPSG4326(), 0, 19, "http://kaart.maakaart.ee/osm/tms/1.0.0/osm_noname_st_EPSG4326/{zoom}/{x}/{yflipped}.png");
        RasterLayer mapLayer = new RasterLayer(rasterDataSource, 1508);
        mapView.getLayers().setBaseLayer(mapLayer);

        // set initial map view camera - optional. "World view" is default
        // Location: San Francisco
        // NB! it must be in base layer projection (EPSG3857), so we convert it
        // from lat and long
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 37.76666666666f));
        mapView.setMapRotation(0f);
        mapView.setZoom(2.0f);

        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(true);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(true);
        mapView.getOptions().setKineticPanning(true);
        mapView.getOptions().setDoubleClickZoomIn(true);
        mapView.getOptions().setDualClickZoomOut(true);
        mapView.getOptions().setRasterTaskPoolSize(4);
        mapView.getConstraints().setZoomRange(new Range(0.0f, 19.0f));
        mapView.getOptions().setTileZoomLevelBias(0.3f);

        // Map background, visible if no map tiles loaded - optional, default - white
        mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setBackgroundPlaneBitmap(
                UnscaledBitmapLoader.decodeResource(getResources(),
                        R.drawable.background_plane));

        // configure texture caching - optional, suggested
        mapView.getOptions().setTextureMemoryCacheSize(20 * 1024 * 1024);
        mapView.getOptions().setCompressedMemoryCacheSize(4 * 1024 * 1024);

        // define online map persistent caching - optional, suggested.
        mapView.getOptions().setPersistentCacheSize(20 * 1024 * 1024);

        // Set backdrop image
        setBackdropImage();

        // Add custom layers 
        addMarkerLayer();
        initTextLayer();
        addLineGridLayer();
        
        // Set up button listener for plane/globe mode switching
        setButtonListener();

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
        ToggleButton toggleProjection = (ToggleButton) findViewById(R.id.toggleProjection);
        mapView.getOptions().setRenderProjection(toggleProjection.isChecked() ? Options.SPHERICAL_RENDERPROJECTION : Options.PLANAR_RENDERPROJECTION);

        toggleProjection.setOnCheckedChangeListener(new OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton button, boolean set) {
                mapView.getOptions().setRenderProjection(set ? Options.SPHERICAL_RENDERPROJECTION : Options.PLANAR_RENDERPROJECTION);
            }
        });
        
        ToggleButton toggleGrid = (ToggleButton) findViewById(R.id.toggleGrid);

        gridLayer.setVisible(toggleGrid.isChecked());
        textLayer.setVisible(toggleGrid.isChecked());
        
        toggleGrid.setOnCheckedChangeListener(new OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton button, boolean set) {
                gridLayer.setVisible(set);
                textLayer.setVisible(set);
                mapView.requestRender();
            }
        });
        
    }

    private void addLineGridLayer() {
        gridLayer = new GeometryLayer(new EPSG4326());
        mapView.getComponents().layers.addLayer(gridLayer);
        LineStyle lineStyleNormal = LineStyle.builder().setLineJoinMode(LineStyle.ROUND_LINEJOIN).build();
        StyleSet<LineStyle> lineStyleNormalSet = new StyleSet<LineStyle> (lineStyleNormal);

        
        LineStyle lineStyleSpecial = LineStyle.builder()
                .setColor(android.graphics.Color.YELLOW)
                .setLineJoinMode(LineStyle.ROUND_LINEJOIN)
                .build();
        StyleSet<LineStyle> lineStyleSpecialSet = new StyleSet<LineStyle> (lineStyleSpecial);
        
        LineStyle lineStyleMinor = LineStyle.builder()
                .setWidth(0.05f)
                .build();
        
        StyleSet<LineStyle> lineStyleMinorSet = new StyleSet<LineStyle> ();
        lineStyleMinorSet.setZoomStyle(MINOR_ZOOM, lineStyleMinor);
        
        // draw degree meridians
        for(int lon = -180; lon < 180; lon += 5){
            String labelString = ""+Math.abs(lon)+" "+(lon > 0 ? "E" : "W");
            Label label = new DefaultLabel(labelString);
            List<MapPos> posList = new ArrayList<MapPos>(Arrays.asList(
                    new MapPos(lon, -85),
                    new MapPos(lon, 85)
                    ));

            StyleSet<LineStyle> lineStyle;
            StyleSet<TextStyle> textStyle;
            
            if((lon % 10) == 0){
                // is dividable by 10
                if(lon==0 || lon == -180){
                    lineStyle = lineStyleSpecialSet;
                }else{
                    lineStyle = lineStyleNormalSet;
                }
                textStyle = textStyleGeneral;
            }else{
                lineStyle = lineStyleMinorSet;
                textStyle = textStyleMinor;
            }
                
            Line line = new Line(posList, label, lineStyle, null);
            
            gridLayer.add(line);
            
            addText(posList, labelString, textStyle);

        }
        
        // draw parallels
        for(int lat = -85; lat<90; lat+=5){
            
            String labelString = "";
            if(lat == 0){
                labelString = "Equator";
            }else{
                labelString = ""+Math.abs(lat)+" "+(lat < 0 ? "S" : "N");
            }
            
            Label label = new DefaultLabel(labelString);
            
            List<MapPos> posList = new ArrayList<MapPos>();
            for (int lon = -180; lon <=180; lon += 5){
                posList.add(new MapPos(lon,lat));
            }

            StyleSet<LineStyle> lineStyle;
            StyleSet<TextStyle> textStyle;
            
            
            if((lat % 10) == 0){
              // is dividable by 10
                if(lat==0){
                    lineStyle = lineStyleSpecialSet;
                }else{
                    lineStyle = lineStyleNormalSet;
                }
                textStyle = textStyleGeneral;
            }else{
                lineStyle = lineStyleMinorSet;
                textStyle = textStyleMinor;
            }
            
            Line line = new Line(posList, label, lineStyle, null);
            gridLayer.add(line);

            addText(posList, labelString, textStyle);
            
        }        
        
        // Tropic lines
       for(int i = -1; i <= 1; i+=2){
            // lat is from http://www.neoprogrammics.com/obliquity_of_the_ecliptic/
           double lat = i * 23.4374255393; 
           String labelString = "Tropic of "+(lat < 0 ? "Capricorn" : "Cancer");
           Label label = new DefaultLabel(labelString);
           List<MapPos> posList = new ArrayList<MapPos>();
           for (int lon = -180; lon <=180; lon += 5){
               posList.add(new MapPos(lon,lat));
           }
           Line line = new Line(posList, label, lineStyleSpecial, null);
           gridLayer.add(line);
           addText(posList, labelString, textStyleGeneral);
       }

       // Polar lines
      for(int i = -1; i <= 1; i+=2){
          double lat = i * 66.56; // in degrees
          String labelString = (lat > 0 ? "Actic circle" : "Antarctic circle");
          Label label = new DefaultLabel(labelString);
          List<MapPos> posList = new ArrayList<MapPos>();
          for (int lon = -180; lon <=180; lon += 5){
              posList.add(new MapPos(lon,lat));
          }
          Line line = new Line(posList, label, lineStyleSpecial, null);
          gridLayer.add(line);
          addText(posList, labelString, textStyleGeneral);
      }

    }


    private void addText(List<MapPos> posList, String labelString, StyleSet<TextStyle> style) {
        Text text = new Text(new Text.BaseLine(posList), labelString, style, null);
        textLayer.add(text);
        
    }

    private void addMarkerLayer() {
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
        MarkerStyle markerStyle = MarkerStyle.builder()
                .setBitmap(pointMarker)
                .setSize(0.5f)
                .setAllowOverlap(false)
                .setPlacementPriority(2)
                .setColor(Color.WHITE)
                .build();
        MarkerLayer markerLayer = new MarkerLayer(mapView.getComponents().layers.getBaseProjection());
        
        Marker poleS = new Marker(
                mapView.getComponents().layers.getBaseProjection().fromWgs84(0f, -90f), 
                new DefaultLabel("South Pole"), markerStyle, null);
        markerLayer.add(poleS);
        
        Marker poleN = new Marker(
                mapView.getComponents().layers.getBaseProjection().fromWgs84(0f, 90f), 
                new DefaultLabel("North Pole"), markerStyle, null);
        markerLayer.add(poleN);
     
        mapView.getLayers().addLayer(markerLayer);
    }

    private void initTextLayer() {
        
        textLayer = new TextLayer(mapView.getComponents().layers.getBaseProjection());
        mapView.getComponents().layers.addLayer(textLayer);
        textStyleGeneral = new StyleSet<TextStyle>(
                        TextStyle.builder()
                            .setOrientation(MarkerStyle.GROUND_ORIENTATION)
                            .setAllowOverlap(false)
                            .setSize(32)
                            .build()
                );

        textStyleMinor = new StyleSet<TextStyle>();
        textStyleMinor.setZoomStyle(MINOR_ZOOM, TextStyle.builder()
                .setOrientation(MarkerStyle.GROUND_ORIENTATION)
                .setAllowOverlap(false)
                .setSize(28)
                .build()
                );
         
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
