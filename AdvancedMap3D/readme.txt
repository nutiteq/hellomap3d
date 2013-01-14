
* Install dependency JARs to maven *

  Needed until there is online repo for these.

mvn install:install-file -DgroupId=com.nutiteq -DartifactId=nutiteq-3d-sdk -Dversion=1.0 -Dpackaging=jar -Dfile=Downloads/nutiteq-3dsdk-1.0.78pre.jar
mvn install:install-file -DgroupId=com.jhlabs -DartifactId=javaproj-noawt -Dversion=1.0.6 -Dpackaging=jar -Dfile=/Users/jaak/git/hellomap3d/HelloMap3D/libs/javaproj-1.0.6-noawt.jar

* Test datasets *

 Spatialite : Romania OSM data, Spatialite 3.0 format: https://www.dropbox.com/c/shmodel?nsid=125341383&sjid=0&state=2&signature=daef840&path=/romania_sp3857.sqlite&id=shmodel
 