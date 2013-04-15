package com.nutiteq.utils;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
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
