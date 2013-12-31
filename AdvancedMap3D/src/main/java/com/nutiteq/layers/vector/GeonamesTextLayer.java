package com.nutiteq.layers.vector;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Vector;

import org.geonames.Toponym;

import android.content.Context;
import android.graphics.Typeface;

import com.nutiteq.components.Color;
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
public class GeonamesTextLayer extends TextLayer {

  private final GeonamesLayer baseLayer;
  private int maxVisibleElements = Integer.MAX_VALUE;
  private float dpi;
  private Context context;


  public GeonamesTextLayer(Projection projection, GeonamesLayer baseLayer, float dpi, Context context) {
    super(projection);
    this.baseLayer = baseLayer;
    this.dpi = dpi;
    this.context = context;
    createStyleSets();
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
        String id = Integer.toString((Integer)text.userData);
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


    // Create text. Put unique id to userdata field, that will be used to identify the element later

    return new Text(baseElement, feature.getName(), textStyles.get(feature.getFeatureCode()), feature.getGeoNameId());
  }

  private LinkedHashMap<String,StyleSet<TextStyle>> textStyles = new LinkedHashMap<String,StyleSet<TextStyle>>();
  
  private void createStyleSets() {
      // country names
      Typeface font = Typeface.create(Typeface.createFromAsset(context.getAssets(), "fonts/zapfino.ttf"), Typeface.BOLD);

      textStyles.put("PCLI",new StyleSet<TextStyle>(
              TextStyle
              .builder()
              .setAllowOverlap(false)
              .setOrientation(TextStyle.GROUND_BILLBOARD_ORIENTATION)
              .setAnchorY(TextStyle.CENTER)
              .setSize((int) (20 * dpi))
              .setColor(Color.argb(255, 100, 100, 100))
              .setPlacementPriority(5)
              .setFont(font)
              .build()));
      
      // capitals
      textStyles.put("PPLC",new StyleSet<TextStyle>(
              TextStyle
              .builder()
              .setAllowOverlap(false)
              .setOrientation(TextStyle.CAMERA_BILLBOARD_ORIENTATION)
              .setSize((int) (26 * dpi))
              .setColor(Color.BLACK)
              .setPlacementPriority(4)
              .build()));
      
      // adm1 areas
      textStyles.put("ADM1",new StyleSet<TextStyle>(
              TextStyle
              .builder()
              .setAllowOverlap(false)
              .setOrientation(TextStyle.GROUND_BILLBOARD_ORIENTATION)
              .setSize((int) (24 * dpi))
              .setColor(android.graphics.Color.GRAY)
              .setPlacementPriority(2)
              .build()));
      
   // city, region capital
      textStyles.put("PPLA",new StyleSet<TextStyle>(
              TextStyle
              .builder()
              .setAllowOverlap(false)
              .setOrientation(TextStyle.CAMERA_BILLBOARD_ORIENTATION)
              .setSize((int) (22 * dpi))
              .setColor(android.graphics.Color.DKGRAY)
              .setPlacementPriority(3)
              .build()));
      
      // city smaller
      textStyles.put("PPL",new StyleSet<TextStyle>(
              TextStyle
              .builder()
              .setAllowOverlap(false)
              .setOrientation(TextStyle.CAMERA_BILLBOARD_ORIENTATION)
              .setSize((int) (18 * dpi))
              .setColor(android.graphics.Color.DKGRAY)
              .setPlacementPriority(1)
              .build()));
      
     // building
      textStyles.put("BDG",new StyleSet<TextStyle>(
              TextStyle
              .builder()
              .setAllowOverlap(false)
              .setOrientation(TextStyle.GROUND_BILLBOARD_ORIENTATION)
              .setSize((int) (16 * dpi))
              .setColor(0xff2f4f4f) // Dark Slate Gray
              .setPlacementPriority(0)
              .build()));
            
      

  }

}
