package com.nutiteq.advancedmap.activity;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.Toast;
import android.widget.ZoomControls;

import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.components.Vector;
import com.nutiteq.editable.EditableMapView;
import com.nutiteq.editable.layers.EditableGeometryLayer;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * 
 * Abstract base class for EditableMapView samples
 * 
 *  Enables editing points, lines and polygons.
 *  Snapping is implemented for line vertexes
 *  
 *  See https://github.com/nutiteq/hellomap3d/wiki/Editable-MapView for details
 * 
 * @author mtehver
 *
 */
public abstract class EditableMapActivityBase extends Activity {

    /**
     * Keeps state of editable elements to enable undo/redo functions
     * 
     * @author mtehver
     *
     */
    private static class Memento {
        public final List<LongHashMap<Geometry>> geometries;

        public Memento(List<LongHashMap<Geometry>> geometries) {
            this.geometries = geometries;
        }
    }

    protected EditableMapView mapView;

    private Stack<Memento> undoStack = new Stack<Memento>(); 
    private Stack<Memento> redoStack = new Stack<Memento>(); 

    private LinearLayout elementEditorLayout;
    private LinearLayout pointEditorLayout;
    private ImageButton createElementBtn;
    private ImageButton modifyElementBtn;
    private ImageButton deleteElementBtn;
    private ImageButton saveChangesBtn;
    private ImageButton discardChangesBtn;
    private ImageButton undoChangeBtn;
    private ImageButton redoChangeBtn;
    private ImageButton addPointBtn;
    private ImageButton deletePointBtn;

