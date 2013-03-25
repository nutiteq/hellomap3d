package com.nutiteq.advancedmap;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.HashMap;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.RelativeLayout;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.maplisteners.MBTileMapEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.geometry.Marker;
import com.nutiteq.layers.raster.MBTilesMapLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.Label;
import com.nutiteq.ui.ViewLabel;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;

public class MBTilesMapActivity extends Activity implements FilePickerActivity{

	private MapView mapView;
    private Marker clickMarker;
    private static final String HTML_CSS = "@font-face {font-family: classic_arial_font; src: url('arial.ttf');} body {font-family: 'classic_arial_font';}";
    public static final String HTML_HEAD = "<html><head><style type=\"text/css\">"+HTML_CSS+"</style></head><body bgcolor=\"transparent\" style=\"background-color:transparent;\" onClick=\"Android.openWebPageData()\">";
    public static final String HTML_FOOT = "</body></html>";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.main);

		// enable logging for troubleshooting - optional
		Log.enableAll();
		Log.setTag("hellomap");

		// 1. Get the MapView from the Layout xml - mandatory
		mapView = (MapView) findViewById(R.id.mapView);

		// Optional, but very useful: restore map state during device rotation,
		// it is saved in onRetainNonConfigurationInstance() below
		Components retainObject = (Components) getLastNonConfigurationInstance();
		if (retainObject != null) {
			// just restore configuration, skip other initializations
			mapView.setComponents(retainObject);
			mapView.startMapping();
			return;
		} else {
			// 2. create and set MapView components - mandatory
		      Components components = new Components();
		      mapView.setComponents(components);
		      }


		// 3. Define map layer for basemap - mandatory
		// MBTiles supports only EPSG3857 projection

        try {
            // read filename from Extras
            Bundle b = getIntent().getExtras();
            String file = b.getString("selectedFile");
            MBTilesMapLayer dbLayer = new MBTilesMapLayer(new EPSG3857(), 0, 19, file.hashCode(), file, this);
            mapView.getLayers().setBaseLayer(dbLayer);
            
            HashMap<String, String> dbMetaData = dbLayer.getDatabase().getMetadata();
            String legend = dbMetaData.get("legend");
            if(legend != null && !legend.equals("")){
                addWebView(legend);
            }
            String center = dbMetaData.get("center");
            String bounds = dbMetaData.get("bounds");
            if(center != null){
                // format: long,lat,zoom
                String[] centerParams = center.split(",");
                mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(Double.parseDouble(centerParams[0]), Double.parseDouble(centerParams[1])));
                mapView.setZoom(Float.parseFloat(centerParams[2]));
            }else if(bounds != null){
                // format: longMin,latMin,longMax,latMax
                String[] boundsParams = bounds.split(",");
                double xCenter = (Double.parseDouble(boundsParams[0]) + Double.parseDouble(boundsParams[2])) / 2; 
                double yCenter = (Double.parseDouble(boundsParams[1]) + Double.parseDouble(boundsParams[3])) / 2; 
                mapView.setFocusPoint(xCenter, yCenter);
                // TODO: calculate and set zoom from bounds
            }else{
                // bulgaria
                  mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(26.483230800000037, 42.550218000000044));
                  // zoom - 0 = world, like on most web maps
                  mapView.setZoom(5.0f);

            }
            
        } catch (IOException e) {
            Log.error(e.getLocalizedMessage());
            e.printStackTrace();
        }

        // add a layer and marker for click labels
        // define small invisible Marker, as Label requires some Marker 
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.01f).setColor(0).build();

        //  define label as WebView to show HTML
        WebView labelView = new WebView(this); 
        // It is important to set size, exception will come otherwise
        labelView.layout(0, 0, 150, 150);
        Label label = new ViewLabel("", labelView, new Handler());
        
        clickMarker = new Marker(new MapPos(0,0), label, markerStyle, null);
        
        MarkerLayer clickMarkerLayer = new MarkerLayer(new EPSG3857());
        clickMarkerLayer.add(clickMarker);
        mapView.getLayers().addLayer(clickMarkerLayer);
        
        // add event listener for clicks
        MBTileMapEventListener mapListener = new MBTileMapEventListener(this, clickMarker);
        mapView.getOptions().setMapListener(mapListener);
        
        
		// set initial map view camera - optional. "World view" is default
        		// rotation - 0 = north-up
		mapView.setRotation(0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
		mapView.setTilt(90.0f);

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

		// 4. Start the map - mandatory
		mapView.startMapping();
        
		// 5. zoom buttons using Android widgets - optional
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
     

	void addWebView(String html){
	       // define WebView for the map legend
        RelativeLayout mainLayout = (RelativeLayout) findViewById(R.id.mainView);

        // first container layout
        RelativeLayout legendLayout = new RelativeLayout(this);
        RelativeLayout.LayoutParams legendLayoutparams = 
                new RelativeLayout.LayoutParams(320, 300);
        legendLayoutparams.addRule(RelativeLayout.ALIGN_PARENT_LEFT, RelativeLayout.TRUE);
        legendLayoutparams.addRule(RelativeLayout.ALIGN_PARENT_TOP, RelativeLayout.TRUE);
        legendLayoutparams.setMargins(15, 15, 0, 0);
        legendLayout.setLayoutParams(legendLayoutparams);
        
        // now create the webview itself, and add to legendView
        WebView webView = new WebView(this);
        webView.getSettings().setJavaScriptEnabled(true);
        // force to open any URLs in native browser instead of WebView 
        webView.setWebViewClient(new WebViewClient() {
              @Override
              public boolean shouldOverrideUrlLoading(WebView view, String url) {
                 return super.shouldOverrideUrlLoading(view, url);
              }
          });
        webView.layout(0, 0, 320, 300);
        webView.loadDataWithBaseURL("file:///android_asset/",HTML_HEAD+html+HTML_FOOT, "text/html", "UTF-8",null);
        legendLayout.addView(webView);
        
        mainLayout.addView(legendLayout);
	}
	
    public MapView getMapView() {
        return mapView;
    }


    @Override
    public String getFileSelectMessage() {
        return "Select a MBTiles (.db, .sqlite or .mbtiles) file";
    }


    @Override
    public FileFilter getFileFilter() {
        return new FileFilter() {
            @Override
            public boolean accept(File file) {
                // accept only readable files
                if (file.canRead()) {
                    if (file.isDirectory()) {
                        // accept all directories
                        return true;
                    } else if (file.isFile()
                            && (file.getName().endsWith(".db")
                                    || file.getName().endsWith(".sqlite") || file
                                    .getName().endsWith(".mbtiles"))) {
                        // accept files with given extension
                        return true;
                    }
                }
                return false;
            };
        };
    }
     
}

