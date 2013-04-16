package com.nutiteq.advancedmap;

import java.io.IOException;
import java.util.Vector;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.Window;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.maplisteners.MapEventListener;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.db.DBLayer;
import com.nutiteq.geometry.Marker;
import com.nutiteq.layers.raster.PackagedMapLayer;
import com.nutiteq.layers.raster.QuadKeyLayer;
import com.nutiteq.layers.raster.TMSMapLayer;
import com.nutiteq.layers.raster.WmsLayer;
import com.nutiteq.layers.vector.Polygon3DOSMLayer;
import com.nutiteq.layers.vector.SpatialLiteDb;
import com.nutiteq.layers.vector.SpatialiteLayer;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.StoredMapLayer;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;
import com.nutiteq.vectorlayers.NMLModelDbLayer;

/**
 * This sample has a set of different layers. 
 * 
 * @author jaak
 *
 */
public class AdvancedMapActivity extends Activity {

	private MapView mapView;

    
	// force to load proj library (needed for spatialite)
	static {
		try {
			System.loadLibrary("proj");
		} catch (Throwable t) {
			System.err.println("Unable to load proj: " + t);
		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

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
		      // set stereo view: works if you rotate to landscape and device has HTC 3D or LG Real3D
		      // Optional - adjust stereo base. Default 1.0
		      components.options.setStereoModeStrength(1.0f);
		      // Set rendering mode to stereo
		      components.options.setRenderMode(Options.STEREO_RENDERMODE);
		      mapView.setComponents(components);
		      }

		// add event listener
		MapEventListener mapListener = new MapEventListener(this);
		mapView.getOptions().setMapListener(mapListener);

		// 3. Define map layer for basemap - mandatory.
		// Here we use MapQuest open tiles
		// Almost all online tiled maps use EPSG3857 projection.
		TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 5, 18, 0,
				"http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");

		mapView.getLayers().setBaseLayer(mapLayer);

		// set initial map view camera - optional. "World view" is default
		// Location: San Francisco
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 37.76666666666f));

	
//		mapView.setFocusPoint(2901450, 5528971);    // Romania
//        mapView.setFocusPoint(2915891.5f, 7984571.0f); // valgamaa
//        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(2.183333f, 41.383333f)); // barcelona
//        mapView.setFocusPoint(new MapPos(2753791.3f, 8275296.0f)); // Tallinn

//        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(65.548178937072f,57.146960113233f)); // Tyumen
        
        // bulgaria
