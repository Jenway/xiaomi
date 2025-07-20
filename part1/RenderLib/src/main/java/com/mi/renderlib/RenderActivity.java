package com.mi.renderlib;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.concurrent.Executors;


public class RenderActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("renderlib");
    }

    private SurfaceView mSurfaceView;
    private Bitmap mCarBitmap;
    private Bitmap mTaiyiBitmap;
    private Bitmap mLianhuaBitmap;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.layout);
        this.mSurfaceView = findViewById(R.id.surface_view);
        this.mSurfaceView.getHolder().addCallback(this);
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        Executors.newSingleThreadExecutor().execute(() -> {
            Bitmap fg = BitmapFactory.decodeResource(getResources(), R.drawable.taiyi);
            Bitmap bg = BitmapFactory.decodeResource(getResources(), R.drawable.lianhua);
            Surface surface = holder.getSurface();

            runOnUiThread(() -> render(bg, fg, surface));
        });
    }


    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        Log.d("MI", "surfaceChanged - Surface size changed to " + width + "x" + height);
        if (mLianhuaBitmap != null && mTaiyiBitmap != null) {
            Log.d("MI", "Calling native render with loaded bitmaps. Lianhua: " + mLianhuaBitmap.hashCode() + ", Taiyi: " + mTaiyiBitmap.hashCode());
            render(mLianhuaBitmap, mTaiyiBitmap, holder.getSurface());
        } else {
            Log.w("MI", "Bitmaps not yet loaded when surfaceChanged was called. mLianhuaBitmap is null: " + (mLianhuaBitmap == null) + ", mTaiyiBitmap is null: " + (mTaiyiBitmap == null));
        }
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {

    }

    public native void render(Bitmap bgBitmap,Bitmap fgBitmap, Surface surface);
}