    @SuppressLint("NewApi")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.editable);

        // enable logging for troubleshooting - optional
        Log.enableAll();
        Log.setTag("editablemap");

        // 1. Get the MapView from the Layout xml - mandatory
        mapView = (EditableMapView) findViewById(R.id.editableMapView);

        // Optional, but very useful: restore map state during device rotation,
        // it is saved in onRetainNonConfigurationInstance() below
        Components retainObject = (Components) getLastNonConfigurationInstance();
        if (retainObject != null) {
            // just restore configuration and update listener, skip other initializations
            mapView.setComponents(retainObject);
            createEditorListener();
            createUIButtons();
            return;
        } else {
            // 2. create and set MapView components - mandatory
            mapView.setComponents(new Components());
        }

        // 3. Define map layer for basemap - mandatory.
        // Here we use MapQuest open tiles
        // Almost all online tiled maps use EPSG3857 projection.
        createBaseLayer();

        // Activate some mapview options to make it smoother - optional
        mapView.getOptions().setPreloading(true);
        mapView.getOptions().setSeamlessHorizontalPan(true);
        mapView.getOptions().setTileFading(true);
        mapView.getOptions().setKineticPanning(true);
        mapView.getOptions().setDoubleClickZoomIn(true);
        mapView.getOptions().setDualClickZoomOut(true);

        // set sky bitmap - optional, default - white
        mapView.getOptions().setSkyDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setSkyOffset(4.86f);
        mapView.getOptions().setSkyBitmap(
                UnscaledBitmapLoader.decodeResource(getResources(),
                        R.drawable.sky_small));

        // Map background, visible if no map tiles loaded - optional, default - white
        mapView.getOptions().setBackgroundPlaneDrawMode(Options.DRAW_BITMAP);
        mapView.getOptions().setBackgroundPlaneBitmap(
                UnscaledBitmapLoader.decodeResource(getResources(),
                        R.drawable.background_plane));
        mapView.getOptions().setClearColor(Color.WHITE);

        // configure texture caching - optional, suggested 
        mapView.getOptions().setTextureMemoryCacheSize(40 * 1024 * 1024);
        mapView.getOptions().setCompressedMemoryCacheSize(8 * 1024 * 1024);

        // define online map persistent caching - optional, suggested. Default - no caching
        mapView.getOptions().setPersistentCachePath(this.getDatabasePath("mapcache3").getPath());
        // set persistent raster cache limit to 100MB
        mapView.getOptions().setPersistentCacheSize(100 * 1024 * 1024);


        // 4. zoom buttons using Android widgets - optional
        // get the zoomcontrols that was defined in main.xml
        ZoomControls zoomControls = (ZoomControls) findViewById(R.id.zoomcontrols);
        // set zoomcontrols listeners to enable zooming
        zoomControls.setOnZoomInClickListener(new View.OnClickListener() {
            public void onClick(final View v) {
                mapView.zoomIn();
            }
        });
        zoomControls.setOnZoomOutClickListener(new View.OnClickListener() {
            public void onClick(final View v) {
                mapView.zoomOut();
            }
        });

        // 6. Set up editable layers
        createEditableLayers();
        createEditorListener();
        createUIButtons();

    }

    @Override
    protected void onStart() {
        mapView.startMapping();
        super.onStart();
    }

    @Override
    public void onStop() {
        mapView.stopMapping();
        super.onStop();
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        return this.mapView.getComponents();
    }

    protected void createBaseLayer() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://kaart.maakaart.ee/osm/tiles/1.0.0/osm_noname_EPSG900913/{zoom}/{x}/{yflipped}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);

        mapView.setFocusPoint(2970894.6988791,8045087.2280313);
        mapView.setZoom(10.0f);
    }

    private void createEditorListener() {
        mapView.setElementListener(new EditableMapView.EditEventListener() {

            VectorElement selectedElement;
            VectorElement dragElement;

            @Override
            public void updateUI() {
                updateUIButtons();
            }

            @Override
            public Vector snapElement(VectorElement element, Vector delta) {
                return delta;
            }

            @Override
            public MapPos snapElementVertex(VectorElement element, int index, MapPos mapPos) {
                /*
                if (element instanceof Line) {
                    List<?> visibleElements = element.getLayer().getVisibleElements();
                    MapPos screenPos = mapView.worldToScreen(mapPos.x, mapPos.y, 0); // assume element.getLayer().getProjection() == baseLayer.getProjection()
                    for (Object obj : visibleElements) {
                        if (obj instanceof Line) {
                            Line line = (Line) obj;
                            List<MapPos> vertexList = line.getVertexList();
                            for (int i = 0; i < vertexList.size(); i++) {
                                if (i == index) {
                                    continue;
                                }
                                MapPos vertexMapPos = vertexList.get(i);
                                MapPos vertexScreenPos = mapView.worldToScreen(vertexMapPos.x, vertexMapPos.y, 0);
                                double dx = vertexScreenPos.x - screenPos.x, dy = vertexScreenPos.y - screenPos.y;
                                if (dx * dx + dy * dy < 30 * 30) {
                                    return vertexMapPos;
                                }
                            }
                        }
                    }
                }
                */
                return mapPos;
            }

            @Override
            public void onElementCreated(VectorElement element) {
                saveState();
                attachEditableElementToLayer((Geometry) element);
            }

            @Override
            public void onBeforeElementChange(VectorElement element) {
                if (element.getLayer() instanceof EditableGeometryLayer) {
                    if (dragElement == null) {
                        saveState();
                    }
                }
            }

            @Override
            public void onElementChanged(VectorElement element) {
                if (element.getLayer() instanceof EditableGeometryLayer) {
                    EditableGeometryLayer layer = (EditableGeometryLayer) element.getLayer();
                    layer.update((Geometry) element);
                }
            }

            @Override
            public void onElementDeleted(VectorElement element) {
                if (element.getLayer() instanceof EditableGeometryLayer) {
                    saveState();
                    EditableGeometryLayer layer = (EditableGeometryLayer) element.getLayer();
                    layer.remove((Geometry) element);
                }
            }

            @Override
            public boolean onElementSelected(VectorElement element) {
                selectedElement = element;
                addPointBtn.setVisibility(selectedElement instanceof Line ? View.VISIBLE : View.GONE);
                return true;
            }

            @Override
            public void onElementDeselected(VectorElement element) {
                addPointBtn.setVisibility(View.GONE);
                selectedElement = null;
            }

            @Override
            public void onDragStart(VectorElement element, float x, float y) {
                dragElement = element;
                if (element.getLayer() instanceof EditableGeometryLayer) {
                    saveState();
                }
                addPointBtn.setVisibility(View.GONE);
                deletePointBtn.setVisibility(View.VISIBLE);
            }

            @Override
            public void onDrag(float x, float y) {
                Rect rect = new Rect();
                deletePointBtn.getHitRect(rect);
                if (rect.contains((int) x, (int) y)) {
                    deletePointBtn.setColorFilter(android.graphics.Color.argb(255, 255, 0, 0));
                } else {
                    deletePointBtn.setColorFilter(null);
                }
            }

            @Override
            public boolean onDragEnd(float x, float y) {
                dragElement = null;
                Rect rect = new Rect();
                deletePointBtn.getHitRect(rect);
                deletePointBtn.setColorFilter(null);
                deletePointBtn.setVisibility(View.GONE);
                addPointBtn.setVisibility(selectedElement instanceof Line ? View.VISIBLE : View.GONE);
                return rect.contains((int) x, (int) y);
            }

        });
    }

    private void createUIButtons() {
        elementEditorLayout = new LinearLayout(this);
        elementEditorLayout.setOrientation(LinearLayout.HORIZONTAL);
        elementEditorLayout.setGravity(Gravity.LEFT | Gravity.CENTER_HORIZONTAL);

        pointEditorLayout = new LinearLayout(this);
        pointEditorLayout.setOrientation(LinearLayout.HORIZONTAL);
        pointEditorLayout.setGravity(Gravity.RIGHT | Gravity.CENTER_HORIZONTAL);

        // Create dialog
        createElementBtn = new ImageButton(this);
        createElementBtn.setImageResource(android.R.drawable.ic_menu_add);
        createElementBtn.setBackgroundDrawable(null);
        createElementBtn.setFocusable(false);
        createElementBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                createEditableElement();
            }
        });
        elementEditorLayout.addView(createElementBtn);

        // Properties editor
        modifyElementBtn = new ImageButton(this);
        modifyElementBtn.setImageResource(android.R.drawable.ic_menu_edit);
        modifyElementBtn.setBackgroundDrawable(null);
        modifyElementBtn.setFocusable(false);
        modifyElementBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                final VectorElement selectedElement = mapView.getSelectedElement();
                if (!(selectedElement.getLayer() instanceof EditableGeometryLayer)) {
                    return;
                }
                modifyElementProperties(selectedElement);
            }
        });
        elementEditorLayout.addView(modifyElementBtn);

        // Delete element
        deleteElementBtn = new ImageButton(this);
        deleteElementBtn.setImageResource(android.R.drawable.ic_menu_delete);
        deleteElementBtn.setBackgroundDrawable(null);
        deleteElementBtn.setFocusable(false);
        deleteElementBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                VectorElement selectedElement = mapView.getSelectedElement();
                mapView.deleteElement(selectedElement);
            }
        });
        elementEditorLayout.addView(deleteElementBtn);

        // Save changes
        saveChangesBtn = new ImageButton(this);
        saveChangesBtn.setImageResource(android.R.drawable.ic_menu_save);
        saveChangesBtn.setBackgroundDrawable(null);
        saveChangesBtn.setFocusable(false);
        saveChangesBtn.setVisibility(View.GONE);
        saveChangesBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                saveChanges();
            }
        });
        elementEditorLayout.addView(saveChangesBtn);

        // Discard changes
        discardChangesBtn = new ImageButton(this);
        discardChangesBtn.setImageResource(android.R.drawable.ic_menu_revert);
        discardChangesBtn.setBackgroundDrawable(null);
        discardChangesBtn.setFocusable(false);
        discardChangesBtn.setVisibility(View.GONE);
        discardChangesBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                discardChanges();
            }
        });
        elementEditorLayout.addView(discardChangesBtn);

        // Redo last undo
        redoChangeBtn = new ImageButton(this);
        redoChangeBtn.setImageResource(android.R.drawable.ic_menu_revert);
        Bitmap image = BitmapFactory.decodeResource(this.getResources(), android.R.drawable.ic_menu_revert);
        if (image != null) {
            Matrix flipMatrix = new Matrix();
            flipMatrix.setScale(-1, 1, image.getWidth() / 2, image.getHeight() / 2);
            redoChangeBtn.setImageMatrix(flipMatrix);
        }
        redoChangeBtn.setScaleType(ScaleType.MATRIX);
        redoChangeBtn.setBackgroundDrawable(null);
        redoChangeBtn.setFocusable(false);
        redoChangeBtn.setVisibility(View.GONE);
        redoChangeBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                redoStateChanges();
            }
        });
        elementEditorLayout.addView(redoChangeBtn);

        // Undo last change
        undoChangeBtn = new ImageButton(this);
        undoChangeBtn.setImageResource(android.R.drawable.ic_menu_revert);
        undoChangeBtn.setBackgroundDrawable(null);
        undoChangeBtn.setFocusable(false);
        undoChangeBtn.setVisibility(View.GONE);
        undoChangeBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                undoStateChanges();
            }
        });
        elementEditorLayout.addView(undoChangeBtn);

        // Add single vertex
        addPointBtn = new ImageButton(this);
        addPointBtn.setImageResource(android.R.drawable.ic_menu_add);
        addPointBtn.setBackgroundDrawable(null);
        addPointBtn.setFocusable(false);
        addPointBtn.setVisibility(View.GONE);
        addPointBtn.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                final VectorElement selectedElement = mapView.getSelectedElement();
                mapView.updateElement(selectedElement, new EditableMapView.ElementUpdater() {
                    @Override
                    public void update(VectorElement element) {
                        addPoint(element);
                    }
                });
            }
        });
        pointEditorLayout.addView(addPointBtn);

        // Delete single vertex
        deletePointBtn = new ImageButton(this);
        deletePointBtn.setImageResource(android.R.drawable.ic_menu_delete);
        deletePointBtn.setBackgroundDrawable(null);
        deletePointBtn.setFocusable(false);
        deletePointBtn.setVisibility(View.GONE);
        pointEditorLayout.addView(deletePointBtn);

        // Create content view
        addContentView(elementEditorLayout, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        addContentView(pointEditorLayout, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        updateUIButtons();
    }

    private void modifyElementProperties(final VectorElement selectedElement) {
        if (selectedElement.userData == null) {
            selectedElement.userData = new HashMap<String, String>();
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(EditableMapActivityBase.this);
        builder.setTitle("Properties");

        // Build the list of properties
        @SuppressWarnings("unchecked")
        final Map<String, String> userData = (Map<String, String>) selectedElement.userData;
        final List<String> userColumns = getEditableElementUserColumns((Geometry) selectedElement);
        List<String> itemList = new ArrayList<String>();
        for (String key : userColumns) {
            String value = userData.get(key);
            itemList.add(key + ": " + (value != null ? value : ""));
        }
        final String[] items = new String[itemList.size()];
        builder.setItems(itemList.toArray(items), new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int item) {
                final String key = userColumns.get(item);
                AlertDialog.Builder propBuilder = new AlertDialog.Builder(EditableMapActivityBase.this);
                propBuilder.setTitle("Set property");
                propBuilder.setMessage("New value for " + key);

                final EditText input = new EditText(EditableMapActivityBase.this);
                propBuilder.setView(input);
                propBuilder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int button) {
                        mapView.updateElement(selectedElement, new EditableMapView.ElementUpdater() {
                            @Override
                            public void update(VectorElement element) {
                                userData.put(key, input.getEditableText().toString());
                            }
                        });
                    }
                });
                propBuilder.setNegativeButton("CANCEL", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int button) {
                        dialog.cancel();
                    }
                });

                AlertDialog propDialog = propBuilder.create();
                propDialog.show();
            }
        });
        AlertDialog alert = builder.create();
        alert.show();
    }

    private void updateUIButtons() {
        runOnUiThread(new Runnable() {
            public void run() {
                VectorElement selectedElement = mapView.getSelectedElement();
                modifyElementBtn.setVisibility(selectedElement != null ? View.VISIBLE : View.GONE);
                deleteElementBtn.setVisibility(selectedElement != null ? View.VISIBLE : View.GONE);
                saveChangesBtn.setVisibility(hasPendingChanges() ? View.VISIBLE : View.GONE);
                //discardChangesBtn.setVisibility(hasPendingChanges() ? View.VISIBLE : View.GONE);
                undoChangeBtn.setVisibility(!undoStack.isEmpty() ? View.VISIBLE : View.GONE);
                redoChangeBtn.setVisibility(!redoStack.isEmpty() ? View.VISIBLE : View.GONE);
            }
        });
    }

    private boolean hasPendingChanges() {
        for (EditableGeometryLayer layer : getEditableLayers()) {
            if (layer.hasPendingChanges()) {
                return true;
            }
        }
        return false;
    }

    private void saveState() {
        List<LongHashMap<Geometry>> geometries = new ArrayList<LongHashMap<Geometry>>();
        for (EditableGeometryLayer layer : getEditableLayers()) {
            geometries.add(layer.saveState());
        }
        Memento memento = new Memento(geometries);
        undoStack.push(memento);
        redoStack.clear();
    }

    private void undoStateChanges() {
        if (undoStack.empty()) {
            return;
        }
        List<LongHashMap<Geometry>> geometries = new ArrayList<LongHashMap<Geometry>>();
        for (EditableGeometryLayer layer : getEditableLayers()) {
            geometries.add(layer.saveState());
        }
        Memento memento = new Memento(geometries);
        redoStack.push(memento);
        memento = undoStack.pop();
        mapView.selectElement(null);
        for (int i = 0; i < memento.geometries.size(); i++) {
            getEditableLayers().get(i).loadState(memento.geometries.get(i));
        }
        updateUIButtons();
    }

    private void redoStateChanges() {
        if (redoStack.empty()) {
            return;
        }
        List<LongHashMap<Geometry>> geometries = new ArrayList<LongHashMap<Geometry>>();
        for (EditableGeometryLayer layer : getEditableLayers()) {
            geometries.add(layer.saveState());
        }
        Memento memento = new Memento(geometries);
        undoStack.push(memento);
        memento = redoStack.pop();
        mapView.selectElement(null);
        for (int i = 0; i < memento.geometries.size(); i++) {
            getEditableLayers().get(i).loadState(memento.geometries.get(i));
        }
        updateUIButtons();
    }

    private void saveChanges() {
        mapView.selectElement(null);

        AsyncTask<Void, Void, Void> task = new AsyncTask<Void, Void, Void>() {
            ProgressDialog dialog;
            RuntimeException exception;

            @Override
            protected void onPreExecute() {
                super.onPreExecute();
                dialog = new ProgressDialog(EditableMapActivityBase.this);
                dialog.setIndeterminate(false);
                dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
                dialog.setCancelable(false);
                dialog.setMessage("Saving...");
                dialog.show();
            }

            @Override
            protected Void doInBackground(final Void... args) {
                try {
                    for (EditableGeometryLayer layer : getEditableLayers()) {
                        layer.saveChanges();
                    }
                }
                catch (RuntimeException e) {
                    exception = e;
                }
                redoStack.clear(); // TODO: currently this is required, otherwise redo/undo will not work properly
                undoStack.clear();
                return null;
            }

            @Override
            protected void onPostExecute(final Void result) {
                super.onPostExecute(result);
                dialog.dismiss();
                if (exception != null) {
                    Toast.makeText(EditableMapActivityBase.this, "Failed to save: " + exception.getMessage(), Toast.LENGTH_LONG).show();
                }
                updateUIButtons();
            }
        };
        task.execute();
    }

    private void discardChanges() {
        mapView.selectElement(null);
        for (EditableGeometryLayer layer : getEditableLayers()) {
            layer.discardChanges();
        }
        redoStack.clear(); // TODO: currently this is required, otherwise redo/undo will not work properly
        undoStack.clear();
        updateUIButtons();
    }

    private void addPoint(VectorElement element) {
        if (element instanceof Line) {
            Line line = (Line) element;
            List<MapPos> mapPoses = new ArrayList<MapPos>(line.getVertexList());
            if (mapPoses.size() >= 2) {
                MapPos p0 = mapPoses.get(mapPoses.size() - 2);
                MapPos p1 = mapPoses.get(mapPoses.size() - 1);
                mapPoses.add(new MapPos(p1.x + (p1.x - p0.x), p1.y + (p1.y - p0.y)));
                line.setVertexList(mapPoses);
            }
        }
    }

    protected abstract void createEditableLayers();

    protected abstract List<EditableGeometryLayer> getEditableLayers();
    
    protected abstract void createEditableElement();

    protected abstract void attachEditableElementToLayer(Geometry element);

    protected abstract List<String> getEditableElementUserColumns(Geometry element);

}
