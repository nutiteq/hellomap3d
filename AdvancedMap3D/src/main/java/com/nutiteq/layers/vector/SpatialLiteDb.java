package com.nutiteq.layers.vector;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.LongBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Vector;

import jsqlite.Blob;
import jsqlite.Callback;
import jsqlite.Database;
import jsqlite.Exception;

import org.proj4.Proj4;
import org.proj4.ProjectionData;

import com.jhlabs.map.proj.Projection;
import com.jhlabs.map.proj.ProjectionFactory;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.db.DBLayer;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.log.Log;
import com.nutiteq.style.LineStyle;
import com.nutiteq.style.PointStyle;
import com.nutiteq.style.PolygonStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.Utils;
import com.nutiteq.utils.WkbRead;

/**
 * Basic communicator with a simple SpatiaLite database.
 * 
 * @author jaak
 * 
 */
public class SpatialLiteDb {
  private static final int DEFAULT_SRID = 4326;
  private static final int SDK_SRID = 3857;

  private final Database db;
  private String dbPath;
  private String sdk_proj4text;

  public SpatialLiteDb(String dbPath) {

    if (!new File(dbPath).exists()) {
      Log.error("File not found: " + dbPath);
      db = null;
      return;

    }

    this.dbPath = dbPath;
    db = new jsqlite.Database();
    try {
      db.open(dbPath, jsqlite.Constants.SQLITE_OPEN_READONLY);
    } catch (Exception e) {
      Log.error("SpatialLite: Failed to open database! " + e.getMessage());
    }
    
    sdk_proj4text = qryProj4Def(SDK_SRID);
    //sdk_proj4text =  "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +no_defs";
  }

