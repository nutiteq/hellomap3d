package com.nutiteq.editable.datasources;

import com.nutiteq.vectordatasources.VectorDataSource;
import com.nutiteq.geometry.VectorElement;

/**
 * Interface for all editable vector data sources.
 * Adds methods for inserting, updating and deleting vector elements.
 *
 * @author mtehver
 */
public interface EditableVectorDataSource<T extends VectorElement> extends VectorDataSource<T> {
	/**
	 * Insert new element into data source
	 *
	 * @param element
	 *          element to insert
	 */
	long insertElement(T element);

	/**
	 * Update existing element in data source
	 *
	 * @param id
	 *          element id to update
	 * @param element
	 *          element to update
	 */
	void updateElement(long id, T element);

	/**
	 * Delete existing element from data source
	 *
	 * @param id
	 *          element id to delete
	 */
	void deleteElement(long id);
}
