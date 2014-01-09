package com.nutiteq.editable.datasources;

import java.io.IOException;

import com.nutiteq.datasources.vector.SpatialiteDataSource;
import com.nutiteq.db.SpatialLiteDbHelper;
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
     * @throws IOException 
     */
    public EditableSpatialiteDataSource(Projection proj, String dbPath, String tableName, String geomColumnName, String[] userColumns, String filter) throws IOException {
        super(proj, dbPath, tableName, geomColumnName, userColumns, filter);
    }

    /**
     * Construct data source with the SpatialLiteDb already opened, and filters
     * 
     * @param proj data source projection
     * @param spatialLiteDb Spatialite database
     * @param tableName table from the database
     * @param geomColumnName geometry column from the table
     * @param userColumns load data from these columns as userData
     * @param filter SQL filter to select some objects, used for WHERE
     */
    public EditableSpatialiteDataSource(Projection proj, SpatialLiteDbHelper spatialLiteDb, String tableName, String geomColumnName, String[] userColumns, String filter) {
        super(proj, spatialLiteDb, tableName, geomColumnName, userColumns, filter);
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
