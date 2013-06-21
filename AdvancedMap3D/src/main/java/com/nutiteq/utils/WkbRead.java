package com.nutiteq.utils;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

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
	
	// Big Endian
	private static int wkbXDR = 0;

	// Little Endian 
	private static  int wkbNDR = 1;

	// geometry types
	private static final int wkbPoint = 1;
	private static final int wkbLineString = 2;
	private static final int wkbPolygon = 3;
	private static final int wkbMultiPoint = 4;
	private static final int wkbMultiLineString = 5;
	private static final int wkbMultiPolygon = 6;
	private static final int wkbGeometryCollection = 7;

	public interface GeometryFactory {
		Point createPoint(MapPos mapPos, int srid);
		Line createLine(List<MapPos> points, int srid);
		Polygon createPolygon(List<MapPos> outerRing, List<List<MapPos>> innerRings, int srid);
		Geometry[] createMultigeometry(List<Geometry> geometry);
	}
	
	public static class DefaultGeometryFactory implements GeometryFactory {
		final Object userData;
		
		public DefaultGeometryFactory(Object userData) {
			this.userData = userData;
		}
		
		@Override
		public Point createPoint(MapPos mapPos, int srid) {
			return new Point(mapPos, null, (PointStyle) null, userData);
		}

		@Override
		public Line createLine(List<MapPos> points, int srid) {
			return new Line(points, null, (LineStyle) null, userData);
		}
		
		@Override
		public Polygon createPolygon(List<MapPos> outerRing, List<List<MapPos>> innerRings, int srid) {
			return new Polygon(outerRing, innerRings, null, (PolygonStyle) null, userData);
		}
		
		@Override
		public Geometry[] createMultigeometry(List<Geometry> geomList) {
			Geometry[] geomArray = new Geometry[geomList.size()];
			geomList.toArray(geomArray);
			return geomArray;
		}
	}

	public static Geometry[] readWkb(ByteArrayInputStream is, Object userData){
		GeometryFactory factory = new DefaultGeometryFactory(userData);
		return readWkb(is, factory);
	}

	public static Geometry[] readWkb(ByteArrayInputStream is, GeometryFactory factory){
		int endianByte = is.read();
		ByteOrder endian;
		if (endianByte == wkbXDR) {
			endian = java.nio.ByteOrder.BIG_ENDIAN;
		} else if (endianByte == wkbNDR) {
			endian = java.nio.ByteOrder.LITTLE_ENDIAN;  
		} else {
			Log.error("Unknown endianess, using little mode");
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
			srid = readInt(is,endian); // read SRID
		}

		Geometry[] result = null;
		switch (geometryType) {
		case wkbPoint :
			result = readPoint(is, dimensions, endian, srid, factory);
			break;
		case wkbLineString :
			result = readLineString(is, dimensions, endian, srid, factory);
			break;
		case wkbPolygon :
			result = readPolygon(is, dimensions, endian, srid, factory);
			break;
		case wkbMultiPoint :
			result = readMultiPoint(is, dimensions, endian, srid, factory);
			break;
		case wkbMultiLineString :
			result = readMultiLineString(is, dimensions, endian, srid, factory);
			break;
		case wkbMultiPolygon :
			result = readMultiPolygon(is, dimensions, endian, srid, factory);
			break;
		case wkbGeometryCollection :
			result = readGeometryCollection(is, dimensions, endian, srid, factory);
			break;
		default:
			Log.error("Unknown geometryType "+geometryType);
		}
		return result;
	}

	private static Geometry[] readGeometryCollection(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		int numGeoms = readInt(is, endian);
		List<Geometry> geomList = new ArrayList<Geometry>(numGeoms + 1);
		for (int i = 0; i<numGeoms;i++){
			Geometry[] geometry = readWkb(is, factory);
			for(int j = 0; j<geometry.length; j++) {
				geomList.add(geometry[j]);
			}
		}
		return factory.createMultigeometry(geomList);
	}

	private static Geometry[] readMultiPolygon(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		int numPolygons = readInt(is, endian);
		List<Geometry> polyList = new ArrayList<Geometry>(numPolygons + 1);
		for (int i = 0; i<numPolygons; i++){
			Geometry[] geometry = readWkb(is, factory);
			if (geometry == null) {
				continue;
			}
			for(int j = 0; j < geometry.length; j++) {
				if(!(geometry[j] instanceof Polygon)){
					Log.error("MultiPolygon must have only Polygon elements");
				} else {
					polyList.add(geometry[j]);
				}
			}
		}
		return factory.createMultigeometry(polyList);
	}

	private static Geometry[] readMultiLineString(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		int numLines = readInt(is, endian);
		List<Geometry> lineList = new ArrayList<Geometry>(numLines + 1);
		for (int i = 0; i<numLines; i++){
			Geometry[] geometry = readWkb(is, factory);
			if (geometry == null) {
				continue;
			}
			for(int j = 0; j < geometry.length; j++) {
				if(!(geometry[j] instanceof Line)){
					Log.error("MultiLineString must have only Line elements");
				} else {
					lineList.add(geometry[j]);
				}
			}
		}
		return factory.createMultigeometry(lineList);
	}

	private static Geometry[] readMultiPoint(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		int numPoints = readInt(is, endian);
		List<Geometry> pointList = new ArrayList<Geometry>(numPoints + 1);
		for (int i = 0; i<numPoints; i++){
			Geometry[] geometry = readWkb(is, factory);
			if (geometry == null) {
				continue;
			}
			for(int j = 0; j < geometry.length; j++) {
				if(!(geometry[j] instanceof Point)){
					Log.error("MultiPoint must have only Point elements");
				} else {
					pointList.add(geometry[j]);
				}
			}
		}
		return factory.createMultigeometry(pointList);
	}

	private static Geometry[] readPolygon(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		int numRings = readInt(is, endian);
		if (numRings < 1) {
			return new Geometry[0];
		}

		int size = readInt(is, endian);
		List<MapPos> outerRing = readCoordinateList(is, dimensions, size, endian);
		if (outerRing.size() > 1) {
			if (outerRing.get(0).equals(outerRing.get(outerRing.size() - 1))) {
				outerRing.remove(0);
			}
		}

		List<List<MapPos>> innerRings = null;
		if (numRings > 1){
			innerRings = new LinkedList<List<MapPos>>();
			for (int i = 1; i < numRings; i++){
				int innerSize = readInt(is, endian);
				List<MapPos> innerRing = readCoordinateList(is, dimensions, innerSize, endian);
				if (innerRing.size() > 1) {
					if (innerRing.get(0).equals(innerRing.get(innerRing.size() - 1))) {
						innerRing.remove(0);
					}
				}
				innerRings.add(innerRing);
			}
		}

		return new Geometry[]{ factory.createPolygon(outerRing, innerRings, srid) };
	}

	private static Geometry[] readLineString(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		int size = readInt(is,endian);
		return new Geometry[] { factory.createLine(readCoordinateList(is, dimensions, size, endian), srid) };
	}

	private static Geometry[] readPoint(ByteArrayInputStream is, int dimensions, ByteOrder endian, int srid, GeometryFactory factory) {
		return new Geometry[] { factory.createPoint(readCoordinate(is,dimensions,endian), srid) };
	}

	private static List<MapPos> readCoordinateList(ByteArrayInputStream is, int dimensions, int size, ByteOrder endian) {
		if(size == 0) {
			return null;
		}

		List<MapPos> mapPoses = new ArrayList<MapPos>(size);
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

	private static int readInt(ByteArrayInputStream is, ByteOrder byteOrder) {
		byte buffer[] = new byte[4];
		try {
			is.read(buffer);
		} catch (IOException e) {
			e.printStackTrace();
		}
		return ByteBuffer.wrap(buffer).order(byteOrder).getInt();
	}

}
