package com.windroid.emu.views;

import static com.windroid.emu.activities.EmulationActivity.handler;
import static com.windroid.emu.activities.MainActivity.enableCpuCounter;
import static com.windroid.emu.activities.MainActivity.enableDebugInfo;
import static com.windroid.emu.activities.MainActivity.enableRamCounter;
import static com.windroid.emu.activities.MainActivity.memoryStats;
import static com.windroid.emu.activities.MainActivity.windroidVersion;
import static com.windroid.emu.activities.MainActivity.selectedD3DXRenderer;
import static com.windroid.emu.activities.MainActivity.selectedDXVK;
import static com.windroid.emu.activities.MainActivity.selectedVKD3D;
import static com.windroid.emu.activities.MainActivity.selectedWineD3D;
import static com.windroid.emu.activities.MainActivity.totalCpuUsage;
import static com.windroid.emu.activities.MainActivity.vulkanDriverDeviceName;
import static com.windroid.emu.activities.MainActivity.vulkanDriverDriverVersion;
import static com.windroid.emu.core.RatPackageManager.getPackageNameVersionById;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

import com.windroid.emu.R;

public class OnScreenInfoView extends View {
    public OnScreenInfoView(Context context) {
        super(context);
        init(context);
    }

    public OnScreenInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public OnScreenInfoView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
    }

    private final Paint paint = new Paint();
    private final Runnable updateRunnable = new Runnable() {
        @Override
        public void run() {
            invalidate();
            handler.postDelayed(this, 800);
        }
    };

    private void init(Context context) {
        paint.setTextSize(40F);
        paint.setTypeface(context.getResources().getFont(R.font.quicksand));
        paint.setStrokeWidth(8F);

        handler.post(updateRunnable);
    }

    private final String vkd3dVersion = getPackageNameVersionById(selectedVKD3D);
    private final String dxvkVersion = getPackageNameVersionById(selectedDXVK);
    private final String wineD3DVersion = getPackageNameVersionById(selectedWineD3D);

    private float statsX = 20F;
    private float statsY = 0F;
    private float maxWidth = 0F;
    private float totalHeight = 0F;
    private boolean isDragging = false;
    private float dX, dY;

    @Override
    protected void onDraw(@NonNull Canvas canvas) {
        super.onDraw(canvas);

        float lineHeight = paint.getTextSize() + 10F;
        float currentY = lineHeight; // Start at the top

        if (enableRamCounter) {
            String text = "RAM: " + memoryStats;
            float x = (canvas.getWidth() - paint.measureText(text)) / 2;
            drawText(text, x, currentY, canvas);
            currentY += lineHeight;
        }
        if (enableCpuCounter) {
            String text = "CPU: " + totalCpuUsage;
            float x = (canvas.getWidth() - paint.measureText(text)) / 2;
            drawText(text, x, currentY, canvas);
            currentY += lineHeight;
        }

        if (enableDebugInfo) {
            onScreenInfo(canvas);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (enableDebugInfo) {
            // Dragging logic was only for RAM/CPU, but since they are now centered,
            // we could either remove it or repurpose it.
            // For now, let's keep it disabled for RAM/CPU as requested.
        }
        return super.onTouchEvent(event);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        handler.removeCallbacks(updateRunnable);
    }

    private void onScreenInfo(Canvas canvas) {
        float lineHeight = paint.getTextSize() + 10F;
        float currentY = lineHeight;

        drawText(windroidVersion, getTextEndX(canvas, windroidVersion), currentY, canvas);
        currentY += lineHeight;

        drawText(vkd3dVersion, getTextEndX(canvas, vkd3dVersion), currentY, canvas);
        currentY += lineHeight;

        if ("DXVK".equals(selectedD3DXRenderer)) {
            drawText(dxvkVersion, getTextEndX(canvas, dxvkVersion), currentY, canvas);
            currentY += lineHeight;
        } else if ("WineD3D".equals(selectedD3DXRenderer)) {
            drawText(wineD3DVersion, getTextEndX(canvas, wineD3DVersion), currentY, canvas);
            currentY += lineHeight;
        }

        drawText(vulkanDriverDeviceName, getTextEndX(canvas, vulkanDriverDeviceName), currentY, canvas);
        currentY += lineHeight;

        drawText(vulkanDriverDriverVersion, getTextEndX(canvas, vulkanDriverDriverVersion), currentY, canvas);
        // currentY += lineHeight;
    }

    private void drawText(String text, float x, float y, Canvas canvas) {
        paint.setStyle(Paint.Style.STROKE);
        paint.setColor(Color.BLACK);
        canvas.drawText(text, x, y, paint);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.WHITE);
        canvas.drawText(text, x, y, paint);
    }

    private float getTextEndX(Canvas canvas, String text) {
        return canvas.getWidth() - paint.measureText(text) - 20F;
    }
}