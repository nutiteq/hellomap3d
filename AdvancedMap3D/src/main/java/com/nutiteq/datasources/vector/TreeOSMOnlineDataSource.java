package com.nutiteq.datasources.vector;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.content.Context;
import android.net.ParseException;
import android.net.Uri;

import com.nutiteq.components.CullState;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MapPos;
import com.nutiteq.components.Vector;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.NMLModel;
import com.nutiteq.geometry.Point;
import com.nutiteq.log.Log;
import com.nutiteq.nmlpackage.NMLPackage;
import com.nutiteq.nmlpackage.NMLPackage.Model;
import com.nutiteq.projections.Projection;
import com.nutiteq.style.ModelStyle;
import com.nutiteq.style.StyleSet;
import com.nutiteq.utils.WkbRead;
import com.nutiteq.vectordatasources.AbstractVectorDataSource;

public class TreeOSMOnlineDataSource extends AbstractVectorDataSource<NMLModel> {
	// we use here a server with non-standard (and appareantly undocumented) API
	// it returns OSM trees in WKB format, for given BBOX
	private String baseUrl = "http://kaart.maakaart.ee/poiexport/trees.php?";

	private int minZoom;
	private int maxObjects;

	private StyleSet<ModelStyle> modelStyleSet;
	private Model nmlModel;
	private double scale;

	public TreeOSMOnlineDataSource(Projection proj, int maxObjects, Context app, int resourceId, int minZoom, double scale) {
		super(proj);

		this.maxObjects = maxObjects;
		this.scale = scale;
		this.minZoom = minZoom;

		ModelStyle modelStyle = ModelStyle.builder().build();
		modelStyleSet = new StyleSet<ModelStyle>(null);
		modelStyleSet.setZoomStyle(minZoom, modelStyle);

		try {
			InputStream is = app.getResources().openRawResource(resourceId);
			nmlModel = NMLPackage.Model.parseFrom(is);
		}
		catch (Exception e) {
			e.printStackTrace();
		}
	}
	
	@Override
	public Envelope getDataExtent() {
		return null; // TODO: implement
	}
	
	@Override
	public Collection<NMLModel> loadElements(CullState cullState) {
		if (cullState.zoom < minZoom) {
			return null;
		}

        Envelope box = projection.fromInternal(cullState.envelope);

        List<NMLModel> trees = new ArrayList<NMLModel>();
		// URL request format: http://kaart.maakaart.ee/poiexport/trees.php?bbox=xmin,ymin,xmax,ymax&output=wkb
		try {
			Uri.Builder uri = Uri.parse(baseUrl).buildUpon();
			uri.appendQueryParameter("bbox", (int) box.minX + "," + (int) box.minY + "," + (int) box.maxX + "," + (int) box.maxY);
			uri.appendQueryParameter("output", "wkb");
			uri.appendQueryParameter("max", String.valueOf(maxObjects));
			Log.debug("url:" + uri.build().toString());

			URLConnection conn = new URL(uri.build().toString()).openConnection();
			DataInputStream data = new DataInputStream(new BufferedInputStream(conn.getInputStream()));
			int n = data.readInt();
			Log.debug("trees to be loaded:" + n);

			for (int i = 0; i < n; i++) {
				long id = data.readInt();
				final Map<String, String> userData = new HashMap<String, String>();
				userData.put("name", data.readUTF());
				userData.put("type", data.readUTF());

				int len = data.readInt();
				byte[] wkb = new byte[len];
				data.read(wkb);
				Geometry[] geoms = WkbRead.readWkb(new ByteArrayInputStream(wkb), userData);
				for (Geometry geom : geoms) {
					if (geom instanceof Point) {
						MapPos location = ((Point) geom).getMapPos();
						NMLModel tree = new NMLModel(location, null, modelStyleSet, nmlModel, null);
						tree.setScale(new Vector(scale, scale, scale));
						tree.setId(id);
						trees.add(tree);
					} else {
						Log.error("loaded object not a point");
					}
				}
			}

		}
		catch (IOException e) {
			Log.error("IO ERROR " + e.getMessage());
		} catch (ParseException e) {
			e.printStackTrace();
		}

		return trees;
	}

}
