package com.nutiteq.editable.datasources;

import java.io.IOException;

import com.nutiteq.datasources.vector.OGRVectorDataSource;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.projections.Projection;

/**
 * 
 * Local OGR file data source that supports editing. 
 * 
 * @author mtehver
 *
 */
public abstract class EditableOGRVectorDataSource extends OGRVectorDataSource implements EditableVectorDataSource<Geometry> {

    /**
     * Default constructor.
     * 
     * @param proj layer projection. NB! data must be in the same projection
     * @param fileName datasource name: file or connection string
     * @param tableName table (OGR layer) name, needed for multi-layer datasets. If null, takes the first layer from dataset
     * @throws IOException file not found or other problem opening OGR datasource
     */
    public EditableOGRVectorDataSource(Projection proj, String fileName, String tableName) throws IOException {
        super(proj, fileName, tableName, true);
    }

    @Override
    public long insertElement(Geometry element) {
        return ogrHelper.insertElement(element);      
    }

    @Override
    public void updateElement(long id, Geometry element) {
        ogrHelper.updateElement(id, element);
    }

    @Override
    public void deleteElement(long id) {
        ogrHelper.deleteElement(id);
    }

}
