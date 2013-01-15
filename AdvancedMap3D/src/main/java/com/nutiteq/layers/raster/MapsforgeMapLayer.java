package com.nutiteq.layers.raster;

import java.io.File;

import org.mapsforge.android.maps.mapgenerator.JobTheme;
import org.mapsforge.android.maps.mapgenerator.MapGenerator;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorFactory;
import org.mapsforge.android.maps.mapgenerator.MapGeneratorInternal;
import org.mapsforge.android.maps.mapgenerator.databaserenderer.DatabaseRenderer;
import org.mapsforge.map.reader.MapDatabase;
import org.mapsforge.map.reader.header.FileOpenResult;

import com.nutiteq.components.TileQuadTreeNode;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;

public class MapsforgeMapLayer extends RasterLayer {

  private MapGenerator mapGenerator;
  private JobTheme theme;

  public MapsforgeMapLayer(Projection projection, int minZoom, int maxZoom, int id, String path, JobTheme theme) {
    super(projection, minZoom, maxZoom, id, path);
    mapGenerator = MapGeneratorFactory.createMapGenerator(MapGeneratorInternal.DATABASE_RENDERER);

    MapDatabase mapDatabase = new MapDatabase();
    mapDatabase.closeFile();
    FileOpenResult fileOpenResult = mapDatabase.openFile(new File("/"
        + path));
    if (fileOpenResult.isSuccess()) {
      Log.debug("MapsforgeLayer MapDatabase opened ok: " + path);
    }

    ((DatabaseRenderer) mapGenerator).setMapDatabase(mapDatabase);
    this.theme = theme;
  }

  @Override
  public void fetchTile(TileQuadTreeNode tile) {
    components.rasterTaskPool.execute(new MapsforgeFetchTileTask(tile, components, tileIdOffset, mapGenerator, theme));

  }

  @Override
  public void flush() {

  }

}
