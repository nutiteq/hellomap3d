package com.nutiteq.editable.styles;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Bitmap.Config;
import android.graphics.Paint.Style;

import com.nutiteq.components.Color;
import com.nutiteq.style.PointStyle;

/**
 * Style class for overlay layer. 
 * The style class can be used to customize bitmaps/colors of the points (circles) shown on top of line and polygon vertices.
 * 
 * @author mtehver
 * 
 */
public class OverlayLayerStyle {
  public final PointStyle editablePointStyle;
  public final PointStyle virtualPointStyle;
  public final PointStyle dragPointStyle;
  public final float maxDragDistance;

  /**
   * Default constructor.
   * 
   * @param builder
   *          builder state for the final style.
   */
  public OverlayLayerStyle(Builder builder) {
    editablePointStyle = builder.editablePointStyle;
    virtualPointStyle = builder.virtualPointStyle;
    dragPointStyle = builder.dragPointStyle;
    maxDragDistance = builder.maxDragDistance;
  }

  /**
   * Builder class for overlay layer style. 
   *
   */
  public static class Builder {
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

    private PointStyle editablePointStyle = PointStyle.builder()
        .setBitmap(OVERLAY_POINT_BITMAP).setColor(Color.WHITE & 0x80ffffff)
        .setSize(0.5f).setPickingSize(0).build();
    private PointStyle virtualPointStyle = PointStyle.builder()
        .setBitmap(OVERLAY_POINT_BITMAP).setColor(Color.WHITE & 0x60ffffff)
        .setSize(0.35f).setPickingSize(0).build();
    private PointStyle dragPointStyle = PointStyle.builder()
        .setBitmap(OVERLAY_POINT_BITMAP).setColor(Color.WHITE & 0xc0ffffff)
        .setSize(0.75f).setPickingSize(0).build();
    private float maxDragDistance = MAX_DRAG_DISTANCE;

    private Builder() {
    }

    /**
     * Set the style for editable overlay point.
     * 
     * @param pointStyle
     *          new style for the point
     * @return self
     */
    public Builder setEditablePointStyle(PointStyle pointStyle) {
      this.editablePointStyle = pointStyle;
      return this;
    }

    /**
     * Set the style for virtual overlay point. Virtual overlay points are virtual until dragged (in that case they are inserted as actual vertices).
     * 
     * @param pointStyle
     *          new style for the point
     * @return self
     */
    public Builder setVirtualPointStyle(PointStyle pointStyle) {
      this.virtualPointStyle = pointStyle;
      return this;
    }

    /**
     * Set the style for active drag point.
     * 
     * @param pointStyle
     *          new style for the point
     * @return self
     */
    public Builder setDragPointStyle(PointStyle pointStyle) {
      this.dragPointStyle = pointStyle;
      return this;
    }

    /**
     * Set the distance to the drag point. For large distances, it becomes easier to hit the drag point.
     *  
     * @param maxDistance
     *          maximum distance to drag point
     * @return self
     */
    public Builder setMaxDragDistance(float maxDistance) {
      this.maxDragDistance = maxDistance;
      return this;
    }

    /**
     * Finalize the style.
     * 
     * @return actual style corresponding to the builder state.
     */
    public OverlayLayerStyle build() {
      return new OverlayLayerStyle(this);
    }
  }

  /**
   * Create new style builder instance.
   * 
   * @return new style builder instance.
   */
  public static Builder builder() {
    return new Builder();
  }
}
