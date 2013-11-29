package com.nutiteq.projections;

import com.nutiteq.components.Bounds;

/**
 * EPSG:32635 uses WGS 84 / UTM zone 35N.
 */
public class EPSG32635 extends JavaProjProjection {
  private static final String ARGS[] = "+proj=utm +zone=35 +ellps=WGS84 +datum=WGS84 +units=m +no_defs  <>".split(" "); 

  // TODO jaakl: these are global bounds in Google Mercator, should be in this projection
  private static final Bounds BOUNDS = new Bounds(-20037508.34f, 20037508.34f, 20037508.34f, -20037508.34f);

  /**
   * Default constructor.
   */
  public EPSG32635() {
    super(ARGS, BOUNDS);
  }
  
  @Override
  public String name() {
    return "EPSG:32635";
  }
}
