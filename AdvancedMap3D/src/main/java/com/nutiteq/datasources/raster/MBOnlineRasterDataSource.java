package com.nutiteq.datasources.raster;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Map;

import org.json.JSONException;

import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterdatasources.HTTPRasterDataSource;
import com.nutiteq.utils.UtfGridHelper;
import com.nutiteq.utils.UtfGridHelper.MBTileUTFGrid;

/**
 * A raster data source class for online MapBox data.
 * It is mainly a basic HTTP raster data source but adds implementation for UTF grid data sources. 
 */
public class MBOnlineRasterDataSource extends HTTPRasterDataSource implements UTFGridDataSource {
	public final String account;
	public final String map;

	/**
	 * Default constructor.
	 * 
	 * @param projection
	 * 			  projection for the data source (practically always EPSG3857)
	 * @param minZoom
	 *            minimum zoom supported by the data source.
	 * @param maxZoom
	 *            maximum zoom supported by the data source.
	 * @param account
	 *            account id to use
	 * @param map
	 * 			  map id to use
	 */
	public MBOnlineRasterDataSource(Projection projection, int minZoom, int maxZoom, String account, String map) {
		super(projection, minZoom, maxZoom, "http://api.tiles.mapbox.com/v3/" + account + "." + map + "/{zoom}/{x}/{y}.png");
		this.account = account;
		this.map = map;
	}

	@Override
	public MBTileUTFGrid loadUTFGrid(MapTile tile) {
		String url = "http://api.tiles.mapbox.com/v3/" + account + "." + map + "/" + tile.zoom + "/" + tile.x + "/" + tile.y + ".grid.json?callback=grid";

		Log.info(getClass().getName() + ": loading UTFgrid " + url);

		HttpURLConnection urlConnection = null;
		try {
			urlConnection = (HttpURLConnection) (new URL(url)).openConnection();
			urlConnection.setConnectTimeout(connectionTimeout);
			urlConnection.setReadTimeout(readTimeout);

			if (httpHeaders != null){
				for (Map.Entry<String, String> entry : httpHeaders.entrySet()) {
					urlConnection.addRequestProperty(entry.getKey(), entry.getValue());  
				}
			}

			BufferedInputStream inputStream = new BufferedInputStream(urlConnection.getInputStream(), BUFFER_SIZE);
			return readTileGrid(inputStream);
		} catch (IOException e) {
			Log.error(getClass().getName() + ": failed to load tile. " + e.getMessage());
		} finally {
			if (urlConnection != null) {
				urlConnection.disconnect();
			}
		}
		return null;
	}

	private MBTileUTFGrid readTileGrid(InputStream inputStream) {
		byte[] buffer = new byte[BUFFER_SIZE];
		ByteArrayOutputStream outputStream = new ByteArrayOutputStream();

		try {
			int bytesRead;
			while ((bytesRead = inputStream.read(buffer)) != -1) {
				outputStream.write(buffer, 0, bytesRead);
			}
			outputStream.flush();
			return UtfGridHelper.decodeUtfGrid(outputStream.toByteArray());
		} catch (JSONException e) {
			Log.error(getClass().getName() + ": failed to load tile. " + e.getMessage());
		} catch (IOException e) {
			Log.error(getClass().getName() + ": failed to load tile. " + e.getMessage());
		} finally {
			try {
				outputStream.close();
				if (inputStream != null) {
					inputStream.close();
				}
			} catch (IOException e) {
				Log.error(getClass().getName() + ": failed to close the stream. " + e.getMessage());
			}
		}
		return null;
	}
}

