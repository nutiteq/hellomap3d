package com.nutiteq.utils;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;


public class TileUtils {
    
    private static final double TILESIZE = 256;
    private static final double initialResolution = 2.0f * Math.PI * 6378137.0f
            / TILESIZE;
    private static final double originShift = 2.0f * Math.PI * 6378137.0f / 2.0f;

    // following is from
    // http://code.google.com/p/gmap-tile-generator/source/browse/trunk/gmaps-tile-creator/src/gov/ca/maps/tile/geom/GlobalMercator.java?r=15
    /**
     * Returns tile for given Mercator coordinates
     * 
     * @return
     */
    public static MapPos MetersToTile(MapPos mp, int zoom) {
        int[] p = MetersToPixels(mp.x, mp.y, zoom);
        return PixelsToTile(p[0], p[1], zoom);
    }

    /**
     * Returns a tile covering region in given pixel coordinates
     * 
     * @param px
     * @param py
     * @param zoom 
     * @return
     */
    public static MapPos PixelsToTile(int px, int py, int zoom) {
        int tx = (int) Math.ceil(px / ((double) TILESIZE) - 1);
        int ty = (int) Math.ceil(py / ((double) TILESIZE) - 1);
        return new MapPos(tx, (1<<(zoom))-1-ty);
    }

    /**
     * Converts EPSG:900913 to pyramid pixel coordinates in given zoom level
     * 
     * @param mx
     * @param my
     * @param zoom
     * @return
     */
    public static int[] MetersToPixels(double mx, double my, int zoom) {
        double res = Resolution(zoom);
        int px = (int) Math.round((mx + originShift) / res);
        int py = (int) Math.round((my + originShift) / res);
        return new int[] { px, py };
    }

    /**
     * Resolution (meters/pixel) for given zoom level (measured at Equator)
     * 
     * @return
     */
    public static double Resolution(int zoom) {
        // return (2 * Math.PI * 6378137) / (this.tileSize * 2**zoom)
        return initialResolution / Math.pow(2, zoom);
    }
    
    /**
     * Returns bounds of the given tile in EPSG:900913 coordinates
     * 
     * @param tx
     * @param ty
     * @param zoom
     * @return
     */
    public static Envelope TileBounds(int tx, int ty, int zoom) {
            double[] min = PixelsToMeters(tx * TILESIZE, ty * TILESIZE, zoom);
            double minx = min[0], miny = min[1];
            double[] max = PixelsToMeters((tx + 1) * TILESIZE, (ty + 1) * TILESIZE,
                            zoom);
            double maxx = max[0], maxy = max[1];
            
            return new Envelope( minx, maxx, miny, maxy);
    }

    
    /**
     * Converts XY point from Spherical Mercator EPSG:900913 to lat/lon in WGS84
     * Datum
     * 
     * @return
     */
    public static double[] MetersToLatLon(double mx, double my) {

            double lon = (mx / originShift) * 180.0;
            double lat = (my / originShift) * 180.0;

            lat = 180
                            / Math.PI
                            * (2 * Math.atan(Math.exp(lat * Math.PI / 180.0)) - Math.PI / 2.0);
            return new double[] { lat, lon };
    }
    
    /**
     * Converts pixel coordinates in given zoom level of pyramid to EPSG:900913
     * 
     * @return
     */
    public static double[] PixelsToMeters(double px, double py, int zoom) {
            double res = Resolution(zoom);
            double mx = px * res - originShift;
            double my = originShift - (py * res);
            return new double[] { mx, my };
    }

    
}
