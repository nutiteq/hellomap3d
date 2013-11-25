package com.nutiteq.imagefilters;

import android.graphics.Bitmap;
import android.graphics.Color;
import com.nutiteq.rasterdatasources.ImageFilterRasterDataSource.ImageFilter;

/**
 * Image filter that creates 'night mode' like effect.
 * 
 * @author nutiteq
 */
public class NightModeImageFilter implements ImageFilter {

  @Override
  public Bitmap filter(Bitmap source) {
    Bitmap target = Bitmap.createBitmap(source.getWidth(), source.getHeight(), Bitmap.Config.ARGB_8888);
    int[] row = new int[source.getWidth()];
    for (int y = 0; y < source.getHeight(); y++) {
      source.getPixels(row, 0, source.getWidth(), 0, y, source.getWidth(), 1);
      for (int x = 0; x < row.length; x++) {
        int c = row[x];
        //  the constant makes balance between 768 (3*256) and 1024 (4*256) because we divide by 4 instead of 3 so we lost some contrast
        int r = (820 - (Color.red(c) + Color.green(c) + Color.blue(c))) >> 2;  
        row[x] = Color.argb(255, r, 0, 0);
      }
      target.setPixels(row, 0, target.getWidth(), 0, y, target.getWidth(), 1);
    }
    return target;
  }
}
