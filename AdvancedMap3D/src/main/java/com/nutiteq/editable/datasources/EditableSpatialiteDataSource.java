package com.nutiteq.editable.datasources;

import com.nutiteq.datasources.vector.SpatialiteDataSource;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;

/**
 * 
 * Local Spatialite database data source that supports editing. 
 * 
 * @author mtehver
 *
 */
public class EditableSpatialiteDataSource extends SpatialiteDataSource implements EditableVectorDataSource<Geometry> {

	/**
	 * Default constructor.
	 * 
	 * @param proj Layer projection
	 * @param dbPath Spatialite file name full path 
	 * @param tableName table name for data
	 * @param geomColumnName column in tableName which has geometries
	 * @param userColumns include values from these additional columns to userData
	 * @param maxObjects maximum number of loaded objects, suggested <2000 or so
	 * @param pointStyleSet required if layer has points
	 * @param lineStyleSet required if layer has lines
	 * @param polygonStyleSet required if layer has lines
	 */
	public EditableSpatialiteDataSource(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, int maxObjects,
			StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet) {
		super(proj, dbPath, tableName, geomColumnName, userColumns, maxObjects, pointStyleSet, lineStyleSet, polygonStyleSet);
	}

	@Override
	public long insertElement(Geometry element) {
		return spatialLite.insertSpatiaLiteGeom(dbLayer, element);		
	}

	@Override
	public void updateElement(long id, Geometry element) {
		spatialLite.updateSpatiaLiteGeom(dbLayer, id, element);
	}
	
	@Override
	public void deleteElement(long id) {
		spatialLite.deleteSpatiaLiteGeom(dbLayer, id);
	}
	
}
