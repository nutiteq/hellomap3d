package com.nutiteq.advancedmap.activity;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.ZoomControls;

import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Bounds;
import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Options;
import com.nutiteq.components.Vector;
import com.nutiteq.editable.EditableMapView;
import com.nutiteq.editable.layers.deprecated.EditableOgrVectorLayer;
import com.nutiteq.filepicker.FilePickerActivity;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterlayers.TMSMapLayer;
import com.nutiteq.style.LabelStyle;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.LongHashMap;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * 
 * Shows usage of EditableMapView with OGR database file
 * 
 *  Enables offline editing of points, lines and polygons. 
 *  Tested with Shapefile only
 *  
 *  See https://github.com/nutiteq/hellomap3d/wiki/Editable-MapView for details
 * 
 * Layers:
 *  TMSMapLayer - base map
 *  EditableOgrLayer - Ogr layer with editing additions.
 * 
 * The Activity enables to open an Shapefile file (or other OGR file where update is implemented)
 * FIXME: jaakl: this class is 90% same as EditableSpatialiteMapActivity, should be merged with this somehow
 *  
 * @author jaakl
 *
 */
public class EditableVectorMapActivity extends Activity implements FilePickerActivity{
    
    // about 2000 lines/polygons for high-end devices is fine, for older devices <1000
    // for points 5000 would work fine with almost any device
    
    private static final int MAX_OBJECTS = 500;

    /**
     * Keeps state of editable elements to enable undo/redo functions
     * 
     * @author mtehver
     *
     */
	private static class Memento {
		final LongHashMap<Geometry> geometries;

		Memento(LongHashMap<Geometry> geometries) {
			this.geometries = geometries;
		}
	}

	private EditableMapView mapView;

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

    private EditableOgrVectorLayer dbEditableLayer;

    private String dbPath;

