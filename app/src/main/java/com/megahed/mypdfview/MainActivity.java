package com.megahed.mypdfview;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.widget.Toast;

import com.megahed.pdfview.PDFView;
import com.megahed.pdfview.listener.OnLoadCompleteListener;
import com.megahed.pdfview.model.Configurator;

public class MainActivity extends AppCompatActivity {

    String fileTypeString ="application/pdf";
    PDFView pdfView;
    Configurator configuration;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        pdfView=findViewById(R.id.ppp);
        configuration=new Configurator();
        add_Book();

    }

    private void add_Book() {
        // String[] perms = {Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE};

        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false);
        intent.setType(fileTypeString);
        someActivityResultLauncher.launch(intent);


    }


    ActivityResultLauncher<Intent> someActivityResultLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult()
            , new ActivityResultCallback<ActivityResult>() {
                @Override
                public void onActivityResult(ActivityResult result) {
                    if (result.getResultCode() == Activity.RESULT_OK&&result.getData() != null) {

                        configuration.fromUri(result.getData().getData())
                                .enableAnnotationRendering(true)
                                .pageSnap(false)
                                .nightMode(false)
                                .enableAntialiasing(false)
                                .autoSpacing(false)
                                .enableSwipe(true)
                                .pageFling(false)
                                .fitEachPage(false)
                                .onLoad(new OnLoadCompleteListener() {
                                    @Override
                                    public void loadComplete(int nbPages) {
                                       // Toast.makeText(MainActivity.this, " "+pdfView.getTextPage(0), Toast.LENGTH_SHORT).show();
                                    }
                                });
                        pdfView.setConfigurator(configuration).load();



                    }
                }
            });


}