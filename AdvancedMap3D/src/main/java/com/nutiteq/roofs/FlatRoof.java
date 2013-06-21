package com.nutiteq.roofs;

import com.vividsolutions.jts.geom.Geometry;

public class FlatRoof extends Roof {

  public FlatRoof() {
    super(0.0f, true);
  }

  @Override
  public void calculateRoof(Geometry minRectangle) {
  }

  @Override
  public double calculateRoofPointHeight(double x, double y) {
    return 0;
  }
}
