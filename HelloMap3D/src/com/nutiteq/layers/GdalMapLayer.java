package com.nutiteq.layers;

import java.util.Enumeration;
import java.util.Vector;

import org.gdal.gdal.Band;
import org.gdal.gdal.ColorTable;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.GCP;
import org.gdal.gdal.RasterAttributeTable;
import org.gdal.gdal.TermProgressCallback;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconstConstants;
import org.gdal.osr.CoordinateTransformation;
import org.gdal.osr.SpatialReference;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;

import cern.colt.Arrays;

import com.nutiteq.MapView;
import com.nutiteq.components.TileQuadTreeNode;
import com.nutiteq.db.MbTilesDatabaseHelper;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.DbFetchTileTask;
import com.vividsolutions.jts.geom.Envelope;

public class GdalMapLayer extends RasterLayer {
    
    
    // force Java to load PROJ.4 library. Needed as we don't call it directly, but 
    // GDAL datasource reading may need it.
    
    static {
        try {
          System.loadLibrary("proj");
        } catch (Throwable t) {
            System.err.println("Unable to load proj: " + t);
        }
        }
    
    private MbTilesDatabaseHelper db;
    private Dataset hDataset;
    private Envelope boundsEnvelope;
    private MapView mapView;

    /**
     * Read raster data source using GDAL library. Tested with:
     *    indexed color: NOAA BSB
     *    RGB bands in GeoTIFF file (tested with Natural Earth geotiff as described in http://mapbox.com/tilemill/docs/guides/reprojecting-geotiff/)
     *    Data must be EPSG:3857, could be with VRT but then slower. Overviews suggested also
     *     
     * @param projection map layer projection. Currently only EPSG3857 is supported
     * @param minZoom minimum Zoom
     * @param maxZoom maximum Zoom
     * @param id caching ID, make it unique
     * @param gdalSource file to be read, full path
     * @param mapView
     */
    public GdalMapLayer(Projection projection, int minZoom, int maxZoom,
            int id, String gdalSource, MapView mapView) {
        super(projection, minZoom, maxZoom, id, gdalSource);
        
        this.mapView = mapView;
        gdal.AllRegister();
        hDataset = gdal.Open(gdalSource, gdalconstConstants.GA_ReadOnly);

        
        this.boundsEnvelope = bounds(hDataset); 

        /* -------------------------------------------------------------------- */
        /*      Report general info for debugging.                              */
        /* -------------------------------------------------------------------- */
        if (hDataset == null) {
            Log.error("GDALOpen failed - " + gdal.GetLastErrorNo());
            Log.error(gdal.GetLastErrorMsg());

            // gdal.DumpOpenDatasets( stderr );

            // gdal.DestroyDriverManager();

            // gdal.DumpSharedList( null );

            return;
        }
        
        listDrivers();
        //fullGdalInfo();
        
    }

    private void listDrivers() {
        for (int i=0;i<gdal.GetDriverCount();i++){
            Driver driver = gdal.GetDriver(i);
            Log.info("driver " + driver.getShortName()+" ("+driver.getLongName()+")");
        }
    }


    private Envelope bounds(Dataset hDataset) {
        double[][] corner= new double[4][2];
        // hardcoded EPSG:3857 here
        // TODO: take real layer projection and  
        SpatialReference layerProjection = new SpatialReference("PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"Popular Visualisation CRS\",DATUM[\"Popular_Visualisation_Datum\",SPHEROID[\"Popular Visualisation Sphere\",6378137,0,AUTHORITY[\"EPSG\",\"7059\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6055\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4055\"]],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],AUTHORITY[\"EPSG\",\"3785\"],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH]]");

   //     corner[0] = corner(hDataset, layerProjection, 0.0, 0.0);
        corner[1] = corner(hDataset, layerProjection, 0.0, hDataset
                .getRasterYSize());
        corner[2] = corner(hDataset,layerProjection, hDataset
                .getRasterXSize(), 0.0);
//        corner[3] = corner(hDataset, layerProjection, hDataset
//                .getRasterXSize(), hDataset.getRasterYSize());
        
        return new Envelope(corner[1][0],corner[2][0],corner[1][1],corner[2][1]);
    }


