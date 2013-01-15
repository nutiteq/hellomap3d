package com.nutiteq.layers.raster;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Vector;

import org.gdal.gdal.Band;
import org.gdal.gdal.ColorTable;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.GCP;
import org.gdal.gdal.ProgressCallback;
import org.gdal.gdal.RasterAttributeTable;
import org.gdal.gdal.TermProgressCallback;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconst;
import org.gdal.gdalconst.gdalconstConstants;
import org.gdal.osr.CoordinateTransformation;
import org.gdal.osr.SpatialReference;
import org.gdal.osr.osr;

import android.graphics.Color;

import com.nutiteq.MapView;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.TileQuadTreeNode;
import com.nutiteq.db.MbTilesDatabaseHelper;
import com.nutiteq.layers.raster.DatasetInfo;
import com.nutiteq.layers.raster.GdalFetchTileTask;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.utils.Serializer;
import com.nutiteq.utils.TileUtils;

public class GdalMapLayer extends RasterLayer {
    
    // force Java to load PROJ.4 library. Needed as we don't call it directly, but 
    // GDAL datasource reading may need it.
    
    private static final double VRT_MAXERROR = 0.125;
    private static final int VRT_RESAMPLER = gdalconst.GRA_NearestNeighbour;
    
    private static final String EPSG_3785_WKT = "PROJCS[\"Google Maps Global Mercator\",    GEOGCS[\"WGS 84\",        DATUM[\"WGS_1984\",            SPHEROID[\"WGS 84\",6378137,298.257223563,                AUTHORITY[\"EPSG\",\"7030\"]],            AUTHORITY[\"EPSG\",\"6326\"]],        PRIMEM[\"Greenwich\",0,            AUTHORITY[\"EPSG\",\"8901\"]],        UNIT[\"degree\",0.01745329251994328,            AUTHORITY[\"EPSG\",\"9122\"]],        AUTHORITY[\"EPSG\",\"4326\"]],    PROJECTION[\"Mercator_2SP\"],    PARAMETER[\"standard_parallel_1\",0],    PARAMETER[\"latitude_of_origin\",0],    PARAMETER[\"central_meridian\",0],    PARAMETER[\"false_easting\",0],    PARAMETER[\"false_northing\",0],    UNIT[\"Meter\",1],    EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],    AUTHORITY[\"EPSG\",\"3785\"]]";
    private static final double WORLD_WIDTH = 20037508.3428; // width of EPSG:3785
    SpatialReference layerProjection = new SpatialReference(EPSG_3785_WKT);

    static {
        try {
          System.loadLibrary("proj");
        } catch (Throwable t) {
            System.err.println("Unable to load proj: " + t);
        }
        }
    
