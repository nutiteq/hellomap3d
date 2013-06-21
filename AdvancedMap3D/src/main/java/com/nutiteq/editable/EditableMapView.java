package com.nutiteq.editable;

import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.util.AttributeSet;
import android.view.MotionEvent;

import com.nutiteq.MapView;
import com.nutiteq.components.Color;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Vector;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Marker;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.layers.Layer;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.ui.MapListener;

public class EditableMapView extends MapView {
	private static final MarkerStyle DEFAULT_MARKER_STYLE = MarkerStyle.builder().setColor(Color.RED).setSize(0.5f).setBitmap(BitmapFactory.decodeResource(Resources.getSystem(), android.R.drawable.ic_dialog_map)).build();
	private static final PointStyle DEFAULT_POINT_STYLE = PointStyle.builder().setColor(Color.RED).setSize(0.2f).build();
	private static final LineStyle DEFAULT_LINE_STYLE = LineStyle.builder().setColor(Color.RED).setWidth(0.1f).build();
	private static final PolygonStyle DEFAULT_POLYGON_STYLE = PolygonStyle.builder().setColor(Color.RED).build();

	public interface EditEventListener {
		void updateUI();
		Vector snapElement(VectorElement element, Vector delta);
		MapPos snapElementVertex(VectorElement element, int index, MapPos mapPos);
		void onElementCreated(VectorElement element);
		void onBeforeElementChange(VectorElement element);
		void onElementChanged(VectorElement element);
		void onElementDeleted(VectorElement element);
		boolean onElementSelected(VectorElement element); // true if element is selectable, false otherwise
		void onElementDeselected(VectorElement element);
		void onDragStart(VectorElement element, float x, float y);
		void onDrag(float x, float y);
		boolean onDragEnd(float x, float y); // true if point should be deleted
	}

	public interface ElementUpdater {
		void update(VectorElement element);
	}

	private class EditMapListener extends MapListener {

		public EditMapListener() {
		}

		@Override
		public void onSurfaceChanged(GL10 gl, int width, int height) {
		}

		@Override
		public void onDrawFrameAfter3D(GL10 gl, float zoomPow2) {
		}

		@Override
		public void onDrawFrameBefore3D(GL10 gl, float zoomPow2) {
		}

		@Override
		public void onLabelClicked(VectorElement vectorElement, boolean longClick) {
		}

		@Override
		public void onVectorElementClicked(VectorElement vectorElement, double x, double y, boolean longClick) {
			if (vectorElement.getLayer() != overlayLayer) {
				selectElement(vectorElement);
				initialElementDragPos = new MapPos(x, y);
				saveElementPos(vectorElement);
			}
		}

		@Override
		public void onMapClicked(double x, double y, boolean longClick) {
			if (initialElementDragPos == null) {
				selectElement(null);
			}
		}

		@Override
		public void onMapMoved() {
		}
	}

	private MapListener mapListener;
	private EditEventListener editEventListener;

	private VectorElement selectedElement;
	private List<Point> overlayPoints = new ArrayList<Point>();
	private OverlayLayer overlayLayer;
	private OverlayLayerStyle overlayLayerStyle = OverlayLayerStyle.builder().build(); 
	private boolean dragMode;
	private Point dragPoint;
	private MapPos initialElementDragPos;
	private List<List<MapPos>> initialElementPoses;

	public EditableMapView(final Context context) {
		super(context);
	}

	public EditableMapView(final Context context, final AttributeSet attrs) {
		super(context, attrs);
	}

	public EditEventListener getElementListener() {
		return editEventListener;
	}

	public void setElementListener(EditEventListener elementListener) {
		this.editEventListener = elementListener;
	}

	public void setOverlayLayerStyle(OverlayLayerStyle style) {
		this.overlayLayerStyle = style;
	}

	public VectorElement getSelectedElement() {
		return selectedElement;
	}
	
