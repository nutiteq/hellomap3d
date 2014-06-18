package com.nutiteq.advancedmap.activity;

import java.io.InputStream;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.maplisteners.MapEventListener;
import com.nutiteq.components.Bounds;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.components.Vector3D;
import com.nutiteq.datasources.raster.StoredRasterDataSource;
import com.nutiteq.datasources.raster.TileDebugRasterDataSource;
import com.nutiteq.datasources.raster.WMSRasterDataSource;
import com.nutiteq.datasources.vector.OSMPolygon3DDataSource;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.NMLModel;
import com.nutiteq.layers.Layer;
import com.nutiteq.log.Log;
import com.nutiteq.nmlpackage.NMLPackage;
import com.nutiteq.projections.EPSG3301;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.PackagedRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.roofs.FlatRoof;
import com.nutiteq.style.LabelStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.Polygon3DStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.MarkerLayer;
import com.nutiteq.vectorlayers.NMLModelLayer;
import com.nutiteq.vectorlayers.Polygon3DLayer;

/**
 * This sample has a set of different layers: 
 * a) raster online: TMS, WMS, Bing, MapBox
 * b) raster offline: StoredMap (MGM format), PackagedMap from app package (res/raw)
 * c) 3D layer: OSMPolygon3D with roof structures and color tags
 * d) Others: MarkerLayer
 * 
 * See private methods in this class which layers can be added.
 * OptionsMenu items are made to select base map layers and overlays, and to jump to more interesting places 
 * 
 * @author jaak
 *
 */
public class AdvancedMapActivity extends Activity {

    // Default OSM building height in meters
    private static final float DEFAULT_BUILDING_HEIGHT = 18.0f; 

    private MapView mapView;
    private Projection proj;


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
        Log.setTag("advancedmap");

        // 1. Get the MapView from the Layout xml - mandatory
        this.mapView = (MapView) findViewById(R.id.mapView);
        this.proj = new EPSG3857();

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration, skip other initializations
            mapView.setComponents(retainObject);
            // add event listener
            MapEventListener mapListener = new MapEventListener(this);
            mapView.getOptions().setMapListener(mapListener);
            return;
        } else {
            // 2. create and set MapView components - mandatory
            Components components = new Components();
            // set stereo view: works if you rotate to landscape and device has HTC 3D or LG Real3D
            // Optional - adjust stereo base. Default 1.0
//          components.options.setStereoModeStrength(1.0f);
            // Set rendering mode to stereo
//          components.options.setRenderMode(Options.STEREO_RENDERMODE);
            mapView.setComponents(components);
            // add event listener
            MapEventListener mapListener = new MapEventListener(this);
            mapView.getOptions().setMapListener(mapListener);
        }


        // 3. Define map layer for basemap - mandatory.
        // Here we use MapQuest open tiles
        // Almost all online tiled maps use EPSG3857 projection.

        baseMapQuest();


        // set initial map view camera - optional. "World view" is default
        // Location: San Francisco
        mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(-122.41666666667f, 37.76666666666f));


//      mapView.setFocusPoint(2901450, 5528971);    // Romania
//      mapView.setFocusPoint(2915891.5f, 7984571.0f); // valgamaa


        // bulgaria
//      mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(26.483230800000037, 42.550218000000044));

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
        // zoom - 0 = world, like on most web maps
        mapView.setZoom(16.0f);
        // tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
//      mapView.setTilt(55.0f);


        // Estonia 
//      mapView.setFocusPoint(mapView.getLayers().getBaseLayer().getProjection().fromWgs84(24.74314f,59.43635f));
//      mapView.setZoom(6.0f);
//      mapView.setMapRotation(0);
//      mapView.setTilt(90f);

        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(false);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(false);
        mapView.getOptions().setKineticPanning(true);
        mapView.getOptions().setDoubleClickZoomIn(true);
        mapView.getOptions().setDualClickZoomOut(true);

        adjustMapDpi();
