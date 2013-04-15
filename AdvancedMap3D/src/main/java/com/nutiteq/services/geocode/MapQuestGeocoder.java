package com.nutiteq.services.geocode;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.net.Uri;
import android.os.AsyncTask;

import com.nutiteq.advancedmap.mapquest.SearchQueryResults;
import com.nutiteq.components.Envelope;
import com.nutiteq.log.Log;
import com.nutiteq.utils.NetUtils;

public class MapQuestGeocoder {

    public void geocode(String request, Envelope bbox, SearchQueryResults callback, String apiKey){
        
        Uri.Builder uri = Uri.parse("http://open.mapquestapi.com/geocoding/v1/address?").buildUpon();
//        if(apiKey != null){
//            uri.appendQueryParameter("key", apiKey);
//        }
        uri.appendQueryParameter("location", request);
        
        if(bbox != null){
            String boundingBox = bbox.minY+","+bbox.minX+","+bbox.maxY+","+bbox.maxX;
            uri.appendQueryParameter("boundingBox", boundingBox);
        }
        
        String url = uri.build().toString();
        
        if(apiKey != null){
            url += "&key="+apiKey;
        }
        Log.debug("geocode url: "+uri.build().toString());
        
        new MqGeocodeTask(callback).execute(url);
        
    }
    
    public static class MqGeocodeTask extends AsyncTask<String, Void, JSONArray> {


        private SearchQueryResults callback;

        public MqGeocodeTask(SearchQueryResults callback){
            this.callback = callback;
        }
        
        protected JSONArray doInBackground(String... urls) {
 
            String json = NetUtils.downloadUrl(urls[0], null, true, "UTF-8");
//            Log.debug("geocode response: "+json);
            
            try {
                JSONObject jObj = new JSONObject(json);
                JSONArray locations = jObj.getJSONArray("results").getJSONObject(0).getJSONArray("locations");
                return locations;
                            
            } catch (JSONException e) {
                Log.error("Error parsing JSON data " + e.toString());
            }

            return null;
        }

        protected void onPostExecute(JSONArray locations) {
            callback.searchResults(locations);
        }
    }
    
}