	public void selectElement(final VectorElement vectorElement) {
		if (vectorElement == selectedElement) {
			return;
		}
		if (editEventListener != null && selectedElement != null) {
			editEventListener.onElementDeselected(selectedElement);
		}
		if (overlayLayer != null) {
			getLayers().removeLayer(overlayLayer);
			overlayLayer = null;
		}
		overlayPoints.clear();
		if (vectorElement == null || vectorElement.getLayer() == null) {
			selectedElement = null;
			updateUI();
			return;
		}
		selectedElement = vectorElement;
		if (editEventListener != null) {
			if (!editEventListener.onElementSelected(selectedElement)) {
				selectedElement = null;
				updateUI();
				return;
			}
		}
		if (overlayLayer == null) {
			overlayLayer = new OverlayLayer(getLayers().getBaseProjection());
			getLayers().addLayer(overlayLayer);
		}
		syncElementOverlayPoints(vectorElement);
		updateUI();
	}

	public VectorElement createElement(Class<?> cls, Object userData) {
		VectorElement element = null;
		if (cls.equals(Marker.class)) {
			MapPos mapPos = screenToWorld(getWidth() / 2, getHeight() / 2);
			element = new Marker(mapPos, null, DEFAULT_MARKER_STYLE, userData);
		}
		if (cls.equals(Point.class)) {
			MapPos mapPos = screenToWorld(getWidth() / 2, getHeight() / 2);
			element = new Point(mapPos, null, DEFAULT_POINT_STYLE, userData);
		}
		if (cls.equals(Line.class)) {
			List<MapPos> mapPoses = new ArrayList<MapPos>();
			mapPoses.add(screenToWorld(getWidth() * 0.4, getHeight() * 0.4));
			mapPoses.add(screenToWorld(getWidth() * 0.6, getHeight() * 0.6));
			element = new Line(mapPoses, null, DEFAULT_LINE_STYLE, userData);
		}
		if (cls.equals(Polygon.class)) {
			List<MapPos> mapPoses = new ArrayList<MapPos>();
			mapPoses.add(screenToWorld(getWidth() * 0.4, getHeight() * 0.4));
			mapPoses.add(screenToWorld(getWidth() * 0.6, getHeight() * 0.4));
			mapPoses.add(screenToWorld(getWidth() * 0.5, getHeight() * 0.6));
			element = new Polygon(mapPoses, null, DEFAULT_POLYGON_STYLE, userData);
		}
		if (editEventListener != null && element != null) {
			editEventListener.onElementCreated(element);
		}
		selectElement(element);
		return element;
	}

	public void updateElement(VectorElement vectorElement, ElementUpdater updater) {
		if (vectorElement == null) {
			return;
		}
		if (editEventListener != null) {
			editEventListener.onBeforeElementChange(vectorElement);
		}
		updater.update(vectorElement);
		syncElementOverlayPoints(vectorElement);
		if (editEventListener != null) {
			editEventListener.onElementChanged(vectorElement);
		}
		updateUI();
	}

	public void deleteElement(VectorElement vectorElement) {
		if (vectorElement == null) {
			return;
		}
		selectElement(null);
		if (editEventListener != null) {
			editEventListener.onElementDeleted(vectorElement);
		}
		updateUI();
	}
	
	private void saveElementPos(VectorElement vectorElement) {
		if (vectorElement == null) {
			return;
		}
		if (vectorElement instanceof Marker) {
			Marker marker = (Marker) vectorElement;
			initialElementPoses = new ArrayList<List<MapPos>>();
			initialElementPoses.add(new ArrayList<MapPos>());
			initialElementPoses.get(0).add(marker.getMapPos());
		}
		if (vectorElement instanceof Point) {
			Point point = (Point) vectorElement;
			initialElementPoses = new ArrayList<List<MapPos>>();
			initialElementPoses.add(new ArrayList<MapPos>());
			initialElementPoses.get(0).add(point.getMapPos());
		}
		if (vectorElement instanceof Line) {
			Line line = (Line) vectorElement;
			initialElementPoses = new ArrayList<List<MapPos>>();
			initialElementPoses.add(line.getVertexList());
		}
		if (vectorElement instanceof Polygon) {
			Polygon polygon = (Polygon) vectorElement;
			initialElementPoses = new ArrayList<List<MapPos>>();
			initialElementPoses.add(polygon.getVertexList());
			if (polygon.getHolePolygonList() != null) {
				initialElementPoses.addAll(polygon.getHolePolygonList());
			}
		}
	}
	