    private MbTilesDatabaseHelper db;
    private Envelope boundsEnvelope;
    private MapView mapView;
    Map<Envelope, DatasetInfo> dataSets = new HashMap<Envelope, DatasetInfo>();
    Map<Envelope, Dataset> openDataSets = new HashMap<Envelope, Dataset>();
    private int counter=1;
    private boolean showAlways;

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
     * @throws IOException 
     */
    @SuppressWarnings("unchecked")
    public GdalMapLayer(Projection projection, int minZoom, int maxZoom,
            int id, String gdalSource, MapView mapView, boolean reproject) throws IOException {
        super(projection, minZoom, maxZoom, id, gdalSource);
        
        this.mapView = mapView;
        gdal.AllRegister();
        
        // debug print list all enabled drivers
        listDrivers();
        
        File rootFile = new File(gdalSource);
        
        if(!rootFile.exists()){
            Log.error("GDAL file or folder does not exist: "+rootFile.getAbsolutePath());
            return;
        }
        
        if(!rootFile.isDirectory()){
            // open single file
            addFileDataSets(rootFile.getPath(), reproject, rootFile.getName());
        }else{
            // open all files in a folder
            // use gdal.index as metadata cache, create if needed
            File indexFile = new File(gdalSource+"/gdal.index");
            if(indexFile.exists()){
                byte [] indexData = new byte[(int)indexFile.length()];
                FileInputStream fis = new FileInputStream(indexFile);
                fis.read(indexData);
                dataSets = (Map<Envelope, DatasetInfo>) Serializer.deserializeObject(indexData);
                Log.debug("loaded gdal.index with "+dataSets.size()+" entries");
                fis.close();
            }else{
                File dir = new File(gdalSource);
                readFilesRecursive(gdalSource, reproject, dir);
                FileOutputStream fos = new FileOutputStream(indexFile);
                fos.write(Serializer.serializeObject(dataSets));
                fos.close();
            }
        }
    }

    
    private void readFilesRecursive(String gdalSource, boolean reproject,
            File dir) throws IOException {
        File[] files = dir.listFiles();
        
        if(files == null){
            Log.error("No files or folder "+dir.getAbsolutePath());
            return;
        }
        
        if (files.length == 0)
            Log.error("No files in given directory "+gdalSource);
        else {
            for (int i=0; i<files.length; i++){
                String name = files[i].getName();
                if(files[i].isDirectory()){
                    // recurse into directory
                    readFilesRecursive(gdalSource+"/"+name,reproject,files[i]);
                }else{
                    addFileDataSets(gdalSource, reproject, name);
                }
            }
        }
    }


    /**
     * @param gdalSource
     * @param reproject
     * @param fileName
     * @throws IOException
     */
    private void addFileDataSets(String gdalSource, boolean reproject,
            String fileName) throws IOException {

        String fullPath;
        if(gdalSource.endsWith(fileName)){
            fullPath = gdalSource; 
        }else{
            fullPath = gdalSource+File.separator+fileName;
        }
        
//        if(fileName.toUpperCase().endsWith("KAP")){
            Log.debug("Adding "+(this.counter)+". "+fileName);
            DatasetInfo dataSet = readGdalFileData(fullPath, reproject);
            if(dataSet != null){
                dataSets.put(dataSet.envelope,dataSet);
                Log.debug("Opened "+(this.counter)+". GDAL file: "+fileName+" bounds: "+dataSet.envelope.minX+" "+dataSet.envelope.minY+" "+dataSet.envelope.maxX+" "+dataSet.envelope.maxY);
            }
//        }else{
//            Log.debug("Skipping file, did not like extension of "+fileName);
//        }
    }

    // reads chart name from BSB/NA field
    private String readKapDescription(String gdalSource) throws IOException {
      //Get the text file
        File file = new File(gdalSource);
        if(file == null || !file.exists() || file.isDirectory())
            return null;
        
        String name = null;
        String scale = null;
            BufferedReader br = new BufferedReader(new FileReader(file));
            String line;

            while (((line = br.readLine()) != null) && !(name != null && scale != null)) {
                if(line.startsWith("BSB/NA")){
                    name = line.substring(7, Math.max(line.indexOf(","),line.length()));
                }else if(line.startsWith("KNP/SC")){
                    scale = line.substring(7, Math.max(line.indexOf(","),line.length()));
                }
            }
        br.close();
        return name+"; scale 1:"+scale;
    }
    
