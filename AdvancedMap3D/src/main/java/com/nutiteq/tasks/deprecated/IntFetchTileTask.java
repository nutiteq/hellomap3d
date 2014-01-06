package com.nutiteq.tasks.deprecated;

import java.io.DataInputStream;

import android.content.res.Resources;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.tasks.FetchTileTask;

/**
 * Task for loading raster tiles from resources.
 */
public class IntFetchTileTask extends FetchTileTask {
  private Resources resources;

  private String path;

  public IntFetchTileTask(MapTile tile, Components components, long tileIdOffset, String path, Resources resources) {
    super(tile, components, tileIdOffset);
    this.path = path;
    this.resources = resources;
  }

  @Override
  public void run() {
    super.run();
    int resourceId = resources.getIdentifier(path, null, null);
    if (resourceId == 0) {
      Log.error(getClass().getName() + ": Resource not found: " + path);
      cleanUp();
      return;
    }

    DataInputStream inputStream = new DataInputStream(resources.openRawResource(resourceId));
    readStream(inputStream);
    cleanUp();
  }
}