	private void updateElementPos(VectorElement vectorElement, MapPos initialPos, MapPos currentPos) {
		if (vectorElement == null) {
			return;
		}
		if (editEventListener != null) {
			editEventListener.onBeforeElementChange(vectorElement);
		}
		Vector delta = new Vector(currentPos.x - initialPos.x, currentPos.y - initialPos.y, currentPos.z - initialPos.z);
		if (editEventListener != null) {
			delta = editEventListener.snapElement(vectorElement, delta);
		}
		
		// Update all points depending on element type
		if (vectorElement instanceof Marker) {
			Marker marker = (Marker) vectorElement;
			MapPos mapPos = initialElementPoses.get(0).get(0);
			marker.setMapPos(new MapPos(mapPos.x + delta.x, mapPos.y + delta.y, mapPos.z + delta.z));
		}
		if (vectorElement instanceof Point) {
			Point point = (Point) vectorElement;
			MapPos mapPos = initialElementPoses.get(0).get(0);
			point.setMapPos(new MapPos(mapPos.x + delta.x, mapPos.y + delta.y, mapPos.z + delta.z));
		}
		if (vectorElement instanceof Line) {
			Line line = (Line) vectorElement;
			List<MapPos> mapPoses = new ArrayList<MapPos>();
			for (ListIterator<MapPos> it = initialElementPoses.get(0).listIterator(); it.hasNext(); ) {
				MapPos mapPos = it.next();
				mapPoses.add(new MapPos(mapPos.x + delta.x, mapPos.y + delta.y, mapPos.z + delta.z));
			}
			line.setVertexList(mapPoses);
		}
		if (vectorElement instanceof Polygon) {
			Polygon polygon = (Polygon) vectorElement;
			List<List<MapPos>> mapPosesList = new ArrayList<List<MapPos>>();
			for (ListIterator<List<MapPos>> it = initialElementPoses.listIterator(); it.hasNext(); ) {
				List<MapPos> mapPoses = new ArrayList<MapPos>();
				for (ListIterator<MapPos> it2 = it.next().listIterator(); it2.hasNext(); ) {
					MapPos mapPos = it2.next();
					mapPoses.add(new MapPos(mapPos.x + delta.x, mapPos.y + delta.y, mapPos.z + delta.z));
				}
				mapPosesList.add(mapPoses);
			}
			polygon.setVertexList(mapPosesList.get(0));
			polygon.setHolePolygonList(mapPosesList.subList(1, mapPosesList.size()));
		}
		syncElementOverlayPoints(vectorElement);
		if (editEventListener != null) {
			editEventListener.onElementChanged(vectorElement);
		}
		updateUI();
	}

