package com.example.androidplayer;

import android.os.Handler;
import android.os.Looper;
import android.view.Surface;

public class Player {

    static {
        System.loadLibrary("androidplayer");
    }

    private long nativeContext;

    // 与 C++ PlayerState 枚举保持一致
    public enum PlayerState {
        None,
        Playing,
        Paused,
        End,
        Seeking,
        Error
    }

    public interface OnStateChangeListener {
        void onStateChanged(PlayerState newState);
    }
    private OnStateChangeListener onStateChangeListener;

    private Surface mSurface;
    private String fileUri;

    public Player() {
        nativeInit();
    }

    public void setOnStateChangeListener(OnStateChangeListener listener) {
        this.onStateChangeListener = listener;
    }

    public void setSurface(Surface surface) {
        this.mSurface = surface;
    }

    public void setDataSource(String uri) {
        this.fileUri = uri;
    }

    public void start() {
        if (fileUri != null && mSurface != null) {
            nativePlay(fileUri, mSurface);
        }
    }

    public void pause(boolean p) {
        nativePause(p);
    }

    public void stop() {
        nativeStop();
    }

    public void seek(double position) {
        nativeSeek(position);
    }

    public double getDuration() {
        return nativeGetDuration();
    }

    public PlayerState getState() {
        int stateInt = nativeGetState();
        if (stateInt >= 0 && stateInt < PlayerState.values().length) {
            return PlayerState.values()[stateInt];
        }
        return PlayerState.None; // 默认或错误情况
    }

    public double getPosition() {
        return nativeGetPosition();
    }

    public void release() {
        nativeRelease();
        nativeContext = 0;
    }

    private void onNativeStateChanged(int newStateInt) {
        PlayerState state = PlayerState.values()[newStateInt];
        new Handler(Looper.getMainLooper()).post(() -> {
            if (onStateChangeListener != null) {
                onStateChangeListener.onStateChanged(state);
            }
        });
    }

    private native void nativeInit();
    private native void nativeRelease();
    private native void nativePlay(String file, Surface surface);
    private native void nativePause(boolean p);
    private native void nativeStop();
    private native void nativeSeek(double position);
    private native double nativeGetDuration();
    private native int nativeGetState();
    private native double nativeGetPosition();
}