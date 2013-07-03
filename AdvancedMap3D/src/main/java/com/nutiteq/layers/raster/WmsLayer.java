package com.nutiteq.layers.raster;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Map;

import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;
import org.json.JSONException;
import org.json.JSONObject;

import android.net.http.AndroidHttpClient;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.log.Log;
import com.nutiteq.projections.EPSG4326;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.NetFetchTileTask;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.TileUtils;
import com.nutiteq.utils.Utils;

/**
 * WMS Map Server API to requests data based on tiles
 * 
 * @author jaak
 */
public class WmsLayer extends RasterLayer {

    private String layer;
    private String format;
    private String style;
    private Projection dataProjection;
    private Map<String, String> httpHeaders;


    /**
     * Default constructor
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
     * Add HTTP headers. Useful for referer, basic-auth etc.
     * @param httpHeaders
     */
    public void setHttpHeaders(Map<String, String> httpHeaders) {
      this.httpHeaders = httpHeaders;
    }

    @Override
    public void fetchTile(MapTile tile) {

        Log.debug("Wms GetMap for tile " + tile);

        if (tile.zoom < minZoom || tile.zoom > maxZoom) {
            return;
        }

        String bbox = getTileBbox(tile);

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
                components, tileIdOffset, urlString, httpHeaders));
    }

    private String getTileBbox(MapTile tile) {
        
        Envelope envelope = TileUtils.TileBounds(tile.x, tile.y, tile.zoom, projection);
        
        String bbox = "" + envelope.getMinX() + "," + envelope.getMinY() + ","
                + envelope.getMaxX() + "," + envelope.getMaxY();
        Log.debug("wmsmap original envelope bbox " + bbox);
        if (!dataProjection.name().equals(projection.name())) {
            // recalculate to WMS dataProjection via WGS84
            MapPos bottomLeft = projection.toWgs84((float) envelope.getMinX(),
                    (float) envelope.getMinY());
            MapPos topRight = projection.toWgs84((float) envelope.getMaxX(),
                    (float) envelope.getMaxY());
            Log.debug("WmsMap bottomLeft " + bottomLeft + " topRight "
                    + topRight);
            if(dataProjection.name().equals(new EPSG4326().name())){
                // no reprojection needed, already WGS84
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
        return bbox;
    }


    @Override
    public void flush() {
    }


    // implements GetFeatureInfo WMS request
    // Uses hardcoded values for several parameters
    
    public String getFeatureInfo(MapTile clickedTile, MutableMapPos tilePos) {
        
        String bbox = getTileBbox(clickedTile);

        StringBuffer url = new StringBuffer(
                Utils.prepareForParameters(location));
        
        // repeat basic WMS getMap parameters
        url.append("LAYERS=").append(Utils.urlEncode(layer));
        url.append("&FORMAT=").append(Utils.urlEncode(format));
        url.append("&SERVICE=WMS&VERSION=1.1.1");
        url.append("&STYLES=").append(Utils.urlEncode(style));
        url.append("&SRS=").append(Utils.urlEncode(dataProjection.name()));
        url.append("&WIDTH=256&HEIGHT=256");
        url.append("&BBOX=").append(Utils.urlEncode(bbox));
        
        // add featureinfo-specific parameters
        url.append("&REQUEST=GetFeatureInfo");
        url.append("&QUERY_LAYERS=").append(Utils.urlEncode(layer));
        url.append("&INFO_FORMAT=").append(Utils.urlEncode("text/html"));
        url.append("&FEATURE_COUNT=10");
        url.append("&X=").append((int) (256 * tilePos.x));
        url.append("&Y=").append(256 - (int) (256 * tilePos.y));
        url.append("&EXCEPTIONS=").append(
                Utils.urlEncode("application/vnd.ogc.se_xml"));
        
        String urlString = url.toString();
        Log.info("WmsLayer: GetFeatureInfo " + urlString);
        
        return NetUtils.downloadUrl(urlString, this.httpHeaders, true, "UTF-8");
    }
    


    
}
