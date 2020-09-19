package com.vantjac.chromatracker;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.Spinner;

public class MainActivity extends AppCompatActivity {
    private static int REQUEST_CODE = 5555;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{ Manifest.permission.READ_EXTERNAL_STORAGE },
                    REQUEST_CODE);
        }
    }

    public void songProperties(View view) {
        long songPtr = getSongPtr();
        if (songPtr == 0)
            return;
        Intent intent = new Intent(this, SongPropertiesActivity.class);
        intent.putExtra("ptr", songPtr);
        startActivity(intent);
    }

    private int getInst() {
        EditText instText = findViewById(R.id.inst_num);
        String text = instText.getText().toString();
        if (text.isEmpty())
            return 0;
        else
            return Integer.parseInt(text);
    }

    private int getPitch() {
        SeekBar pitchBar = findViewById(R.id.key);
        return pitchBar.getProgress() + 60;
    }

    private float getVelocity() {
        SeekBar velBar = findViewById(R.id.velocity);
        return velBar.getProgress() / 256.0f;
    }

    public void onButton(View view) {
        noteOn(getInst(), getPitch(), getVelocity());
    }

    public void offButton(View view) {
        noteOff();
    }

    public void cutButton(View view) {
        noteCut();
    }

    public void glideButton(View view) {
        noteGlide(getPitch());
    }

    public void velButton(View view) {
        noteVelocity(getVelocity());
    }

    public native void startAudio(View view);
    public native void stopAudio(View view);
    public native void startPattern(View view);
    public native void stopPattern(View view);
    private native long getSongPtr();
    private native void noteOn(int inst_num, int pitch, float velocity);
    private native void noteGlide(int pitch);
    private native void noteVelocity(float velocity);
    private native void noteOff();
    private native void noteCut();
}
