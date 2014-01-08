package com.nutiteq.layers.raster.deprecated;

import java.util.Map;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.deprecated.NetFetchTileTask;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.TileUtils;
import com.nutiteq.utils.Utils;

/**
 * WMS Map Server API to requests data based on tiles
 * 
 * @author jaak
 */
@Deprecated
public class WmsLayer extends RasterLayer {

    private String layer;
    private String format;
    private String style;
    private Map<String, String> httpHeaders;
    private int tileSize = 256;


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
     */
    public WmsLayer(Projection projection, int minZoom, int maxZoom, int id,
            String baseUrl, String style, String layer, String format) {
        super(projection, minZoom, maxZoom, id, baseUrl);
        this.style = style;
        this.layer = layer;
        this.format = format;
        setPersistentCaching(true);
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
        Log.debug("WmsLayer: fetchTile " + tile);

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
        url.append("&SRS=").append(Utils.urlEncode(getProjection().name()));
        url.append("&WIDTH="+tileSize+"&HEIGHT="+tileSize);
        url.append("&BBOX=").append(Utils.urlEncode(bbox));
        String urlString = url.toString();
        Log.info("WmsLayer: Start loading " + urlString);

        // finally you need to add a task with download URL to the raster tile download pool
        components.rasterTaskPool.execute(new NetFetchTileTask(tile,
                components, tileIdOffset, urlString, httpHeaders, memoryCaching, persistentCaching));
    }

    private String getTileBbox(MapTile tile) {
        Envelope envelope = TileUtils.TileBounds(tile.x, tile.y, tile.zoom, projection);

        String bbox = "" + envelope.getMinX() + "," + envelope.getMinY() + ","
                + envelope.getMaxX() + "," + envelope.getMaxY();
        Log.debug("WmsLayer: envelope bbox " + bbox);
        return bbox;
    }

    public int getTileSize() {
        return tileSize;
    }


    public void setTileSize(int tileSize) {
        this.tileSize = tileSize;
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
        url.append("&SRS=").append(Utils.urlEncode(getProjection().name()));
        url.append("&WIDTH="+tileSize+"&HEIGHT="+tileSize);
        url.append("&BBOX=").append(Utils.urlEncode(bbox));

        // add featureinfo-specific parameters
        url.append("&REQUEST=GetFeatureInfo");
        url.append("&QUERY_LAYERS=").append(Utils.urlEncode(layer));
        url.append("&INFO_FORMAT=").append(Utils.urlEncode("text/html"));
        url.append("&FEATURE_COUNT=10");
        url.append("&X=").append((int) (tileSize * tilePos.x));
        url.append("&Y=").append(tileSize - (int) (tileSize * tilePos.y));
        url.append("&EXCEPTIONS=").append(
                Utils.urlEncode("application/vnd.ogc.se_xml"));

        String urlString = url.toString();
        Log.info("WmsLayer: getFeatureInfo " + urlString);

        return NetUtils.downloadUrl(urlString, this.httpHeaders, true, "UTF-8");
    }

}
