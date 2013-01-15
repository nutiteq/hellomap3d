package com.nutiteq.utils;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;

public class WkbRead {

    /// Big Endian
    private static int wkbXDR = 0;

    /// Little Endian 
    private static  int wkbNDR = 1;

    // geometry types
    private static final int wkbPoint = 1;
    private static final int wkbLineString = 2;
    private static final int wkbPolygon = 3;
    private static final int wkbMultiPoint = 4;
    private static final int wkbMultiLineString = 5;
    private static final int wkbMultiPolygon = 6;
    private static final int wkbGeometryCollection = 7;
    
public static Geometry[] readWkb(ByteArrayInputStream is, Object userData){
    int endinanByte = is.read();
    ByteOrder endian;
    if(endinanByte == 0){
       endian = java.nio.ByteOrder.BIG_ENDIAN;
     }else{
       endian = java.nio.ByteOrder.LITTLE_ENDIAN;  
     }
    
    int type = readInt(is,endian);
    int geometryType = type & 0xff;
    
    boolean hasZ = ((type & 0x80000000) != 0);
    int dimensions = 2;
    if (hasZ){
        dimensions  = 3;
    }
    boolean hasSRID = ((type & 0x20000000) != 0);
    
    int srid = 0;
    if (hasSRID){
        srid = is.read(); // read SRID
        Log.debug("SRID ignored in WKB: "+srid);
    }

    Geometry[] result = null;
    
    switch (geometryType) {
    case wkbPoint :
        result = readPoint(is,dimensions,endian,userData);
        break;
    case wkbLineString :
        result = readLineString(is, dimensions, endian, userData);
        break;
    case wkbPolygon :
        result = readPolygon(is, dimensions, endian, userData);
        break;
    case wkbMultiPoint :
        result = readMultiPoint(is, dimensions, endian, userData);
        break;
    case wkbMultiLineString :
        result = readMultiLineString(is, dimensions, endian, userData);
        break;
    case wkbMultiPolygon :
        result = readMultiPolygon(is, dimensions, endian, userData);
        break;
    case wkbGeometryCollection :
        result = readGeometryCollection(is, dimensions, endian, userData);
        break;
    default:
        Log.error("Unknown geometryType "+geometryType);
}
    return result;
}

private static int readInt(ByteArrayInputStream is, ByteOrder byteOrder) {
    byte buffer[] = new byte[4];
    try {
        is.read(buffer);
    } catch (IOException e) {
        e.printStackTrace();
    }
    return ByteBuffer.wrap(buffer).order(byteOrder).getInt();
 }

private static Geometry[] readGeometryCollection(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
    int numGeoms = readInt(is, endian);
    Geometry[] geoms = new Geometry[numGeoms]; 
    for(int i = 0; i<numGeoms;i++){
        Geometry[] geometry = readWkb(is, userData);
        geoms[i] = geometry[0];
    }
    return geoms;
 }

private static Geometry[] readMultiPolygon(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
    int numPolygons = readInt(is, endian);
    Geometry[] polygons = new Geometry[numPolygons]; 
    for(int i = 0; i<numPolygons;i++){
        Geometry[] geometry = readWkb(is, userData);
        if(geometry == null || !(geometry[0] instanceof Polygon)){
            Log.error("MultiPolygon must have only Polygon elements");
        }else{
            polygons[i] = geometry[0];
        }
    }
    return polygons;
}

private static Geometry[] readMultiLineString(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
    int numLines = readInt(is, endian);
    Geometry[] lines = new Geometry[numLines]; 
    for(int i = 0; i<numLines;i++){
        
        Geometry[] geometry = readWkb(is, userData);
        if(!(geometry[0] instanceof Line)){
            Log.error("MultiLineString must have only Line elements");
        }else{
            lines[i] = geometry[0];
        }
    }
    return lines;
}

private static Geometry[] readMultiPoint(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
    Log.error("Not implemented readMultiPoint WKB reader");
    return null;
}

private static Geometry[] readPolygon(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
    int numRings = readInt(is, endian);
    int size = readInt(is, endian);
    List<MapPos> outerRing = readCoordinateList(is, dimensions, size, endian);
    
    List<List<MapPos>> innerRings = null;
    
    if(numRings > 0){
        innerRings = new LinkedList<List<MapPos>>();
        for (int i = 0; i < numRings; i++){
            int innerSize = is.read();
            List<MapPos> innerRing = readCoordinateList(is, dimensions, innerSize, endian);
            innerRings.add(innerRing);
        }
    }
    
    return new Geometry[]{new Polygon(outerRing, innerRings, null, (PolygonStyle) null, userData)};
}

private static Geometry[] readLineString(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
 
    int size = readInt(is,endian);
    
    return new Geometry[]{new Line(readCoordinateList(is, dimensions, size, endian), null, (LineStyle) null, userData)};
}


private static Geometry[] readPoint(ByteArrayInputStream is, int dimensions, ByteOrder endian, Object userData) {
    return new Geometry[]{new Point(readCoordinate(is,dimensions,endian), null, (PointStyle) null, userData)};
}


private static List<MapPos> readCoordinateList(ByteArrayInputStream is,
        int dimensions, int size, ByteOrder endian) {
    if(size == 0)
        return null;
    
    List<MapPos> mapPoses = new ArrayList<MapPos>();
    for (int i = 0; i < size; i++) {
        mapPoses.add(readCoordinate(is,dimensions,endian));
    }    
    return mapPoses;
}

private static MapPos readCoordinate(ByteArrayInputStream is, int dimensions, ByteOrder endian) {
    double x = 0;
    double y = 0;
    double z = 0;
    byte buffer[] = new byte[8];
    try {
        is.read(buffer);
        x = ByteBuffer.wrap(buffer).order(endian).getDouble();
        is.read(buffer);
        y = ByteBuffer.wrap(buffer).order(endian).getDouble();
        if (dimensions == 3) {
            is.read(buffer);
            z = ByteBuffer.wrap(buffer).order(endian).getDouble();
        }
    } catch (IOException e) {
        Log.error("read coordinate error "+e.getMessage());
    }
    return new MapPos(x,y,z);
}
}
