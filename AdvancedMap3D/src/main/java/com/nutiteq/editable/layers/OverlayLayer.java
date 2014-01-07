package com.nutiteq.editable.layers;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import com.nutiteq.components.Components;
import com.nutiteq.components.Envelope;
import com.nutiteq.components.MutableEnvelope;
import com.nutiteq.geometry.Geometry;
import com.nutiteq.geometry.VectorElement;
import com.nutiteq.projections.Projection;
import com.nutiteq.vectorlayers.GeometryLayer;

/**
 * This is used for virtual editing circles - markers which are shown in corners of Lines and Polygons
 * 
 * @author mtehver
 *
 */
public class OverlayLayer extends GeometryLayer {
    List<Geometry> elements = new ArrayList<Geometry>();

    public OverlayLayer(Projection projection) {
        super(projection);
    }

    public List<Geometry> getAll() {
        synchronized (elements) {
            return new ArrayList<Geometry>(elements);
        }
    }

    public void setAll(List<? extends Geometry> elements) {
        for (Geometry element : this.elements) {
            if (!elements.contains(element)) {
                element.detachFromLayer();
            }
        }
        for (Geometry element : elements) {
            element.attachToLayer(this);
            element.setActiveStyle(getCurrentZoomLevel());
        }
        synchronized (elements) {
            this.elements = new ArrayList<Geometry>(elements);
            setVisibleElements(this.elements);
        }
    }

    @Override
    public void clear() {
        synchronized (elements) {
            this.elements.clear();
            setVisibleElements(this.elements);
        }

        for (Geometry element : this.elements) {
            element.detachFromLayer();
        }
    }

    @Override
    public void addAll(Collection<? extends Geometry> elements) {
        for (Geometry element : elements) {
            element.attachToLayer(this);
            element.setActiveStyle(getCurrentZoomLevel());
        }

        synchronized (elements) {
            this.elements.addAll(elements);
            setVisibleElements(this.elements);
        }
    }

    @Override
    public void removeAll(Collection<? extends Geometry> elements) {
        synchronized (elements) {
            this.elements.removeAll(elements);
            setVisibleElements(this.elements);
        }

        for (Geometry element : elements) {
            element.detachFromLayer();
        }
    }

    @Override
    public Envelope getDataExtent() {
        MutableEnvelope envelope = new MutableEnvelope(super.getDataExtent());
        synchronized (elements) {
            for (Geometry element : elements) {
                Envelope internalEnv = element.getInternalState().envelope;
                envelope.add(projection.fromInternal(internalEnv));
            }
        }
        return new Envelope(envelope);
    }

    @Override
    public void onElementChanged(VectorElement element) {
        if (element instanceof Geometry) {
            element.calculateInternalState();
            Components components = getComponents();
            if (components != null) {
                components.mapRenderers.getMapRenderer().requestRenderView();
            }
        } else {
            super.onElementChanged(element);
        }
    }

    @Override
    public void calculateVisibleElements(Envelope envelope, int zoom) {
        synchronized (elements) {
            setVisibleElements(elements);
        }
    }
}
