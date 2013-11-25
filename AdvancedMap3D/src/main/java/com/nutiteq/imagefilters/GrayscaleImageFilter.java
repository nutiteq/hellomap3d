package com.nutiteq.imagefilters;

import android.graphics.Bitmap;
import android.graphics.Color;
import com.nutiteq.rasterdatasources.ImageFilterRasterDataSource.ImageFilter;

/**
 * Image filter that converts source image to gray scale image using standard luminance calculation.
 *  
 * @author mark
 *
 */
public class GrayscaleImageFilter implements ImageFilter {

  @Override
  public Bitmap filter(Bitmap source) {
    Bitmap target = Bitmap.createBitmap(source.getWidth(), source.getHeight(), Bitmap.Config.ARGB_8888);
    int[] row = new int[source.getWidth()];
    for (int y = 0; y < source.getHeight(); y++) {
      source.getPixels(row, 0, source.getWidth(), 0, y, source.getWidth(), 1);
      for (int x = 0; x < row.length; x++) {
        int c = row[x];
        int luma = (77 * Color.red(c) + 151 * Color.green(c) + 28 * Color.blue(c)) >> 8;
        row[x] = Color.argb(Color.alpha(c), luma, luma, luma);
      }
      target.setPixels(row, 0, target.getWidth(), 0, y, target.getWidth(), 1);
    }
    return target;
  }

}
