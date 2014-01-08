package com.nutiteq.datasources.raster;

import java.util.Map;

import android.net.Uri;

import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapTile;
import com.nutiteq.components.MutableMapPos;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.utils.NetUtils;
import com.nutiteq.utils.TileUtils;

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

        Uri.Builder uri = createBaseUri("GetFeatureInfo");
        uri.appendQueryParameter("BBOX", bbox);
        uri.appendQueryParameter("QUERY_LAYERS", layer);
        uri.appendQueryParameter("INFO_FORMAT", "text/html");
        uri.appendQueryParameter("FEATURE_COUNT", "10");
        uri.appendQueryParameter("X", Integer.toString((int) (tileSize * tilePos.x)));
        uri.appendQueryParameter("Y", Integer.toString(tileSize - (int) (tileSize * tilePos.y)));
        return NetUtils.downloadUrl(uri.toString(), this.httpHeaders, true, "UTF-8");
    }

    @Override
    protected String buildTileURL(MapTile tile) {
        String bbox = getTileBbox(tile);

        Uri.Builder uri = createBaseUri("GetMap");
        uri.appendQueryParameter("BBOX", bbox);
        return uri.toString();
    }
    
    private Uri.Builder createBaseUri(String request) {
        Uri.Builder uri = Uri.parse(baseUrl).buildUpon();
        uri.appendQueryParameter("LAYERS", layer);
        uri.appendQueryParameter("FORMAT", format);
        uri.appendQueryParameter("SERVICE", "WMS");
        uri.appendQueryParameter("VERSION", "1.1.0");
        uri.appendQueryParameter("REQUEST", request);
        uri.appendQueryParameter("STYLES", style);
        uri.appendQueryParameter("EXCEPTIONS", "application/vnd.ogc.se_inimage");
        uri.appendQueryParameter("SRS", getProjection().name());
        uri.appendQueryParameter("WIDTH", Integer.toString(tileSize));
        uri.appendQueryParameter("HEIGHT", Integer.toString(tileSize));
        return uri;
    }

    private String getTileBbox(MapTile tile) {
        Envelope envelope = TileUtils.TileBounds(tile.x, tile.y, tile.zoom, projection);

        String bbox = ""
                + envelope.getMinX() + "," + envelope.getMinY() + ","
                + envelope.getMaxX() + "," + envelope.getMaxY();
        Log.debug("WMSRasterDataSource: envelope bbox " + bbox);
        return bbox;
    }

}
