package com.nutiteq.utils;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.Map;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;
import org.json.JSONException;
import org.json.JSONObject;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.http.AndroidHttpClient;

import com.nutiteq.log.Log;

public class NetUtils {
    public static String downloadUrl(String url, Map<String, String> httpHeaders, boolean gzip, String encoding) {
        try {
            HttpClient client = new DefaultHttpClient();
            HttpGet request = new HttpGet();
            request.setURI(new URI(url));
            
            if(gzip){
                AndroidHttpClient.modifyRequestToAcceptGzipResponse(request);
            }

            if(httpHeaders != null){
                for (Map.Entry<String, String> entry : httpHeaders.entrySet()) {
                    request.addHeader(entry.getKey(), entry.getValue());
                }
              }
            
            HttpResponse response = client.execute(request);
            InputStream ips;
            if(gzip){
                ips  = AndroidHttpClient
                        .getUngzippedContent(response.getEntity());
            }else{
                ips  = new ByteArrayInputStream(EntityUtils.toByteArray(response.getEntity()));
            }
            
            BufferedReader buf = new BufferedReader(new InputStreamReader(ips,encoding));

            StringBuilder sb = new StringBuilder();
            String s;

            while ((s = buf.readLine()) != null) {
                sb.append(s);
            }
            
            buf.close();
            ips.close();
            Log.debug("loaded: "+sb.toString());
            return sb.toString();

            } catch (URISyntaxException e) {
                e.printStackTrace();
            } catch (ClientProtocolException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        return null;
    }


    /**
     * HTTP Post data
     * @param url
     * @param httpHeaders
     * @param gzip
     * @param postData
     * @return
     */
    public static String postUrl(String url, Map<String, String> httpHeaders, boolean gzip, HttpEntity postData, String encoding ) {
        try {
            HttpClient client = new DefaultHttpClient();
            HttpPost request = new HttpPost();
            request.setURI(new URI(url));
            Log.debug("POST to "+url);
            if(gzip){
                AndroidHttpClient.modifyRequestToAcceptGzipResponse(request);
            }

            if(httpHeaders != null){
                for (Map.Entry<String, String> entry : httpHeaders.entrySet()) {
                    request.addHeader(entry.getKey(), entry.getValue());
                }
              }
            
            request.setEntity(postData);
            
            HttpResponse response = client.execute(request);
            InputStream ips;
            if(gzip){
                ips  = AndroidHttpClient
                        .getUngzippedContent(response.getEntity());
            }else{
                ips  = new ByteArrayInputStream(EntityUtils.toByteArray(response.getEntity()));
            }
            BufferedReader buf = new BufferedReader(new InputStreamReader(ips, encoding));

            StringBuilder sb = new StringBuilder();
            String s;

            while ((s = buf.readLine()) != null) {
                sb.append(s);
            }
            
            buf.close();
            ips.close();
            Log.debug("loaded: "+sb.toString());
            return sb.toString();
            
            } catch (URISyntaxException e) {
                e.printStackTrace();
            } catch (ClientProtocolException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        return null;
    }

    
    /**
     * Helper method to load JSON 
     */
    
    public static JSONObject getJSONFromUrl(String url) {

        InputStream is = null;
        // Making HTTP request
        try {
            // defaultHttpClient
            DefaultHttpClient httpClient = new DefaultHttpClient();
            HttpGet httpRequest = new HttpGet(url);

            AndroidHttpClient.modifyRequestToAcceptGzipResponse(httpRequest);

            HttpResponse httpResponse = httpClient.execute(httpRequest);
            is = AndroidHttpClient
                    .getUngzippedContent(httpResponse.getEntity());

            String json = null;
            try {
                BufferedReader reader = new BufferedReader(
                        new InputStreamReader(is, "utf-8"), 8);
                StringBuilder sb = new StringBuilder();
                String line = null;
                while ((line = reader.readLine()) != null) {
                    sb.append(line + "\n");
                }
                is.close();
                json = sb.toString();
            } catch (Exception e) {
                Log.error("Error converting result " + e.toString());
            }

            JSONObject jObj = null;
            // try parse the string to a JSON object
            try {
                jObj = new JSONObject(json);
            } catch (JSONException e) {
                Log.error("Error parsing data " + e.toString());
            }

            // return JSON String
            return jObj;

        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        } catch (ClientProtocolException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;

    }
    
    public static Bitmap getBitmapFromURL(String src) {
        try {
            URL url = new URL(src);
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setDoInput(true);
            connection.connect();
            InputStream input = connection.getInputStream();
            Bitmap myBitmap = BitmapFactory.decodeStream(input);
            return myBitmap;
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }
    
}