    // reads GDAL dataset metadata
    private DatasetInfo readGdalFileData(String gdalSource, boolean reproject) throws IOException {
        
        String datasetName;
        if(gdalSource.toUpperCase().endsWith("KAP")){
            datasetName = readKapDescription(gdalSource);
        }else{
            datasetName = gdalSource;
        }
                
        Dataset originalData = gdal.Open(gdalSource, gdalconstConstants.GA_ReadOnly);
        if (originalData == null)
            return null;
        Dataset openData = null;
        
        // get original bounds in Wgs84
        SpatialReference hLatLong = new SpatialReference(osr.SRS_WKT_WGS84);
        double[][] originalBounds = boundsWgs84(originalData, hLatLong);
        
        if(reproject){
            // on the fly reprojection - slower reading, but fast open and less memory
            openData = gdal.AutoCreateWarpedVRT(originalData,null, layerProjection.ExportToWkt(),VRT_RESAMPLER, VRT_MAXERROR);

   //         fullGdalInfo(openData);
            originalData.delete();
            
        }else{
            openData = originalData;
        }
        
        Envelope bbox = bounds(openData, layerProjection);
        if(bbox == null){
            return null;
        }
        DatasetInfo datasetInfo = new DatasetInfo(datasetName, (Vector<String>) openData.GetFileList().clone(), bestZoom(bbox.getWidth(),openData.getRasterXSize()), counter++, bbox, originalBounds);
        
        openData.delete();
        
        return datasetInfo;
    }
    
    // Opens GDAL file, gets dataset pointer
    private Dataset openGdalFile(String gdalSource, boolean reproject) {
        Dataset originalData = gdal.Open(gdalSource, gdalconstConstants.GA_ReadOnly);
        if (originalData == null)
            return null;
        Dataset openData = null;
        
        // get original bounds
        Envelope originalBounds = bounds(originalData, null);
        Log.debug("original Bounds "+originalBounds);
        fullGdalInfo(originalData);

//        SpatialReference fromProj = new SpatialReference(originalData.GetProjectionRef());
        
        if(reproject){
            // on the fly reprojection - slower reading, fast open and less memory
     //       openData = gdal.AutoCreateWarpedVRT(originalData,fromProj.ExportToWkt(), layerProjection.ExportToWkt(),VRT_RESAMPLER, VRT_MAXERROR);
            openData = gdal.AutoCreateWarpedVRT(originalData,null, layerProjection.ExportToWkt(),VRT_RESAMPLER, VRT_MAXERROR);

            // reproject to memory - faster reading, more memory and time needed to open
//            hDataset = reprojectDataset(originalData, 10.0, fromProj, layerProjection);
   //         fullGdalInfo(openData);
            originalData.delete();
        }else{
            openData = originalData;
        }
        
        if (openData == null){
            return null;
        }
        
        this.boundsEnvelope = bounds(openData,layerProjection); 

        /* -------------------------------------------------------------------- */
        /*      Report general info for debugging.                              */
        /* -------------------------------------------------------------------- */
        if (openData == null) {
            Log.error("GDALOpen failed - " + gdal.GetLastErrorNo());
            Log.error(gdal.GetLastErrorMsg());

            // gdal.DumpOpenDatasets( stderr );

            // gdal.DestroyDriverManager();

            // gdal.DumpSharedList( null );

            return null;
        }
        return openData;
    }

