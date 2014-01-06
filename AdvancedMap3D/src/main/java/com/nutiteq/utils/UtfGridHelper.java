package com.nutiteq.utils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.InflaterInputStream;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;


public class UtfGridHelper {

	private static final int UTFGRID_NODATA = 32;
	public static final String TEMPLATED_FULL_KEY = "templated_full";
	public static final String TEMPLATED_TEASER_KEY = "templated_teaser";
	public static final String TEMPLATED_LOCATION_KEY = "templated_location";


	static byte[] ZLIB_HEADER = {(byte) 0x78, (byte) 0x9C};

	public static class MBTileUTFGrid {
		public String[] grid = null;
		public String[] keys = null;
		public JSONObject data = null;
	}

	public static MBTileUTFGrid decodeUtfGrid(byte[] gridBytes) throws IOException, JSONException {

		String gridJSON;

		if(gridBytes[0] == ZLIB_HEADER[0] && gridBytes[1] == ZLIB_HEADER[1]){
			// seems to be compressed with ZLIB
			InflaterInputStream in = new InflaterInputStream(
					new ByteArrayInputStream(gridBytes));
			ByteArrayOutputStream inflatedOut = new ByteArrayOutputStream();
			int readLength;
			byte[] block = new byte[1024];
			while ((readLength = in.read(block)) != -1)
				inflatedOut.write(block, 0, readLength);
			inflatedOut.flush();
			gridJSON = new String(inflatedOut.toByteArray());
		}else{
			// uncompressed
			gridJSON = new String(gridBytes);

			// remove callback
			if(!gridJSON.startsWith("{")){
				gridJSON = gridJSON.substring(gridJSON.indexOf("(")+1,gridJSON.lastIndexOf(")"));
			}
		}

		MBTileUTFGrid grid = new MBTileUTFGrid();
		JSONObject root = new JSONObject(gridJSON);
		JSONArray gridA = root.getJSONArray("grid");
		JSONArray keysA = root.getJSONArray("keys");
		grid.grid = new String[gridA.length()];

		for (int i = 0; i < gridA.length(); i++) {
			grid.grid[i] = gridA.getString(i);
		}
		grid.keys = new String[keysA.length()];
		for (int i = 0; i < keysA.length(); i++) {
			grid.keys[i] = keysA.getString(i);
		}

		grid.data = root.optJSONObject("data");

		return grid;

	}

	/**
	 * get clicked UTFgrid code within the tile. 
	 * from https://github.com/mapbox/mbtiles-spec/blob/master/1.1/utfgrid.md "Mapping an ID to a key"
	 * @param tileSize usually 256
	 * @param clickedX 
	 * @param clickedY
	 * @param grid
	 * @param utfgridRadius 
	 * @return
	 */
	public static int utfGridCode(int tileSize, int clickedX, int clickedY,
			MBTileUTFGrid grid, int utfgridRadius) {

		double factor = tileSize / grid.grid.length;

		// quick lookup with exact location
		int row = (int) Math.round(clickedY / factor);
		int col = (int) Math.round(clickedX / factor);

		int id = grid.grid[row].codePointAt(col);

		// search nearby if not found
		// TODO: approximate search works within one tile only, so if click is near tile border, and data is in neighbor tile, then it will not be found
		if(utfgridRadius > 0 && id == UTFGRID_NODATA){
			// search with pixel tolerance, limit to range [0...grid.grid.length[
			int rowMin = Math.max((int) Math.round((clickedY - utfgridRadius) / factor), 0);
			int rowMax = Math.min((int) Math.round((clickedY + utfgridRadius)  / factor), grid.grid.length-1);
			int colMin = Math.max((int) Math.round((clickedX - utfgridRadius) / factor), 0);
			int colMax = Math.min((int) Math.round((clickedX + utfgridRadius)  / factor), grid.grid.length-1);

			// find first match, may be not the closest one really.
			for(row = rowMin; row<=rowMax && id == UTFGRID_NODATA;row++)
				for(col = colMin; col<=colMax && id == UTFGRID_NODATA;col++){
					id = grid.grid[row].codePointAt(col);
				}
		}

		// decode id
		if(id >= 93) --id;
		if(id >= 35) --id;
		id -= UTFGRID_NODATA;
		return id;
	}
}
