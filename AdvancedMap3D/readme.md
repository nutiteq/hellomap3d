# Install dependency JARs

 You need dependent JAR files in the project. There are two ways to accomplish this:

If you use maven, then dependent libraries will be downloaded automatically from Nutiteq repository. Only if you do not use maven for some reason, just copy all *extlibs/* jar files to *libs* folder within the project

# Test datasets

Depending on layers you may find useful to copy following files to the sdcard of your device, and modify paths in the code accordingly:

* Spatialite : Romania OpenStreetMap data, Spatialite 3.0 format: https://www.dropbox.com/s/j4aahjo7whkzx2r/romania_sp3857.sqlite
* Raster data: Digital Earth, Natural earth converted to Spherical Mercator: https://www.dropbox.com/s/fwd7f1l4gy36u94/natural-earth-2-mercator.tif
* Mapsforge: http://ftp.mapsforge.org/maps
* Shapefiles: OpenStreetMap data, Estonia: https://www.dropbox.com/s/72yhmo2adl01dho/shp_ee_3857.zip
* MBTiles: European countries with UTFGrid interaction: http://a.tiles.mapbox.com/v3/nutiteq.geography-class.mbtiles . Or you can download free TileMill http://mapbox.com/tilemill/ and create a package yourself.