//        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(26.483230800000037, 42.550218000000044));

		// rotation - 0 = north-up
		mapView.setRotation(0f);
		// zoom - 0 = world, like on most web maps
		mapView.setZoom(16.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
		mapView.setTilt(55.0f);


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


		// 5. Add various layers to map - optional
        //    comment in needed ones, make sure that data file(s) exists in given folder

		addBingBaseLayer(mapLayer.getProjection(),"http://ecn.t3.tiles.virtualearth.net/tiles/r",".png?g=1&mkt=en-US&shading=hill&n=z");
        //addPackagedBaseLayer(mapLayer.getProjection());

		// addStoredBaseLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory().getPath()+"/nutimaps/allimages/");
		 
        addMarkerLayer(mapLayer.getProjection(),mapLayer.getProjection().fromWgs84(-122.416667f, 37.766667f));

//		addSpatialiteLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory().getPath()+"/mapxt/romania_sp3857.sqlite");

        // Overlay layer from http://toolserver.org/~cmarqu/hill/$%7Bz%7D/$%7Bx%7D/$%7By%7D.png
        TMSMapLayer hillsLayer = new TMSMapLayer(new EPSG3857(), 5, 18, 0,
                "http://toolserver.org/~cmarqu/hill/", "/", ".png");
        mapView.getLayers().addLayer(hillsLayer);
        
		addOsmPolygonLayer(mapLayer.getProjection());

//        add3dModelLayer(mapLayer.getProjection(),Environment.getExternalStorageDirectory() + "/mapxt/tallinn28.nml");
//        addWmsLayer(mapLayer.getProjection(),"http://kaart.maakaart.ee/service?","osm", new EPSG4326());
		
	}


	private void addPackagedBaseLayer(Projection projection) {
	    PackagedMapLayer packagedMapLayer = new PackagedMapLayer(projection, 0, 4, 13, "t", this);
	    mapView.getLayers().setBaseLayer(packagedMapLayer);
    }

    private void addStoredBaseLayer(Projection projection, String dir) {
        StoredMapLayer storedMapLayer = new StoredMapLayer(projection, 256, 0,
                17, 13, "OpenStreetMap", dir);
        mapView.getLayers().setBaseLayer(storedMapLayer);
    }

	// ** Add simple marker to map.
	private void addMarkerLayer(Projection proj, MapPos markerLocation) {
		// define marker style (image, size, color)
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.5f).setColor(Color.WHITE).build();
		// define label what is shown when you click on marker
        Label markerLabel = new DefaultLabel("San Francisco", "Here is a marker");
        

        // create layer and add object to the layer, finally add layer to the map. 
        // All overlay layers must be same projection as base layer, so we reuse it
		MarkerLayer markerLayer = new MarkerLayer(proj);
        markerLayer.add(new Marker(markerLocation, markerLabel, markerStyle, null));
		mapView.getLayers().addLayer(markerLayer);
	}

	// Load online simple building 3D boxes
	private void addOsmPolygonLayer(Projection proj) {
		// Set style visible from zoom 15
        Polygon3DStyle polygon3DStyle = Polygon3DStyle.builder().setColor(Color.BLACK | 0x40ffffff).build();
        StyleSet<Polygon3DStyle> polygon3DStyleSet = new StyleSet<Polygon3DStyle>(null);
		polygon3DStyleSet.setZoomStyle(15, polygon3DStyle);

        Polygon3DOSMLayer osm3dLayer = new Polygon3DOSMLayer(new EPSG3857(), 0.500f, 200, polygon3DStyleSet);
		mapView.getLayers().addLayer(osm3dLayer);
	}

	// load 3D models from special database
	private void add3dModelLayer(Projection proj, String filePath) {

		// define style for 3D to define minimum zoom = 14
		ModelStyle modelStyle = ModelStyle.builder().build();
		StyleSet<ModelStyle> modelStyleSet = new StyleSet<ModelStyle>(null);
		modelStyleSet.setZoomStyle(14, modelStyle);

		// ** 3D Model layer
        try {
            NMLModelDbLayer modelLayer = new NMLModelDbLayer(new EPSG3857(),
            		filePath, modelStyleSet);
            modelLayer.setMemoryLimit(20*1024*1024);
            mapView.getLayers().addLayer(modelLayer);
        } catch (IOException e) {
            e.printStackTrace();
        }

	}

	private void addSpatialiteLayer(Projection proj, String dbPath) {

		// ** Spatialite
		// print out list of tables first
		int minZoom = 10;

		SpatialLiteDb spatialLite = new SpatialLiteDb(dbPath);
		Vector<DBLayer> dbMetaData = spatialLite.qrySpatialLayerMetadata();

		for (DBLayer dbLayer : dbMetaData) {
            Log.debug("layer: "+dbLayer.table+" "+dbLayer.type+" geom:"+dbLayer.geomColumn);
		}

		// set styles for all 3 object types: point, line and polygon

		StyleSet<PointStyle> pointStyleSet = new StyleSet<PointStyle>();
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
        PointStyle pointStyle = PointStyle.builder().setBitmap(pointMarker).setSize(0.05f).setColor(Color.BLACK).build();
		pointStyleSet.setZoomStyle(minZoom, pointStyle);

		StyleSet<LineStyle> lineStyleSet = new StyleSet<LineStyle>();
        lineStyleSet.setZoomStyle(minZoom, LineStyle.builder().setWidth(0.1f).setColor(Color.GREEN).build());

		StyleSet<LineStyle> lineStyleSetHw = new StyleSet<LineStyle>();
        lineStyleSetHw.setZoomStyle(minZoom, LineStyle.builder().setWidth(0.07f).setColor(Color.GRAY).build());

        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(Color.BLUE).build();
        StyleSet<PolygonStyle> polygonStyleSet = new StyleSet<PolygonStyle>(null);
		polygonStyleSet.setZoomStyle(minZoom, polygonStyle);

        SpatialiteLayer spatialiteLayerPt = new SpatialiteLayer(proj, dbPath, "pt_tourism",
                "GEOMETRY", new String[]{"name"}, 500, pointStyleSet, null, null);

		mapView.getLayers().addLayer(spatialiteLayerPt);

        SpatialiteLayer spatialiteLayerLn = new SpatialiteLayer(proj, dbPath, "ln_railway",
                "GEOMETRY", new String[]{"sub_type"}, 500, null, lineStyleSet, null);

		mapView.getLayers().addLayer(spatialiteLayerLn);

        SpatialiteLayer spatialiteLayerHw = new SpatialiteLayer(proj, dbPath, "ln_highway",
                "GEOMETRY", new String[]{"name"}, 500, null, lineStyleSetHw, null);

		mapView.getLayers().addLayer(spatialiteLayerHw);

        SpatialiteLayer spatialiteLayerPoly = new SpatialiteLayer(proj, dbPath, "pg_boundary",
                "GEOMETRY", new String[]{"name"}, 500, null, null, polygonStyleSet);

		mapView.getLayers().addLayer(spatialiteLayerPoly);

	}

     private void addWmsLayer(Projection proj, String url, String layers, Projection dataProjection){
       WmsLayer wmsLayer = new WmsLayer(proj, 0, 19, 1012, url, "", layers, "image/png", dataProjection);
		wmsLayer.setFetchPriority(-5);
		mapView.getLayers().addLayer(wmsLayer);
	}
     

     private void addBingBaseLayer(Projection proj, String url, String extension){
         QuadKeyLayer bingMap = new QuadKeyLayer(proj, 0, 19, 1013, url, extension);
         mapView.getLayers().setBaseLayer(bingMap);
      }


    public MapView getMapView() {
        return mapView;
    }
    
    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

     
}

