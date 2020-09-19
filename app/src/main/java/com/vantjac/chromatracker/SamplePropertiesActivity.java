package com.vantjac.chromatracker;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

public class SamplePropertiesActivity extends AppCompatActivity {
    private static final String TAG = "SampleProperties";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.sample_properties);

        final long samplePtr;
        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            samplePtr = extras.getLong("ptr", 0);
        } else {
            Log.e(TAG, "No pointer given!");
            finish();
            return;
        }
        if (samplePtr == 0) {
            Log.e(TAG, "Null pointer!");
            finish();
            return;
        }

        /*  Update properties  */

        EditText nameText = findViewById(R.id.name);
        nameText.setText(EditProperties.sampleGetName(samplePtr));
        nameText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                EditProperties.sampleSetName(samplePtr, s.toString());
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });

        TextView channelsText = findViewById(R.id.wave_channels);
        channelsText.setText(Integer.toString(EditProperties.sampleGetWaveChannels(samplePtr)));

        TextView framesText = findViewById(R.id.wave_frames);
        int waveFrames = EditProperties.sampleGetWaveFrames(samplePtr);
        framesText.setText(Integer.toString(waveFrames));

        TextView frameRateText = findViewById(R.id.wave_frame_rate);
        frameRateText.setText(Integer.toString(EditProperties.sampleGetWaveFrameRate(samplePtr)));

        Spinner playbackModeSpinner = findViewById(R.id.playback_mode);
        playbackModeSpinner.setSelection(EditProperties.sampleGetPlaybackMode(samplePtr));
        playbackModeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                EditProperties.sampleSetPlaybackMode(samplePtr, position);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) { }
        });

        Spinner loopTypeSpinner = findViewById(R.id.loop_type);
        loopTypeSpinner.setSelection(EditProperties.sampleGetLoopType(samplePtr));
        loopTypeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                EditProperties.sampleSetLoopType(samplePtr, position);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) { }
        });

        SeekBar loopStartBar = findViewById(R.id.loop_start);
        loopStartBar.setMax(waveFrames);
        loopStartBar.setProgress(EditProperties.sampleGetLoopStart(samplePtr));
        loopStartBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.sampleSetLoopStart(samplePtr, progress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar loopEndBar = findViewById(R.id.loop_end);
        loopEndBar.setMax(waveFrames);
        loopEndBar.setProgress(EditProperties.sampleGetLoopEnd(samplePtr));
        loopEndBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.sampleSetLoopEnd(samplePtr, progress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar volumeBar = findViewById(R.id.volume);
        volumeBar.setMax(256);
        volumeBar.setProgress((int)(EditProperties.sampleGetVolume(samplePtr) * 256.0f));
        volumeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.sampleSetVolume(samplePtr, (float)progress / 256.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar panningBar = findViewById(R.id.panning);
        panningBar.setMax(256);
        float panning = EditProperties.sampleGetPanning(samplePtr);
        panningBar.setProgress((int)((panning + 1.0f) * 128.0f));
        panningBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.sampleSetPanning(samplePtr, ((float)progress / 128.0f) - 1.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        EditText baseKeyText = findViewById(R.id.base_key);
        baseKeyText.setText(Integer.toString(EditProperties.sampleGetBaseKey(samplePtr)));
        baseKeyText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                try {
                    EditProperties.sampleSetBaseKey(samplePtr, Integer.parseInt(s.toString()));
                } catch (NumberFormatException e) { }
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });

        SeekBar finetuneBar = findViewById(R.id.finetune);
        finetuneBar.setMax(256);
        float finetune = EditProperties.sampleGetFinetune(samplePtr);
        finetuneBar.setProgress((int)((finetune + 1.0f) * 128.0f));
        finetuneBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.sampleSetFinetune(samplePtr,
                        ((float)progress / 128.0f) - 1.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        EditText keyStartText = findViewById(R.id.key_start);
        keyStartText.setText(Integer.toString(EditProperties.sampleGetKeyStart(samplePtr)));
        keyStartText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                try {
                    EditProperties.sampleSetKeyStart(samplePtr, Integer.parseInt(s.toString()));
                } catch (NumberFormatException e) { }
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });

        EditText keyEndText = findViewById(R.id.key_end);
        keyEndText.setText(Integer.toString(EditProperties.sampleGetKeyEnd(samplePtr)));
        keyEndText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                try {
                    EditProperties.sampleSetKeyEnd(samplePtr, Integer.parseInt(s.toString()));
                } catch (NumberFormatException e) { }
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });
    }
}