    private String[] userColumns;

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
			dbEditableLayer = (EditableOgrVectorLayer) retainObject.layers.getLayers().get(0);
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
		TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 0, 18, 0,
				"http://kaart.maakaart.ee/osm/tiles/1.0.0/osm_noname_EPSG900913/", "/", ".png");
		mapLayer.setTmsY(true);

		mapView.getLayers().setBaseLayer(mapLayer);

		// set initial map view camera - optional. "World view" is default
		mapView.setFocusPoint(2970894.6988791,8045087.2280313);

		// zoom - 0 = world, like on most web maps
		mapView.setZoom(4.0f);
		// tilt means perspective view. Default is 90 degrees for "normal" 2D map view, minimum allowed is 30 degrees.
		mapView.setTilt(35.0f);

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
		
	    // read filename from extras
        Bundle b = getIntent().getExtras();
        dbPath = b.getString("selectedFile");
        
        // create stylesets. You may need just one of them
        PointStyle pointStyle = PointStyle.builder().setColor(Color.GREEN).setSize(0.2f).build();
        LineStyle lineStyle = LineStyle.builder().setWidth(0.1f).setColor(Color.BLUE).build();
        PolygonStyle polygonStyle = PolygonStyle.builder().setColor(Color.BLUE | Color.GREEN).build();
        
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        float dpi = metrics.density;
        
        LabelStyle labelStyle = 
                LabelStyle.builder()
                       .setEdgePadding((int) (12 * dpi))
                       .setLinePadding((int) (6 * dpi))
                       .setTitleFont(Typeface.create("Arial", Typeface.BOLD), (int) (16 * dpi))
                       .setDescriptionFont(Typeface.create("Arial", Typeface.NORMAL), (int) (13 * dpi))
                       .build();
        
        // create editable layer
        try {
            // defines which column valus can be edited
            // currently only String columns supported
            this.userColumns = new String[]{"name"};
            
            dbEditableLayer = new EditableOgrVectorLayer(new EPSG3857(), dbPath, null,
                    false, MAX_OBJECTS, new StyleSet<PointStyle>(pointStyle), 
                    new StyleSet<LineStyle>(lineStyle), 
                    new StyleSet<PolygonStyle>(polygonStyle), labelStyle, this);
            mapView.getLayers().addLayer(dbEditableLayer);

            // zoom map to data extent
            Envelope extent = dbEditableLayer.getDataExtent();
            mapView.setBoundingBox(new Bounds(extent.minX, extent.maxY, extent.maxX, extent.minY), false);
            
            createEditorListener();
            createUIButtons();
            
            
        } catch (IOException e) {
            Log.error("could not open file "+dbPath+ " ex:"+e.getMessage());
        }

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


	protected void createEditableSpatialiteLayers(int selectedPosition) throws IOException {
	    

		
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
				return mapPos;
			}

			@Override
			public void onElementCreated(VectorElement element) {
				saveState();
				if (element instanceof Point) {
				  ((Point) element).setStyleSet(dbEditableLayer.getPointStyleSet());
				}
                if (element instanceof Line) {
                  ((Line) element).setStyleSet(dbEditableLayer.getLineStyleSet());
                }
                if (element instanceof Polygon) {
                  ((Polygon) element).setStyleSet(dbEditableLayer.getPolygonStyleSet());
                }
				dbEditableLayer.add((Geometry) element); 
			}

			@Override
			public void onBeforeElementChange(VectorElement element) {
				if (element.getLayer() instanceof EditableOgrVectorLayer) {
					if (dragElement == null) {
						saveState();
					}
				}
			}

			@Override
			public void onElementChanged(VectorElement element) {
				if (element.getLayer() instanceof EditableOgrVectorLayer) {
				    EditableOgrVectorLayer layer = (EditableOgrVectorLayer) element.getLayer();
					layer.update((Geometry) element);
				}
			}

			@Override
			public void onElementDeleted(VectorElement element) {
				if (element.getLayer() instanceof EditableOgrVectorLayer) {
					saveState();
					EditableOgrVectorLayer layer = (EditableOgrVectorLayer) element.getLayer();
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
				if (element.getLayer() instanceof EditableOgrVectorLayer) {
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
				AlertDialog.Builder typeBuilder = new AlertDialog.Builder(EditableVectorMapActivity.this);
				typeBuilder.setTitle("Choose type");
				final String[] items = new String[] { "Point", "Line", "Polygon" };
				typeBuilder.setItems(items, new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int item) {
						Map<String, String> userData = new HashMap<String, String>();
						switch (item) {
						case 0:
							mapView.createElement(Point.class, userData);
							break;
						case 1:
							mapView.createElement(Line.class, userData);
							break;
						case 2:
							mapView.createElement(Polygon.class, userData);
							break;
						}
					}
				});
				AlertDialog typeDialog = typeBuilder.create();
				typeDialog.show();
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
				if (!(selectedElement.getLayer() instanceof EditableOgrVectorLayer)) {
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
		AlertDialog.Builder builder = new AlertDialog.Builder(EditableVectorMapActivity.this);
		builder.setTitle("Properties");

		// Build the list of properties
		@SuppressWarnings("unchecked")
		final Map<String, String> userData = (Map<String, String>) selectedElement.userData;
		List<String> itemList = new ArrayList<String>();
		for (String key : this.userColumns) {
			String value = userData.get(key);
			itemList.add(key + ": " + (value != null ? value : ""));
		}
		final String[] items = new String[itemList.size()];
		builder.setItems(itemList.toArray(items), new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int item) {
				final String key = userColumns[item];
				AlertDialog.Builder propBuilder = new AlertDialog.Builder(EditableVectorMapActivity.this);
				propBuilder.setTitle("Set property");
				propBuilder.setMessage("New value for " + key);

				final EditText input = new EditText(EditableVectorMapActivity.this);
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
				saveChangesBtn.setVisibility(!undoStack.isEmpty() ? View.VISIBLE : View.GONE);
				//discardChangesBtn.setVisibility(pendingChanges ? View.VISIBLE : View.GONE);
				undoChangeBtn.setVisibility(!undoStack.isEmpty() ? View.VISIBLE : View.GONE);
				redoChangeBtn.setVisibility(!redoStack.isEmpty() ? View.VISIBLE : View.GONE);
			}
		});
	}

	private void saveState() {
		Memento memento = new Memento(dbEditableLayer.saveState());
		undoStack.push(memento);
		redoStack.clear();
	}

	private void undoStateChanges() {
		if (undoStack.empty()) {
			return;
		}
		Memento memento = new Memento(dbEditableLayer.saveState());
		redoStack.push(memento);
		memento = undoStack.pop();
		mapView.selectElement(null);
		dbEditableLayer.loadState(memento.geometries);
		updateUIButtons();
	}

	private void redoStateChanges() {
		if (redoStack.empty()) {
			return;
		}
		Memento memento = new Memento(dbEditableLayer.saveState());
		undoStack.push(memento);
		memento = redoStack.pop();
		mapView.selectElement(null);
		dbEditableLayer.loadState(memento.geometries);
		updateUIButtons();
	}

	
	private void saveChanges() {
		mapView.selectElement(null);
		dbEditableLayer.saveChanges();
        redoStack.clear(); // TODO: currently this is required, otherwise redo/undo will not work properly
        undoStack.clear();
		updateUIButtons();
	}

	private void discardChanges() {
		mapView.selectElement(null);
		dbEditableLayer.discardChanges();
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

	// Methods for FilePicker
	
    @Override
    public String getFileSelectMessage() {
        return "Select Shapefile to edit (.shp)";
    }

    @Override
    public FileFilter getFileFilter() {
        return new FileFilter() {
            @Override
            public boolean accept(File file) {
                // accept only readable files
                if (file.canRead()) {
                    if (file.isDirectory()) {
                        // accept all directories
                        return true;
                    } else if (file.isFile()
                            && (file.getName().endsWith(".shp"))) {
                        // accept currently only shapefile
                        return true;
                    }
                }
                return false;
            };
        };
    }
	
}
