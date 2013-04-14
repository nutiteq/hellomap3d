package com.nutiteq.services.routing;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

import org.apache.http.NameValuePair;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.message.BasicNameValuePair;

import com.nutiteq.utils.NetUtils;
import com.nutiteq.log.Log;

public class CloudMadeToken {
  
  // method to request CloudMade token
    
  public static String getCloudMadeToken(String apiKey, String userId) {
      try {

          String url = "http://auth.cloudmade.com/token/" + apiKey;
    
          List<NameValuePair> postData = new ArrayList<NameValuePair>(2);
          postData.add(new BasicNameValuePair("apikey", apiKey));
          postData.add(new BasicNameValuePair("userid", userId));
      
//      Map<String, String> httpHeaders = new HashMap<String, String> ();
//      httpHeaders.put(key, value);
      
        return NetUtils.postUrl(url, null, true, new UrlEncodedFormEntity(postData));
        
    } catch (UnsupportedEncodingException e) {
        e.printStackTrace();
    }
      Log.error("cannot get CloudMade Token");
      return null;
  }

}
