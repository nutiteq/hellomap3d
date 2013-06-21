package com.nutiteq.utils;

import java.util.List;

import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;

public class WktWriter {

	public static String writeWkt(Geometry geom, String type) {
		type = (type == null ? "" : type);
		StringBuilder builder = new StringBuilder();
		if (geom instanceof Point) {
			Point point = (Point) geom;
			builder.append(type.equals("MULTIPOINT") ? "MULTIPOINT((" : "POINT(");
			builder.append(point.getMapPos().x).append(" ").append(point.getMapPos().y);
			builder.append(type.equals("MULTIPOINT") ? "))" : ")");
		} else if (geom instanceof Line) {
			Line line = (Line) geom;
			builder.append(type.equals("MULTILINESTRING") ? "MULTILINESTRING((" : "LINESTRING(");
			int count = 0;
			for (MapPos mapPos : line.getVertexList()) {
				if (count++ > 0) builder.append(",");
				builder.append(mapPos.x).append(" ").append(mapPos.y);
			}
			builder.append(type.equals("MULTILINESTRING") ? "))" : ")");
		} else if (geom instanceof Polygon) {
			Polygon polygon = (Polygon) geom;
			builder.append(type.equals("MULTIPOLYGON") ? "MULTIPOLYGON(((" : "POLYGON((");
			int count = 0;
			List<MapPos> vertexList = polygon.getVertexList();
			for (int i = 0; i <= vertexList.size(); i++) {
				if (count++ > 0) builder.append(",");
				MapPos mapPos = vertexList.get(i % vertexList.size());
				builder.append(mapPos.x).append(" ").append(mapPos.y);
			}
			builder.append(")");
			if (polygon.getHolePolygonList() != null) {
				for (List<MapPos> holeList : polygon.getHolePolygonList()) {
					builder.append(",(");
					count = 0;
					for (int i = 0; i <= holeList.size(); i++) {
						if (count++ > 0) builder.append(",");
						MapPos mapPos = holeList.get(i % holeList.size());
						builder.append(mapPos.x).append(" ").append(mapPos.y);
					}
					builder.append(")");
				}
			}
			builder.append(type.equals("MULTIPOLYGON") ? "))" : ")");
		} else {
			Log.error("WktWriter: unsupported geometry type!");
		}
		return builder.toString();
	}
}
