package com.example.androidplayer;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Toast;

import com.example.androidplayer.databinding.ActivityMainBinding;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";

    private Player player;
    private SurfaceView surfaceView;
    private Button playPauseButton;
    private Button stopButton;
    private SeekBar mSeekBar;

    private final ExecutorService playerExecutor = Executors.newSingleThreadExecutor();
    private Thread progressThread;
    private boolean isProgressThreadRunning = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        }

        ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        surfaceView = binding.surfaceView;
        playPauseButton = binding.button;
        stopButton = binding.button2;
        mSeekBar = binding.seekBar;

        player = new Player();
        player.setDataSource("file:/sdcard/test12.mp4");

        player.setOnStateChangeListener(this::updateUiForState);

        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                Log.d(TAG, "Surface created.");
                player.setSurface(holder.getSurface());
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                Log.d(TAG, "Surface destroyed.");
                playerExecutor.execute(player::stop);
            }
        });

        playPauseButton.setOnClickListener(v -> handlePlayPauseClick());
        stopButton.setOnClickListener(v -> playerExecutor.execute(player::stop));

        mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    double duration = player.getDuration();
                    if (duration > 0) {
                        double position = (progress / 100.0) * duration;
                        playerExecutor.execute(() -> player.seek(position));
                    }
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        updateUiForState(Player.PlayerState.None);
    }

    private void handlePlayPauseClick() {
        Player.PlayerState currentState = player.getState();
        Log.d(TAG, "Play/Pause button clicked. Current state: " + currentState);

        switch (currentState) {
            case None:
            case End:
                playerExecutor.execute(player::start);
                break;
            case Playing:
                playerExecutor.execute(() -> player.pause(true));
                break;
            case Paused:
                playerExecutor.execute(() -> player.pause(false));
                break;
        }
    }

    private void updateUiForState(Player.PlayerState newState) {
        Log.d(TAG, "Updating UI for state: " + newState);
        switch (newState) {
            case None:
            case End:
                playPauseButton.setText("播放");
                playPauseButton.setEnabled(true);
                stopButton.setEnabled(false);
                mSeekBar.setProgress(0);
                stopProgressThread();
                break;
            case Playing:
                playPauseButton.setText("暂停");
                playPauseButton.setEnabled(true);
                stopButton.setEnabled(true);
                startProgressThread();
                break;
            case Paused:
                playPauseButton.setText("播放");
                playPauseButton.setEnabled(true);
                stopButton.setEnabled(true);
                stopProgressThread();
                break;
            case Seeking:
                playPauseButton.setEnabled(false);
                stopButton.setEnabled(false);
                break;
            case Error:
                playPauseButton.setText("错误");
                playPauseButton.setEnabled(false);
                stopButton.setEnabled(false);
                stopProgressThread();
                Toast.makeText(this, "播放器发生错误", Toast.LENGTH_SHORT).show();
                break;
        }
    }

    private void startProgressThread() {
        if (progressThread != null && progressThread.isAlive()) {
            return; // 已经在运行
        }
        isProgressThreadRunning = true;
        progressThread = new Thread(() -> {
            while (isProgressThreadRunning) {
                double duration = player.getDuration();
                double position = player.getPosition(); // 假设 getPosition 存在

                if (duration > 0) {
                    int progress = (int) ((position / duration) * 100);
                    runOnUiThread(() -> mSeekBar.setProgress(progress));
                }

                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    Log.d(TAG, "Progress thread interrupted.");
                    break;

                }
            }
            Log.d(TAG, "Progress thread finished.");
        });
        progressThread.start();
    }

    private void stopProgressThread() {
        isProgressThreadRunning = false;
        if (progressThread != null) {
            progressThread.interrupt();
            progressThread = null;
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.d(TAG, "onStop called. Stopping player.");
        playerExecutor.execute(player::stop);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy called. Releasing player and shutting down executor.");
        player.release();
        playerExecutor.shutdown();
        stopProgressThread();
    }
}