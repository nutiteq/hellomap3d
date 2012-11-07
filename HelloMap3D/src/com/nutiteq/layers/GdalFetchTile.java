package com.nutiteq.layers;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import org.gdal.gdal.Band;
import org.gdal.gdal.ColorTable;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconst;
import org.gdal.gdalconst.gdalconstConstants;
import org.gdal.osr.CoordinateTransformation;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import cern.colt.Arrays;

import com.nutiteq.components.Color;
import com.nutiteq.components.Components;
import com.nutiteq.components.TileQuadTreeNode;
import com.nutiteq.db.MbTilesDatabaseHelper;
import com.nutiteq.log.Log;
import com.nutiteq.tasks.FetchTileTask;
import com.vividsolutions.jts.geom.Envelope;

public class GdalFetchTile {

    private static final int TILE_SIZE = 256;

    private int z;
    private int x;
    private int y;
    private Dataset hDataset;

    private Envelope dataBounds;

    public GdalFetchTile(TileQuadTreeNode tile, Components components,
            long tileIdOffset, Dataset hDataset, Envelope dataBounds) {
        this.z = tile.zoom;
        this.x = tile.x;
        this.y = tile.y;
        this.hDataset = hDataset;
        this.dataBounds = dataBounds;

    }

    public byte[] getData() {
        Log.debug("fetchTile run start " + x + " " + y + " zoom:" + z);
        long time = System.nanoTime();
        
        // 1. convert tile coordinates to geographical ones
        Envelope requestedBounds = TileUtils.TileBounds(x, y, z);
        Log.debug("requested bounds:" + requestedBounds+ "data bounds:"+dataBounds);

        // make quick check if bounds overlap

        if (!dataBounds.intersects(requestedBounds)) {
            Log.debug("tile and data Envelopes do not overlap");
            return null;
        }

        // 2. calculate pixel ranges of source image
        double[] adfGeoTransform = new double[6];
        hDataset.GetGeoTransform(adfGeoTransform);
//        Log.debug("geoTransform:" + Arrays.toString(adfGeoTransform));
        int[] pixelsSrcMin = convertGeoLocationToPixelLocation(
                requestedBounds.getMinX(), requestedBounds.getMaxY(),
                adfGeoTransform);
        Log.debug("minxy:" + pixelsSrcMin[0] + " " + pixelsSrcMin[1]);
        int[] pixelsSrcMax = convertGeoLocationToPixelLocation(
                requestedBounds.getMaxX(), requestedBounds.getMinY(),
                adfGeoTransform);
        Log.debug("maxxy:" + pixelsSrcMax[0] + " " + pixelsSrcMax[1]);
        
        // tile dimensions
        int xSizeBuf = TILE_SIZE;
        int ySizeBuf = TILE_SIZE;
        
        int xOffsetBuf = 0;
        int yOffsetBuf = 0;
        int xMaxBuf = TILE_SIZE;
        int yMaxBuf = TILE_SIZE;
        float xScale = (pixelsSrcMax[0]-pixelsSrcMin[0]) / (float)xSizeBuf; // unit: srcPix/screenPix
        float yScale = (pixelsSrcMax[1]-pixelsSrcMin[1]) / (float)ySizeBuf;

        // 3. handle border tiles which have only partial data
        if(pixelsSrcMax[0] > hDataset.getRasterXSize()){
            // x over real size, reduce both buffer and loaded data proportionally
            xMaxBuf = (int) ((hDataset.getRasterXSize()- pixelsSrcMin[0]) / xScale);
            xSizeBuf = xMaxBuf;
            pixelsSrcMax[0] = hDataset.getRasterXSize();
            Log.debug ("adjusted maxxy xSizeBuf=xMaxBuf to "+xSizeBuf+" pixelsMax "+pixelsSrcMax[0]);
        }
        
        if(pixelsSrcMax[1] > hDataset.getRasterYSize()){
            // y over real size, reduce both buffer and loaded data proportionally
            yMaxBuf = (int) ((hDataset.getRasterYSize()- pixelsSrcMin[1]) / yScale);
            ySizeBuf = yMaxBuf;
            pixelsSrcMax[1] = hDataset.getRasterYSize();
            Log.debug ("adjusted maxxy ySizeBuf=yMaxBuf to "+ySizeBuf+" pixelsMax "+pixelsSrcMax[1]);
        }
        
        if(pixelsSrcMin[0] < 0){
            // x below 0, reduce both buffer and loaded data proportionally
            xOffsetBuf = (int) - (pixelsSrcMin[0] / xScale);
            xSizeBuf -= xOffsetBuf;
            pixelsSrcMin[0] = 0;
            Log.debug ("adjusted neg maxxy xSizeBuf to "+xSizeBuf+" pixelsSrcMax "+pixelsSrcMax[0]+" offset "+xOffsetBuf);
        }

        if(pixelsSrcMin[1] < 0){
            // y below 0, reduce both buffer and loaded data proportionally
            yOffsetBuf = (int) - (pixelsSrcMin[1] / yScale);
            ySizeBuf -= yOffsetBuf;
            pixelsSrcMin[1] = 0;
            Log.debug ("adjusted neg maxxy ySizeBuf to "+ySizeBuf+" pixelsMax "+pixelsSrcMax[1]+" offset "+yOffsetBuf);
        }
        
        if(xSizeBuf<0 || ySizeBuf<0){
            Log.debug("negative tile size, probably out of area");
            return null;
        }

        // 4. Read data
        int xSizeData = (pixelsSrcMax[0] - pixelsSrcMin[0]);
        int ySizeData = (pixelsSrcMax[1] - pixelsSrcMin[1]);
//        Log.debug("xy size:" + xSizeData + "x" + ySizeData);

        int[] tileData = new int[TILE_SIZE * TILE_SIZE];

        for (int iBand = 0; iBand < hDataset.getRasterCount(); iBand++) {
        
        Band band = hDataset.GetRasterBand(iBand+1);

        // TODO jaak: it could be 8 times (bits2bytes) too large in some(?) cases
        byte[] byteBuffer = new byte[gdal
                .GetDataTypeSize(band.getDataType()) * xSizeBuf * ySizeBuf];
        
        Log.debug(String.format("reading pixels %d %d dataSize %d %d bufSize %d %d pixelBits %d dataType %d", pixelsSrcMin[0],
                pixelsSrcMin[1], xSizeData, ySizeData, xSizeBuf, ySizeBuf,gdal
              .GetDataTypeSize(band.getDataType()),band.getDataType()));

        //  read data to byte array
        int res = band.ReadRaster(pixelsSrcMin[0], pixelsSrcMin[1], xSizeData,
                ySizeData, xSizeBuf, ySizeBuf, band.getDataType(), byteBuffer);
        
        if (res == gdalconstConstants.CE_Failure) {
            Log.error("error reading raster");
            return null;
        }
        
        Log.debug("gdalfetchtile time for reading: " + (System.nanoTime() - time)
                / 1000000 + " ms");
        
        // copy data to tile buffer tileData, and apply color table or combine bands
        int colorType = band.GetRasterColorInterpretation();
        ColorTable ct = band.GetRasterColorTable();
            for (int y = 0; y < TILE_SIZE; y++) {
                for (int x = 0; x < TILE_SIZE; x++) {
                    if(x >= xOffsetBuf && y >= yOffsetBuf && x<=xMaxBuf && y<=yMaxBuf){
                        byte val = byteBuffer[((y-yOffsetBuf) * xSizeBuf) + (x-xOffsetBuf)];
 
                        // decode color
                        int decoded = 0; 

                        // 1) if indexed color
                        if(colorType == gdalconst.GCI_PaletteIndex){ 
                            if(ct != null && val<ct.GetCount()){
                                decoded = ct.GetColorEntry(val);
                            }else{
                                // no colortable match found value, should not happen
                                decoded = android.graphics.Color.CYAN & 0x88ffffff;
                              Log.debug("no colortable found for value "+val);
                            }

                         // 2) ARGB bands to int
                        }else if (colorType == gdalconst.GCI_AlphaBand){
                            decoded = (int) val  & 0xff << 24;
                        }else if(colorType == gdalconst.GCI_RedBand){
                            decoded = ((int) val & 0xff) << 16;
                        }else if (colorType == gdalconst.GCI_GreenBand){
                            decoded = ((int) val & 0xff) << 8;
                        }else if (colorType == gdalconst.GCI_BlueBand){
                            decoded = (int) val  & 0xff;
                        }
                        
                        // TODO Handle other color schemas: RGB in one band etc. Test data needed

                        tileData[y * TILE_SIZE + x] |=  decoded | 0xff000000;
                        
                    }else{
                        // outside of tile bounds. Normally keep transparent, give color for debugging 
                        //tileData[y * TILE_SIZE + x] = android.graphics.Color.GREEN & 0x88ffffff;
                    }
                }
            }

        } // loop for over bands
//        Log.debug("gdalfetchtile time  = " + (System.nanoTime() - time)
//                / 1000000 + " ms");
        
        // finally compress bitmap as PNG
        Bitmap bitmap = Bitmap.createBitmap(tileData, 256, 256,
                Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 90, bos);
        try {
            bos.close();
        } catch (IOException e) {
            e.printStackTrace();
        }

        Log.debug("finising gdalfetchtile total time = " + (System.nanoTime() - time)
                / 1000000 + " ms");
        return bos.toByteArray();
    }

    private int[] convertGeoLocationToPixelLocation(double xGeo, double yGeo,
            double[] g) {

        int xPixel = 0, yPixel = 0;

        if (true /*g[2] == 0*/) {
            xPixel = (int) ((xGeo - g[0]) / g[1]);
            yPixel = (int) ((yGeo - g[3] - xPixel * g[4]) / g[5]);
        } else {
            xPixel = (int) ((yGeo * g[2] - xGeo * g[5] + g[0] * g[5] - g[2]
                    * g[3]) / (g[2] * g[4] - g[1] * g[5]));
            yPixel = (int) ((xGeo - g[0] - xPixel * g[1]) / g[2]);
        }
        return new int[] { xPixel, yPixel };
    }
}
