package com.megahed.mypdfview;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.Preference;
import android.preference.PreferenceManager;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Date;

public class MyPreferences extends Preference {

    SharedPreferences preferences;

    public MyPreferences(Context context) {
        super(context);
        preferences= PreferenceManager.getDefaultSharedPreferences(context);
    }

    public int getBookDefaultPage(String bookId){
        return preferences.getInt(bookId,0);
    }

    @SuppressLint("CommitPrefEdits")
    public void setBookDefaultPage(String bookId, int page){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putInt(bookId,page);
        editor.apply();
    }

    public void setCacheBooksEnable(){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("isCacheExistToday",new Date().getTime());
        editor.apply();
    }

    public long getCacheBooksEnable(){
        return preferences.getLong("isCacheExistToday",0L);
    }



    public void setCacheUniversityEnable(){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("isCacheUniversity",new Date().getTime());
        editor.apply();
    }

    public long getCacheUniversityEnable(){
        return preferences.getLong("isCacheUniversity",0L);
    }


    public void setCacheFacultyEnable(String universityId){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("isCacheFaculty"+universityId,new Date().getTime());
        editor.apply();
    }

    public long getCacheFacultyEnable(String universityId){
        return preferences.getLong("isCacheFaculty"+universityId,0L);
    }

    public void setCacheDepartmentEnable(String FacultyId){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("isCacheDepartment"+FacultyId,new Date().getTime());
        editor.apply();
    }

    public long getCacheDepartmentEnable(String FacultyId){
        return preferences.getLong("isCacheDepartment"+FacultyId,0L);
    }


    public void setCacheLevelDegreeEnable(){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("isCacheLevelDegree",new Date().getTime());
        editor.apply();
    }

    public long getCacheLevelDegreeEnable(){
        return preferences.getLong("isCacheLevelDegree",0L);
    }

    public void setCacheTermEnable(){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putBoolean("isCacheTerm",true);
        editor.apply();
    }

    public Boolean getCacheTermEnable(){
        return preferences.getBoolean("isCacheTerm",false);
    }

    public void setCacheFileTypeEnable(){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("isCacheFileType",new Date().getTime());
        editor.apply();
    }

    public Long getCacheFileTypeEnable(){
        return preferences.getLong("isCacheFileType",0L);
    }


    public void setLastUpdateTime(long timeMilli){
        SharedPreferences.Editor editor= preferences.edit();
        editor.putLong("LastUpdateTime",timeMilli);
        editor.apply();
    }

    public Long getLastUpdateTime(){
        return preferences.getLong("LastUpdateTime",0L);
    }


    public String getBookBytes(){

        return preferences.getString("bookId"," ");
    }

    @SuppressLint("CommitPrefEdits")
    public void setBookBytes(String s){


        SharedPreferences.Editor editor= preferences.edit();
        editor.putString("bookId",s);
        editor.apply();
    }


    public static byte[] readAllBytes(InputStream inputStream) throws IOException {
        final int bufLen = 4 * 0x400; // 4KB
        byte[] buf = new byte[bufLen];
        int readLen;
        IOException exception = null;

        try {
            try (ByteArrayOutputStream outputStream = new ByteArrayOutputStream()) {
                while ((readLen = inputStream.read(buf, 0, bufLen)) != -1)
                    outputStream.write(buf, 0, readLen);

                return outputStream.toByteArray();
            }
        } catch (IOException e) {
            exception = e;
            throw e;
        } finally {
            if (exception == null) inputStream.close();
            else try {
                inputStream.close();
            } catch (IOException e) {
                exception.addSuppressed(e);
            }
        }
    }

    public static String byteArrayToString(byte[] data){
        String response = Arrays.toString(data);

        String[] byteValues = response.substring(1, response.length() - 1).split(",");
        byte[] bytes = new byte[byteValues.length];

        for (int i=0, len=bytes.length; i<len; i++) {
            bytes[i] = Byte.parseByte(byteValues[i].trim());
        }

        String str = new String(bytes);
        return str.toLowerCase();
    }



    public void getAsByteArray(URL url,String bookId) throws IOException {
        URLConnection connection = url.openConnection();
        // Since you get a URLConnection, use it to get the InputStream
        InputStream in = connection.getInputStream();
        // Now that the InputStream is open, get the content length
        int contentLength = connection.getContentLength();

        // To avoid having to resize the array over and over and over as
        // bytes are written to the array, provide an accurate estimate of
        // the ultimate size of the byte array
        ByteArrayOutputStream tmpOut;
        if (contentLength != -1) {
            tmpOut = new ByteArrayOutputStream(contentLength);
        } else {
            tmpOut = new ByteArrayOutputStream(16384); // Pick some appropriate size
        }

        byte[] buf = new byte[512];
        while (true) {
            int len = in.read(buf);
            if (len == -1) {
                break;
            }
            tmpOut.write(buf, 0, len);
        }
        in.close();
        tmpOut.flush();
        tmpOut.close(); // No effect, but good to do anyway to keep the metaphor alive

        //Lines below used to test if file is corrupt
        //FileOutputStream fos = new FileOutputStream("C:\\abc.pdf");
        //fos.write(array);
        //fos.close();
        String s= tmpOut.toString();
        SharedPreferences.Editor editor= preferences.edit();
        editor.putString(bookId,s);
        editor.apply();


    }



}