//      mapView.getOptions().setTileSize(512);
//      mapView.getOptions().setFPSIndicator(true);
        mapView.getOptions().setRasterTaskPoolSize(4);

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
//      mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
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

    // adjust zooming to DPI, so texts on rasters will be not too small
    // useful for non-retina rasters, they would look like "digitally zoomed"
    private void adjustMapDpi() {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        float dpi = metrics.densityDpi;
        // following is equal to  -log2(dpi / DEFAULT_DPI)
        float adjustment = (float) - (Math.log(dpi / DisplayMetrics.DENSITY_HIGH) / Math.log(2));
        Log.debug("adjust DPI = "+dpi+" as zoom adjustment = "+adjustment);
        mapView.getOptions().setTileZoomLevelBias(adjustment / 2.0f);
    }

    @Override
    protected void onStop() {
        super.onStop();
        mapView.stopMapping();
    }

    @Override
    public boolean onCreateOptionsMenu(final Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.mainmenu, menu);
        return true;
    }

    @Override
    public boolean onMenuItemSelected(final int featureId, final MenuItem item) {

        item.setChecked(true);

        switch (item.getItemId()) {

        // map types
        case R.id.menu_openstreetmap:
            baseMapQuest();
            break;

        case R.id.menu_mapboxsatellite:
            baseLayerMapBoxSatelliteLayer(false);
            break;

        case R.id.menu_mapboxsatelliteretina:
            baseLayerMapBoxSatelliteLayer(true);
            break;  

        case R.id.menu_mapbox:
            baseLayerMapBoxStreetsLayer(false);
            break;

        case R.id.menu_mapboxretina:
            baseLayerMapBoxStreetsLayer(true);
            break;

        case R.id.menu_stamenterrain:
            baseLayerStamenTerrainLayer();
            break;


        case R.id.menu_bing:
            addBingBaseLayer("http://ecn.t3.tiles.virtualearth.net/tiles/r{quadkey}.png?g=1&mkt=en-US&shading=hill&n=z");
            break;

        case R.id.menu_bingaerial:
            baseBingAerial();
            break;

        case R.id.menu_esriimagery:
            baseEsriBasemapImagery();
            break;

        case R.id.menu_stored:
            basePackagedLayer();
            mapView.zoom(4.0f - mapView.getZoom(), 500);
            break;

        case R.id.menu_mgm:
            // NB! path is hardcoded here
            addStoredBaseLayer("/sdcard/mapxt/mgm/est_tallinn/");
            break;

        case R.id.menu_regio:
            baseCustomProjectionLayer();
            break;

            // overlays

        case R.id.menu_nml:
            singleNmlModelLayer();
            break;

        case R.id.menu_osm3d:
            addOsmPolygonLayer();
            break;

        case R.id.menu_wms:
//          String url = "http://kaart.maakaart.ee/geoserver/wms?transparent=true&";
//          String layers = "topp:states";
            String url = "http://kaart.maakaart.ee/service?";
            String layers = "osm";
            addWmsLayer(url, layers, new EPSG4326());

            break;

        case R.id.menu_hillshade:
            addHillShadeLayer();
            break;

        case R.id.menu_marker:
            addMarkerLayer(proj.fromWgs84(-122.416667f, 37.766667f));
            break;

        case R.id.menu_tileborders:
            addTileBorderLayer(256);
            break;

            // Locations
        case R.id.menu_coburg:
            // Coburg, germany
            mapView.setFocusPoint(proj.fromWgs84(10.96465, 50.27082), 1000);
            mapView.setZoom(16.0f);
            break;

        case R.id.menu_petronas:
            // Petronas towers, Kuala Lumpur, Malaisia
            mapView.setFocusPoint(proj.fromWgs84(101.71339, 3.15622), 1000);
            mapView.setZoom(17.0f);
            mapView.setTilt(60);
            break;

        case R.id.menu_sf:
            // San Francisco
            mapView.setFocusPoint(proj.fromWgs84(-122.416667f, 37.766667f), 1000);
            break;

        case R.id.menu_barcelona:
            // San Francisco
            mapView.setFocusPoint(proj.fromWgs84(2.183333f, 41.383333f), 1000);
            break;

        case R.id.menu_tll:
            // Tallinn
            mapView.setFocusPoint(new MapPos(2753791.3f, 8275296.0f)); 
            break;    
        }

        return true;

    }

    private void updateBaseLayer(Layer baseLayer) {
        // Get view dimensions
        Rect rect = new Rect();
        mapView.getGlobalVisibleRect(rect);

        // Get visible corners in base coordinate system
        MapPos mapPos1Old = mapView.screenToWorld(rect.left, rect.top);
        MapPos mapPos2Old = mapView.screenToWorld(rect.right, rect.bottom);

        // Transform to WGS84
        Projection baseProjOld = mapView.getComponents().layers.getBaseProjection();
        MapPos mapPos1Wgs = baseProjOld.toWgs84(mapPos1Old.x, mapPos1Old.y);
        MapPos mapPos2Wgs = baseProjOld.toWgs84(mapPos2Old.x, mapPos2Old.y);

        // Update base layer
        mapView.getLayers().setBaseLayer(baseLayer);
        proj = baseLayer.getProjection();

        // Transform corner coordinates to new base coordinate system
        Projection baseProjNew = mapView.getComponents().layers.getBaseProjection();
        MapPos mapPos1New = baseProjNew.fromWgs84(mapPos1Wgs.x, mapPos1Wgs.y); 
        MapPos mapPos2New = baseProjNew.fromWgs84(mapPos2Wgs.x, mapPos2Wgs.y);

        // Make bounding box from calculated points
        if (!baseProjOld.equals(baseProjNew)) {
            Bounds bounds = new Bounds(mapPos1New.x, mapPos1New.y, mapPos2New.x, mapPos2New.y);
            mapView.setBoundingBox(bounds, rect, false, false, false, 0);
        }
    }


    private void baseCustomProjectionLayer() {
        // use custom layer with Quadtree tile numbering
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3301(), 0, 19, "http://pump.regio.ee/delfi/?rq=2{quadkey}");
        RasterLayer baseMapLayer = new RasterLayer(dataSource, 1007);
        updateBaseLayer(baseMapLayer);
    }
    
    private void addHillShadeLayer() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 5, 18, "http://toolserver.org/~cmarqu/hill/{zoom}/{x}/{y}.png");
        RasterLayer hillsLayer = new RasterLayer(dataSource, 25);
        mapView.getLayers().addLayer(hillsLayer);
    }

    private void addTileBorderLayer(int size) {
        RasterDataSource dataSource = new TileDebugRasterDataSource(this.proj, 0, 22, size);
        RasterLayer tileDebugLayer = new RasterLayer(dataSource, 17);
        tileDebugLayer.setMemoryCaching(false);
        mapView.getLayers().addLayer(tileDebugLayer);
    }


    private void basePackagedLayer() {
        RasterDataSource dataSource = new PackagedRasterDataSource(new EPSG3857(), 0, 3, "t{zoom}_{x}_{y}", getApplicationContext());
        RasterLayer packagedMapLayer = new RasterLayer(dataSource, 16);
        updateBaseLayer(packagedMapLayer);
    }

    private void addStoredBaseLayer(String dir) {
        StoredRasterDataSource dataSource = new StoredRasterDataSource(new EPSG3857(), 256, 0, 17, "OpenStreetMap", dir);
        RasterLayer storedMapLayer = new RasterLayer(dataSource, 135);
        updateBaseLayer(storedMapLayer);

        mapView.setFocusPoint(dataSource.getCenter());
        mapView.setZoom((float) dataSource.getCenter().z);
    }

    // ** Add simple marker to map.
    private void addMarkerLayer(MapPos markerLocation) {
        // define marker style (image, size, color)
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker).setSize(0.5f).setColor(Color.WHITE).build();
        // define label what is shown when you click on marker

        LabelStyle labelStyle = 
                LabelStyle.builder()
                .setEdgePadding(24)
                .setLinePadding(12)
                .setTitleFont(Typeface.create("Arial", Typeface.BOLD), 38)
                .setDescriptionFont(Typeface.create("Arial", Typeface.NORMAL), 32)
                .build();

        Label markerLabel = new DefaultLabel("San Francisco", "Here is a marker", labelStyle);


        // create layer and add object to the layer, finally add layer to the map. 
        // All overlay layers must be same projection as base layer, so we reuse it
        MarkerLayer markerLayer = new MarkerLayer(proj);
        Marker marker = new Marker(markerLocation, markerLabel, markerStyle, null);
        markerLayer.add(marker);
        mapView.getLayers().addLayer(markerLayer);
        mapView.selectVectorElement(marker);
    }

    // Load online simple building 3D boxes
    private void addOsmPolygonLayer() {
        // Set style visible from zoom 15
        // note: & 0xaaffffff makes the color a bit transparent
        Polygon3DStyle polygon3DStyle = Polygon3DStyle.builder().setColor(Color.WHITE & 0xaaffffff).build();
        StyleSet<Polygon3DStyle> polygon3DStyleSet = new StyleSet<Polygon3DStyle>(null);
        polygon3DStyleSet.setZoomStyle(15, polygon3DStyle);

        OSMPolygon3DDataSource dataSource = new OSMPolygon3DDataSource(new EPSG3857(), DEFAULT_BUILDING_HEIGHT, new FlatRoof(),  Color.WHITE, Color.GRAY, 1500, polygon3DStyleSet);
        Polygon3DLayer osm3dLayer = new Polygon3DLayer(dataSource);
        mapView.getLayers().addLayer(osm3dLayer);
    }

    private void addWmsLayer(String url, String layers, Projection dataProjection) {
        RasterDataSource dataSource = new WMSRasterDataSource(dataProjection, 0, 19, url, "", layers, "image/png");
        RasterLayer wmsLayer = new RasterLayer(dataSource, 1012);
        wmsLayer.setFetchPriority(-5);
        mapView.getLayers().addLayer(wmsLayer);
    }

    private void addBingBaseLayer(String urlTemplate){
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 19, urlTemplate);
        RasterLayer bingMapLayer = new RasterLayer(dataSource, 1013);
        updateBaseLayer(bingMapLayer);
    }

    private void baseLayerMapBoxSatelliteLayer(boolean retina){
        String mapId;
        int cacheID;
        if(retina){
            mapId = "nutiteq.map-78tlnlmb";
            cacheID = 24;
        }else{
            mapId = "nutiteq.map-f0sfyluv";
            cacheID = 25;
        }

        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 19, "http://api.tiles.mapbox.com/v3/" + mapId + "/{zoom}/{x}/{y}.png");
        RasterLayer mapBoxLayer = new RasterLayer(dataSource, cacheID);
        updateBaseLayer(mapBoxLayer);
    }

    private void baseLayerMapBoxStreetsLayer(boolean retina){
        String mapId;
        int cacheID;
        if(retina){
            mapId = "nutiteq.map-aasha5ru";
            cacheID = 22;
        }else{
            mapId = "nutiteq.map-j6a1wkx0";
            cacheID = 23;
        }
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 19, "http://api.tiles.mapbox.com/v3/" + mapId + "/{zoom}/{x}/{y}.png");
        RasterLayer mapBoxLayer = new RasterLayer(dataSource, cacheID);
        updateBaseLayer(mapBoxLayer);
    }


    private void baseMapQuest() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 20, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapQuestLayer = new RasterLayer(dataSource, 11);
        updateBaseLayer(mapQuestLayer);
    }

    private void baseLayerStamenTerrainLayer() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 20, "http://tile.stamen.com/terrain/{zoom}/{x}/{y}.png");
        RasterLayer stamenLayer = new RasterLayer(dataSource, 18);
        updateBaseLayer(stamenLayer);
    }


    private void baseBingAerial() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 19, "http://ecn.t3.tiles.virtualearth.net/tiles/a{quadkey}.jpeg?g=471&mkt=en-US");
        RasterLayer bingLayer = new RasterLayer(dataSource, 14);
        updateBaseLayer(bingLayer);
    }

    private void baseMapOpenAerial() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 11, "http://otile1.mqcdn.com/tiles/1.0.0/sat/{zoom}/{x}/{y}.png");
        RasterLayer aerialLayer = new RasterLayer(dataSource, 15);
        updateBaseLayer(aerialLayer);
    }
    private void baseEsriBasemapImagery() {

        // ESRI BaseMap. Note: ESRI requires attribution and account
        // Check https://developers.arcgis.com for details
        
        // World_Imagery as base
        HTTPRasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 19, "http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{zoom}/{y}/{x}/");
        RasterLayer mapLayer = new RasterLayer(dataSource, 17);
        updateBaseLayer(mapLayer);
        
        // Add World_Boundaries_and_Places with placenames as transparent overlay
        HTTPRasterDataSource dataSourceOvl = new HTTPRasterDataSource(new EPSG3857(), 0, 19, "http://server.arcgisonline.com/ArcGIS/rest/services/Reference/World_Boundaries_and_Places/MapServer/tile/{zoom}/{y}/{x}/");
        RasterLayer mapLayerOvl = new RasterLayer(dataSourceOvl, 18);
        mapView.getLayers().addLayer(mapLayerOvl);
    }

    
    private void singleNmlModelLayer() {
        ModelStyle modelStyle = ModelStyle.builder().build();
        StyleSet<ModelStyle> modelStyleSet = new StyleSet<ModelStyle>(null);
        modelStyleSet.setZoomStyle(14, modelStyle);

        // create layer and an model
        MapPos mapPos1 = proj.fromWgs84(20.466027f, 44.810537f);

        // set it to fly abit
        MapPos mapPos = new MapPos(mapPos1.x, mapPos1.y, 0.1f);
        NMLModelLayer nmlModelLayer = new NMLModelLayer(new EPSG3857());
        try {
            InputStream is = this.getResources().openRawResource(R.raw.milktruck);
            NMLPackage.Model nmlModel = NMLPackage.Model.parseFrom(is);
            NMLModel model = new NMLModel(mapPos, null, modelStyleSet, nmlModel, null);

            // set size, 10 is clear oversize, but this makes it visible
            model.setScale(new Vector3D(10, 10, 10));

            nmlModelLayer.add(model);
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        mapView.getLayers().addLayer(nmlModelLayer);

        mapView.setFocusPoint(mapPos);
        mapView.setTilt(45);
    }

    public MapView getMapView() {
        return mapView;
    }

}