    @Override
    public void fetchTile(TileQuadTreeNode tile) {
        Log.debug("GdalMapLayer: Start loading " + " zoom=" + tile.zoom + " x="
                + tile.x + " y=" + tile.y);
        
        // use task - thread issues!
//        components.rendererTaskPool.execute(new GdalFetchTileTask(tile,
//                components, getTileIdOffset(), hDataset, 1, boundsEnvelope));
        
        // do not use tasks, as GDAL is not threadsafe
        GdalFetchTile getTile = new GdalFetchTile(tile,
              components, getTileIdOffset(), hDataset, boundsEnvelope);
        byte[] data = getTile.getData();

          if (data == null) {
            Log.error(getClass().getName() + " : No data.");
          } else {
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inScaled = false;
            Bitmap bitmap = BitmapFactory.decodeByteArray(data, 0, data.length, opts);
            if (bitmap == null) {
              // If the compressed image is corrupt, delete it
              Log.error(getClass().getName() + " : Failed to decode the image.");
            } else {
              // Add the compressed image to persistentCache
              components.persistentCache.add(tileIdOffset + tile.id, data);
              // Add the compressed image to compressedMemoryCache
              components.compressedMemoryCache.add(tileIdOffset + tile.id, data);
              // If not corrupt, add to the textureMemoryCache
              components.textureMemoryCache.add(tileIdOffset + tile.id, bitmap);
              mapView.requestRender();
            }
          }
    }

    @Override
    public void flush() {

    }

    public void close() {
        db.close();
    }
    
    /**
     * Calculate corner coordinates of dataset, in layerProjection
     * @param hDataset
     * @param layerProjection
     * @param x coordinates of bounds
     * @param y coordinates of bounds 
     * @return
     */
    static double[] corner(Dataset hDataset, SpatialReference layerProjection, double x, double y)

    {
        double dfGeoX, dfGeoY;
        String pszProjection;
        double[] adfGeoTransform = new double[6];
        CoordinateTransformation hTransform = null;

        /* -------------------------------------------------------------------- */
        /*      Transform the point into georeferenced coordinates.             */
        /* -------------------------------------------------------------------- */
        hDataset.GetGeoTransform(adfGeoTransform);
        
        {
            pszProjection = hDataset.GetProjectionRef();
          //  pszProjection = "EPSG:4326";

            dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
                    + adfGeoTransform[2] * y;
            dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
                    + adfGeoTransform[5] * y;
        }

        return new double[]{dfGeoX, dfGeoY};
/*        
        if (adfGeoTransform[0] == 0 && adfGeoTransform[1] == 0
                && adfGeoTransform[2] == 0 && adfGeoTransform[3] == 0
                && adfGeoTransform[4] == 0 && adfGeoTransform[5] == 0) {
            return null;
        }

        if (pszProjection != null && pszProjection.length() > 0) {
            SpatialReference hProj;

            hProj = new SpatialReference(pszProjection);
            
            // if not defined, use default projection EPSG:4326
//            if (hProj == null)
//                hLatLong = new SpatialReference("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]]");

            // force EPSG:3857 as source projection
            hProj = layerProjection.CloneGeogCS();

            if (layerProjection != null) {
                gdal.PushErrorHandler( "CPLQuietErrorHandler" );
                hTransform = new CoordinateTransformation(hProj, layerProjection);
                gdal.PopErrorHandler();
                layerProjection.delete();
                if (gdal.GetLastErrorMsg().indexOf("Unable to load PROJ.4 library") != -1)
                    hTransform = null;
            }

            if (hProj != null)
                hProj.delete();
        }

        double[] transPoint = new double[3];
        if (hTransform != null) {
            hTransform.TransformPoint(transPoint, dfGeoX, dfGeoY, 0);
        }

        if (hTransform != null)
            hTransform.delete();

        return new double[]{transPoint[0], transPoint[1]};
        */
    }
    