	private void updateElementPoint(VectorElement vectorElement, Point dragPoint) {
		if (vectorElement == null) {
			return;
		}
		if (editEventListener != null) {
			editEventListener.onBeforeElementChange(vectorElement);
		}
		MapPos mapPos = dragPoint.getMapPos();
		Layer layer = vectorElement.getLayer();
		if (layer != null) {
			mapPos = reprojectPoint(mapPos, overlayLayer.getProjection(), layer.getProjection());
		}
		if (editEventListener != null) {
			int index = overlayPoints.indexOf(dragPoint);
			mapPos = editEventListener.snapElementVertex(vectorElement, index % 2 == 0 ? index / 2 : -1, mapPos);
		}
		
		// Update point depending on element type
		if (vectorElement instanceof Marker) {
			Marker marker = (Marker) vectorElement;
			marker.setMapPos(mapPos);
		}
		if (vectorElement instanceof Point) {
			Point point = (Point) vectorElement;
			point.setMapPos(mapPos);
		}
		if (vectorElement instanceof Line) {
			Line line = (Line) vectorElement;
			int index = overlayPoints.indexOf(dragPoint);
			List<MapPos> mapPoses = new ArrayList<MapPos>(line.getVertexList());
			if (index % 2 == 0) {
				mapPoses.set(index / 2, mapPos);
			} else {
				mapPoses.add(index / 2 + 1, mapPos);
				overlayPoints.add(index + 1, createOverlayPoint());
				overlayPoints.add(index - 0, createOverlayPoint());
			}
			line.setVertexList(mapPoses);
		}
		if (vectorElement instanceof Polygon) {
			Polygon polygon = (Polygon) vectorElement;
			int index = overlayPoints.indexOf(dragPoint);
			List<MapPos> mapPoses = new ArrayList<MapPos>(polygon.getVertexList());
			if (index % 2 == 0) {
				mapPoses.set(index / 2, mapPos);
			} else {
				mapPoses.add(index / 2 + 1, mapPos);
				overlayPoints.add(index + 1, createOverlayPoint());
				overlayPoints.add(index - 0, createOverlayPoint());
			}
			polygon.setVertexList(mapPoses);
		}
		syncElementOverlayPoints(vectorElement);
		if (editEventListener != null) {
			editEventListener.onElementChanged(vectorElement);
		}
		updateUI();
	}

	private void removeElementPoint(VectorElement vectorElement, Point dragPoint) {
		if (vectorElement == null) {
			return;
		}
		
		// Remove point/element depending on type
		if (vectorElement instanceof Marker) {
			selectElement(null);
			if (editEventListener != null) {
				editEventListener.onElementDeleted(vectorElement);
			}
		}
		if (vectorElement instanceof Point) {
			selectElement(null);
			if (editEventListener != null) {
				editEventListener.onElementDeleted(vectorElement);
			}
		}
		if (vectorElement instanceof Line) {
			Line line = (Line) vectorElement;
			int index = overlayPoints.indexOf(dragPoint);
			List<MapPos> mapPoses = new ArrayList<MapPos>(line.getVertexList());
			if (index % 2 == 0) {
				if (mapPoses.size() > 2) {
					if (editEventListener != null) {
						editEventListener.onBeforeElementChange(vectorElement);
					}
					mapPoses.remove(index / 2);
					overlayPoints.remove(index);
					overlayPoints.remove(index > 0 ? index - 1 : index);
					line.setVertexList(mapPoses);
					syncElementOverlayPoints(line);
					if (editEventListener != null) {
						editEventListener.onElementChanged(vectorElement);
					}
				} else {
					selectElement(null);
					if (editEventListener != null) {
						editEventListener.onElementDeleted(vectorElement);
					}
				}
			}
		}
		if (vectorElement instanceof Polygon) {
			Polygon polygon = (Polygon) vectorElement;
			int index = overlayPoints.indexOf(dragPoint);
			List<MapPos> mapPoses = new ArrayList<MapPos>(polygon.getVertexList());
			if (index % 2 == 0) {
				if (mapPoses.size() > 3) {
					if (editEventListener != null) {
						editEventListener.onBeforeElementChange(vectorElement);
					}
					mapPoses.remove(index / 2);
					overlayPoints.remove(index);
					overlayPoints.remove(index % overlayPoints.size());
					polygon.setVertexList(mapPoses);
					syncElementOverlayPoints(polygon);
					if (editEventListener != null) {
						editEventListener.onElementChanged(vectorElement);
					}
				} else {
					selectElement(null);
					if (editEventListener != null) {
						editEventListener.onElementDeleted(vectorElement);
					}
				}
			}
		}
		updateUI();
	}

