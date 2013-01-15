
* Install dependency JARs to maven *

  Needed until there is online repo for these.

mvn install:install-file -DgroupId=com.nutiteq -DartifactId=nutiteq-3d-sdk -Dversion=1.0 -Dpackaging=jar -Dfile=Downloads/nutiteq-3dsdk-1.0.78pre.jar
mvn install:install-file -DgroupId=com.jhlabs -DartifactId=javaproj-noawt -Dversion=1.0.6 -Dpackaging=jar -Dfile=/Users/jaak/git/hellomap3d/HelloMap3D/libs/javaproj-1.0.6-noawt.jar

* Test datasets *

 Spatialite : Romania OSM data, Spatialite 3.0 format: https://www.dropbox.com/s/j4aahjo7whkzx2r/romania_sp3857.sqlite
 Raster data: Digital Earth, Natural earth converted to Spherical Mercator: natural-earth-2-mercator.tif
  