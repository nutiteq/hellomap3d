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
import com.nutiteq.utils.GeoUtils;
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
        // sdk_proj4text =
        // "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +no_defs";
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

                // @Override
                public void columns(String[] coldata) {
                }

                // @Override
                public void types(String[] types) {
                }

                // @Override
                public boolean newrow(String[] rowdata) {
                    int srid = DEFAULT_SRID;
                    try {
                        srid = Integer.parseInt(rowdata[4]);
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                    DBLayer dbLayer = new DBLayer(rowdata[0], rowdata[1],
                            rowdata[2], rowdata[3], srid, rowdata[5]
                                    .equals("1") ? true : false, rowdata[6]);

                    dbLayers.add(dbLayer);
                    return false;
                }
            });

        } catch (Exception e) {
            Log.error("SpatialLite: Failed to query metadata! "
                    + e.getMessage());
        }
        return dbLayers;
    }

    public Vector<Geometry> qrySpatiaLiteGeom(final Envelope bbox,
            final int limit, final DBLayer dbLayer, final String[] userColumns) {
        final Vector<Geometry> geoms = new Vector<Geometry>();
        final long start = System.currentTimeMillis();

        Callback cb = new Callback() {
            // @Override
            public void columns(String[] coldata) {
            }

            // @Override
            public void types(String[] types) {
            }

            // @Override
            public boolean newrow(String[] rowdata) {

                // Column values to userData Map
                final Map<String, String> userData = new HashMap<String, String>();
                for (int i = 1; i < rowdata.length; i++) {
                    userData.put(userColumns[i - 1], rowdata[i]);
                }

                // First column is always geometry
                Geometry[] g1 = WkbRead.readWkb(
                        new ByteArrayInputStream(Utils
                                .hexStringToByteArray(rowdata[0])), userData);
                for (int i = 0; i < g1.length; i++) {
                    geoms.add(g1[i]);
                }

                return false;
            }
        };

        String userColumn = "";
        if (userColumns != null) {
            userColumn = ", "
                    + Arrays.asList(userColumns).toString()
                            .replaceAll("^\\[|\\]$", "");
        }

        String geomCol = dbLayer.geomColumn;
        Envelope queryBbox;

        if (dbLayer.srid != SDK_SRID) {
            Log.debug("SpatialLite: Data must be transformed from " + SDK_SRID
                    + " to " + dbLayer.srid);
            geomCol = "Transform(" + dbLayer.geomColumn + "," + SDK_SRID + ")";

            Log.debug("original bbox :" + bbox);

            queryBbox = GeoUtils.transformBboxProj4(bbox, sdk_proj4text,
                    dbLayer.proj4txt);

            Log.debug("converted to Layer SRID:" + queryBbox);
        } else {
            queryBbox = bbox;
        }

        String noIndexWhere = "MBRIntersects(BuildMBR(" + queryBbox.getMinX()
                + "," + queryBbox.getMinY() + "," + queryBbox.getMaxX() + ","
                + queryBbox.getMaxY() + ")," + dbLayer.geomColumn + ")";

        String qry;
        if (!dbLayer.spatialIndex) {
            qry = "SELECT HEX(AsBinary(" + geomCol + ")) " + userColumn
                    + " from \"" + dbLayer.table + "\" where " + noIndexWhere
                    + " LIMIT " + limit + ";";
        } else {
            qry = "SELECT HEX(AsBinary(" + geomCol + ")) " + userColumn
                    + " from \"" + dbLayer.table
                    + "\" where ROWID IN (select pkid from idx_"
                    + dbLayer.table + "_" + dbLayer.geomColumn
                    + " where pkid MATCH RtreeIntersects("
                    + +queryBbox.getMinX() + "," + queryBbox.getMinY() + ","
                    + queryBbox.getMaxX() + "," + queryBbox.getMaxY()
                    + ")) LIMIT " + limit;
        }

        Log.debug(qry);
        try {
            db.exec(qry, cb);
        } catch (Exception e) {
            Log.error("SpatialLite: Failed to query data! " + e.getMessage());
        }
        Log.debug("Query time: " + (System.currentTimeMillis() - start)
                + " , size: " + geoms.size());

        return geoms;
    }



    private String qryProj4Def(int srid) {

        String qry = "select proj4text from spatial_ref_sys where srid = "
                + srid;
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

        // @Override
        public void columns(String[] coldata) {
            Log.debug("SRID Columns: " + Arrays.toString(coldata));
        }

        // @Override
        public void types(String[] types) {
            // not called really
            Log.debug("Types: " + Arrays.toString(types));
        }

        // @Override
        public boolean newrow(String[] rowdata) {
            Log.debug("srid row: " + Arrays.toString(rowdata));
            foundName = rowdata[0];
            return false;
        }
    }

}