	private void syncElementOverlayPoints(VectorElement vectorElement) {
		if (vectorElement == null) {
			overlayPoints.clear();
			overlayLayer.setAll(overlayPoints);
			return;
		}
		Projection projection = null;
		Layer layer = vectorElement.getLayer(); 
		if (layer != null) {
			projection = layer.getProjection();
		}
		int overlayPointsSize = overlayPoints.size();
		
		// Create overlay points depending on element type
		if (vectorElement instanceof Marker) {
			Marker marker = (Marker) vectorElement;
			if (overlayPoints.size() != 1) {
				overlayPoints.clear();
				overlayPoints.add(createOverlayPoint(marker.getMapPos(), projection));
			}
			overlayPoints.get(0).setMapPos(marker.getMapPos());
		}
		if (vectorElement instanceof Point) {
			Point point = (Point) vectorElement;
			if (overlayPoints.size() != 1) {
				overlayPoints.clear();
				overlayPoints.add(createOverlayPoint(point.getMapPos(), projection));
			}
			overlayPoints.get(0).setMapPos(point.getMapPos());
		}
		if (vectorElement instanceof Line) {
			Line line = (Line) vectorElement;
			List<MapPos> mapPoses = line.getVertexList();
			while (overlayPoints.size() > mapPoses.size() * 2 - 1) {
				overlayPoints.remove(overlayPoints.size() - 1);
			}
			while (overlayPoints.size() < mapPoses.size() * 2 - 1) {
				int n = overlayPoints.size();
				overlayPoints.add(n % 2 == 0 ? createOverlayPoint(mapPoses.get(n / 2), projection) : createOverlayPoint());
			}
			for (int index = 0; index < mapPoses.size(); index++) {
				overlayPoints.get(index * 2).setMapPos(mapPoses.get(index));
				if (index > 0) {
					overlayPoints.get(index * 2 - 1).setMapPos(getMidPoint(mapPoses.get(index - 1), mapPoses.get(index))); 
				}
			}
		}
		if (vectorElement instanceof Polygon) {
			Polygon polygon = (Polygon) vectorElement;
			List<MapPos> mapPoses = polygon.getVertexList();
			while (overlayPoints.size() > mapPoses.size() * 2) {
				overlayPoints.remove(overlayPoints.size() - 1);
			}
			while (overlayPoints.size() < mapPoses.size() * 2) {
				int n = overlayPoints.size();
				overlayPoints.add(n % 2 == 0 ? createOverlayPoint(mapPoses.get(n / 2), projection) : createOverlayPoint());
			}
			for (int index = 0; index < polygon.getVertexList().size(); index++) {
				overlayPoints.get(index * 2).setMapPos(mapPoses.get(index));
				overlayPoints.get(index * 2 + 1).setMapPos(getMidPoint(mapPoses.get(index), mapPoses.get((index + 1) % mapPoses.size())));
			}
		}
		
		// Update all points if some points have to be added/deleted
		if (overlayPointsSize != overlayPoints.size()) {
			overlayLayer.setAll(overlayPoints);
		}
	}

	private Point createOverlayPoint() {
		return new Point(new MapPos(0, 0), null, overlayLayerStyle.virtualPointStyle, null);
	}

	private Point createOverlayPoint(MapPos mapPos, Projection projection) {
		return new Point(reprojectPoint(mapPos, projection, overlayLayer.getProjection()), null, overlayLayerStyle.editablePointStyle, null);
	}

	private MapPos reprojectPoint(MapPos mapPos, Projection source, Projection target) {
		if (source == target) {
			return mapPos;
		}
		if (source == null || target == null) {
			return mapPos;
		}
		MapPos wgs84 = source.toWgs84(mapPos.x, mapPos.y);
		return target.fromWgs84(wgs84.x, wgs84.y);
	}