    // pre-reproject dataset 
    // ported from http://jgomezdans.github.com/gdal_notes/reprojection.html
    // FIXME: it does not work yet, produces "black data" for BSB
    private Dataset reprojectDataset(Dataset data, double pixelSpacing, SpatialReference fromProj,
            SpatialReference toProjection) {

        double[] adfGeoTransform = new double[6];
        data.GetGeoTransform(adfGeoTransform);
        
        Envelope newBounds = bounds(data,toProjection);
        Driver memoryDriver = gdal.GetDriverByName("MEM");
        int w = (int)((newBounds.getMaxX() - newBounds.getMinX())/pixelSpacing);
        int h =  (int)((newBounds.getMaxY() - newBounds.getMinY())/pixelSpacing);
                
        Dataset dest = memoryDriver.Create("", w, h, data.getRasterCount(), gdalconst.GDT_Byte);
//        double[] newGeotransform = {newBounds.getMinX(), pixelSpacing, adfGeoTransform[2], newBounds.getMaxY() , adfGeoTransform[4], -pixelSpacing };
        double[] newGeotransform = {-9662887.997233687,9.269968462003103,0,3574568.743162234,0,-9.269968462003103};
        dest.SetGeoTransform( newGeotransform );
        dest.SetProjection ( toProjection.ExportToWkt() );
        Log.debug("start reprojection");
        long time = System.currentTimeMillis();
        int res = gdal.ReprojectImage( data, dest, 
                toProjection.ExportToWkt(), fromProj.ExportToWkt(),
                gdalconst.GRA_NearestNeighbour, 6.71089e+07, 0.125 ,new ProgressCallback(){
            @Override
            public int run(double dfComplete, String message)
            {
                Log.debug("Progress: "+dfComplete+" msg:"+message);
                return 0;
            }
        });
        long timeTook =  System.currentTimeMillis()-time;
        Log.debug("projection res = " + res + " time ms: "+timeTook);
        if(res == gdalconst.CE_Failure){
            Log.error("error in reprojecting: "+gdal.GetLastErrorMsg());
        }
        dest.GetRasterBand(1).SetColorInterpretation(data.GetRasterBand(1).GetColorInterpretation());
        dest.GetRasterBand(1).SetColorTable(data.GetRasterBand(1).GetColorTable());
        return dest;
    }

    // debug print compiled in drivers
    private void listDrivers() {
        for (int i=0;i<gdal.GetDriverCount();i++){
            Driver driver = gdal.GetDriver(i);
            Log.info("driver " + driver.getShortName()+" ("+driver.getLongName()+")");
        }
    }


    private Envelope bounds(Dataset data,SpatialReference layerProjection) {
        double[][] corner= new double[4][2];
        if(data == null){
            Log.error("data null");
            return null;
        }
        corner[0] = corner(data, layerProjection, 0.0, 0.0);
        corner[1] = corner(data, layerProjection, 0.0, data
                .getRasterYSize());
        corner[2] = corner(data,layerProjection, data
                .getRasterXSize(), 0.0);
        corner[3] = corner(data, layerProjection, data
                .getRasterXSize(), data.getRasterYSize());
        
        return new Envelope(corner[1][0],corner[2][0],corner[1][1],corner[2][1]);
    }

    private double[][] boundsWgs84(Dataset data,SpatialReference layerProjection) {
        double[][] corners= new double[4][2];
        
        corners[0] = corner(data, layerProjection, 0.0, 0.0);
        corners[1] = corner(data, layerProjection, 0.0, data
                .getRasterYSize());
        corners[2] = corner(data,layerProjection, data
                .getRasterXSize(), 0.0);
        corners[3] = corner(data, layerProjection, data
                .getRasterXSize(), data.getRasterYSize());
        
        return corners; 
    }
    
    @Override
    public void fetchTile(TileQuadTreeNode tile) {
        Log.debug("GdalMapLayer: Start loading " + " zoom=" + tile.zoom + " x="
                + tile.x + " y=" + tile.y);
        long timeStart = System.currentTimeMillis();
       
        
        Envelope requestedTileBounds = TileUtils.TileBounds(tile.x, tile.y, tile.zoom, projection);
        
        boolean found = false;
        
        for(Entry<Envelope, DatasetInfo> entry : dataSets.entrySet()){
            Envelope dataBounds = entry.getKey();
            
            if ((this.showAlways && dataBounds.intersects(requestedTileBounds)) 
                    || (!this.showAlways && dataBounds.contains(requestedTileBounds) && isSuitableZoom(entry.getValue().bestZoom, tile.zoom)) ) {
                long timeEnd = System.currentTimeMillis();
                Log.debug("found intersection with x="+tile.x+" y="+tile.y+" zoom="+tile.zoom+" took ms:"+(timeEnd-timeStart));
                
                found  = true;
                Dataset dataSet;
                dataSet = openDataSets.get(dataBounds);

                // lazy loading (opening) of dataset
                if(dataSet == null){
                    Vector<String> fileName = dataSets.get(dataBounds).dataFile;
                    dataSet = openGdalFile((String)fileName.firstElement(),true);
                    openDataSets.put(dataBounds,dataSet);
                }
                
                //create task
                GdalFetchTileTask tileTask = new GdalFetchTileTask(tile,
                      components, requestedTileBounds, getTileIdOffset(), dataSet, boundsEnvelope, mapView);
                components.rasterTaskPool.execute(tileTask);
                break;
            }
        }
        if(!found)
            Log.debug("GdalMapLayer: tile not found in any dataset");
    }

