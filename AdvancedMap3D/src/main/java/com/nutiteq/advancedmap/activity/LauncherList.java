package com.nutiteq.advancedmap.activity;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.nutiteq.advancedmap.R;
import com.nutiteq.filepicker.FilePicker;
import com.nutiteq.filepicker.FilePickerActivity;

/**
 * 
 * Shows list of demo Activities. Enables to open pre-launch activity for file picking.
 * This is the "main" of samples
 * 
 * @author jaak
 *
 */
public class LauncherList extends ListActivity{

    // list of demos: MapActivity, ParameterSelectorActivity (can be null)
    // if parameter selector is given, then this is launched first to get a parameter (file path)
    
    private Object[][] samples={
            {AdvancedMapActivity.class,null},
            {BasicMapActivity.class,null},
            {GlobeRenderingActivity.class,null},
            {ComposedRasterDataSourceActivity.class,null},
            {AddressSearchActivity.class,null},
            {AnimatedLocationActivity.class,null},
            {EditableCartoDbMapActivity.class,null},
            {EditableVectorFileMapActivity.class,FilePicker.class},
            {CartoDbVectorMapActivity.class,null},
            {MapBoxMapActivity.class,null},
            {MapQuestRouteActivity.class,null},
            {CloudMadeRouteActivity.class,null},
            {Online3DMapActivity.class,null},
            {MBTilesMapActivity.class,FilePicker.class},
            {VectorFileMapActivity.class,FilePicker.class},
            {RasterFileMapActivity.class,FilePicker.class},
            {MapsForgeMapActivity.class,FilePicker.class},
            {GraphhopperRouteActivity.class,FilePicker.class},
            {Offline3DMapActivity.class,FilePicker.class},
            {WmsMapActivity.class, null},
            {WfsMapActivity.class, null},
            {com.nutiteq.fragmentmap.FragmentMapActivity.class,null},
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
            sampleNames[i] = ((Class<?>) samples[i][0]).getSimpleName();
        }
        return sampleNames;
    }

    public void onListItemClick(ListView parent, View v, int position, long id) {
        if (samples[position][1] != null) {

            try {

                Intent myIntent = new Intent(LauncherList.this,
                        (Class<?>) samples[position][1]);

                Class<?> activityToRun = (Class<?>) samples[position][0];
                FilePickerActivity activityInstance = (FilePickerActivity) activityToRun
                        .newInstance();

                FilePicker.setFileSelectMessage(activityInstance
                        .getFileSelectMessage());
                FilePicker.setFileDisplayFilter(activityInstance
                        .getFileFilter());

                Bundle b = new Bundle();
                b.putString("class", ((Class<?>) samples[position][0]).getName());
                myIntent.putExtras(b);
                startActivityForResult(myIntent, 1);
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InstantiationException e) {
                e.printStackTrace();
            }

        } else {
            Intent myIntent = new Intent(LauncherList.this,
                    (Class<?>) samples[position][0]);
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
