package com.nutiteq.editable.layers.deprecated;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

import android.content.Context;
import android.widget.Toast;

import com.nutiteq.components.Envelope;
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
import com.nutiteq.ui.Label;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.vectorlayers.GeometryLayer;


/**
 * Vector Layer with extra functions for editing elements
 * Supports all kind of Geometries: Markers, Points, Lines and Polygons
 * 
 * @author mtehver
 *
 */
@Deprecated
public abstract class EditableGeometryDbLayer extends GeometryLayer {
	protected StyleSet<PointStyle> pointStyleSet;
	protected StyleSet<LineStyle> lineStyleSet;
	protected StyleSet<PolygonStyle> polygonStyleSet;

	protected int minZoom;

	private LongHashMap<Geometry> currentElementMap = new LongHashMap<Geometry>();
	private LongHashMap<Geometry> editedElementMap = new LongHashMap<Geometry>();
	private Context context;

	public EditableGeometryDbLayer(Projection proj, StyleSet<PointStyle> pointStyleSet, StyleSet<LineStyle> lineStyleSet, StyleSet<PolygonStyle> polygonStyleSet, Context context) {
		super(proj);
		this.pointStyleSet = pointStyleSet;
		this.lineStyleSet = lineStyleSet;
		this.polygonStyleSet = polygonStyleSet;
		this.context = context;
		
		if (pointStyleSet != null) {
			minZoom = pointStyleSet.getFirstNonNullZoomStyleZoom();
		}
		if (lineStyleSet != null) {
			minZoom = lineStyleSet.getFirstNonNullZoomStyleZoom();
		}
		if (polygonStyleSet != null) {
			minZoom = polygonStyleSet.getFirstNonNullZoomStyleZoom();
		}
	}

	public synchronized LongHashMap<Geometry> saveState() {
		return cloneElementMap(editedElementMap);
	}

	public synchronized void loadState(LongHashMap<Geometry> elementMap) {
		for (Iterator<LongHashMap.Entry<Geometry>> it = editedElementMap.entrySetIterator(); it.hasNext(); ) {
			LongHashMap.Entry<Geometry> entry = it.next();
			long id = entry.getKey();
			Geometry element = entry.getValue();
			if (element != null) {
				element.detachFromLayer();
			}
			currentElementMap.remove(id);
		}

		editedElementMap = cloneElementMap(elementMap);

		for (Iterator<LongHashMap.Entry<Geometry>> it = editedElementMap.entrySetIterator(); it.hasNext(); ) {
			LongHashMap.Entry<Geometry> entry = it.next();
			long id = entry.getKey();
			Geometry element = entry.getValue();
			if (element != null) {
				element.attachToLayer(this);
				element.setActiveStyle(getCurrentZoomLevel());
				currentElementMap.put(id, element);
			} else {
				currentElementMap.remove(id);
			}
		}
		setVisibleElementsList(new ArrayList<Geometry>(currentElementMap.values()));

		updateVisibleElements();
	}
	
	public StyleSet<PointStyle> getPointStyleSet() {
	  return pointStyleSet;
	}
	
	public StyleSet<LineStyle> getLineStyleSet() {
	  return lineStyleSet;
	}
	
	public StyleSet<PolygonStyle> getPolygonStyleSet() {
	  return polygonStyleSet;
	}

	@Override
	public void addAll(Collection<? extends Geometry> elements) {
		for (Geometry element : elements) {
			element.attachToLayer(this);
			element.setActiveStyle(getCurrentZoomLevel());
			onElementCreated(element);
		}

		updateVisibleElements();
	}

	@Override
	public void removeAll(Collection<? extends Geometry> elements) {
		for (Geometry element : elements) {
			element.detachFromLayer();
			onElementDeleted(element);
		}

		updateVisibleElements();
	}

	public void update(Geometry element) {
		onElementChanged(element);
	}

	@Override
	public void calculateVisibleElements(Envelope envelope, int zoom) {
		if (zoom < minZoom) {
			setVisibleElementsList(null);
			return;
		}

		LongHashMap<Geometry> objectMap = queryElements(envelope, zoom);
		LongHashMap<Geometry> newElementMap = new LongHashMap<Geometry>(); 

		synchronized (this) {
			// apply styles, create new objects for these
			for (Iterator<LongHashMap.Entry<Geometry>> it = objectMap.entrySetIterator(); it.hasNext(); ){
				LongHashMap.Entry<Geometry> entry = it.next();
				long id = entry.getKey();
				Geometry object = entry.getValue();

				Geometry oldElement = currentElementMap.get(id);
				if (oldElement != null) {
					newElementMap.put(id, oldElement);
				}
				if (currentElementMap.containsKey(id)) {
					continue;
				}

				Label label = createLabel(object.userData);

				Geometry newElement = null;
				if (object instanceof Point) {
					newElement = new Point(((Point) object).getMapPos(), label, pointStyleSet, object.userData);
				} else if (object instanceof Line) {
					newElement = new Line(((Line) object).getVertexList(), label, lineStyleSet, object.userData);
				} else if (object instanceof Polygon) {
					newElement = new Polygon(((Polygon) object).getVertexList(), ((Polygon) object).getHolePolygonList(), label, polygonStyleSet, object.userData);
				}

				if (newElement != null) {
					newElement.attachToLayer(this);
					newElement.setActiveStyle(zoom);
					newElementMap.put(id, newElement);
				}
			}

			// add edited elements not added yet
			for (long id : editedElementMap.keys().toArray()) {
				Geometry oldElement = editedElementMap.get(id);
				if (oldElement == null) {
					newElementMap.remove(id);
				} else {
					newElementMap.put(id, oldElement);
				}
			}
		}

		List<Geometry> newElements = new ArrayList<Geometry>(newElementMap.values());
		setVisibleElementsList(newElements);
		currentElementMap = newElementMap;
	}

