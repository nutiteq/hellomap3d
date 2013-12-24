package com.nutiteq.advancedmap.activity;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.widget.RadioGroup;
import android.widget.RadioGroup.OnCheckedChangeListener;

import com.nutiteq.MapView;
import com.nutiteq.advancedmap.R;
import com.nutiteq.cachestores.PersistentCacheStore;
import com.nutiteq.components.Components;
import com.nutiteq.components.Options;
import com.nutiteq.imagefilters.GrayscaleImageFilter;
import com.nutiteq.imagefilters.NightModeImageFilter;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.rasterdatasources.CacheRasterDataSource;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.ImageFilterRasterDataSource;
import com.nutiteq.rasterdatasources.ImageFilterRasterDataSource.ImageFilter;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * This is an example how to compose multiple raster data sources into one:
 * first an online HTTP raster data source is created, this is connected to caching virtual data source
 * (with persistent cache store) which in turn is connected to image filter virtual data source.
 * Finally image filter data source is connected to the layer.
 *  
 * @author mark
 * 
 */
public class ComposedRasterDataSourceActivity extends Activity {
  public MapView mapView;
  private RasterDataSource originalDS;
  private CacheRasterDataSource cacheDS;
  private ImageFilterRasterDataSource imageFilterDS;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.imagefilter);

    // enable logging for troubleshooting - optional
    Log.enableAll();
    Log.setTag("composeddatasources");

    // 1. Get the MapView from the Layout xml - mandatory
    mapView = (MapView) findViewById(R.id.mapView);

    // Optional, but very useful: restore map state during device rotation,
    // it is saved in onRetainNonConfigurationInstance() below
    Components retainObject = (Components) getLastNonConfigurationInstance();
    if (retainObject != null) {
      // just restore configuration and update listener, skip other
      // initializations
      mapView.setComponents(retainObject);

      setBaseMapLayer();

      setButtonListener();

      mapView.startMapping();
      return;
    } else {
      // 2. create and set MapView components - mandatory
      mapView.setComponents(new Components());
    }
    
    // 3. create base map layer
    setBaseMapLayer();

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
    mapView.getOptions().setRasterTaskPoolSize(1);

    // Map background, visible if no map tiles loaded - optional, default - white
    mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_BITMAP);
    mapView.getOptions().setBackgroundPlaneBitmap(
            UnscaledBitmapLoader.decodeResource(getResources(),
                    R.drawable.background_plane));
    mapView.getOptions().setClearColor(Color.WHITE);

    // configure texture caching - optional, suggested
    mapView.getOptions().setTextureMemoryCacheSize(20 * 1024 * 1024);

    // Set up button listener for filter selection
    setButtonListener();
  }
  
  private void setBaseMapLayer() {
    RasterDataSource rasterDataSource = createComposedDataSource();
    RasterLayer mapLayer = new RasterLayer(rasterDataSource, 1508);
    mapLayer.setPersistentCaching(false);
    mapLayer.setMemoryCaching(false);
    mapView.getLayers().setBaseLayer(mapLayer);    
  }
  
  private RasterDataSource createComposedDataSource() {
    originalDS = new HTTPRasterDataSource(new EPSG4326(), 0, 19, "http://www.staremapy.cz/naturalearth/{zoom}/{x}/{yflipped}.png");
    cacheDS = new CacheRasterDataSource(originalDS, new PersistentCacheStore(this.getDatabasePath("mapcache_composedrds").getPath(), 10 * 1024 * 1024));
    imageFilterDS = new ImageFilterRasterDataSource(cacheDS);
    
    cacheDS.open();

    return imageFilterDS;
  }
  
  private void destroyComposedDataSource() {
    if (cacheDS != null) {
      cacheDS.close();
    }
    
    imageFilterDS = null;
    cacheDS = null;
    originalDS = null;
  }
  
  private ImageFilter createImageFilter(int id) {
    switch (id) {
    case R.id.no_imagefilter:
      return null;
    case R.id.grayscale_imagefilter:
      return new GrayscaleImageFilter();
    case R.id.nightmode_imagefilter:
      return new NightModeImageFilter();
    }
    return null;
  }

  private void setButtonListener() {
    RadioGroup radio = (RadioGroup) findViewById(R.id.radioImageFilter);
    ImageFilter imageFilter = createImageFilter(radio.getCheckedRadioButtonId());
    imageFilterDS.setImageFilter(imageFilter);

    radio.setOnCheckedChangeListener(new OnCheckedChangeListener() {
      @Override
      public void onCheckedChanged(RadioGroup radio, int checkedId) {
        // Cancel all 'in-flight' processes that may change tiles in background after caches are reset
        ImageFilter imageFilter = createImageFilter(radio.getCheckedRadioButtonId());
        imageFilterDS.setImageFilter(imageFilter);
      }
    });
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
    // Close the caches, destroy data sources
    destroyComposedDataSource();
    super.onDestroy();
  }

}
