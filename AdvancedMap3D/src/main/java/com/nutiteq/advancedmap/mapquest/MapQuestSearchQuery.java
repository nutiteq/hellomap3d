package com.nutiteq.advancedmap.mapquest;

import java.util.ArrayList;
import java.util.HashMap;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Dialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.app.SearchManager;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.provider.SearchRecentSuggestions;
import android.view.View;
import android.widget.AdapterView;
import android.widget.SimpleAdapter;
import android.widget.Toast;

import com.nutiteq.advancedmap.R;
import com.nutiteq.advancedmap.activity.AddressSearchActivity;
import com.nutiteq.components.Color;
import com.nutiteq.geometry.Marker;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.Projection;
import com.nutiteq.services.geocode.MapQuestGeocoder;
import com.nutiteq.services.geocode.SearchQueryResults;
import com.nutiteq.style.MarkerStyle;
import com.nutiteq.ui.DefaultLabel;
import com.nutiteq.ui.Label;
import com.nutiteq.utils.UnscaledBitmapLoader;


public class MapQuestSearchQuery extends ListActivity implements SearchQueryResults  
{  
    private static final int SEARCH_DIALOG = 1;

    private static final String MAPQUEST_KEY = "Fmjtd%7Cluub2qu82q%2C70%3Do5-961w1w";

    // UI elements
    private ProgressDialog progressDialog;
    private Marker[] searchResultPlaces;

    private ArrayList<HashMap<String, String>> list = new ArrayList<HashMap<String, String>>();
    
    /** Called with the activity is first created.
    * 
    *  After the typical activity setup code, we check to see if we were launched
    *  with the ACTION_SEARCH intent, and if so, we handle it.
    */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.debug("onCreate search");
        // get and process search query here
        final Intent queryIntent = getIntent();
        final String queryAction = queryIntent.getAction();
        if (Intent.ACTION_SEARCH.equals(queryAction)) {
            doSearchQuery(queryIntent, "onCreate()");
        }
        getListView().setOnItemClickListener(new AdapterView.OnItemClickListener() {
        	
          @Override
          public void onItemClick(AdapterView<?> aparent, View v, int position, long id) {
            Log.debug("Clicked: " + position );
            AddressSearchActivity.setSearchResult(searchResultPlaces[position]);
            finish();
          }

        } );

    }
    
    /** 
     * Called when new intent is delivered.
     *
     * This is where we check the incoming intent for a query string.
     * 
     * @param newIntent The intent used to restart this activity
     */
    @Override
    public void onNewIntent(final Intent newIntent) {
        super.onNewIntent(newIntent);
        Log.debug("onNewIntent search");
        
        // get and process search query here
        final Intent queryIntent = getIntent();
        final String queryAction = queryIntent.getAction();
        if (Intent.ACTION_SEARCH.equals(queryAction)) {
            doSearchQuery(queryIntent, "onNewIntent()");
        }
    }
    
    
    @Override
    protected void onStop() {
        super.onStop();
        Log.debug("onStop search");
        progressDialog.dismiss();
    }

    private void doSearchQuery(final Intent queryIntent, final String entryPoint) {
        
        // The search query is provided as an "extra" string in the query intent
        final String queryString = queryIntent.getStringExtra(SearchManager.QUERY);
        
        // Record the query string in the recent queries suggestions provider.
        SearchRecentSuggestions suggestions = new SearchRecentSuggestions(this, 
                SearchSuggestionProvider.AUTHORITY, SearchSuggestionProvider.MODE);
        suggestions.saveRecentQuery(queryString, null);
        
        
        // Do the actual search, write to searchResults field
         showDialog(SEARCH_DIALOG);
         
         MapQuestGeocoder geocoder = new MapQuestGeocoder();
         geocoder.geocode(queryString, null, this, MAPQUEST_KEY);
    }

    // handler to send search results to UI thread
    final Handler handler = new Handler() {
        public void handleMessage(Message msg) {
            SimpleAdapter listAdapter = new SimpleAdapter( 
                    MapQuestSearchQuery.this, 
                    list,
                    R.layout.searchrow,
                    new String[] { "line1","line2" },
                    new int[] { R.id.text1, R.id.text2 }  );
            setListAdapter(listAdapter);
            getListView().setTextFilterEnabled(true);
        }
    };
    
    final Handler errorHandler = new Handler() {
        public void handleMessage(Message msg) {
            Toast.makeText(MapQuestSearchQuery.this, "Nothing found", Toast.LENGTH_LONG).show();
            finish();
        }
    };
    
    public void searchResults(JSONArray locations) {

        if(locations == null || locations.length() == 0){
            Log.debug("no results found");
            Message msg = errorHandler.obtainMessage();
            errorHandler.sendMessage(msg);
            return;
        }
        
        Log.debug("geocode results: "+locations.length());
        progressDialog.dismiss();
        searchResultPlaces = new Marker[locations.length()];
        
        Projection proj = new EPSG3857();
        Bitmap pointMarker = UnscaledBitmapLoader.decodeResource(getResources(), R.drawable.olmarker);
        MarkerStyle markerStyle = MarkerStyle.builder().setBitmap(pointMarker)./*setSize(0.001f).*/setColor(Color.WHITE).build();
        
        for (int i=0;i<locations.length();i++){
            
            try {
                JSONObject location = locations.getJSONObject(i);
                
                String street = location.optString("street");
                String city = location.optString("adminArea5"); // city
                String county = location.optString("adminArea4"); // county
                String state = location.optString("adminArea3"); // state
                String country = location.optString("adminArea1"); // country
                
                String line1 = notNull(street)+" "+city;
                String line2 = notNull(county)+" "+notNull(state)+" "+notNull(country);
                
                HashMap<String,String> item = new HashMap<String,String>();
                item.put( "line1",line1);
                item.put( "line2",line2);
                list.add( item );
                
                Label label = new DefaultLabel(line1,line2);
                JSONObject latLng = location.getJSONObject("latLng");
                double lng = latLng.getDouble("lng");
                double lat = latLng.getDouble("lat");
                
                Marker marker = new Marker(proj.fromWgs84(lng, lat), label, markerStyle, location);
                searchResultPlaces[i] = marker;
                
            } catch (JSONException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

          }        
        
        Message msg = handler.obtainMessage();
        handler.sendMessage(msg);
    }

    
    @Override
    protected Dialog onCreateDialog(int id) {

        switch (id) {
        case SEARCH_DIALOG:
            progressDialog = new ProgressDialog(MapQuestSearchQuery.this);
            progressDialog.setTitle("Searching...");
            progressDialog.setCancelable(true);
            return progressDialog;

        default:
            return null;
        }

    }
    
    private String notNull(String txt) {
        if(txt == null)
            return "";
        else
            return txt;
    }
}
