package com.nutiteq.fragmentmap;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;

import com.nutiteq.advancedmap.R;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;

/**
 * 
 * Shows how to embed MapView class into Fragment. Displays number of random markers and lets user select one active marker. 
 * Map state is serialized and restored between orientation changes and when moving between info and map fragments.
 * Layout is chosen based on orientation - landscape orientation includes 2 fragments (map and info), while portrait includes only single fragment.
 * Activity is used for dispatching marker selection events from map fragment to info fragment.  
 *  
 * @author mtehver
 *
 */
public class FragmentMapActivity extends FragmentActivity implements MapFragment.OnMarkerSelectedListener {
  private String info; 

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.fragment_main);

    Log.enableAll();
    Log.setTag("fragmentmap");
  }

  @Override
  protected void onSaveInstanceState(Bundle state) {
    super.onSaveInstanceState(state);
    state.putString("info", info);
  }
  
  @Override
  protected void onRestoreInstanceState(Bundle state) {
    if (state != null) {
      info = state.getString("info");
      InfoFragment infoFragment = (InfoFragment) getSupportFragmentManager().findFragmentById(R.id.info_fragment);
      if (infoFragment != null && infoFragment.isInLayout()) {
        infoFragment.setText(info);
      }
    }
  }

  @Override
  public void onMarkerSelected(Marker marker, String info) {
    this.info = info;
    
    // Decide whether to set info text directly or launch activity for displaying the info
    InfoFragment infoFragment = (InfoFragment) getSupportFragmentManager().findFragmentById(R.id.info_fragment);
    if (infoFragment != null && infoFragment.isInLayout()) {
      infoFragment.setText(info);
    } else {
      Intent intent = new Intent(getApplicationContext(), InfoActivity.class);
      intent.putExtra(InfoActivity.EXTRA_TEXT, info);
      startActivity(intent);
    }
  }
  
}

