package com.nutiteq.tasks.deprecated;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

import android.os.Environment;

import com.nutiteq.components.Components;
import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.tasks.FetchTileTask;
import com.nutiteq.utils.IOUtils;
import com.nutiteq.utils.Utils;

/**
 * Task for loading raster tiles from external storage.
 */
public class ExtFetchTileTask extends FetchTileTask {
  protected String path;

  private int tilesPerFile;
  private int dx;
  private int dy;

  public ExtFetchTileTask(MapTile tile, Components components, long tileIdOffset,
      String path, int tilesPerFile, int dx, int dy) {
    super(tile, components, tileIdOffset);
    this.path = path;
    this.tilesPerFile = tilesPerFile;
    this.dx = dx;
    this.dy = dy;
  }

  @Override
  public void run() {
    super.run();
    String storageState = Environment.getExternalStorageState();
    if (!storageState.equals(Environment.MEDIA_MOUNTED) && !(storageState.equals(Environment.MEDIA_MOUNTED_READ_ONLY))) {
      Log.warning(getClass().getName() + ": Failed to fetch tile. " + "(SD Card not available)");
      cleanUp();
      return;
    }

    File file = new File(path);
    try {
      FileInputStream inputStream = new FileInputStream(file);
      readStream(inputStream);
    } catch (Exception e) {
      Log.error(getClass().getName() + ": Failed to read file " + path + " ex:" + e.getMessage());
    }
    cleanUp();
  }

  @Override
  public void readStream(InputStream inputStream) {
    try {

      // Read header
      int toRead = 6 * tilesPerFile + 2;
      final byte[] header = new byte[toRead];
      long ch = 0;
      int rd = 0;
      while ((rd < toRead) && (ch >= 0)) {
        ch = inputStream.read(header, rd, toRead - rd);
        if (ch > 0) {
          rd += ch;
        }
      }

      // Search for the tile
      final int numberOfTilesStored = (Utils.unsigned(header[0]) << 8) + Utils.unsigned(header[1]);
      int offset = -1;
      int offset2 = -1;
      final int n6 = numberOfTilesStored * 6;
      for (int i6 = 0; i6 < n6; i6 += 6) {
        if ((header[2 + i6] == dx || header[2 + i6] + 256 == dx)
            && (header[3 + i6] == dy || header[3 + i6] + 256 == dy)) {
          offset2 = (Utils.unsigned(header[4 + i6]) << 24) + (Utils.unsigned(header[5 + i6]) << 16)
              + (Utils.unsigned(header[6 + i6]) << 8) + (Utils.unsigned(header[7 + i6]));
          offset = (i6 == 0) ? toRead : ((Utils.unsigned(header[i6 - 2]) << 24)
              + (Utils.unsigned(header[i6 - 1]) << 16) + (Utils.unsigned(header[i6]) << 8) + (Utils
              .unsigned(header[i6 + 1])));
          break;
        }
      }

      if (offset < 0) {
        throw new IllegalArgumentException("Tile not found");
      }

      // Seek
      IOUtils.skip(inputStream, offset - toRead, BUFFER_SIZE);

      // read data
      ch = 0;
      rd = 0;
      toRead = offset2 - offset;
      final byte[] result = new byte[toRead];
      while ((rd < toRead) && (ch >= 0)) {
        ch = inputStream.read(result, rd, (toRead - rd) > BUFFER_SIZE ? BUFFER_SIZE : (toRead - rd));
        if (ch > 0) {
          rd += ch;
        }
      }
      Log.debug("Loaded " + path);
      finished(result);

    } catch (IOException e) {
      Log.error(getClass().getName() + ": Failed to fetch tile. " + e.getMessage());
    } finally {
      try {
        if (inputStream != null) {
          inputStream.close();
        }
      } catch (IOException e) {
        Log.error(getClass().getName() + ": Failed to close the stream. " + e.getMessage());
      }
    }
  }
}