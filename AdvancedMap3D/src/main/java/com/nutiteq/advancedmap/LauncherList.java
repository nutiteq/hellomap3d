package com.nutiteq.advancedmap;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

public class LauncherList extends ListActivity{

    private Class[] samples={
            AdvancedMapActivity.class,
            MBTilesMapActivity.class,
            BasicMapActivity.class,
            OgrMapActivity.class
    };
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        setContentView(R.layout.list);

        ListView lv = this.getListView();
        lv.setAdapter(new ArrayAdapter<String>(
                this, 
                android.R.layout.simple_list_item_1, 
                getStringArray()));
    }
    
    private String[] getStringArray() {
        String[] sampleNames = new String[samples.length];
        for(int i=0; i < samples.length; i++) {
            sampleNames[i] = samples[i].getSimpleName();
        }
        return sampleNames;
    }

    public void onListItemClick(ListView parent, View v, int position, long id){
        Intent myIntent = new Intent(LauncherList.this, samples[position]);
        this.startActivity(myIntent);
    }
    
}
