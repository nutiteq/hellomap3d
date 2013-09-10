package com.nutiteq.fragmentmap;

import com.nutiteq.advancedmap.R;
import com.nutiteq.log.Log;

import android.content.res.Configuration;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;

/**
 * 
 * This activity is used only in portrait mode and it simply displays a text sent via intent.
 *  
 * @author mtehver
 *
 */
public class InfoActivity extends FragmentActivity {
  public static final String EXTRA_TEXT = "text";
  
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Check if activity has been switched to landscape mode
    if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
      finish();
      return;
    }
    
    setContentView(R.layout.fragment_info);
    
    Bundle extras = getIntent().getExtras();
    if (extras != null) {
      InfoFragment infoFragment = (InfoFragment) getSupportFragmentManager().findFragmentById(R.id.info_fragment);
      String text = extras.getString(EXTRA_TEXT);
      infoFragment.setText(text);
    }

    Log.enableAll();
    Log.setTag("fragmentmap");
  }
}
