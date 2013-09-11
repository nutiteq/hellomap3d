package com.nutiteq.fragmentmap;

import com.nutiteq.advancedmap.R;

import android.support.v4.app.Fragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

/**
 * 
 * Basic fragment containing single text view and methods for setting it content.
 *  
 * @author mtehver
 *
 */
public class InfoFragment extends Fragment {

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    View view = inflater.inflate(R.layout.info_fragment, container, false);
    return view;
  }

  @Override
  public void onSaveInstanceState(Bundle state) {
    super.onSaveInstanceState(state);
    
    TextView infoText = (TextView) getView().findViewById(R.id.info_text);
    state.putCharSequence("text", infoText.getText());
  }
  
  @Override
  public void onActivityCreated(Bundle savedInstanceState) {
    super.onActivityCreated(savedInstanceState);
    
    if (savedInstanceState != null) {
      CharSequence text = savedInstanceState.getCharSequence("text");
      TextView infoText = (TextView) getView().findViewById(R.id.info_text);
      infoText.setText(text == null ? "" : text);
    }
  }

  public void setText(String text) {
    TextView infoText = (TextView) getView().findViewById(R.id.info_text);
    infoText.setText(text);
  }

}
