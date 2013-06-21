package com.nutiteq.editable;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Bitmap.Config;
import android.graphics.Paint.Style;

import com.nutiteq.components.Color;
import com.nutiteq.style.PointStyle;

public class OverlayLayerStyle {
	public final PointStyle editablePointStyle;
	public final PointStyle virtualPointStyle;
	public final PointStyle dragPointStyle;
	public final float maxDragDistance;


  public OverlayLayerStyle(Builder<?> builder) {
  	editablePointStyle = builder.editablePointStyle;
  	virtualPointStyle = builder.virtualPointStyle;
  	dragPointStyle = builder.dragPointStyle;
  	maxDragDistance = builder.maxDragDistance;
  }

  public static class Builder<T extends Builder<T>> {
  	private static final float MAX_DRAG_DISTANCE = 40.0f;
  	private static final Bitmap OVERLAY_POINT_BITMAP;

  	static {
  		OVERLAY_POINT_BITMAP = Bitmap.createBitmap(64, 64, Config.ARGB_8888);
  		Canvas canvas = new Canvas(OVERLAY_POINT_BITMAP);
  		Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
  		paint.setStyle(Style.FILL);
  		paint.setColor(Color.BLACK);
  		canvas.drawCircle(32, 32, 25.0f, paint);
  		paint.setColor(Color.WHITE);
  		canvas.drawCircle(32, 32, 24.2f, paint);
  	}

  	private PointStyle editablePointStyle = PointStyle.builder().setBitmap(OVERLAY_POINT_BITMAP).setColor(Color.WHITE & 0x80ffffff).setSize(0.5f).setPickingSize(0).build();
  	private PointStyle virtualPointStyle = PointStyle.builder().setBitmap(OVERLAY_POINT_BITMAP).setColor(Color.WHITE & 0x60ffffff).setSize(0.35f).setPickingSize(0).build();
  	private PointStyle dragPointStyle = PointStyle.builder().setBitmap(OVERLAY_POINT_BITMAP).setColor(Color.WHITE & 0xc0ffffff).setSize(0.75f).setPickingSize(0).build();
  	private float maxDragDistance = MAX_DRAG_DISTANCE;

    public Builder() {
    }

    public T setEditablePointStyle(PointStyle pointStyle) {
      this.editablePointStyle = pointStyle;
      return self();
    }

    public T setVirtualPointStyle(PointStyle pointStyle) {
      this.virtualPointStyle = pointStyle;
      return self();
    }

    public T setDragPointStyle(PointStyle pointStyle) {
      this.dragPointStyle = pointStyle;
      return self();
    }

    public T setMaxDragDistance(float maxDistance) {
    	this.maxDragDistance = maxDistance;
    	return self();
    }

    public OverlayLayerStyle build() {
      return new OverlayLayerStyle(this);
    }
    
    @SuppressWarnings("unchecked")
    protected T self() {
      return (T) this;
    }
  }

  @SuppressWarnings("rawtypes")
  public static Builder<?> builder() {
    return new Builder();
  }
}
