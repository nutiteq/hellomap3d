package com.nutiteq.advancedmap.activity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Bitmap;

import com.nutiteq.advancedmap.R;
import com.nutiteq.components.Color;
import com.nutiteq.editable.datasources.EditableCartoDbDataSource;
import com.nutiteq.editable.layers.EditableGeometryLayer;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.Line;
import com.nutiteq.geometry.Point;
import com.nutiteq.geometry.Polygon;
import com.nutiteq.layers.Layer;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.rasterdatasources.RasterDataSource;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;

/**
 * 
 * Shows usage of EditableMapView with CartoDB.com online API
 * 
 *  Enables editing points, lines and polygons.
 *  Snapping is implemented for line vertexes
 *  
 *  See https://github.com/nutiteq/hellomap3d/wiki/Editable-MapView for details
 * 
 * @author mtehver
 *
 */
public class EditableCartoDbMapActivity extends EditableMapActivityBase {

	private StyleSet<PointStyle> pointStyleSet;
	private StyleSet<LineStyle> lineStyleSet;
	private StyleSet<PolygonStyle> polygonStyleSet;

	@Override
	protected void createBaseLayer() {
        RasterDataSource dataSource = new HTTPRasterDataSource(new EPSG3857(), 0, 18, "http://kaart.maakaart.ee/osm/tiles/1.0.0/osm_noname_EPSG900913/{zoom}/{x}/{yflipped}.png");
        RasterLayer mapLayer = new RasterLayer(dataSource, 0);
        mapView.getLayers().setBaseLayer(mapLayer);
        mapView.setFocusPoint(2970894.6988791,8045087.2280313);
        mapView.setZoom(10.0f);
	}

	@Override
    protected void createEditableLayers() {
        createStyleSets();
        createEditableCartoDbLayer("random_points", false);
        createEditableCartoDbLayer("lines", false);
        createEditableCartoDbLayer("european_countries", true);
    }
	
	@Override
	protected List<EditableGeometryLayer> getEditableLayers() {
	    List<EditableGeometryLayer> layers = new ArrayList<EditableGeometryLayer>();
	    for (Layer layer : mapView.getComponents().layers.getLayers()) {
	        if (layer instanceof EditableGeometryLayer) {
	            layers.add((EditableGeometryLayer) layer);
	        }
	    }
	    return layers;
	}
	
	@Override
	protected void createEditableElement() {
        AlertDialog.Builder typeBuilder = new AlertDialog.Builder(this);
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
	
	@Override
	protected void attachEditableElementToLayer(Geometry element) {
	    List<EditableGeometryLayer> layers = getEditableLayers();
        if (element instanceof Point) {
            Point point = (Point) element;
            point.setStyleSet(pointStyleSet);
            layers.get(0).add(point); 
        } else if (element instanceof Line) {
            Line line = (Line) element;
            line.setStyleSet(lineStyleSet);
            layers.get(1).add(line); 
        } else if (element instanceof Polygon) {
            Polygon polygon = (Polygon) element;
            polygon.setStyleSet(polygonStyleSet);
            layers.get(2).add(polygon);
        }
	}

    @Override
    protected List<String> getEditableElementUserColumns(Geometry element) {
        if (element instanceof Polygon) {
            return Arrays.asList(new String[] { "name" });
        }
        return new ArrayList<String>();
    }

	private void createStyleSets() {
		pointStyleSet = new StyleSet<PointStyle>();
		Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.point);
		PointStyle pointStyle = PointStyle.builder()
		         .setBitmap(pointMarker).setSize(0.05f).setColor(Color.GREEN).setPickingSize(0.2f)
		         .build();
		pointStyleSet.setZoomStyle(0, pointStyle);

		lineStyleSet = new StyleSet<LineStyle>();
		LineStyle lineStyle = LineStyle.builder()
		        .setWidth(0.05f).setColor(Color.RED)
		        .setPointStyle(PointStyle.builder().setBitmap(pointMarker).setSize(0.04f).setColor(0xffc00000).build())
		        .build();
		lineStyleSet.setZoomStyle(0, lineStyle);

		polygonStyleSet = new StyleSet<PolygonStyle>();
		PolygonStyle polygonStyle = PolygonStyle.builder()
		        .setColor(Color.BLUE & 0x80FFFFFF).setLineStyle(lineStyle)
		        .build();
		polygonStyleSet.setZoomStyle(0, polygonStyle);
	}

	private EditableGeometryLayer createEditableCartoDbLayer(String table, boolean multiGeometry) {
		String account = "nutiteq-dev";
		String apiKey = "a4f7b8026fe4860eb6348c6c76a39cb1c24da5ac";
		String querySql  = "SELECT cartodb_id, the_geom_webmercator, name FROM "+table+" WHERE the_geom_webmercator && ST_SetSRID('BOX3D(!bbox!)'::box3d, 3857)";
		String insertSql = "INSERT INTO "+table+" (the_geom) VALUES(ST_Transform(!geom!, 4326)) RETURNING cartodb_id";
		String updateSql = "UPDATE "+table+" SET the_geom=ST_Transform(!geom!, 4326), name=!name! WHERE cartodb_id=!id!";
		String deleteSql = "DELETE FROM "+table+" WHERE cartodb_id=!id!";

		EditableCartoDbDataSource dataSource = new EditableCartoDbDataSource(mapView.getLayers().getBaseProjection(), account, apiKey, querySql, insertSql, updateSql, deleteSql, multiGeometry) {

            @Override
            protected Label createLabel(Map<String, String> userData) {
                StringBuffer labelTxt = new StringBuffer();
                for (Map.Entry<String, String> entry : userData.entrySet()){
                    labelTxt.append(entry.getKey() + ": " + entry.getValue() + "\n");
                }
                return new DefaultLabel("Data:", labelTxt.toString());
            }

            @Override
            protected StyleSet<PointStyle> createPointStyleSet(Map<String, String> userData, int zoom) {
                return pointStyleSet;
            }

            @Override
            protected StyleSet<LineStyle> createLineStyleSet(Map<String, String> userData, int zoom) {
                return lineStyleSet;
            }

            @Override
            protected StyleSet<PolygonStyle> createPolygonStyleSet(Map<String, String> userData, int zoom) {
                return polygonStyleSet;
            }
		};

		EditableGeometryLayer layer = new EditableGeometryLayer(dataSource);
		mapView.getLayers().addLayer(layer);

		return layer;
	}
	
}