    /**
     * Reports GDAL info (like gdalinfo utility)
     * Following code is from gdalinfo.java sample
     */
    private void fullGdalInfo(){
        
        Band hBand;
        int i, iBand;
        double[] adfGeoTransform = new double[6];
        Driver hDriver;
        Vector papszMetadata;
        boolean bComputeMinMax = false, bSample = false;
        boolean bShowGCPs = true, bShowMetadata = true;
        boolean bStats = false, bApproxStats = true;
                    boolean bShowColorTable = true, bComputeChecksum = false;
                    boolean bReportHistograms = false;
                    boolean bShowRAT = true;
        String pszFilename = null;
                    Vector papszFileList;
                    Vector papszExtraMDDomains = new Vector();

        
        /* -------------------------------------------------------------------- */
        /*      Report general info.                                            */
        /* -------------------------------------------------------------------- */
        hDriver = hDataset.GetDriver();
        Log.info("Driver: " + hDriver.getShortName() + "/"
                + hDriver.getLongName());

                    papszFileList = hDataset.GetFileList( );
                    if( papszFileList.size() == 0 )
                    {
                        Log.info( "Files: none associated" );
                    }
                    else
                    {
                        Enumeration e = papszFileList.elements();
                        Log.info( "Files: " + (String)e.nextElement() );
                        while(e.hasMoreElements())
                            Log.info( "       " +  (String)e.nextElement() );
                    }

        Log.info("Size is " + hDataset.getRasterXSize() + ", "
                + hDataset.getRasterYSize());

        /* -------------------------------------------------------------------- */
        /*      Report projection.                                              */
        /* -------------------------------------------------------------------- */
        if (hDataset.GetProjectionRef() != null) {
            SpatialReference hSRS;
            String pszProjection;

            pszProjection = hDataset.GetProjectionRef();

            hSRS = new SpatialReference(pszProjection);
            if (hSRS != null && pszProjection.length() != 0) {
                String[] pszPrettyWkt = new String[1];

                hSRS.ExportToPrettyWkt(pszPrettyWkt, 0);
                Log.info("Coordinate System is:");
                Log.info(pszPrettyWkt[0]);
                //gdal.CPLFree( pszPrettyWkt );
            } else
                Log.info("Coordinate System is `"
                        + hDataset.GetProjectionRef() + "'");

            hSRS.delete();
        }

        /* -------------------------------------------------------------------- */
        /*      Report Geotransform.                                            */
        /* -------------------------------------------------------------------- */
        hDataset.GetGeoTransform(adfGeoTransform);
        Log.info("geotransform "+ Arrays.toString(adfGeoTransform));
        {
            if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0) {
                Log.info("Origin = (" + adfGeoTransform[0] + ","
                        + adfGeoTransform[3] + ")");

                Log.info("Pixel Size = (" + adfGeoTransform[1]
                        + "," + adfGeoTransform[5] + ")");
            } else {
                Log.info("GeoTransform =");
                                    Log.info("  " + adfGeoTransform[0] + ", "
                                                    + adfGeoTransform[1] + ", " + adfGeoTransform[2]);
                                    Log.info("  " + adfGeoTransform[3] + ", "
                                                    + adfGeoTransform[4] + ", " + adfGeoTransform[5]);
                            }
        }

        /* -------------------------------------------------------------------- */
        /*      Report GCPs.                                                    */
        /* -------------------------------------------------------------------- */
        if (bShowGCPs && hDataset.GetGCPCount() > 0) {
            Log.info("GCP Projection = "
                    + hDataset.GetGCPProjection());

            int count = 0;
            Vector GCPs = new Vector();
            hDataset.GetGCPs(GCPs);

            Enumeration e = GCPs.elements();
            while (e.hasMoreElements()) {
                GCP gcp = (GCP) e.nextElement();
                Log.info("GCP[" + (count++) + "]: Id="
                        + gcp.getId() + ", Info=" + gcp.getInfo());
                Log.info("    (" + gcp.getGCPPixel() + ","
                        + gcp.getGCPLine() + ") (" + gcp.getGCPX() + ","
                        + gcp.getGCPY() + "," + gcp.getGCPZ() + ")");
            }

        }

        /* -------------------------------------------------------------------- */
        /*      Report metadata.                                                */
        /* -------------------------------------------------------------------- */
        papszMetadata = hDataset.GetMetadata_List("");
        if (bShowMetadata && papszMetadata.size() > 0) {
            Enumeration keys = papszMetadata.elements();
            Log.info("Metadata:");
            while (keys.hasMoreElements()) {
                Log.info("  " + (String) keys.nextElement());
            }
        }
                    
