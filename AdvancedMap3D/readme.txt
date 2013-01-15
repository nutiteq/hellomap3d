
* Install dependency JARs to maven *

  Needed until there is online repo for these.

mvn install:install-file -DgroupId=com.nutiteq -DartifactId=nutiteq-3d-sdk -Dversion=1.0 -Dpackaging=jar -Dfile=extlibs/nutiteq-3dsdk-1.0.83pre.jar
mvn install:install-file -DgroupId=com.jhlabs -DartifactId=javaproj-noawt -Dversion=1.0.6 -Dpackaging=jar -Dfile=extlibs/javaproj-1.0.6-noawt.jar
mvn install:install-file -DgroupId=org.mapsforge.android -DartifactId=mapsforge-map -Dversion=0.3.0 -Dpackaging=jar -Dfile=extlibs/mapsforge-map-0.3.0-jar-with-dependencies.jar

* Test datasets *

 Spatialite : Romania OSM data, Spatialite 3.0 format: https://www.dropbox.com/s/j4aahjo7whkzx2r/romania_sp3857.sqlite
 Raster data: Digital Earth, Natural earth converted to Spherical Mercator: https://www.dropbox.com/s/fwd7f1l4gy36u94/natural-earth-2-mercator.tif
 Mapsforge: http://ftp.mapsforge.org/maps