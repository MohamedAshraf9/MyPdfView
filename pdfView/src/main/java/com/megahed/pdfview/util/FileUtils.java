package com.megahed.pdfview.util;

import android.content.Context;
import android.os.ParcelFileDescriptor;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;

public class FileUtils {

    private static final Class FD_CLASS = FileDescriptor.class;

    private static Field mFdField = null;

    public static int getNumFd(ParcelFileDescriptor fdObj) {
        try {
            if (mFdField == null) {
                mFdField = FD_CLASS.getDeclaredField("descriptor");
                mFdField.setAccessible(true);
            }

            return mFdField.getInt(fdObj.getFileDescriptor());
        } catch (ReflectiveOperationException e) {
            e.printStackTrace();
            return -1;
        }
    }


    private FileUtils() {
        // Prevents instantiation
    }

    public static File fileFromAsset(Context context, String assetName) throws IOException {
        File outFile = new File(context.getCacheDir(), assetName + "-pdfview.pdf");
        if (assetName.contains("/")) {
            outFile.getParentFile().mkdirs();
        }
        copy(context.getAssets().open(assetName), outFile);
        return outFile;
    }

    public static void copy(InputStream inputStream, File output) throws IOException {
        OutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(output);
            int read = 0;
            byte[] bytes = new byte[1024];
            while ((read = inputStream.read(bytes)) != -1) {
                outputStream.write(bytes, 0, read);
            }
        } finally {
            try {
                if (inputStream != null) {
                    inputStream.close();
                }
            } finally {
                if (outputStream != null) {
                    outputStream.close();
                }
            }
        }
    }

}
