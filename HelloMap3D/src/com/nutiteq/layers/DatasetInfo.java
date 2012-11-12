package com.nutiteq.layers;

import java.io.Serializable;
import java.util.Vector;

public class DatasetInfo implements Serializable{
    private static final long serialVersionUID = -481536974330949149L;

    public Vector<String> dataSets;
    public double bestZoom;
    DatasetInfo(Vector<String> dataSets,double d){
        this.dataSets = dataSets;
        this.bestZoom = d;
    }
}