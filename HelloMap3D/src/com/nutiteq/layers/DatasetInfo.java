package com.nutiteq.layers;

import java.io.Serializable;
import java.util.Vector;

import com.vividsolutions.jts.geom.Envelope;

public class DatasetInfo implements Serializable{
    private static final long serialVersionUID = -481536974330949149L;

    public Vector<String> dataFile;
    public double bestZoom;
    public int id;
    public Envelope envelope;
    
    DatasetInfo(Vector<String> dataSets,double d, int id, Envelope envelope){
        this.dataFile = dataSets;
        this.bestZoom = d;
        this.id = id;
        this.envelope = envelope;
    }
}