== Getting started with Nutiteq Android 3D Maps SDK ==

=== Introduction ===

This is a simple guide to get started with [[http://www.nutiteq.com/|Nutiteq]] Android 3D mapping SDK.

If you upgrade from [[http://www.nutiteq.com/map-api|Nutiteq Map API 1.x]] then be ready that this 3D SDK has significantly different (cleaned up and simplified) API compared to Nutiteq Maps SDK 1.x version. Some old features may be not available in the new version.

=== Prerequisities ===
* Android SDK installed and running
* Basic knowledge about Android app development
* GIT client running and basic usage knowledge

=== Try it out ===
Go ahead and try. This project has basic map viewing and all the needed libraries included:

{{{
$ git clone https://bitbucket.org/nutiteq/hellomap3d.git/wiki
}}}

Click to image to play video with 3D map browsing:

[[http://youtu.be/WzqpEBjx6jg|
{{screen1.jpeg|Hello 3D Map}}
]]
=== Getting started step by step ===
==== Preparations ====
# Clone HelloMap project as given above. You will need some files from it. 
# Create new Android project. Set Android SDK level 8 as minimum
# Give minimum permissions in AndroidManifest.xml: **android.permission.INTERNET** to read maps online
# Copy some useful resource files and libraries from the demo project to your project. Take all files to same folders of your project: **res/drawable/** and **libs/**

==== Create map ====
1. **Define your application main layout** as **res/layout/main.xml**, so it has map element:
{{{
#!xml
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="fill_parent"
    android:layout_height="fill_parent"
    android:orientation="vertical" >
   <com.nutiteq.MapView
    android:id="@+id/mapView"
    android:layout_width="fill_parent" 
    android:layout_height="fill_parent" 
    />
</LinearLayout>
}}}

2. **Create map object**. Now we can define MapView type of member in your main activity class, load layout and load the MapView from layout. The object is created now.
{{{
#!java
public class HelloMap3DActivity extends Activity {

    private MapView mapView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mapView = (MapView) findViewById(R.id.mapView);
...
}}}

3. **Initialize and start map**. Map object itself does not work right away. Three steps are needed here as minimum: (a) define map configuration package, which is put in Components object, (b) define base map layer and finally (c) tell map object to start map activities (downloading threads etc). 
{{{
#!java
      // define new configuration holder object
      mapView.setComponents(new Components());

      // Define base layer. Here we use MapQuest open tiles which are free to use
      // Almost all online maps use EPSG3857 projection.
      TMSMapLayer mapLayer = new TMSMapLayer(new EPSG3857(), 0, 18, 0,
             "http://otile1.mqcdn.com/tiles/1.0.0/osm/", "/", ".png");
      mapView.getLayers().setBaseLayer(mapLayer);
 
      // start mapping
      mapView.startMapping();
}}}

After this, if you are lucky, you can start the application on your phone and it should show map. Congratulations!

== Next steps ==
* [[BasicMarker|Add a clickable marker to map]]
* [[BasicEvents|Listen and handle map events]] - clicks on objects, map movement and drawings
* [[GPSAnimation|Show animated GPS location]]
* [[BasicConfiguration|Set MapView options]] - caching and other options for smoother UX
* [[HandleFlip|Handle device orientation change]], so map view is not reset
* Short overview of [[AdvancedLayers|other included layers]] 
* Take look to JavaDoc package nutiteq-3dlib-preview-javadoc.zip, found in root of sample application project. Check out MapView methods first of all, then Options and different Geometries which require Style definitions.

== Advanced tasks ==
* [[CustomMarkerLayer|Custom Vector layer with Markers]] - sample custom layer with Marker objects. Data is loaded on-line, based on tiles (so it is cacheable in both server and client side).

== Possible problems and limitations ==
* All resource images should be prepared as square with size of power of 2 to be compatible with maximum number of devices: 8x8, 16x16,32x32 etc. Some functions do fix (resize) the image automatically, but not all (yet).
* Testing with emulator. To run the app you need latest (4.x) emulator ABI, and make sure that "GPU Emulation" feature is turned on (it is off by default). Note that Android Emulator still does not support multi-touch gestures, so we suggest to use real device for testing.