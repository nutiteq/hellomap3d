package com.nutiteq.datasources.vector;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.datasources.vector.WFSVectorDataSource.Feature;
import com.nutiteq.geometry.Text;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.style.StyleSet;
import com.nutiteq.style.TextStyle;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

/**
 * Special WFS text data source that uses WFSVectorDataSource as input.
 * 
 * @author jaak
 *
 */
public abstract class WFSTextVectorDataSource extends AbstractVectorDataSource<Text> {

    private class DataSourceChangeListener implements OnChangeListener {
        @Override
        public void onElementChanged(VectorElement element) {
            synchronized (WFSTextVectorDataSource.this) {
                Feature feature = (Feature) element.userData;
                loadedElementMap.remove(feature.properties.osm_id);
            }

            notifyElementsChanged();
        }

        @Override
        public void onElementsChanged() {
            synchronized (WFSTextVectorDataSource.this) {
                loadedElementMap.clear();
            }
            
            notifyElementsChanged();
        }
    }

    private final WFSVectorDataSource dataSource;
    private int maxElements = Integer.MAX_VALUE;
    private Map<String, Text> loadedElementMap = new HashMap<String, Text>();
    
    public WFSTextVectorDataSource(WFSVectorDataSource dataSource) {
        super(dataSource.getProjection());
        this.dataSource = dataSource;
        dataSource.addOnChangeListener(new DataSourceChangeListener()); // TODO: bad practice, causes memory leak. Should use WeakRef listener. Or better, separate methods for create/destroy phase
    }

    public void setMaxElements(int maxElements) {
        this.maxElements = maxElements;
    }

    @Override
    public Envelope getDataExtent() {
        return dataSource.getDataExtent();
    }

    @Override
    public Collection<Text> loadElements(CullState cullState) {
        Collection<com.nutiteq.geometry.Geometry> geometryElements = dataSource.loadElements(cullState);

        synchronized (this) {
            Map<String, Text> elementMap = new HashMap<String, Text>();

            for (com.nutiteq.geometry.Geometry geometry : geometryElements) { // first, add existing elements
                Feature feature = (Feature) geometry.userData;
                Text element = loadedElementMap.get(feature.properties.osm_id);
                if (element != null && elementMap.size() < maxElements) {
                    elementMap.put(feature.properties.osm_id, element);
                }
            }

            for (com.nutiteq.geometry.Geometry geometry : geometryElements) { // now add new elements
                Feature feature = (Feature) geometry.userData;
                Text element = loadedElementMap.get(feature.properties.osm_id);
                if (element == null && elementMap.size() < maxElements) {
                    element = createText(feature, cullState.zoom);
                    if (element != null) {
                        element.attachToDataSource(this);
                        elementMap.put(feature.properties.osm_id, element);
                    }
                }
            }

            loadedElementMap = elementMap;
            return elementMap.values();
        }
    }

    protected Text createText(Feature feature, int zoom) {
        if (feature.properties.name == null) {
            return null;
        }
        if (feature.properties.name.trim().equals("")) {
            return null;
        }

        // Create base element based on geometry type
        Text.BaseElement baseElement = null;
        if (feature.geometry.type.equals("LineString")) {
            List<MapPos> mapPoses = new ArrayList<MapPos>();
            for (double[] coords : feature.geometry.lineCoordinates) {
                mapPoses.add(new MapPos(coords[0], coords[1]));
            }
            baseElement = new Text.BaseLine(mapPoses);
        } else if (feature.geometry.type.equals("Point")){
            MapPos mapPos = new MapPos(feature.geometry.pointCoordinates[0], feature.geometry.pointCoordinates[1]);
            baseElement = new Text.BasePoint(mapPos);

        } else {
            return null;
        }

        // Create styleset for the feature
        StyleSet<TextStyle> styleSet = createFeatureStyleSet(feature, zoom);
        if (styleSet == null) {
            return null;
        }

        // Create text. Put unique id to userdata field, that will be used to identify the element later
        return new Text(baseElement, feature.properties.name, styleSet, feature.properties.osm_id);
    }

    protected abstract StyleSet<TextStyle> createFeatureStyleSet(Feature feature, int zoom);

}
