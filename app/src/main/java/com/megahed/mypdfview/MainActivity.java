package com.megahed.mypdfview;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Toast;

import com.megahed.pdfview.PDFView;
import com.megahed.pdfview.PdfiumSDK;
import com.megahed.pdfview.enms.FitPolicy;
import com.megahed.pdfview.listener.OnLoadCompleteListener;
import com.megahed.pdfview.listener.OnPageChangeListener;
import com.megahed.pdfview.listener.OnTapListener;
import com.megahed.pdfview.model.Configurator;
import com.megahed.pdfview.scroll.DefaultScrollHandle;
import com.megahed.pdfview.util.Util;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

public class MainActivity extends AppCompatActivity {

    String fileTypeString ="application/pdf";
    PDFView pdfView;
    Configurator configuration;
    MyPreferences myPreferences;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        pdfView=findViewById(R.id.ppp);
        configuration=new Configurator();
        myPreferences=new MyPreferences(this);
        //add_Book();


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




    class retire extends AsyncTask<String,Void, InputStream> implements OnPageChangeListener {


        Context context;


        public retire(Context context) {
            this.context = context;
        }

        @Override
        protected InputStream doInBackground(String... strings) {
            InputStream inputStream=null;
            try {
                URL url =new URL(strings[0]);
                HttpURLConnection urlConnection =(HttpURLConnection)url.openConnection();
                if (urlConnection.getResponseCode()==200){
                    inputStream =new BufferedInputStream(urlConnection.getInputStream());
                }

            } catch (IOException e) {
                return null;
            }
            return inputStream;

        }

        @Override
        protected void onPostExecute(InputStream inputStream) {
            if (inputStream!=null){
                String s="";
                configuration.fromStream(inputStream)
                        .defaultPage(1)
                        .enableSwipe(true)
                        .enableDoubletap(true)
                        .onPageChange(this)
                        .pageFitPolicy(FitPolicy.BOTH)
                        .enableAntialiasing(true)
                        .enableAnnotationRendering(false)
                        .onLoad(new OnLoadCompleteListener() {
                            @Override
                            public void loadComplete(int nbPages) {
                                myPreferences.setBookBytes(configuration.getS());
                                //Log.d("dwefrwerfewr",configuration.getS());
                            }
                        }).scrollHandle(new DefaultScrollHandle(context));
                pdfView.setConfigurator(configuration).load();


            }
            else {
                Toast.makeText(context,"error",Toast.LENGTH_LONG).show();
            }







        }

        @Override
        public void onPageChanged(int page, int pageCount) {

        }
    }






}