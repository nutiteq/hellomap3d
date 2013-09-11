package com.nutiteq.layers.raster;

import com.nutiteq.components.MapTile;
import com.nutiteq.log.Log;
import com.nutiteq.projections.Projection;
import com.nutiteq.rasterlayers.RasterLayer;
import com.nutiteq.tasks.NetFetchTileTask;

public class Regio extends RasterLayer {

	public Regio(Projection projection, int minZoom, int maxZoom,
			int id, String location) {
		super(projection, minZoom, maxZoom, id, location);

	}

	@Override
	public void fetchTile(MapTile tile) {
	    // Sample: http://pump.regio.ee/delfi/?rq=2212

		final StringBuffer buf = new StringBuffer(
				"http://pump.regio.ee/delfi/?rq=2");

		for (int i = tile.zoom - 1; i >= 0; i--) {
			buf.append((((tile.y >> i) & 1) << 1) + ((tile.x >> i) & 1));
		}

		Log.debug(buf.toString());

		executeFetchTask(new NetFetchTileTask(tile, components, tileIdOffset, buf.toString()));

	}


	@Override
	public void flush() {

	}

}