	private synchronized void onElementCreated(Geometry element) {
		long minId = 0;
		for (long id : editedElementMap.keys().toArray()) {
			minId = Math.min(id, minId);
		}
		long id = minId - 1;
		editedElementMap.put(id, element);
	}

	private synchronized void onElementChanged(Geometry element) {
	    element.setLabel(createLabel(element.userData));
		for (long id : currentElementMap.keys().toArray()) {
			if (currentElementMap.get(id) == element) {
				editedElementMap.put(id, element);
			}
		}
	}

	private synchronized void onElementDeleted(Geometry element) {
		for (long id : currentElementMap.keys().toArray()) {
			if (currentElementMap.get(id) == element) {
				editedElementMap.put(id, null);
			}
		}
	}

	public synchronized boolean hasPendingChanges() {
		for (long id : editedElementMap.keys().toArray()) {
			Geometry element = editedElementMap.get(id);
			if (element != null) {
				return true;
			}
			if (id >= 0) {
				return true;
			}
		}
		return false;
	}

	public synchronized void saveChanges() {
		for (long id : editedElementMap.keys().toArray()) {
			Geometry element = editedElementMap.get(id);
			if (id < 0) {
				if (element != null) {
					long realId = insertElement(element);
					if(realId > 0){
	                    currentElementMap.remove(id);
	                    currentElementMap.put(realId, element);
					}else{
					    Toast.makeText(context, "SQL Error inserting. See logcat for details", Toast.LENGTH_SHORT).show();
					}
				}
			} else {
				if (element != null) {
					updateElement(id, element);
					currentElementMap.put(id, element);
				} else {
					deleteElement(id);
					currentElementMap.remove(id);
				}
			}
		}
		editedElementMap.clear();

		updateVisibleElements();
	}

	public synchronized void discardChanges() {
		for (long id : editedElementMap.keys().toArray()) {
			currentElementMap.remove(id);
		}
		editedElementMap.clear();

		updateVisibleElements();
	}

	private LongHashMap<Geometry> cloneElementMap(LongHashMap<Geometry> oldElementMap) {
		LongHashMap<Geometry> newElementMap = new LongHashMap<Geometry>();
		for (Iterator<LongHashMap.Entry<Geometry>> it = oldElementMap.entrySetIterator(); it.hasNext(); ) {
			LongHashMap.Entry<Geometry> entry = it.next();
			long id = entry.getKey();
			Geometry oldElement = entry.getValue();
			if (oldElement == null) {
				newElementMap.put(id, null);
			} else {
				Object userData = cloneUserData(oldElement.userData);
				if (oldElement instanceof Point) {
					Point oldPoint = (Point) oldElement;
					Point newPoint = new Point(oldPoint.getMapPos(), oldPoint.getLabel(), oldPoint.getStyleSet(), userData);
					newElementMap.put(id, newPoint);
				} else if (oldElement instanceof Line) {
					Line oldLine = (Line) oldElement;
					Line newLine = new Line(oldLine.getVertexList(), oldLine.getLabel(), oldLine.getStyleSet(), userData);
					newElementMap.put(id, newLine);
				} else if (oldElement instanceof Polygon) {
					Polygon oldPolygon = (Polygon) oldElement;
					Polygon newPolygon = new Polygon(oldPolygon.getVertexList(), oldPolygon.getHolePolygonList(), oldPolygon.getLabel(), oldPolygon.getStyleSet(), userData);
					newElementMap.put(id, newPolygon);
				} else {
					Log.error("EditableSpatialiteLayer: could not clone element, element type unsupported!");
				}
			}
		}
		return newElementMap;
	}

	/**
	 * Read Vector objects for given Envelope
	 * 
	 * @param env
	 * @param zoom
	 * @return LongHashMap of Geometry objects, 
	 *     the key must be unique Long value for each object, it will be used as update and delete key
	 */
	protected abstract LongHashMap<Geometry> queryElements(Envelope env, int zoom);
	
	/**
	 * Insert new object
	 * 
	 * @param element element to insert
	 * @return inserted element id
	 */
	protected abstract long insertElement(Geometry element);
	
	/**
	 * 
	 * Update vector object with given ID
	 * 
	 * @param id element id to update
	 * @param element new element state
	 */
	protected abstract void updateElement(long id, Geometry element);
	
	/**
	 * 
	 * Delete vectro object with given ID
	 * 
	 * @param id element id to delete
	 */
	protected abstract void deleteElement(long id);
	
	/**
	 * Generate pop-up labels, shown if you click on object. 
	 * 
	 * @param userData object custom attributes
	 * @return new label
	 */
	protected abstract Label createLabel(Object userData);
	
	/**
	 * 
	 * Create clone for userData, specific on Object type what you use
	 * 
	 * @param userData object to clone
	 * @return clone of the userData
	 */
	protected abstract Object cloneUserData(Object userData);
}