    // calculate "best" (native) zoom for given raster
    private double bestZoom(double boundWidth, double pixWidth){
        return Math.log(((pixWidth * WORLD_WIDTH) / (boundWidth * 256.0))) / (Math.log(2));
    }
    
    // is zoom in given range
    private boolean isSuitableZoom(double bestZoom, int zoom) {
       return (zoom>=(bestZoom - 3.0) && zoom<=(bestZoom + 1.0));
    }

    @Override
    public void flush() {
        Log.debug("GdalMapLayer flush");
    }

    public void close() {
        db.close();
    }
    
    /**
     * Calculate corner coordinates of dataset, in layerProjection
     * @param data
     * @param dstProj
     * @param x coordinates of bounds
     * @param y coordinates of bounds 
     * @return
     */
    static double[] corner(Dataset data, SpatialReference dstProj, double x, double y)

    {
        double dfGeoX, dfGeoY;
        String dataProjection;
        double[] adfGeoTransform = new double[6];
        CoordinateTransformation hTransform = null;

        /* -------------------------------------------------------------------- */
        /*      Transform the point into georeferenced coordinates.             */
        /* -------------------------------------------------------------------- */
        data.GetGeoTransform(adfGeoTransform);
        
        {
            dataProjection = data.GetProjectionRef();
            if(dataProjection.equals("")){
                dataProjection = data.GetGCPProjection();
            }
//Log.debug("dataProjection "+dataProjection);
            dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
                    + adfGeoTransform[2] * y;
            dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
                    + adfGeoTransform[5] * y;
        }
        
        SpatialReference dataProj = new SpatialReference(dataProjection);
        
        // is reprojection needed?
       // Log.debug("dataProj "+dataProj.GetAuthorityCode(null)+ " layerProj "+layerProj.GetAuthorityCode(null));
        if(dstProj == null || (dataProj.GetAuthorityCode(null) != null && dataProj.GetAuthorityCode(null).equals(dstProj.GetAuthorityCode(null)))){
            return new double[]{dfGeoX, dfGeoY};
        }
        
        if (adfGeoTransform[0] == 0 && adfGeoTransform[1] == 0
                && adfGeoTransform[2] == 0 && adfGeoTransform[3] == 0
                && adfGeoTransform[4] == 0 && adfGeoTransform[5] == 0) {
            return null;
        }

        if (dataProjection != null && dataProjection.length() > 0) {

            if (dstProj != null) {
                gdal.PushErrorHandler( "CPLQuietErrorHandler" );
                hTransform = new CoordinateTransformation(dataProj, dstProj);
                gdal.PopErrorHandler();
//                layerProj.delete();
                if (gdal.GetLastErrorMsg().indexOf("Unable to load PROJ.4 library") != -1){
                    Log.error(gdal.GetLastErrorMsg());
                    hTransform = null;
                }
            }

//            if (dataProj != null)
//                dataProj.delete();
        }

        double[] transPoint = new double[3];
        if (hTransform != null) {
            hTransform.TransformPoint(transPoint, dfGeoX, dfGeoY, 0);
        }

        if (hTransform != null)
            hTransform.delete();

        return new double[]{transPoint[0], transPoint[1]};
        
    }
    
