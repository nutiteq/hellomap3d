
=Install dependency JARs=

 You need dependent JAR files in the project. There are two ways to accomplish this:

a) If you use maven, then install the files to local repo using following commands:

mvn install:install-file -DgroupId=com.nutiteq -DartifactId=nutiteq-3d-sdk -Dversion=1.0 -Dpackaging=jar -Dfile=extlibs/nutiteq-3dsdk-1.0.83pre.jar
mvn install:install-file -DgroupId=com.jhlabs -DartifactId=javaproj-noawt -Dversion=1.0.6 -Dpackaging=jar -Dfile=extlibs/javaproj-1.0.6-noawt.jar
mvn install:install-file -DgroupId=org.mapsforge.android -DartifactId=mapsforge-map -Dversion=0.3.0 -Dpackaging=jar -Dfile=extlibs/mapsforge-map-0.3.0-jar-with-dependencies.jar

b) If you do not use maven for some reason, just copy all *extlibs/* jar files to *libs* folder within the project


=Test datasets=

Depending on layers you may find useful to copy following files to the sdcard of your device, and modify paths in code accordingly:

 Spatialite : Romania OSM data, Spatialite 3.0 format: https://www.dropbox.com/s/j4aahjo7whkzx2r/romania_sp3857.sqlite
 Raster data: Digital Earth, Natural earth converted to Spherical Mercator: https://www.dropbox.com/s/fwd7f1l4gy36u94/natural-earth-2-mercator.tif
 Mapsforge: http://ftp.mapsforge.org/maps
 