package com.nutiteq.editable.datasources;

import com.nutiteq.datasources.vector.SpatialiteDataSource;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.projections.Projection;

/**
 * 
 * Local Spatialite database data source that supports editing. 
 * 
 * @author mtehver
 *
 */
public abstract class EditableSpatialiteDataSource extends SpatialiteDataSource implements EditableVectorDataSource<Geometry> {

    /**
     * Default constructor.
     * 
     * @param proj Layer projection
     * @param dbPath Spatialite file name full path 
     * @param tableName table name for data
     * @param geomColumnName column in tableName which has geometries
     * @param userColumns include values from these additional columns to userData
     * @param filter extra filter expression for SQL
     */
    public EditableSpatialiteDataSource(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, String filter) {
        super(proj, dbPath, tableName, geomColumnName, userColumns, filter);
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