    /**
     * Reports GDAL info (like gdalinfo utility)
     * Following code is from gdalinfo.java sample
     * @param data 
     */
    private void fullGdalInfo(Dataset data){
        
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
        hDriver = data.GetDriver();
        Log.info("Driver: " + hDriver.getShortName() + "/"
                + hDriver.getLongName());

                    papszFileList = data.GetFileList( );
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

        Log.info("Size is " + data.getRasterXSize() + ", "
                + data.getRasterYSize());

        /* -------------------------------------------------------------------- */
        /*      Report projection.                                              */
        /* -------------------------------------------------------------------- */
        if (data.GetProjectionRef() != null) {
            SpatialReference hSRS;
            String pszProjection;

            pszProjection = data.GetProjectionRef();

            hSRS = new SpatialReference(pszProjection);
            if (hSRS != null && pszProjection.length() != 0) {
                String[] pszPrettyWkt = new String[1];

                hSRS.ExportToPrettyWkt(pszPrettyWkt, 0);
                Log.info("Coordinate System is:");
                Log.info(pszPrettyWkt[0]);
                //gdal.CPLFree( pszPrettyWkt );
            } else
                Log.info("Coordinate System is `"
                        + data.GetProjectionRef() + "'");

            hSRS.delete();
        }

        /* -------------------------------------------------------------------- */
        /*      Report Geotransform.                                            */
        /* -------------------------------------------------------------------- */
        data.GetGeoTransform(adfGeoTransform);
    //    Log.info("geotransform "+ Arrays.toString(adfGeoTransform));
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
        if (bShowGCPs && data.GetGCPCount() > 0) {
            Log.info("GCP Projection = "
                    + data.GetGCPProjection());

            int count = 0;
            Vector GCPs = new Vector();
            data.GetGCPs(GCPs);

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
        papszMetadata = data.GetMetadata_List("");
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
                        papszMetadata = data.GetMetadata_List(pszDomain);
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
                    papszMetadata = data.GetMetadata_List("IMAGE_STRUCTURE" );
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
        papszMetadata = data.GetMetadata_List("SUBDATASETS");
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
                    papszMetadata = data.GetMetadata_List("GEOLOCATION" );
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
                    papszMetadata = data.GetMetadata_List("RPC" );
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
        corner[0] = GDALInfoReportCorner(data, "Upper Left ", 0.0, 0.0);
        corner[1] = GDALInfoReportCorner(data, "Lower Left ", 0.0, data
                .getRasterYSize());
        corner[2] = GDALInfoReportCorner(data, "Upper Right", data
                .getRasterXSize(), 0.0);
        corner[3] = GDALInfoReportCorner(data, "Lower Right", data
                .getRasterXSize(), data.getRasterYSize());
        GDALInfoReportCorner(data, "Center     ",
                data.getRasterXSize() / 2.0,
                data.getRasterYSize() / 2.0);

        
        
        /* ==================================================================== */
        /*      Loop over bands.                                                */
        /* ==================================================================== */
        for (iBand = 0; iBand < data.getRasterCount(); iBand++) {
            Double[] pass1 = new Double[1], pass2 = new Double[1];
            double[] adfCMinMax = new double[2];
            ColorTable hTable;

            hBand = data.GetRasterBand(iBand + 1);

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

    static double[] GDALInfoReportCorner(Dataset data, String corner_name,
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
        data.GetGeoTransform(adfGeoTransform);
        
        
        {
            pszProjection = data.GetProjectionRef();
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
            
            // use default projection EPSG:4326
            hLatLong = new SpatialReference("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.01745329251994328,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]]");

            // force EPSG:3857 as destination projection
//            hLatLong = new SpatialReference(EPSG_3785_WKT);
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

    public  Map<Envelope, DatasetInfo> getDatasets() {
        
        return dataSets;
    }

    /**
     * If set, ignore best zoom and have partial tiles
     * @param showAlways
     */
    public void setShowAlways(boolean showAlways) {
        this.showAlways = showAlways;
    }
    
 
    
}
