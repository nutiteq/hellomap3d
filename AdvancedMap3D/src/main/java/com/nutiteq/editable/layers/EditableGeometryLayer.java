package com.nutiteq.editable.layers;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import com.nutiteq.components.CullState;
import com.nutiteq.editable.datasources.EditableVectorDataSource;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.log.Log;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.LongMap;
import com.nutiteq.vectorlayers.GeometryLayer;

/**
 * Geometry layer with extra functions for editing elements.
 * Supports all kind of geometries: Points, Lines and Polygons
 * Can connect to any editable data source (data source that supports EditableVectorDataSource<Geometry> interface)
 * 
 * @author mtehver
 *
 */
public class EditableGeometryLayer extends GeometryLayer {
    private final EditableVectorDataSource<Geometry> editableDataSource;

    private LongHashMap<Geometry> currentElementMap = new LongHashMap<Geometry>();
    private LongHashMap<Geometry> editedElementMap = new LongHashMap<Geometry>();

    public EditableGeometryLayer(EditableVectorDataSource<Geometry> dataSource) {
        super(dataSource);
        this.editableDataSource = dataSource;
    }

    public synchronized LongHashMap<Geometry> saveState() {
        return cloneElementMap(editedElementMap);
    }

    public synchronized void loadState(LongHashMap<Geometry> elementMap) {
        for (Iterator<LongMap.Entry<Geometry>> it = editedElementMap.entrySetIterator(); it.hasNext(); ) {
            LongMap.Entry<Geometry> entry = it.next();
            long id = entry.getKey();
            Geometry element = entry.getValue();
            if (element != null) {
                element.detachFromLayer();
            }
            currentElementMap.remove(id);
        }

        editedElementMap = cloneElementMap(elementMap);

        for (Iterator<LongMap.Entry<Geometry>> it = editedElementMap.entrySetIterator(); it.hasNext(); ) {
            LongMap.Entry<Geometry> entry = it.next();
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
        setVisibleElements(new ArrayList<Geometry>(currentElementMap.values()));

        updateVisibleElements();
    }

    @Override
    public void add(Geometry element) {
        element.attachToLayer(this);
        element.setActiveStyle(getCurrentZoomLevel());
        onElementCreated(element);

        updateVisibleElements();
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
    public void remove(Geometry element) {
        element.detachFromLayer();
        onElementDeleted(element);

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
    public void calculateVisibleElements(CullState cullState) {

        Collection<Geometry> newElements = dataSource.loadElements(cullState);
        LongHashMap<Geometry> newElementMap = new LongHashMap<Geometry>(); 

        synchronized (this) {
            // apply styles, create new objects for these
            for (Iterator<Geometry> it = newElements.iterator(); it.hasNext(); ){
                Geometry newElement = it.next();
                long id = newElement.getId();

                Geometry oldElement = currentElementMap.get(id);
                if (oldElement != null) {
                    oldElement.setActiveStyle(cullState.zoom);
                    newElementMap.put(id, oldElement);
                }
                if (currentElementMap.containsKey(id)) {
                    continue;
                }

                if (newElement != null) {
                    newElement.attachToLayer(this);
                    newElement.setActiveStyle(cullState.zoom);
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

        setVisibleElements(new ArrayList<Geometry>(newElementMap.values()));
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
                    long realId = editableDataSource.insertElement(element);
                    element.setId(realId);
                    currentElementMap.remove(id);
                    currentElementMap.put(realId, element);
                }
            } else {
                if (element != null) {
                    editableDataSource.updateElement(id, element);
                    currentElementMap.put(id, element);
                } else {
                    editableDataSource.deleteElement(id);
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
        for (Iterator<LongMap.Entry<Geometry>> it = oldElementMap.entrySetIterator(); it.hasNext(); ) {
            LongMap.Entry<Geometry> entry = it.next();
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

    protected Object cloneUserData(Object userData) {
        if (userData == null) {
            return null;
        }
        if (userData instanceof String) {
            return new String((String) userData);
        }
        if (userData instanceof List<?>) {
            return new ArrayList<Object>((List<?>) userData);
        }
        if (userData instanceof Map<?, ?>) {
            return new HashMap<Object, Object>((Map<?, ?>) userData);
        }
        throw new UnsupportedOperationException();
    }
}
