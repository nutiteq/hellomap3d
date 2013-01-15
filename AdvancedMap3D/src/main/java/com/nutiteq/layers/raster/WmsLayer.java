package com.nutiteq.layers.raster;

import java.util.Map;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.TileQuadTreeNode;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG3857;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.NetFetchTileTask;
import com.nutiteq.utils.TileUtils;
import com.nutiteq.utils.Utils;

public class WmsLayer extends RasterLayer {

    private String layer;
    private String format;
    private String style;
    private Projection dataProjection;
    private int offsetX = 0;
    private int offsetY = 0;
    private int offsetZoom = 0;
    private Map<String, String> httpHeaders;


    /**
     * WMS Map Server API to requests data based on tiles
     * 
     * @param projection
     *            Projection for your map
     * @param minZoom
     * @param maxZoom
     * @param id
     *            unique ID used in caching
     * @param baseUrl
     *            For geoserver example:
     *            http://MYSERVER/geoserver/SERVICE/wms?transparent=true&
     * @param style
     *            for Style parameter in WMS request
     * @param layer
     *            for Layers parameter
     * @param format
     *            for Format parameter
     * @param dataProjection
     *            Used to give SRS and recalculate BBOX parameters where needed.
     *            Normally use the same as for projection parameter.
     *            Warning: it may be OK to use different projections in some
     *            special cases (e.g. EPSG:3857 for map and EPSG:4236 for WMS),
     *            but on most other cases you should have same projection as for
     *            layer, otherwise you will have inaccurate map!
     */
    public WmsLayer(Projection projection, int minZoom, int maxZoom, int id,
            String baseUrl, String style, String layer, String format,
            Projection dataProjection) {
        super(projection, minZoom, maxZoom, id, baseUrl);
        this.style = style;
        this.layer = layer;
        this.format = format;
        this.dataProjection = dataProjection;

    }
    
    /**
     * Change highest level ("World") tile to something else than global 0,0,0
     * Useful for regional maps, with #com.nutiteq.projections.SingleTileEPSG3857 
     * @param offsetX
     * @param offsetY
     * @param offsetZoom
     */
    public void setWorldTile(int offsetX, int offsetY, int offsetZoom){
      this.offsetX = offsetX;
      this.offsetY = offsetY;
      this.offsetZoom = offsetZoom;
    }
    
    /**
     * Add HTTP headers. Useful for referer, basic-auth etc.
     * @param httpHeaders
     */
    public void setHttpHeaders(Map<String, String> httpHeaders) {
      this.httpHeaders = httpHeaders;
    }

    @Override
    public void fetchTile(TileQuadTreeNode tile) {

        Log.debug("WmsMap for tile " + tile);

        // adjust to WorldTile (if default 0,0,0 then no change)
        int zoomPow2 = 2 << (tile.zoom-1); // same as Math.pow(2,tile.zoom)
        int tileX = offsetX * zoomPow2 + tile.x;
        int tileY = offsetY * zoomPow2 + tile.y;
        int tileZoom = tile.zoom + offsetZoom;
        
        if (tileZoom < minZoom || tileZoom > maxZoom) {
            return;
        }

        Envelope envelope = TileUtils.TileBounds(tileX, tileY, tileZoom,projection);
        
        EPSG3857 epsg3857 = new EPSG3857();
        String bbox = "" + envelope.getMinX() + "," + envelope.getMinY() + ","
                + envelope.getMaxX() + "," + envelope.getMaxY();
        Log.debug("wmsmap original envelope bbox " + bbox);
        if (!dataProjection.name().equals(epsg3857.name())) {
            // recalculate to WMS dataProjection via WGS84
            MapPos bottomLeft = epsg3857.toWgs84((float) envelope.getMinX(),
                    (float) envelope.getMinY());
            MapPos topRight = epsg3857.toWgs84((float) envelope.getMaxX(),
                    (float) envelope.getMaxY());
            Log.debug("WmsMap bottomLeft " + bottomLeft + " topRight "
                    + topRight);
            if(dataProjection.name().equals(new EPSG4326().name())){
                // no reprojection needed, already Wgs84
               bbox = "" + bottomLeft.x + "," + bottomLeft.y
                        + "," + topRight.x +","+ topRight.y;
            }else{
                bbox = "" + dataProjection.fromWgs84(bottomLeft.x, bottomLeft.y).x
                        + ","
                        + dataProjection.fromWgs84(bottomLeft.x, bottomLeft.y).y
                        + "," + dataProjection.fromWgs84(topRight.x, topRight.y).x
                        + "," + dataProjection.fromWgs84(topRight.x, topRight.y).y;
                
            }
            Log.debug("wmsmap recalculated bbox " + bbox);
        } else {
            Log.debug("wmsmap keeps original bbox " + bbox);
        }

        StringBuffer url = new StringBuffer(
                Utils.prepareForParameters(location));
        url.append("LAYERS=").append(Utils.urlEncode(layer));
        url.append("&FORMAT=").append(Utils.urlEncode(format));
        url.append("&SERVICE=WMS&VERSION=1.1.0");
        url.append("&REQUEST=GetMap");
        url.append("&STYLES=").append(Utils.urlEncode(style));
        url.append("&EXCEPTIONS=").append(
                Utils.urlEncode("application/vnd.ogc.se_inimage"));
        url.append("&SRS=").append(Utils.urlEncode(dataProjection.name()));
        url.append("&WIDTH=256&HEIGHT=256");
        url.append("&BBOX=").append(Utils.urlEncode(bbox));
        String urlString = url.toString();
        Log.info("WmsLayer: Start loading " + urlString);
        
        // finally you need to add a task with download URL to the raster tile download pool
        components.rasterTaskPool.execute(new NetFetchTileTask(tile,
                components, tileIdOffset, urlString));
    }

    @Override
    public void flush() {
    }
}