  public void close() {
    try {
      db.close();
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

  public Vector<DBLayer> qrySpatialLayerMetadata() {
    final Vector<DBLayer> dbLayers = new Vector<DBLayer>();
    String qry = "SELECT \"f_table_name\", \"f_geometry_column\", \"type\", \"coord_dimension\", geometry_columns.srid, \"spatial_index_enabled\", proj4text FROM \"geometry_columns\", spatial_ref_sys where geometry_columns.srid=spatial_ref_sys.srid";
    Log.debug(qry);
    try {
      db.exec(qry, new Callback() {

        //@Override
        public void columns(String[] coldata) {
        }

        //@Override
        public void types(String[] types) {
        }

        //@Override
        public boolean newrow(String[] rowdata) {
          int srid = DEFAULT_SRID;
          try {
            srid = Integer.parseInt(rowdata[4]);
          } catch (NumberFormatException e) {
            e.printStackTrace();
          }
          DBLayer dbLayer = new DBLayer(rowdata[0], rowdata[1], rowdata[2], rowdata[3], srid,
              rowdata[5].equals("1") ? true : false, rowdata[6]);

          dbLayers.add(dbLayer);
          return false;
        }
      });

    } catch (Exception e) {
      Log.error("SpatialLite: Failed to query metadata! " + e.getMessage());
    }
    return dbLayers;
  }

  public Vector<Geometry> qrySpatiaLiteGeom(final Envelope bbox, final int limit, final DBLayer dbLayer,
      final String[] userColumns) {
    final Vector<Geometry> geoms = new Vector<Geometry>();
    final long start = System.currentTimeMillis();

    Callback cb = new Callback() {
      //@Override
      public void columns(String[] coldata) {
      }

      //@Override
      public void types(String[] types) {
      }

      //@Override
      public boolean newrow(String[] rowdata) {
//        try {
 
          // Column values to userData Map
          final Map<String, String> userData = new HashMap<String, String>();
          for (int i = 1; i < rowdata.length; i++) {
            userData.put(userColumns[i - 1], rowdata[i]);
          }

          // First column is always geometry
          Geometry[] g1 = WkbRead.readWkb(new ByteArrayInputStream(Utils.hexStringToByteArray(rowdata[0])),userData);
          for(int i = 0; i < g1.length; i++){
             geoms.add(g1[i]);
          }
//        } catch (ParseException e) {
//          Log.error("SpatialLite: Failed to parse geometry! " + e.getMessage());
//        }

        return false;
      }
    };

    String userColumn = "";
    if (userColumns != null) {
      userColumn = ", " + Arrays.asList(userColumns).toString().replaceAll("^\\[|\\]$", "");
    }

    String geomCol = dbLayer.geomColumn;
    Envelope queryBbox;

    if (dbLayer.srid != SDK_SRID) {
      Log.debug("SpatialLite: Data must be transformed from "+SDK_SRID+" to "+dbLayer.srid);
      geomCol = "Transform(" + dbLayer.geomColumn + "," + SDK_SRID + ")";

      Log.debug("original bbox :"+bbox);      
      
      queryBbox = transformBboxProj4(bbox, sdk_proj4text, dbLayer.proj4txt);

      Log.debug("converted to Layer SRID:"+queryBbox);
    } else {
        queryBbox = bbox;
    }

    String noIndexWhere = "MBRIntersects(BuildMBR(" + queryBbox.getMinX() + "," + queryBbox.getMinY() + "," + queryBbox.getMaxX() + "," + queryBbox.getMaxY() + "),"
        + dbLayer.geomColumn + ")";

    String qry;
    if (!dbLayer.spatialIndex) {
      qry = "SELECT HEX(AsBinary(" + geomCol + ")) " + userColumn + " from \"" + dbLayer.table + "\" where "
          + noIndexWhere
          + " LIMIT " + limit + ";";
    } else {
      qry = "SELECT HEX(AsBinary(" + geomCol + ")) " + userColumn + " from \"" + dbLayer.table
          + "\" where ROWID IN (select pkid from idx_" + dbLayer.table + "_" + dbLayer.geomColumn
          + " where pkid MATCH RtreeIntersects(" + + queryBbox.getMinX() + "," + queryBbox.getMinY() + "," + queryBbox.getMaxX() + "," + queryBbox.getMaxY() + ")) LIMIT " + limit;
    }

    Log.debug(qry);
    try {
      db.exec(qry, cb);
    } catch (Exception e) {
      Log.error("SpatialLite: Failed to query data! " + e.getMessage());
    }
    Log.debug("Query time: " + (System.currentTimeMillis() - start) + " , size: " + geoms.size());

    return geoms;
  }

  /**
   * Reproject bounding box using Proj.4 (NDK) library
   * @param bbox input bounding box
   * @param fromProj proj4 text
   * @param toProj
   * @return projected bounding box
   */

    private Envelope transformBboxProj4(Envelope bbox, String fromProj,
            String toProj) {
        ProjectionData dataTP = new ProjectionData(new double[][] {
                { bbox.getMinX(), bbox.getMinY() },
                { bbox.getMaxX(), bbox.getMaxY() } }, new double[] { 0, 0 });

        Proj4 toWgsProjection = new Proj4(fromProj, toProj);
        toWgsProjection.transform(dataTP, 2, 1);

        return new Envelope(dataTP.x[0], dataTP.x[1], dataTP.y[0], dataTP.y[1]);
    }

    /**
     * Reproject bounding box using javaProj library. Slower and not so stable
     * than Proj.4 but does not require NDK
     * 
     * @param bbox
     *            input bounding box
     * @param fromProj
     *            proj4 text
     * @param toProj
     * @return projected bounding box
     */
    private Envelope transformBboxJavaProj(Envelope bbox, String fromProj,
            String toProj) {

        Projection projection = ProjectionFactory
                .fromPROJ4Specification(fromProj.split(" "));
        com.jhlabs.map.Point2D.Double minA = new com.jhlabs.map.Point2D.Double(
                bbox.getMinX(), bbox.getMinY());
        com.jhlabs.map.Point2D.Double maxA = new com.jhlabs.map.Point2D.Double(
                bbox.getMaxX(), bbox.getMaxY());
        com.jhlabs.map.Point2D.Double minB = new com.jhlabs.map.Point2D.Double();
        com.jhlabs.map.Point2D.Double maxB = new com.jhlabs.map.Point2D.Double();
        projection.inverseTransform(minA, minB);
        projection.inverseTransform(maxA, maxB);
        Log.debug("converted to wgs84:" + minA + " " + minB + " " + maxA + " "
                + maxB);
        // then from Wgs84 to Layer SRID
        Projection projection2 = ProjectionFactory
                .fromPROJ4Specification(toProj.split(" "));
        com.jhlabs.map.Point2D.Double minC = new com.jhlabs.map.Point2D.Double();
        com.jhlabs.map.Point2D.Double maxC = new com.jhlabs.map.Point2D.Double();
        projection2.transform(minB, minC);
        projection2.transform(maxB, maxC);

        Log.debug("converted to Layer SRID:" + minA + " " + minB + " " + maxA
                + " " + maxB);

        return new Envelope(minC.x, maxC.x, minC.y, maxC.y);
    }

public List<List<MapPos>> qrySpatiaLiteTriangles(Envelope bbox, int limit, DBLayer dbLayer, String idColumn,
      String trianglesColumn) {
    final List<List<MapPos>> trianglesLists = new LinkedList<List<MapPos>>();
    final long start = System.currentTimeMillis();

    Callback cb = new Callback() {
      //@Override
      public void columns(String[] coldata) {
      }

      //@Override
      public void types(String[] types) {
      }

      //@Override
      public boolean newrow(String[] rowdata) {
        byte[] bytes = Utils.hexStringToByteArray(rowdata[1]);
        ByteBuffer byteBuffer = ByteBuffer.wrap(bytes);
        LongBuffer longBuffer = byteBuffer.asLongBuffer();
        List<MapPos> triangles = new LinkedList<MapPos>();
        for (int tsj = 0; tsj < bytes.length / 16; tsj++) {
          int index = tsj * 2;
          triangles.add(new MapPos(longBuffer.get(index) / 1000.0f, longBuffer.get(index + 1) / 1000.0f, 1));
          // Log.debug("Point: "
          // + (new ImmutableMapPos(longBuffer.get(index) / 1000.0f, longBuffer.get(index + 1) / 1000.0f, 1f)
          // .toString()));

        }
        trianglesLists.add(triangles);

        return false;
      }
    };

    double minX = bbox.getMinX();
    double minY = bbox.getMinY();
    double maxX = bbox.getMaxX();
    double maxY = bbox.getMaxY();

    String noIndexWhere = "MBRIntersects(BuildMBR(" + minX + "," + minY + "," + maxX + "," + maxY + "),"
        + dbLayer.geomColumn + ")";

    String qry;
    if (!dbLayer.spatialIndex) {
      qry = "SELECT " + idColumn + ", HEX(" + trianglesColumn + ") from " + dbLayer.table + " where " + noIndexWhere
          + " LIMIT " + limit + ";";
    } else {
      qry = "SELECT " + idColumn + ", HEX(" + trianglesColumn + ") from " + dbLayer.table
          + " where ROWID IN (select pkid from idx_" + dbLayer.table + "_" + dbLayer.geomColumn
          + " where pkid MATCH RtreeIntersects(" + minX + "," + minY + "," + maxX + "," + maxY + ")) LIMIT " + limit;
    }

    Log.debug(qry);
    try {
      db.exec(qry, cb);
    } catch (Exception e) {
      Log.error("SpatialLite: Failed to query data! " + e.getMessage());
    }
    Log.debug("Query time: " + (System.currentTimeMillis() - start) + " , size: " + trianglesLists.size());

    return trianglesLists;
  }

  public String guessNameColumn(String table) {
    String qry = "PRAGMA table_info(" + table + ")";
    PragmaRowCallBack rcb = new PragmaRowCallBack();

    try {
      db.exec(qry, rcb);
    } catch (Exception e) {
      Log.error("SpatialLite: Failed to query table info! " + e.getMessage());
    }

    return rcb.foundName != null ? rcb.foundName : rcb.nameCandidate;
  }
  
  public byte[] readBlob(String table, String field, long rowid, int offset, int length) {
    Blob blob = null;
    try {
      blob = db.open_blob("main", table, field, rowid, false);
    } catch (Exception e1) {
      Log.error("SpatialLite: open_blob error "+e1.getMessage());
      return null;
    }

    byte[] data = new byte[length];
    try {
      blob.getInputStream().read(data, offset, length);
    } catch (IOException e1) {
      blob.close();
      Log.error("SpatialLite: Blob reading failed!");
      return null;
    }
    blob.close();
    return data;
  }

  public class PragmaRowCallBack implements Callback {
    public String foundName;
    public String nameCandidate;

    //@Override
    public void columns(String[] coldata) {
    }

    //@Override
    public void types(String[] types) {
    }

    //@Override
    public boolean newrow(String[] rowdata) {
      if (rowdata[1].toLowerCase().equals("name")) {
        foundName = rowdata[1];
        return false;
      }

      if (rowdata[2].toLowerCase().equals("text") && nameCandidate == null) {
        nameCandidate = rowdata[1];
      }

      return false;
    }
  }

  /*
  public Map<String, NMLModel.MipChain> qryModelTextures(final NMLModel model, Integer id, String localTextureId, int minLevel, int maxLevel) {

    //final long start = System.currentTimeMillis();
    final Map<String, NMLModel.MipChain> mipChainMap = new HashMap<String, NMLModel.MipChain>();

    Callback cb = new Callback() {
      //@Override
      public void columns(String[] coldata) {
      }

      //@Override
      public void types(String[] types) {
      }

      //@Override
      public boolean newrow(String[] rowdata) {

        String localtexture_id = rowdata[0];
        long texture_id = Long.parseLong(rowdata[1]);
        int texture_size = Integer.parseInt(rowdata[2]);
        int level = Integer.parseInt(rowdata[3]);

        byte[] nmltexture = readBlob("textures", "nmltexture", texture_id, 0, texture_size);
        if (nmltexture == null || nmltexture.length == 0) {
          Log.debug("SpatialLite: Texture is empty");
          return false;
        }
        try {
          NMLPackage.Texture texture = NMLPackage.Texture.parseFrom(nmltexture);
          List<NMLPackage.Texture> textures = new ArrayList<NMLPackage.Texture>();
          textures.add(texture);
          NMLModel.MipChain mipChain = mipChainMap.get(localtexture_id);
          if (mipChain != null) {
            if (mipChain.getMinLevel() == level + 1)
              textures.addAll(mipChain.getTextures());
            else
              Log.debug("SpatialLite: Missing texture level!");
          }
          mipChainMap.put(localtexture_id, model.new MipChain(level, textures));
        } catch (InvalidProtocolBufferException e) {
          Log.debug("SpatialLite: Illegal texture data!");
          return false;
        }
        return false;
      }
    };

    String qry = "SELECT localtexture_id, textures.rowid, length(nmltexture), textures.level FROM textures, buildingtextures WHERE building_id=" + id + " AND textures.id=texture_id AND textures.level>=" + minLevel + " AND textures.level<=" + maxLevel + (localTextureId != null ? " AND localtexture_id='" + localTextureId + "'" : "") + " ORDER BY level DESC";
    //Log.debug(qry);
    try {
      db.exec(qry, cb);
    } catch (Exception e) {
      Log.error("SpatialLite: Failed to query data! " + e.getMessage());
    }
    //Log.debug("Query time: " + (System.currentTimeMillis() - start));
    return mipChainMap;
  }

  public Map<String, Integer> qryModelMaxTextureLevels(NMLModel model, Integer id) {
    
    final Map<String, Integer> maxLevels = new HashMap<String, Integer>();
    
    Callback cb = new Callback() {
      @Override
      public void columns(String[] coldata) {
      }

      @Override
      public void types(String[] types) {
      }

      @Override
      public boolean newrow(String[] rowdata) {
        String localtexture_id = rowdata[0];
        Integer maxLevel = Integer.parseInt(rowdata[1]);
        maxLevels.put(localtexture_id, maxLevel);
        return false;
      }
    };
    
    String qry = "SELECT localtexture_id, MAX(textures.level) FROM textures, buildingtextures WHERE building_id=" + id + " AND textures.id=texture_id GROUP BY localtexture_id";
    //Log.debug(qry);
    try {
      db.exec(qry, cb);
    } catch (Exception e) {
      Log.error("SpatialLite: Failed to query data! " + e.getMessage());
    }
    return maxLevels;
  }*/

private String qryProj4Def(int srid){

    String qry = "select proj4text from spatial_ref_sys where srid = "+srid;
    ProjRowCallBack rcb = new ProjRowCallBack();

    Log.debug(qry);
    try {
       db.exec(qry, rcb);
       
    } catch (Exception e) {
        // TODO Auto-generated catch block
        e.printStackTrace();
    }
    // if not found
    return rcb.foundName;
}

public class ProjRowCallBack implements Callback {

public String foundName;

//@Override
public void columns(String[] coldata) {
    Log.debug("SRID Columns: " + Arrays.toString(coldata));
}

//@Override
public void types(String[] types) {
    // not called really
    Log.debug("Types: " + Arrays.toString(types));
}

//@Override
public boolean newrow(String[] rowdata) {
    Log.debug("srid row: " + Arrays.toString(rowdata));
    foundName = rowdata[0];
    return false;
}
}

/*
public Vector<Geometry> qrySpatiaLiteGeomTexture(final Envelope bbox, final int limit, final DBLayer dbLayer,
        final String[] userColumns) {
      final Vector<Geometry> geoms = new Vector<Geometry>();
      final long start = System.currentTimeMillis();

      Callback cb = new Callback() {
        //@Override
        public void columns(String[] coldata) {
        }

        //@Override
        public void types(String[] types) {
        }

        //@Override
        public boolean newrow(String[] rowdata) {
          try {
            // First column is always geometry
            Geometry g1 = new WKBReader().read(Utils.hexStringToByteArray(rowdata[0]));
            // Add other column values to userData
            final Map<String, String> userData = new HashMap<String, String>();
            for (int i = 1; i < rowdata.length; i++) {
              userData.put(userColumns[i - 1], rowdata[i]);
            }
            g1.setUserData(userData);
            geoms.add(g1);
          } catch (ParseException e) {
            Log.error("SpatialLite: Failed to parse geometry! " + e.getMessage());
          }

          return false;
        }
      };

      String userColumn = "";
      if (userColumns != null) {
        userColumn = ", " + Arrays.asList(userColumns).toString().replaceAll("^\\[|\\]$", "");
      }

      String geomCol = dbLayer.geomColumn;
      double minX;
      double minY;
      double maxX;
      double maxY;
      if (dbLayer.srid != SDK_SRID) {
        Log.debug("SpatialLite: Data must be transformed!");
        geomCol = "Transform(" + dbLayer.geomColumn + "," + SDK_SRID + ")";
        // ProjectionData dataTP = new ProjectionData(new double[][] { { bbox.getMinX(), bbox.getMinY() },
        // { bbox.getMaxX(), bbox.getMaxY() } }, new double[] { 0, 0 });
        // String srid = "+init=epsg:" + dbLayer.srid;
        // Proj4 toWgsProjection = new Proj4("+proj=latlong +datum=WGS84", srid);
        // toWgsProjection.transform(dataTP, 2, 1);
        // minX = dataTP.x[0];
        // minY = dataTP.y[0];
        // maxX = dataTP.x[1];
        // maxY = dataTP.y[1];

        // first from SDK_SRID to Wgs84
        Projection projection = ProjectionFactory.fromPROJ4Specification(new String[] { "+proj=merc", "+a=6378137",
            "+b=6378137", "+lat_ts=0.0", "+lon_0=0.0", "+x_0=0.0", "+y_0=0", "+k=1.0", "+units=m", "+nadgrids=@null",
            "+no_defs" });
        com.jhlabs.map.Point2D.Double minA = new Double(bbox.getMinX(), bbox.getMinY());
        com.jhlabs.map.Point2D.Double maxA = new Double(bbox.getMaxX(), bbox.getMaxY());
        com.jhlabs.map.Point2D.Double minB = new Double();
        com.jhlabs.map.Point2D.Double maxB = new Double();
        projection.inverseTransform(minA, minB);
        projection.inverseTransform(maxA, maxB);
        // then from Wgs84 to Layer SRID
        Projection projection2 = ProjectionFactory.fromPROJ4Specification(dbLayer.proj4txt.split(" "));
        com.jhlabs.map.Point2D.Double minC = new Double();
        com.jhlabs.map.Point2D.Double maxC = new Double();
        projection2.transform(minB, minC);
        projection2.transform(maxB, maxC);

        minX = minC.x;
        minY = minC.y;
        maxX = maxC.x;
        maxY = maxC.y;

      } else {
        minX = bbox.getMinX();
        minY = bbox.getMinY();
        maxX = bbox.getMaxX();
        maxY = bbox.getMaxY();
      }

      String noIndexWhere = "MBRIntersects(BuildMBR(" + minX + "," + minY + "," + maxX + "," + maxY + "),"
          + dbLayer.geomColumn + ")";

      String qry;
      if (!dbLayer.spatialIndex) {
        qry = "SELECT HEX(AsBinary(" + geomCol + ")) " + userColumn + " from \"" + dbLayer.table + "\" where "
            + noIndexWhere
            + " AND building_id=buildings.id AND textures.id=texture_id LIMIT " + limit + ";";
      } else {
        qry = "SELECT HEX(AsBinary(" + geomCol + ")) " + userColumn + " from \"" + dbLayer.table
            + "\", textures, buildingtextures where buildings.ROWID IN (select pkid from idx_" + dbLayer.table + "_" + dbLayer.geomColumn
            + " WHERE pkid MATCH RtreeIntersects(" + minX + "," + minY + "," + maxX + "," + maxY + ")) AND building_id=buildings.id AND textures.id=texture_id LIMIT " + limit;
      }

      Log.debug(qry);
      try {
        db.exec(qry, cb);
      } catch (Exception e) {
        Log.error("SpatialLite: Failed to query data! " + e.getMessage());
      }
      Log.debug("Query time: " + (System.currentTimeMillis() - start) + " , size: " + geoms.size());

      return geoms;
    }

*/
}
