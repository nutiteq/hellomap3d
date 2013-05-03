package com.nutiteq.layers.vector;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Text;
import com.nutiteq.layers.vector.WfsLayer.Feature;
import com.nutiteq.layers.vector.WfsLayer.FeatureCollection;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.StyleSet;
import com.nutiteq.style.TextStyle;
import com.nutiteq.vectorlayers.TextLayer;

public abstract class WfsTextLayer extends TextLayer {
	
    private WfsLayer baseLayer;

	public WfsTextLayer(Projection projection, WfsLayer baseLayer) {
	  super(projection);
	  this.baseLayer = baseLayer;
  }

  @Override
  public void calculateVisibleElements(CullState cullState) {
		// Create id-based map from old visible elements. We will keep them if they remain visible
    Map<String, Text> oldVisibleElementsMap = new HashMap<String, Text>();
    List<Text> oldVisibleElementsList = getVisibleElements();
    if (oldVisibleElementsList != null) {
    	for (Text text : oldVisibleElementsList) {
    		String id = (String) text.userData;
    		oldVisibleElementsMap.put(id, text);
    	}
    }
    Set<Text> oldVisibleElementsSet = new HashSet<Text>(oldVisibleElementsMap.values()); 

    // get the features, which should be already loaded by baseLayer
    
	FeatureCollection features = baseLayer.features;
		
	if(features == null){
	    Log.debug("no features loaded yet for WfsTextLayer");
        return;
	}
	
		// Create list of new visible elements 
    List<Text> newVisibleElementsList = new ArrayList<Text>();
    for (Feature feature : features.features) {
    	Text element = oldVisibleElementsMap.get(feature.properties.osm_id);
    	if (element == null) {
    		element = createText(feature, cullState.zoom);
    		if (element != null) {
    			element.attachToLayer(this);
    		}
    	}
    	if (element != null) {
    		newVisibleElementsList.add(element);
    	}
    }
    for (Text element : newVisibleElementsList) {
      element.setActiveStyle(cullState.zoom);
    }
    
    // Merge new visible elements 
    rebuildVisibleElementsLayout(cullState, oldVisibleElementsSet, newVisibleElementsList, getMaxVisibleElements());
    setVisibleElementsList(newVisibleElementsList);
    
    // Release visible elements from last frame that are no longer used
    oldVisibleElementsSet.removeAll(newVisibleElementsList);
    for (Text element : oldVisibleElementsSet) {
      element.clearActiveStyle();
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
		StyleSet<TextStyle> styleSet = createStyleSet(feature, zoom);
		if (styleSet == null) {
			return null;
		}
		
		// Create text. Put unique id to userdata field, that will be used to identify the element later
		
		return new Text(baseElement, feature.properties.name, styleSet, feature.properties.osm_id);
	}
	
	protected abstract StyleSet<TextStyle> createStyleSet(Feature feature, int zoom);

}
