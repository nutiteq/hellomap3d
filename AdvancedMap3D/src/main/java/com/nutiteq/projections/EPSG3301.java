package com.nutiteq.projections;

import com.nutiteq.components.Bounds;

/**
 * EPSG:3301 is L-EST projection, Estonian official (Lambert Conformal Conical)
 */
public class EPSG3301 extends JavaProjProjection {
  private static final String ARGS[] = "+proj=lcc +lat_1=59.33333333333334 +lat_2=58 +lat_0=57.51755393055556 +lon_0=24 +x_0=500000 +y_0=6375000 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs"
      .split(" ");
  
  // SDK 1.0 params:
  public static final int MIN_E = -211000; 
  public static final int MIN_N = 5732000; 
  public static final int MAX_E = 1325000; 
  public static final int MAX_N = 7268000; 

  // Estonian bounds in LEST
  private static final Bounds BOUNDS = new Bounds(MIN_E, MAX_N, MAX_E, MIN_N);
  
  /**
   * Default constructor.
   */
  public EPSG3301() {
    super(ARGS, BOUNDS);
  }
  
  @Override
  public String name() {
    return "EPSG:3301";
  }

}
