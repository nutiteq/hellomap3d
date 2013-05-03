package com.nutiteq.utils;

import java.util.Vector;

import org.proj4.Proj4;
import org.proj4.ProjectionData;

import com.jhlabs.map.proj.Projection;
import com.jhlabs.map.proj.ProjectionFactory;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.log.Log;

public class GeoUtils {

    /**
     * Reproject bounding box using javaProj library. Slower and not so stable
     * than Proj.4 but does not require NDK
     * 
     * @param bbox
     *            input bounding box
     * @param fromProj
     *            proj4 text
     * @param toProj
     * @return projected bounding box
     */
    public static Envelope transformBboxJavaProj(Envelope bbox, String fromProj,
            String toProj) {

        Projection projection = ProjectionFactory
                .fromPROJ4Specification(fromProj.split(" "));
        com.jhlabs.map.Point2D.Double minA = new com.jhlabs.map.Point2D.Double(
                bbox.getMinX(), bbox.getMinY());
        com.jhlabs.map.Point2D.Double maxA = new com.jhlabs.map.Point2D.Double(
                bbox.getMaxX(), bbox.getMaxY());
        com.jhlabs.map.Point2D.Double minB = new com.jhlabs.map.Point2D.Double();
        com.jhlabs.map.Point2D.Double maxB = new com.jhlabs.map.Point2D.Double();
        projection.inverseTransform(minA, minB);
        projection.inverseTransform(maxA, maxB);
        Log.debug("converted to wgs84:" + minA + " " + minB + " " + maxA + " "
                + maxB);
        // then from Wgs84 to Layer SRID
        Projection projection2 = ProjectionFactory
                .fromPROJ4Specification(toProj.split(" "));
        com.jhlabs.map.Point2D.Double minC = new com.jhlabs.map.Point2D.Double();
        com.jhlabs.map.Point2D.Double maxC = new com.jhlabs.map.Point2D.Double();
        projection2.transform(minB, minC);
        projection2.transform(maxB, maxC);

        Log.debug("converted to Layer SRID:" + minA + " " + minB + " " + maxA
                + " " + maxB);

        return new Envelope(minC.x, maxC.x, minC.y, maxC.y);
    }
    
    /**
     * Reproject bounding box using Proj.4 (NDK) library
     * 
     * @param bbox
     *            input bounding box
     * @param fromProj
     *            proj4 text
     * @param toProj
     * @return projected bounding box
     */

    public static Envelope transformBboxProj4(Envelope bbox, String fromProj,
            String toProj) {
        ProjectionData dataTP = new ProjectionData(new double[][] {
                { bbox.getMinX(), bbox.getMinY() },
                { bbox.getMaxX(), bbox.getMaxY() } }, new double[] { 0, 0 });

        Proj4 toWgsProjection = new Proj4(fromProj, toProj);
        toWgsProjection.transform(dataTP, 2, 1);

        return new Envelope(dataTP.x[0], dataTP.x[1], dataTP.y[0], dataTP.y[1]);
    }

      /**
       * 
       * decompress Google Polyline Encoding Format. Code ported from http://open.mapquestapi.com/common/encodedecode.html sample
       * 
       * @param encoded
       * @param precision , usually 5
       * @param proj result is converted from WGS84 to this projection
       * @return Vector of MapPos in proj Projection
       */
    public static Vector<MapPos> decompress(final String encoded, final double precision, com.nutiteq.projections.Projection proj) {
          double precisionMultiple = Math.pow(10, -precision);
          int len = encoded.length();
          int index = 0;
          int lat = 0;
          int lng = 0;
          Vector<MapPos> array = new Vector<MapPos>();
          
          while (index < len) {
             int b;
             int shift = 0;
             int result = 0;
             do {
                b = encoded.codePointAt(index++) - 63;
                result |= (b & 0x1f) << shift;
                shift += 5;
             } while (b >= 0x20);
             int dlat = (((result & 1)==1) ? ~(result >> 1) : (result >> 1));
             lat += dlat;
             shift = 0;
             result = 0;
             do {
                b = encoded.codePointAt(index++) - 63;
                result |= (b & 0x1f) << shift;
                shift += 5;
             } while (b >= 0x20);
             double dlng = (((result & 1)==1) ? ~(result >> 1) : (result >> 1));
             lng += dlng;
             
             array.add(proj.fromWgs84(lng * precisionMultiple, lat * precisionMultiple));
          }
          return array;
       }

    
}