                    Enumeration eExtraMDDDomains = papszExtraMDDomains.elements();
                    while(eExtraMDDDomains.hasMoreElements())
                    {
                        String pszDomain = (String)eExtraMDDDomains.nextElement();
                        papszMetadata = hDataset.GetMetadata_List(pszDomain);
                        if( bShowMetadata && papszMetadata.size() > 0 )
                        {
                            Enumeration keys = papszMetadata.elements();
                            Log.info("Metadata (" + pszDomain + "):");
                            while (keys.hasMoreElements()) {
                Log.info("  " + (String) keys.nextElement());
            }
                        }
                    }
                    /* -------------------------------------------------------------------- */
                    /*      Report "IMAGE_STRUCTURE" metadata.                              */
                    /* -------------------------------------------------------------------- */
                    papszMetadata = hDataset.GetMetadata_List("IMAGE_STRUCTURE" );
                    if( bShowMetadata && papszMetadata.size() > 0) {
            Enumeration keys = papszMetadata.elements();
            Log.info("Image Structure Metadata:");
            while (keys.hasMoreElements()) {
                Log.info("  " + (String) keys.nextElement());
            }
        }
        /* -------------------------------------------------------------------- */
        /*      Report subdatasets.                                             */
        /* -------------------------------------------------------------------- */
        papszMetadata = hDataset.GetMetadata_List("SUBDATASETS");
        if (papszMetadata.size() > 0) {
            Log.info("Subdatasets:");
            Enumeration keys = papszMetadata.elements();
            while (keys.hasMoreElements()) {
                Log.info("  " + (String) keys.nextElement());
            }
        }
                
                /* -------------------------------------------------------------------- */
                /*      Report geolocation.                                             */
                /* -------------------------------------------------------------------- */
                    papszMetadata = hDataset.GetMetadata_List("GEOLOCATION" );
                    if (papszMetadata.size() > 0) {
                        Log.info( "Geolocation:" );
                        Enumeration keys = papszMetadata.elements();
                        while (keys.hasMoreElements()) {
                                Log.info("  " + (String) keys.nextElement());
                        }
                    }
                
                /* -------------------------------------------------------------------- */
                /*      Report RPCs                                                     */
                /* -------------------------------------------------------------------- */
                    papszMetadata = hDataset.GetMetadata_List("RPC" );
                    if (papszMetadata.size() > 0) {
                        Log.info( "RPC Metadata:" );
                        Enumeration keys = papszMetadata.elements();
                        while (keys.hasMoreElements()) {
                                Log.info("  " + (String) keys.nextElement());
                        }
                    }

        /* -------------------------------------------------------------------- */
        /*      Report corners.                                                 */
        /* -------------------------------------------------------------------- */
        Log.info("Corner Coordinates:");
        double[][] corner= new double[4][2];
        corner[0] = GDALInfoReportCorner(hDataset, "Upper Left ", 0.0, 0.0);
        corner[1] = GDALInfoReportCorner(hDataset, "Lower Left ", 0.0, hDataset
                .getRasterYSize());
        corner[2] = GDALInfoReportCorner(hDataset, "Upper Right", hDataset
                .getRasterXSize(), 0.0);
        corner[3] = GDALInfoReportCorner(hDataset, "Lower Right", hDataset
                .getRasterXSize(), hDataset.getRasterYSize());
        GDALInfoReportCorner(hDataset, "Center     ",
                hDataset.getRasterXSize() / 2.0,
                hDataset.getRasterYSize() / 2.0);

        
        
