package com.nutiteq.datasources.vector;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.db.SpatialLiteDbHelper;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

// TODO: implement simplification, as in previous layer implementation
public class SpatialiteDataSource extends AbstractVectorDataSource<Geometry> {
	protected final SpatialLiteDbHelper spatialLite;
	protected SpatialLiteDbHelper.DbLayer dbLayer;

	protected final StyleSet<PointStyle> pointStyleSet;
	protected final StyleSet<LineStyle> lineStyleSet;
	protected final StyleSet<PolygonStyle> polygonStyleSet;
	protected int minZoom;

    private int autoSimplifyPixels;
	private int maxObjects;
	private String[] userColumns;
	private String filter;

	public SpatialiteDataSource(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, int maxObjects,
			StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet) {
	    super(proj);
		this.pointStyleSet = pointStyleSet;
		this.lineStyleSet = lineStyleSet;
		this.polygonStyleSet = polygonStyleSet;

		this.userColumns = userColumns;
		this.maxObjects = maxObjects;

		this.spatialLite = new SpatialLiteDbHelper(dbPath);

        Map<String, SpatialLiteDbHelper.DbLayer> dbLayers = spatialLite.qrySpatialLayerMetadata();
        for (String layerKey : dbLayers.keySet()) {
          SpatialLiteDbHelper.DbLayer layer = dbLayers.get(layerKey);
            if (layer.table.compareTo(tableName) == 0
                    && layer.geomColumn.compareTo(geomColumnName) == 0) {
                this.dbLayer = layer;
                break;
            }
        }

		if (this.dbLayer == null) {
			Log.error("SpatialiteLayer: Could not find a matching layer " + tableName + "." + geomColumnName);
		}

		if (pointStyleSet != null) {
			minZoom = Math.min(minZoom, pointStyleSet.getFirstNonNullZoomStyleZoom());
		}
		if (lineStyleSet != null) {
			minZoom = Math.min(minZoom, lineStyleSet.getFirstNonNullZoomStyleZoom());
		}
		if (polygonStyleSet != null) {
			minZoom = Math.min(minZoom, polygonStyleSet.getFirstNonNullZoomStyleZoom());
		}
	}

	public String[] getUserColumns() {
		return userColumns;
	}
	
	@Override
	public Envelope getDataExtent() {
		return null; // TODO: implement
	}
	
	@Override
	public Collection<Geometry> loadElements(CullState cullState) {
		if (dbLayer == null) {
			return null;
		}

		if (cullState.zoom < minZoom) {
			return null;
		}

		Envelope envelope = projection.fromInternal(cullState.envelope);
		List<Geometry> queryList = spatialLite.qrySpatiaLiteGeom(envelope, maxObjects, dbLayer, userColumns, null, 0, 0);
		List<Geometry> elements = new ArrayList<Geometry>(queryList.size() + 1); 
		for (Geometry element : queryList) {
			Label label = createLabel(element.userData);
		
			Geometry newElement = null;
			if (element instanceof Point) {
				newElement = new Point(((Point) element).getMapPos(), label, pointStyleSet, element.userData);
			} else if (element instanceof Line) {
				newElement = new Line(((Line) element).getVertexList(), label, lineStyleSet, element.userData);
			} else if (element instanceof Polygon) {
				newElement = new Polygon(((Polygon) element).getVertexList(), ((Polygon) element).getHolePolygonList(), label, polygonStyleSet, element.userData);
			}

			elements.add(newElement);
		}
		return elements;
	}
	
	@SuppressWarnings("unchecked")
	protected Label createLabel(Object userData) {
		StringBuffer labelTxt = new StringBuffer();
		for(Map.Entry<String, String> entry : ((Map<String, String>) userData).entrySet()){
			labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
		}
		return new DefaultLabel("Data:", labelTxt.toString());
	}
	
}
