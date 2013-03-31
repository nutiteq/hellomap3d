package com.nutiteq.utils;

import android.app.Activity;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.RelativeLayout;

public class UiUtils {
    private static final String HTML_CSS = "@font-face {font-family: classic_arial_font; src: url('arial.ttf');} body {font-family: 'classic_arial_font';}";
    public static final String HTML_HEAD = "<html><head><style type=\"text/css\">"+HTML_CSS+"</style></head><body bgcolor=\"transparent\" style=\"background-color:transparent;\" onClick=\"Android.openWebPageData()\">";
    public static final String HTML_FOOT = "</body></html>";

    // add WebView to a layout (for the map legend)
    public static void addWebView(RelativeLayout mainLayout, Activity activity, String html){

     // first container layout
     RelativeLayout legendLayout = new RelativeLayout(activity);
     RelativeLayout.LayoutParams legendLayoutparams = 
             new RelativeLayout.LayoutParams(320, 300);
     legendLayoutparams.addRule(RelativeLayout.ALIGN_PARENT_LEFT, RelativeLayout.TRUE);
     legendLayoutparams.addRule(RelativeLayout.ALIGN_PARENT_TOP, RelativeLayout.TRUE);
     legendLayoutparams.setMargins(15, 15, 0, 0);
     legendLayout.setLayoutParams(legendLayoutparams);
     
     // now create the webview itself, and add to legendView
     WebView webView = new WebView(activity);
     webView.getSettings().setJavaScriptEnabled(true);
     // force to open any URLs in native browser instead of WebView 
     webView.setWebViewClient(new WebViewClient() {
           @Override
           public boolean shouldOverrideUrlLoading(WebView view, String url) {
              return super.shouldOverrideUrlLoading(view, url);
           }
       });
     webView.layout(0, 0, 320, 300);
     webView.loadDataWithBaseURL("file:///android_asset/",HTML_HEAD+html+HTML_FOOT, "text/html", "UTF-8",null);
     legendLayout.addView(webView);
     
     mainLayout.addView(legendLayout);
 }
}
