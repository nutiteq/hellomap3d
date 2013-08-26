package com.nutiteq.advancedmap.activity;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.nutiteq.advancedmap.R;

public class MarkerMenu extends LinearLayout {

    private TextView markerName;
    private Button markerEdit;
    private String post;

    public MarkerMenu(Context context, AttributeSet attrs) {
        super(context, attrs);
        // TODO Auto-generated constructor stub
    }

    public MarkerMenu(Context context, String post) {
            super(context);
            this.post = post;
            // TODO Auto-generated constructor stub
             LayoutInflater inflater = (LayoutInflater) context
                        .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                    inflater.inflate(R.layout.detailrow, this, true);
                  markerName =   (TextView)findViewById(R.id.text1);
                  markerEdit =   (Button)findViewById(R.id.text2);  
                  
                 markerName.setText(post);
                  
        }
}