        /* ==================================================================== */
        /*      Loop over bands.                                                */
        /* ==================================================================== */
        for (iBand = 0; iBand < hDataset.getRasterCount(); iBand++) {
            Double[] pass1 = new Double[1], pass2 = new Double[1];
            double[] adfCMinMax = new double[2];
            ColorTable hTable;

            hBand = hDataset.GetRasterBand(iBand + 1);

            /*if( bSample )
             {
             float[] afSample = new float[10000];
             int   nCount;

             nCount = hBand.GetRandomRasterSample( 10000, afSample );
             Log.info( "Got " + nCount + " samples." );
             }*/

                            int[] blockXSize = new int[1];
                            int[] blockYSize = new int[1];
                            hBand.GetBlockSize(blockXSize, blockYSize);
            Log.info("Band "
                    + (iBand+1)
                                            + " Block="
                                            + blockXSize[0] + "x" + blockYSize[0]
                    + " Type="
                    + gdal.GetDataTypeName(hBand.getDataType())
                    + ", ColorInterp="
                    + gdal.GetColorInterpretationName(hBand
                            .GetRasterColorInterpretation()));

            String hBandDesc = hBand.GetDescription();
            if (hBandDesc != null && hBandDesc.length() > 0)
                Log.info("  Description = " + hBandDesc);

            hBand.GetMinimum(pass1);
            hBand.GetMaximum(pass2);
            if(pass1[0] != null || pass2[0] != null || bComputeMinMax) {
                                Log.info( "  " );
                                if( pass1[0] != null )
                                    Log.info( "Min=" + pass1[0] + " ");
                                if( pass2[0] != null )
                                    Log.info( "Max=" + pass2[0] + " ");
                            
                                if( bComputeMinMax )
                                {
                                    hBand.ComputeRasterMinMax(adfCMinMax, 0);
                                    Log.info( "  Computed Min/Max=" + adfCMinMax[0]
                        + "," + adfCMinMax[1]);
                                }
                    
                                Log.info( "\n" );
            }

                            double dfMin[] = new double[1];
                            double dfMax[] = new double[1];
                            double dfMean[] = new double[1];
                            double dfStdDev[] = new double[1];
            if( hBand.GetStatistics( bApproxStats, bStats,
                                                     dfMin, dfMax, dfMean, dfStdDev ) == gdalconstConstants.CE_None )
            {
                Log.info( "  Minimum=" + dfMin[0] + ", Maximum=" + dfMax[0] +
                                                    ", Mean=" + dfMean[0] + ", StdDev=" + dfStdDev[0] );
            }

                            if( bReportHistograms )
                            {
                                int[][] panHistogram = new int[1][];
                                int eErr = hBand.GetDefaultHistogram(dfMin, dfMax, panHistogram, true, new TermProgressCallback());
                                if( eErr == gdalconstConstants.CE_None )
                                {
                                    int iBucket;
                                    int nBucketCount = panHistogram[0].length;
                                    Log.info( "  " + nBucketCount + " buckets from " +
                                                       dfMin[0] + " to " + dfMax[0] + ":\n  " );
                                    for( iBucket = 0; iBucket < nBucketCount; iBucket++ )
                                        Log.info( panHistogram[0][iBucket] + " ");
                                    Log.info( "\n" );
                                }
                            }

                            if ( bComputeChecksum)
                            {
                                Log.info( "  Checksum=" + hBand.Checksum());
                            }

            hBand.GetNoDataValue(pass1);
            if(pass1[0] != null)
            {
                Log.info("  NoData Value=" + pass1[0]);
            }

            if (hBand.GetOverviewCount() > 0) {
                int iOverview;

                Log.info("  Overviews: ");
                for (iOverview = 0; iOverview < hBand.GetOverviewCount(); iOverview++) {
                    Band hOverview;

                    if (iOverview != 0)
                        Log.info(", ");

                    hOverview = hBand.GetOverview(iOverview);
                    Log.info(hOverview.getXSize() + "x"
                            + hOverview.getYSize());
                }
                Log.info("\n");

                                    if ( bComputeChecksum)
                                    {
                                        Log.info( "  Overviews checksum: " );
                                        for( iOverview = 0; 
                                            iOverview < hBand.GetOverviewCount();
                                            iOverview++ )
                                        {
                                            Band    hOverview;
                        
                                            if( iOverview != 0 )
                                                Log.info( ", " );
                        
                                            hOverview = hBand.GetOverview(iOverview);
                                            Log.info(""+ hOverview.Checksum());
                                        }
                                        Log.info( "\n" );
                                    }
            }

            if( hBand.HasArbitraryOverviews() )
            {
                Log.info( "  Overviews: arbitrary" );
            }


                            int nMaskFlags = hBand.GetMaskFlags(  );
                            if( (nMaskFlags & (gdalconstConstants.GMF_NODATA|gdalconstConstants.GMF_ALL_VALID)) == 0 )
                            {
                                Band hMaskBand = hBand.GetMaskBand() ;
                    
                                Log.info( "  Mask Flags: " );
                                if( (nMaskFlags & gdalconstConstants.GMF_PER_DATASET) != 0 )
                                    Log.info( "PER_DATASET " );
                                if( (nMaskFlags & gdalconstConstants.GMF_ALPHA) != 0 )
                                    Log.info( "ALPHA " );
                                if( (nMaskFlags & gdalconstConstants.GMF_NODATA) != 0 )
                                    Log.info( "NODATA " );
                                if( (nMaskFlags & gdalconstConstants.GMF_ALL_VALID) != 0 )
                                    Log.info( "ALL_VALID " );
                                Log.info( "\n" );
                    
                                if( hMaskBand != null &&
                                    hMaskBand.GetOverviewCount() > 0 )
                                {
                                    int     iOverview;
                    
                                    Log.info( "  Overviews of mask band: " );
                                    for( iOverview = 0; 
                                        iOverview < hMaskBand.GetOverviewCount();
                                        iOverview++ )
                                    {
                                        Band    hOverview;
                    
                                        if( iOverview != 0 )
                                            Log.info( ", " );
                    
                                        hOverview = hMaskBand.GetOverview( iOverview );
                                        Log.info( 
                                                hOverview.getXSize() + "x" +
                                                hOverview.getYSize() );
                                    }
                                    Log.info( "\n" );
                                }
                            }
                            
            if( hBand.GetUnitType() != null && hBand.GetUnitType().length() > 0)
            {
                 Log.info( "  Unit Type: " + hBand.GetUnitType() );
            }

                            Vector papszCategories = hBand.GetRasterCategoryNames();
                            if (papszCategories.size() > 0)
                            {
                                Log.info( "  Categories:" );
                                Enumeration eCategories = papszCategories.elements();
                                i = 0;
                while (eCategories.hasMoreElements()) {
                                        Log.info("    " + i + ": " + (String) eCategories.nextElement());
                                        i ++;
                                }
                            }

            hBand.GetOffset(pass1);
            if(pass1[0] != null && pass1[0].doubleValue() != 0) {
                Log.info("  Offset: " + pass1[0]);
            }
            hBand.GetScale(pass1);
            if(pass1[0] != null && pass1[0].doubleValue() != 1) {
                Log.info(",   Scale:" + pass1[0]);
            }

            papszMetadata = hBand.GetMetadata_List("");
             if( bShowMetadata && papszMetadata.size() > 0 ) {
                    Enumeration keys = papszMetadata.elements();
                    Log.info("  Metadata:");
                    while (keys.hasMoreElements()) {
                        Log.info("    " + (String) keys.nextElement());
                    }
             }
            if (hBand.GetRasterColorInterpretation() == gdalconstConstants.GCI_PaletteIndex
                    && (hTable = hBand.GetRasterColorTable()) != null) {
                int count;

                Log.info("  Color Table ("
                        + gdal.GetPaletteInterpretationName(hTable
                                .GetPaletteInterpretation()) + " with "
                        + hTable.GetCount() + " entries)");

                                    if (bShowColorTable)
                                    {
                                        for (count = 0; count < hTable.GetCount(); count++) {
                                                int c = hTable.GetColorEntry(count);
                                                Log.info("color = " + count + " = " + Color.red(c)+ " " + Color.green(c)+ " " + Color.blue(c)+ " " +Color.alpha(c));

                                        }
                                    }
            }

                            RasterAttributeTable rat = hBand.GetDefaultRAT();
                            if( bShowRAT && rat != null )
                            {
                                Log.info("<GDALRasterAttributeTable ");
                                double[] pdfRow0Min = new double[1];
                                double[] pdfBinSize = new double[1];
                                if (rat.GetLinearBinning(pdfRow0Min, pdfBinSize))
                                {
                                    Log.info("Row0Min=\"" + pdfRow0Min[0] + "\" BinSize=\"" + pdfBinSize[0] + "\">");
                                }
                                Log.info("\n");
                                int colCount = rat.GetColumnCount();
                                for(int col=0;col<colCount;col++)
                                {
                                    Log.info("  <FieldDefn index=\"" + col + "\">");
                                    Log.info("    <Name>" + rat.GetNameOfCol(col) + "</Name>");
                                    Log.info("    <Type>" + rat.GetTypeOfCol(col) + "</Type>");
                                    Log.info("    <Usage>" + rat.GetUsageOfCol(col) + "</Usage>");
                                    Log.info("  </FieldDefn>");
                                }
                                int rowCount = rat.GetRowCount();
                                for(int row=0;row<rowCount;row++)
                                {
                                    Log.info("  <Row index=\"" + row + "\">");
                                    for(int col=0;col<colCount;col++)
                                    {
                                        Log.info("    <F>" + rat.GetValueAsString(row, col)+ "</F>");
                                    }
                                    Log.info("  </Row>");
                                }
                                Log.info("</GDALRasterAttributeTable>");
                            }
        }

    }
    
    /************************************************************************/
    /*                        GDALInfoReportCorner()                        */
    /************************************************************************/

    static double[] GDALInfoReportCorner(Dataset hDataset, String corner_name,
            double x, double y)

    {
        double dfGeoX, dfGeoY;
        String pszProjection;
        double[] adfGeoTransform = new double[6];
        CoordinateTransformation hTransform = null;

        Log.info(corner_name + " ");

        /* -------------------------------------------------------------------- */
        /*      Transform the point into georeferenced coordinates.             */
        /* -------------------------------------------------------------------- */
        hDataset.GetGeoTransform(adfGeoTransform);
        
        
        {
            pszProjection = hDataset.GetProjectionRef();
          //  pszProjection = "EPSG:4326";

            dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
                    + adfGeoTransform[2] * y;
            dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
                    + adfGeoTransform[5] * y;
        }

        if (adfGeoTransform[0] == 0 && adfGeoTransform[1] == 0
                && adfGeoTransform[2] == 0 && adfGeoTransform[3] == 0
                && adfGeoTransform[4] == 0 && adfGeoTransform[5] == 0) {
            Log.info("(" + x + "," + y + ")");
            return null;
        }

        /* -------------------------------------------------------------------- */
        /*      Report the georeferenced coordinates.                           */
        /* -------------------------------------------------------------------- */
        Log.info("(" + dfGeoX + "," + dfGeoY + ") ");

        /* -------------------------------------------------------------------- */
        /*      Setup transformation to lat/long.                               */
        /* -------------------------------------------------------------------- */
        if (pszProjection != null && pszProjection.length() > 0) {
            SpatialReference hProj, hLatLong = null;

            hProj = new SpatialReference(pszProjection);
            
            // if not defined, use default projection EPSG:4326
//            if (hProj == null)
//                hLatLong = new SpatialReference("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]]");

            // force EPSG:3857 as destination projection
            hLatLong = new SpatialReference("PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"Popular Visualisation CRS\",DATUM[\"Popular_Visualisation_Datum\",SPHEROID[\"Popular Visualisation Sphere\",6378137,0,AUTHORITY[\"EPSG\",\"7059\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6055\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4055\"]],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],AUTHORITY[\"EPSG\",\"3785\"],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH]]");
            // hLatLong = hProj.CloneGeogCS();
            
            if (hLatLong != null) {
                gdal.PushErrorHandler( "CPLQuietErrorHandler" );
                hTransform = new CoordinateTransformation(hProj, hLatLong);
                gdal.PopErrorHandler();
                hLatLong.delete();
                if (gdal.GetLastErrorMsg().indexOf("Unable to load PROJ.4 library") != -1)
                    hTransform = null;
            }

            if (hProj != null)
                hProj.delete();
        }

        /* -------------------------------------------------------------------- */
        /*      Transform to latlong and report.                                */
        /* -------------------------------------------------------------------- */
        double[] transPoint = new double[3];
        if (hTransform != null) {
            hTransform.TransformPoint(transPoint, dfGeoX, dfGeoY, 0);
            Log.info(" -> (" + transPoint[0] + ", "+transPoint[1] + ")");
        }

        if (hTransform != null)
            hTransform.delete();

        return new double[]{transPoint[0], transPoint[1]};
    }
    
 
    
}