	private MapPos getMidPoint(MapPos mapPos1, MapPos mapPos2) {
		return new MapPos((mapPos1.x + mapPos2.x) * 0.5, (mapPos1.y + mapPos2.y) * 0.5);
	}

	private boolean startDrag(float x, float y) {
		if (selectedElement == null) {
			return false;
		}
		
		// Select closest vertex as drag point
		dragPoint = null;
		initialElementDragPos = null;
		double dragPointDistance = overlayLayerStyle.maxDragDistance;
		for (Point point : overlayPoints) {
			MapPos mapPos = point.getMapPos();
			MapPos screenPos = worldToScreen(mapPos.x, mapPos.y, mapPos.z);
			double pointDistance = new Vector(screenPos.x - x, screenPos.y - y, 0).getLength2D();
			if (pointDistance < dragPointDistance) {
				dragPoint = point;
				dragPointDistance = pointDistance;
			}
		}
		return dragPoint != null;
	}

	private boolean dragTo(float x, float y) {
		if (selectedElement == null) {
			return false;
		}
		if (dragPoint == null && initialElementDragPos == null) {
			return false;
		}

		// Dispatch events to listener
		if (!dragMode) {
			super.deselectVectorElement(); // HACK-FIX to hide label which is not updated when the element is moved
			if (editEventListener != null) {
				editEventListener.onDragStart(selectedElement, x, y);
			}
			dragMode = true;
		}
		if (editEventListener != null) {
			editEventListener.onDrag(x, y);
		}

		// Drag element/point 
		MapPos dragPos = screenToWorld(x, y);
		if (dragPoint == null) {
			updateElementPos(selectedElement, initialElementDragPos, dragPos);
		} else {
			dragPoint.setStyle(overlayLayerStyle.dragPointStyle);
			dragPoint.setMapPos(dragPos);
			updateElementPoint(selectedElement, dragPoint);
		}
		return true;
	}

	private boolean endDrag(float x, float y, boolean update) {
		if (selectedElement == null) {
			return false;
		}
		if (dragPoint == null && initialElementDragPos == null) {
			return false;
		}
		
		// Finalize element/point drag 
		if (dragPoint != null) {
			dragPoint.setStyle(overlayLayerStyle.editablePointStyle);
		}
		
		// Dispatch event
		if (dragMode) {
			if (editEventListener != null) {
				if (editEventListener.onDragEnd(x, y)) {
					removeElementPoint(selectedElement, dragPoint);
				}
			}
			dragMode = false;
		}
		
		// Reset state
		dragPoint = null;
		initialElementDragPos = null;
		return false;
	}

	private void updateUI() {
		if (editEventListener != null) {
			editEventListener.updateUI();
		}
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		switch (event.getAction()) {
		case MotionEvent.ACTION_DOWN:
			if (startDrag(event.getX(), event.getY()))
				return true;
			break;
		case MotionEvent.ACTION_MOVE:
			if (dragTo(event.getX(), event.getY()))
				return true;
			break;
		case MotionEvent.ACTION_CANCEL:
			if (endDrag(event.getX(), event.getY(), false))
				return true;
			break;
		case MotionEvent.ACTION_UP:
			if (endDrag(event.getX(), event.getY(), true))
				return true;
			break;
		}  	
		return super.onTouchEvent(event);
	}

	@Override
	public void startMapping() {
		super.startMapping();
		if (mapListener == null) {
			mapListener = getOptions().getMapListener();
			getOptions().setMapListener(new EditMapListener());
		}
	}

	@Override
	public void stopMapping() {
		if (overlayLayer != null) {
			getLayers().removeLayer(overlayLayer);
			overlayLayer = null;
		}
		overlayPoints.clear();
		dragMode = false;
		dragPoint = null;
		initialElementDragPos = null;
		initialElementPoses = null;
		if (mapListener != null) {
			getOptions().setMapListener(mapListener);
			mapListener = null;
		}
		super.stopMapping();
	}

}
