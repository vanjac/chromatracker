package com.vantjac.chromatracker;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.SeekBar;

import androidx.appcompat.app.AppCompatActivity;

public class SongPropertiesActivity extends AppCompatActivity {
    private static final String TAG = "SongProperties";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.song_properties);

        final long songPtr;
        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            songPtr = extras.getLong("ptr", 0);
        } else {
            Log.e(TAG, "No pointer given!");
            finish();
            return;
        }
        if (songPtr == 0) {
            Log.e(TAG, "Null pointer!");
            finish();
            return;
        }

        /* Update properties */

        SeekBar masterVolumeBar = findViewById(R.id.master_volume);
        masterVolumeBar.setMax(256);
        masterVolumeBar.setProgress((int)(EditProperties.songGetMasterVolume(songPtr) * 256.0f));
        masterVolumeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.songSetMasterVolume(songPtr, (float)progress / 256.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        final long[] instruments = EditProperties.songGetInstruments(songPtr);
        String[] instrumentNames = new String[instruments.length];
        for (int i = 0; i < instruments.length; i++) {
            long instPtr = instruments[i];
            if (instPtr != 0)
                instrumentNames[i] = Character.toString(EditProperties.instrumentGetID0(instPtr))
                        + Character.toString(EditProperties.instrumentGetID1(instPtr))
                        + ": " + EditProperties.instrumentGetName(instPtr);
            else
                instrumentNames[i] = "ERROR";
        }

        ListView instrumentList = findViewById(R.id.instruments);
        instrumentList.setAdapter(new ArrayAdapter<String>(this,
                android.R.layout.simple_list_item_1, instrumentNames));
        final SongPropertiesActivity thisActivity = this;
        instrumentList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                long instrumentPtr = instruments[position];
                if (instrumentPtr != 0) {
                    Intent intent = new Intent(thisActivity, InstrumentPropertiesActivity.class);
                    intent.putExtra("ptr", instrumentPtr);
                    startActivity(intent);
                }
            }
        });
    }
}