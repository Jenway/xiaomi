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

import java.util.function.Function;

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
        mCarBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.car);
        mTaiyiBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.taiyi);
        mLianhuaBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.lianhua);

        // Add logging to verify the dimensions on the Java side
        Log.d("RenderActivity", "Loaded mCarBitmap dimensions: " + mCarBitmap.getWidth() + "x" + mCarBitmap.getHeight());
        Log.d("RenderActivity", "Loaded mTaiyiBitmap dimensions: " + mTaiyiBitmap.getWidth() + "x" + mTaiyiBitmap.getHeight());
        Log.d("RenderActivity", "Loaded mLianhuaBitmap dimensions: " + mLianhuaBitmap.getWidth() + "x" + mLianhuaBitmap.getHeight());
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        Log.d("MI", "surfaceChanged");
        render(mLianhuaBitmap,mTaiyiBitmap, holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {

    }

    public native void render(Bitmap bgBitmap,Bitmap fgBitmap, Surface surface);
}
