package com.nutiteq.datasources.raster;

import java.util.Map;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.TileUtils;
import com.nutiteq.utils.Utils;

/**
 * WMS Map Server API to requests data based on tiles
 * 
 * @author jaak
 */
public class WMSRasterDataSource extends HTTPRasterDataSource {

    private String baseUrl;

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
    public WMSRasterDataSource(Projection projection, int minZoom, int maxZoom, 
            String baseUrl, String style, String layer, String format) {
        super(projection, minZoom, maxZoom, null);
        this.baseUrl = baseUrl;
        this.style = style;
        this.layer = layer;
        this.format = format;
    }

    public int getTileSize() {
        return tileSize;
    }

    public void setTileSize(int tileSize) {
        this.tileSize = tileSize;
    }

    // implements GetFeatureInfo WMS request
    // Uses hardcoded values for several parameters
    public String getFeatureInfo(MapTile clickedTile, MutableMapPos tilePos) {
        String bbox = getTileBbox(clickedTile);

        // repeat basic WMS getMap parameters
        StringBuffer url = new StringBuffer(prepareForParameters(baseUrl));
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
        return NetUtils.downloadUrl(urlString, this.httpHeaders, true, "UTF-8");
    }

    @Override
    protected String buildTileURL(MapTile tile) {
        String bbox = getTileBbox(tile);

        StringBuffer url = new StringBuffer(prepareForParameters(baseUrl));
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
        return url.toString();
    }

    private String getTileBbox(MapTile tile) {
        Envelope envelope = TileUtils.TileBounds(tile.x, tile.y, tile.zoom, projection);

        String bbox = ""
                + envelope.getMinX() + "," + envelope.getMinY() + ","
                + envelope.getMaxX() + "," + envelope.getMaxY();
        Log.debug("WMSRasterDataSource: envelope bbox " + bbox);
        return bbox;
    }

    private static String prepareForParameters(final String url) {
        if (url.endsWith("?") || url.endsWith("&")) {
            return url;
        }

        if (url.indexOf("?") > 0) {
            return url + "&";
        } else {
            return url + "?";
        }
    }
}
