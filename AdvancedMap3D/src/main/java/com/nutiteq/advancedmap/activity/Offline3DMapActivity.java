package com.nutiteq.advancedmap.activity;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import org.apache.http.entity.ByteArrayEntity;

import android.app.Activity;
import android.graphics.Color;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.widget.ZoomControls;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Bounds;
import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.components.Vector3D;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.geometry.NMLModel;
import com.nutiteq.log.Log;
import com.nutiteq.nmlpackage.NMLPackage;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.IOUtils;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.UnscaledBitmapLoader;
import com.nutiteq.vectorlayers.NMLModelDbLayer;
import com.nutiteq.vectorlayers.NMLModelLayer;

/**
 * 
 * Demonstrates NMLModelDbLayer - 3D model layer which loads data fom a .nmldb file
 * 
 * After file loading the map is recentered to content coverage area.
 * 
 * To use this sample a .nmldb file must be loaded to SDCard file.
 * See https://github.com/nutiteq/hellomap3d/wiki/Nml-3d-models-map-layer for details and sample data download
 * 
 * @author jaak
 *
 */
public class Offline3DMapActivity extends Activity implements FilePickerActivity {

    private MapView mapView;
    private EPSG3857 proj;
    private StyleSet<ModelStyle> modelStyleSet;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // spinner in status bar, for progress indication
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

        setContentView(R.layout.main);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("nml3d");

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

        this.proj = new EPSG3857();

        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://otile1.mqcdn.com/tiles/1.0.0/osm/{zoom}/{x}/{y}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);


        // define style for 3D to define minimum zoom = 14
        ModelStyle modelStyle = ModelStyle.builder().build();
        modelStyleSet = new StyleSet<ModelStyle>(null);
        modelStyleSet.setZoomStyle(14, modelStyle);

        // ** 3D Model layer
        try {
            Bundle b = getIntent().getExtras();
            String mapFile = b.getString("selectedFile");

            if(mapFile.endsWith("nml")){
                // single model nml file
                addNml(new BufferedInputStream(new FileInputStream(new File(mapFile))));

            }else if(mapFile.endsWith("dae") || mapFile.endsWith("zip")){
                // convert dae to NML using online API
                new DaeConverterServiceTask(this, mapFile).execute(IOUtils.readFully(new BufferedInputStream(new FileInputStream(new File(mapFile)))));

            }else{
                // nmlDB, if sqlite or nmldb file extension
                addNmlDb(mapFile);

            }
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }

        // rotation - 0 = north-up
        mapView.setMapRotation(0f);
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

        // define online map persistent caching - optional, suggested. Default - no caching
        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache").getPath());
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

    private void addNmlDb(String mapFile) throws IOException {

        NMLModelDbLayer modelLayer = new NMLModelDbLayer(proj,
                mapFile, modelStyleSet);
        modelLayer.setMemoryLimit(20*1024*1024);
        mapView.getLayers().addLayer(modelLayer);

        // set initial map view camera from database
        Envelope extent = modelLayer.getDataExtent();

        // or you can just set map view bounds directly 
        mapView.setBoundingBox(new Bounds(extent.minX, extent.maxY, extent.maxX, extent.minY), false);
    }

    /**
     * adds Nml to map
     * 
     * @param modelStyleSet
     * @param mapFile
     * @throws FileNotFoundException
     * @throws IOException
     */
    private void addNml(InputStream is)
            throws FileNotFoundException, IOException {

        // create layer and an model
        MapPos mapPos1 = proj.fromWgs84(20.466027f, 44.810537f);

        // set it to fly a bit with Z = 0.1f
        MapPos mapPos = new MapPos(mapPos1.x, mapPos1.y, 1.0f);
        NMLModelLayer nmlModelLayer = new NMLModelLayer(proj);
        mapView.getLayers().addLayer(nmlModelLayer);


        NMLPackage.Model nmlModel = NMLPackage.Model.parseFrom(new BufferedInputStream(is));
        // set initial position for the milk truck
        Log.debug("nmlModel loaded");

        NMLModel model = new NMLModel(mapPos, null, modelStyleSet, nmlModel, null);

        // set size, 10 is clear oversize, but this makes it visible
        model.setScale(new Vector3D(10, 10, 10));

        nmlModelLayer.add(model);

        mapView.setFocusPoint(mapPos);
        mapView.setTilt(45);
        mapView.setZoom(17.0f);
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
                            && (file.getName().endsWith(".db") ||
                                    file.getName().endsWith(".dae") ||
                                    file.getName().endsWith(".zip") || // zipped dae file
                                    file.getName().endsWith(".nml") ||
                                    file.getName().endsWith(".nmldb")||
                                    file.getName().endsWith(".sqlite"))) {
                        // accept files with given extension
                        return true;
                    }
                }
                return false;
            };
        };
    }

    public MapView getMapView() {
        return mapView;
    }

    @Override
    public String getFileSelectMessage() {
        return "Select 3D file (NML or DAE)";
    }


    public class DaeConverterServiceTask extends AsyncTask<byte[], Void, InputStream> {

        private Offline3DMapActivity offlineActivity;
        private String mapFile;

        public DaeConverterServiceTask(Offline3DMapActivity offlineActivity, String mapFile){
            this.offlineActivity = offlineActivity;
            // replace .zip -> .dae in end of dae file name, and remove folder
            this.mapFile = mapFile.substring(mapFile.lastIndexOf("/")+1, mapFile.length()-4);
        }

        protected InputStream doInBackground(byte[]... dae) {

            String url = "http://aws-lb.nutiteq.com/daeconvert/?key=Aq7M28a93Huik&dae="+mapFile+".dae&max-single-texture-size=2048";
            Log.debug("connecting "+url);
            ByteArrayEntity daeEntity;
            daeEntity = new ByteArrayEntity(dae[0]);

            InputStream nmlStream = NetUtils.postUrlasStream(url, null, false, daeEntity);
            return nmlStream;
        }

        protected void onPostExecute(InputStream nml) {
            try {
                offlineActivity.addNml(nml);
            } catch (FileNotFoundException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }

    }
}



