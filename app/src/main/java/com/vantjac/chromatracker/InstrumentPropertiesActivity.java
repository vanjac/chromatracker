package com.vantjac.chromatracker;

import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.Spinner;

import androidx.appcompat.app.AppCompatActivity;

public class InstrumentPropertiesActivity extends AppCompatActivity {
    private static final String TAG = "InstrumentProperties";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.instrument_properties);

        final long instPtr;
        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            instPtr = extras.getLong("ptr", 0);
        } else {
            Log.e(TAG, "No pointer given!");
            finish();
            return;
        }
        if (instPtr == 0) {
            Log.e(TAG, "Null pointer!");
            finish();
            return;
        }

        /* Update properties */

        EditText idText = findViewById(R.id.id);
        idText.setText(Character.toString(EditProperties.instrumentGetID0(instPtr))
                + Character.toString(EditProperties.instrumentGetID1(instPtr)));
        idText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (s.length() > 0) {
                    EditProperties.instrumentSetID0(instPtr, s.charAt(0));
                }
                if (s.length() > 1) {
                    EditProperties.instrumentSetID1(instPtr, s.charAt(1));
                }
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });

        EditText nameText = findViewById(R.id.name);
        nameText.setText(EditProperties.instrumentGetName(instPtr));
        nameText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                EditProperties.instrumentSetName(instPtr, s.toString());
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });

        Button colorButton = findViewById(R.id.color);
        colorButton.setBackgroundColor(EditProperties.instrumentGetColor(instPtr)
                | Color.argb(255, 0, 0, 0));

        Spinner sampleOverlapModeSpinner = findViewById(R.id.sample_overlap_mode);
        sampleOverlapModeSpinner.setSelection(
                EditProperties.instrumentGetSampleOverlapMode(instPtr));
        sampleOverlapModeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                EditProperties.instrumentSetSampleOverlapMode(instPtr, position);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) { }
        });

        Spinner newNoteActionSpinner = findViewById(R.id.new_note_action);
        newNoteActionSpinner.setSelection(
                EditProperties.instrumentGetNewNoteAction(instPtr));
        newNoteActionSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                EditProperties.instrumentSetNewNoteAction(instPtr, position);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) { }
        });

        SeekBar randomDelayBar = findViewById(R.id.random_delay);
        randomDelayBar.setProgress(EditProperties.instrumentGetRandomDelay(instPtr));
        randomDelayBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.instrumentSetRandomDelay(instPtr, progress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar volumeBar = findViewById(R.id.volume);
        volumeBar.setMax(256);
        volumeBar.setProgress((int)(EditProperties.instrumentGetVolume(instPtr) * 256.0f));
        volumeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.instrumentSetVolume(instPtr, (float)progress / 256.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        final long adsrPtr = EditProperties.instrumentGetVolumeADSR(instPtr);

        SeekBar volumeAttackBar = findViewById(R.id.volume_attack);
        volumeAttackBar.setMax(192 * 4);
        volumeAttackBar.setProgress(EditProperties.adsrGetAttack(adsrPtr));
        volumeAttackBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.adsrSetAttack(adsrPtr, progress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar volumeDecayBar = findViewById(R.id.volume_decay);
        volumeDecayBar.setMax(192 * 4);
        volumeDecayBar.setProgress(EditProperties.adsrGetDecay(adsrPtr));
        volumeDecayBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.adsrSetDecay(adsrPtr, progress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar volumeSustainBar = findViewById(R.id.volume_sustain);
        volumeSustainBar.setMax(256);
        volumeSustainBar.setProgress((int)(EditProperties.adsrGetSustain(adsrPtr) * 256.0f));
        volumeSustainBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.adsrSetSustain(adsrPtr, (float)progress / 256.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar volumeReleaseBar = findViewById(R.id.volume_release);
        volumeReleaseBar.setMax(192 * 4);
        volumeReleaseBar.setProgress(EditProperties.adsrGetRelease(adsrPtr));
        volumeReleaseBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.adsrSetRelease(adsrPtr, progress);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar panningBar = findViewById(R.id.panning);
        panningBar.setMax(256);
        float panning = EditProperties.instrumentGetPanning(instPtr);
        panningBar.setProgress((int)((panning + 1.0f) * 128.0f));
        panningBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.instrumentSetPanning(instPtr, ((float)progress / 128.0f) - 1.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        EditText transposeText = findViewById(R.id.transpose);
        transposeText.setText(Integer.toString(EditProperties.instrumentGetTranspose(instPtr)));
        transposeText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                try {
                    EditProperties.instrumentSetTranspose(instPtr, Integer.parseInt(s.toString()));
                } catch (NumberFormatException e) { }
            }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) { }
        });

        SeekBar finetuneBar = findViewById(R.id.finetune);
        finetuneBar.setMax(256);
        float finetune = EditProperties.instrumentGetFinetune(instPtr);
        finetuneBar.setProgress((int)((finetune + 1.0f) * 128.0f));
        finetuneBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.instrumentSetFinetune(instPtr,
                        ((float)progress / 128.0f) - 1.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        SeekBar glideBar = findViewById(R.id.glide);
        glideBar.setMax(256);
        glideBar.setProgress((int)(EditProperties.instrumentGetGlide(instPtr) * 256.0f));
        glideBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                EditProperties.instrumentSetGlide(instPtr, (float)progress / 256.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        final long[] samples = EditProperties.instrumentGetSamples(instPtr);
        String[] sampleNames = new String[samples.length];
        for (int i = 0; i < samples.length; i++) {
            long samplePtr = samples[i];
            if (samplePtr != 0)
                sampleNames[i] = i + ": " + EditProperties.sampleGetName(samplePtr);
            else
                sampleNames[i] = "ERROR";
        }

        ListView sampleList = findViewById(R.id.samples);
        sampleList.setAdapter(new ArrayAdapter<String>(this,
                android.R.layout.simple_list_item_1, sampleNames));
        final InstrumentPropertiesActivity thisActivity = this;
        sampleList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                long samplePtr = samples[position];
                if (samplePtr != 0) {
                    Intent intent = new Intent(thisActivity, SamplePropertiesActivity.class);
                    intent.putExtra("ptr", samplePtr);
                    startActivity(intent);
                }
            }
        });
    }
}
