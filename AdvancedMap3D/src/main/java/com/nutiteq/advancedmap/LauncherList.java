package com.nutiteq.advancedmap;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.nutiteq.filepicker.FilePicker;
import com.nutiteq.filepicker.FilePickerActivity;

public class LauncherList extends ListActivity{

    private Object[][] samples={
            {BasicMapActivity.class,null},
            {CartoDbVectorMapActivity.class,null},
            {MapBoxMapActivity.class,null},
            {MBTilesMapActivity.class,FilePicker.class},
            {OgrMapActivity.class,FilePicker.class},
            {AdvancedMapActivity.class,null}
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
            sampleNames[i] = ((Class) samples[i][0]).getSimpleName();
        }
        return sampleNames;
    }

    public void onListItemClick(ListView parent, View v, int position, long id) {
        if (samples[position][1] != null) {

            try {

                Intent myIntent = new Intent(LauncherList.this,
                        (Class) samples[position][1]);

                Class activityToRun = (Class) samples[position][0];
                FilePickerActivity activityInstance = (FilePickerActivity) activityToRun
                        .newInstance();

                FilePicker.setFileSelectMessage(activityInstance
                        .getFileSelectMessage());
                FilePicker.setFileDisplayFilter(activityInstance
                        .getFileFilter());

                Bundle b = new Bundle();
                b.putString("class", ((Class) samples[position][0]).getName());
                myIntent.putExtras(b);
                startActivityForResult(myIntent, 1);
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InstantiationException e) {
                e.printStackTrace();
            }

        } else {
            Intent myIntent = new Intent(LauncherList.this,
                    (Class) samples[position][0]);
            this.startActivity(myIntent);
        }
    }
    
    
    // gets fileName from FilePicker and starts Map Activity with fileName as parameter
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (data == null){
            return;
        }
        String fileName = data.getStringExtra("selectedFile");
        String className = data.getStringExtra("class");
        if(fileName != null && className != null){
            try {
                Intent myIntent = new Intent(LauncherList.this,
                            Class.forName(className));
    
                Bundle b = new Bundle();
                b.putString("selectedFile", fileName);
                myIntent.putExtras(b);
                this.startActivity(myIntent);
            
            } catch (ClassNotFoundException e) {
                e.printStackTrace();
            }
            
        }
        
    }
    
}
