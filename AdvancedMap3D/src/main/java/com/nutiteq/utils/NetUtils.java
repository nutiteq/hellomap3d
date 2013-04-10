package com.nutiteq.utils;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Map;

import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;

import android.net.http.AndroidHttpClient;

import com.nutiteq.log.Log;

public class NetUtils {
    public static String downloadUrl(String url, Map<String, String> httpHeaders, boolean gzip ) {
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
            BufferedReader buf = new BufferedReader(new InputStreamReader(ips,"UTF-8"));

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
}
