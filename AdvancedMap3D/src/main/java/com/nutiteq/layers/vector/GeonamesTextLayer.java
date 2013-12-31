package com.nutiteq.layers.vector;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.geonames.Toponym;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.geometry.Text;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.StyleSet;
import com.nutiteq.style.TextStyle;
import com.nutiteq.vectorlayers.TextLayer;

/**
 * Special text layer that should be used together with GeonamesLayer.
 * 
 * @author jaak
 *
 */
public abstract class GeonamesTextLayer extends TextLayer {

  private final GeonamesLayer baseLayer;
  private int maxVisibleElements = Integer.MAX_VALUE;

  public GeonamesTextLayer(Projection projection, GeonamesLayer baseLayer) {
    super(projection);
    this.baseLayer = baseLayer;
  }
  
  public GeonamesLayer getBaseLayer() {
    return baseLayer;
  }
  
  public void setMaxVisibleElements(int maxElements) {
    this.maxVisibleElements = maxElements;
  }

  public void calculateVisibleElements(List<Toponym> features, int zoom) {
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

    // Create list of new visible elements 
    List<Text> newVisibleElementsList = new ArrayList<Text>();
    for (Toponym feature : features) { // first, add existing elements
      Text element = oldVisibleElementsMap.get(feature.getGeoNameId());
      if (element != null && newVisibleElementsList.size() < maxVisibleElements) {
        newVisibleElementsList.add(element);
      }
    }
    for (Toponym feature : features) { // now add new elements
      Text element = oldVisibleElementsMap.get(feature.getGeoNameId());
      if (element == null && newVisibleElementsList.size() < maxVisibleElements) {
        element = createText(feature, zoom);
        if (element != null) {
          element.attachToLayer(this);
          newVisibleElementsList.add(element);
        }
      }
    }
    for (Text element : newVisibleElementsList) {
      element.setActiveStyle(zoom);
    }

    // Update visible elements 
    setVisibleElements(newVisibleElementsList);

    // Release visible elements from last frame that are no longer used
    oldVisibleElementsSet.removeAll(newVisibleElementsList);
    for (Text element : oldVisibleElementsSet) {
      element.clearActiveStyle();
    }
  }
  
  @Override
  public void calculateVisibleElements(Envelope env, int zoom) {
    // Do nothing here - other calculateVisibleElements is used instead
  }

  protected Text createText(Toponym feature, int zoom) {
    if (feature.getName() == null) {
      return null;
    }
    if (feature.getName().trim().equals("")) {
      return null;
    }

    // Create base element based on geometry type
    Text.BaseElement baseElement = null;
    MapPos mapPos = getProjection().fromWgs84(feature.getLongitude(), feature.getLatitude());
    baseElement = new Text.BasePoint(mapPos);


    // Create styleset for the feature
    StyleSet<TextStyle> styleSet = createStyleSet(feature, zoom);
    if (styleSet == null) {
      return null;
    }

    // Create text. Put unique id to userdata field, that will be used to identify the element later

    return new Text(baseElement, feature.getName(), styleSet, feature.getGeoNameId());
  }

  protected abstract StyleSet<TextStyle> createStyleSet(Toponym feature, int zoom);

}
