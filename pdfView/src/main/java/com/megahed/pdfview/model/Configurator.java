package com.megahed.pdfview.model;

import android.graphics.Color;
import android.net.Uri;

import com.megahed.pdfview.enms.FitPolicy;
import com.megahed.pdfview.listener.OnDrawListener;
import com.megahed.pdfview.listener.OnErrorListener;
import com.megahed.pdfview.listener.OnLoadCompleteListener;
import com.megahed.pdfview.listener.OnLongPressListener;
import com.megahed.pdfview.listener.OnPageChangeListener;
import com.megahed.pdfview.listener.OnPageErrorListener;
import com.megahed.pdfview.listener.OnPageScrollListener;
import com.megahed.pdfview.listener.OnRenderListener;
import com.megahed.pdfview.listener.OnScrollListener;
import com.megahed.pdfview.listener.OnTapListener;
import com.megahed.pdfview.listener.OnZoomChangeListener;
import com.megahed.pdfview.scroll.ScrollHandle;
import com.megahed.pdfview.source.AssetSource;
import com.megahed.pdfview.source.ByteArraySource;
import com.megahed.pdfview.source.DocumentSource;
import com.megahed.pdfview.source.FileSource;
import com.megahed.pdfview.source.InputStreamSource;
import com.megahed.pdfview.source.UriSource;

import java.io.File;
import java.io.InputStream;

public class Configurator {

    private  DocumentSource documentSource;

    private int[] pageNumbers = null;

    private boolean enableSwipe = true;

    private boolean enableDoubletap = true;

    private OnDrawListener onDrawListener;

    private OnDrawListener onDrawAllListener;

    private OnLoadCompleteListener onLoadCompleteListener;

    private OnErrorListener onErrorListener;

    private OnPageChangeListener onPageChangeListener;

    private OnScrollListener onScrollListener;

    private OnZoomChangeListener onZoomChangeListener;

    private OnPageScrollListener onPageScrollListener;

    private OnRenderListener onRenderListener;

    private OnTapListener onTapListener;

    private OnLongPressListener onLongPressListener;

    private OnPageErrorListener onPageErrorListener;


    private int defaultPage = 0;

    private int backgroundColor = Color.GRAY;

    private boolean landscapeOrientation = false;

    private boolean dualPageMode = false;

    private boolean hasCover = false;

    private boolean swipeHorizontal = false;

    private boolean annotationRendering = false;

    private String password = null;

    private ScrollHandle scrollHandle = null;

    private boolean antialiasing = true;

    private int spacing = 5;

    private boolean autoSpacing = false;

    //new
    private boolean autoReleasingWhenDetachedFromWindow = true;

    private FitPolicy pageFitPolicy = FitPolicy.WIDTH;

    private boolean fitEachPage = false;

    private boolean pageFling = false;

    private boolean pageSnap = false;

    private boolean nightMode = false;

    private boolean disableLongpress=false;

    private long duration=400;

    //private PDFView pdfView;

   /* public Configurator(PDFView pdfView) {
       // this.pdfView = pdfView;
        linkHandler = new DefaultLinkHandler(pdfView);
    }*/

    /*public Configurator(DocumentSource documentSource) {
        this.documentSource = documentSource;
        //linkHandler = new DefaultLinkHandler(pdfView);
    }*/


    public Configurator() {

    }

    private void Configurator(DocumentSource documentSource) {
        this.documentSource = documentSource;
    }


    /** Use an asset file as the pdf source */
    public Configurator fromAsset(String assetName) {
          Configurator(new AssetSource(assetName));
          return this;
    }

    /** Use a file as the pdf source */
    public Configurator fromFile(File file) {
       Configurator(new FileSource(file));
        return this;
    }

    /** Use URI as the pdf source, for use with content providers */
    public Configurator fromUri(Uri uri) {
        Configurator(new UriSource(uri));
        return this;
    }

    /** Use bytearray as the pdf source, documents is not saved */
    public Configurator fromBytes(byte[] bytes) {
       Configurator(new ByteArraySource(bytes));
        return this;
    }

    /**
     * Use stream as the pdf source. Stream will be written to bytearray, because native code does not
     * support Java Streams
     */
    String s;
    InputStreamSource inputStreamSource;
    public Configurator fromStream(InputStream stream) {
        inputStreamSource=new InputStreamSource(stream,s);
        Configurator(inputStreamSource);
        return this;
    }
    public String getS() {
        return inputStreamSource.getS();
    }

    /** Use custom source as pdf source */
    public Configurator fromSource(DocumentSource docSource) {
        Configurator(docSource);
        return this;
    }



    public Configurator pages(int... pageNumbers) {
        this.pageNumbers = pageNumbers;
        return this;
    }

    public Configurator enableSwipe(boolean enableSwipe) {
        this.enableSwipe = enableSwipe;
        return this;
    }

    public Configurator enableDoubletap(boolean enableDoubletap) {
        this.enableDoubletap = enableDoubletap;
        return this;
    }

    public Configurator enableAnnotationRendering(boolean annotationRendering) {
        this.annotationRendering = annotationRendering;
        return this;
    }

    public Configurator onDraw(OnDrawListener onDrawListener) {
        this.onDrawListener = onDrawListener;
        return this;
    }

    public Configurator onDrawAll(OnDrawListener onDrawAllListener) {
        this.onDrawAllListener = onDrawAllListener;
        return this;
    }

    public Configurator onLoad(OnLoadCompleteListener onLoadCompleteListener) {
        this.onLoadCompleteListener = onLoadCompleteListener;
        return this;
    }

    public Configurator onPageScroll(OnPageScrollListener onPageScrollListener) {
        this.onPageScrollListener = onPageScrollListener;
        return this;
    }

    public Configurator onError(OnErrorListener onErrorListener) {
        this.onErrorListener = onErrorListener;
        return this;
    }

    public Configurator onPageError(OnPageErrorListener onPageErrorListener) {
        this.onPageErrorListener = onPageErrorListener;
        return this;
    }

    public Configurator onPageChange(OnPageChangeListener onPageChangeListener) {
        this.onPageChangeListener = onPageChangeListener;
        return this;
    }


    public Configurator onScroll(OnScrollListener onScrollListener) {
        this.onScrollListener = onScrollListener;
        return this;
    }

    public Configurator onZoomChange(OnZoomChangeListener onZoomChangeListener) {
        this.onZoomChangeListener = onZoomChangeListener;
        return this;
    }

    public Configurator onRender(OnRenderListener onRenderListener) {
        this.onRenderListener = onRenderListener;
        return this;
    }

    public Configurator onTap(OnTapListener onTapListener) {
        this.onTapListener = onTapListener;
        return this;
    }

    public Configurator onLongPress(OnLongPressListener onLongPressListener) {
        this.onLongPressListener = onLongPressListener;
        return this;
    }



    public Configurator defaultPage(int defaultPage) {
        this.defaultPage = defaultPage;
        return this;
    }

    public Configurator landscapeOrientation(boolean landscapeOrientation) {
        this.landscapeOrientation = landscapeOrientation;
        return this;
    }

    public Configurator backgroundColor(int color) {
        this.backgroundColor = color;
        return this;
    }

    public Configurator dualPageMode(boolean dualPageMode) {
        this.dualPageMode = dualPageMode;
        return this;
    }

    public Configurator displayAsBook(boolean hasCover) {
        this.hasCover = hasCover;
        return this;
    }

    public Configurator swipeHorizontal(boolean swipeHorizontal) {
        this.swipeHorizontal = swipeHorizontal;
        return this;
    }

    public Configurator password(String password) {
        this.password = password;
        return this;
    }

    public Configurator scrollHandle(ScrollHandle scrollHandle) {
        this.scrollHandle = scrollHandle;
        return this;
    }

    public Configurator enableAntialiasing(boolean antialiasing) {
        this.antialiasing = antialiasing;
        return this;
    }

    public Configurator spacing(int spacing) {
        this.spacing = spacing;
        return this;
    }

    public Configurator autoSpacing(boolean autoSpacing) {
        this.autoSpacing = autoSpacing;
        return this;
    }

    public Configurator autoReleasingWhenDetachedFromWindow(boolean autoReleasing){
        this.autoReleasingWhenDetachedFromWindow = autoReleasing;
        return this;
    }

    public Configurator pageFitPolicy(FitPolicy pageFitPolicy) {
        this.pageFitPolicy = pageFitPolicy;
        return this;
    }

    public Configurator fitEachPage(boolean fitEachPage) {
        this.fitEachPage = fitEachPage;
        return this;
    }

    public Configurator pageSnap(boolean pageSnap) {
        this.pageSnap = pageSnap;
        return this;
    }

    public Configurator pageFling(boolean pageFling) {
        this.pageFling = pageFling;
        return this;
    }

    public Configurator nightMode(boolean nightMode) {
        this.nightMode = nightMode;
        return this;
    }

    public Configurator disableLongpress(boolean disableLongpress) {
        this.disableLongpress=disableLongpress;
      //this.dragPinchManager.disableLongpress();
        return this;
    }

    public Configurator animationDuration(long duration){
        this.duration=duration;
        //animationManager.setAnimationDuration(duration);
        return this;
    }


    public DocumentSource getDocumentSource() {
        return documentSource;
    }

    public int[] getPageNumbers() {
        return pageNumbers;
    }

    public boolean isEnableSwipe() {
        return enableSwipe;
    }

    public boolean isEnableDoubletap() {
        return enableDoubletap;
    }

    public OnDrawListener getOnDrawListener() {
        return onDrawListener;
    }

    public OnDrawListener getOnDrawAllListener() {
        return onDrawAllListener;
    }

    public OnLoadCompleteListener getOnLoadCompleteListener() {
        return onLoadCompleteListener;
    }

    public OnErrorListener getOnErrorListener() {
        return onErrorListener;
    }

    public OnPageChangeListener getOnPageChangeListener() {
        return onPageChangeListener;
    }

    public OnScrollListener getOnScrollListener() {
        return onScrollListener;
    }

    public OnZoomChangeListener getOnZoomChangeListener() {
        return onZoomChangeListener;
    }

    public OnPageScrollListener getOnPageScrollListener() {
        return onPageScrollListener;
    }

    public OnRenderListener getOnRenderListener() {
        return onRenderListener;
    }

    public OnTapListener getOnTapListener() {
        return onTapListener;
    }

    public OnLongPressListener getOnLongPressListener() {
        return onLongPressListener;
    }

    public OnPageErrorListener getOnPageErrorListener() {
        return onPageErrorListener;
    }

   /* public LinkHandler getLinkHandler() {
        return linkHandler;
    }*/

    public int getDefaultPage() {
        return defaultPage;
    }

    public int getBackgroundColor() {
        return backgroundColor;
    }

    public boolean isLandscapeOrientation() {
        return landscapeOrientation;
    }

    public boolean isDualPageMode() {
        return dualPageMode;
    }

    public boolean isHasCover() {
        return hasCover;
    }

    public boolean isSwipeHorizontal() {
        return swipeHorizontal;
    }

    public boolean isAnnotationRendering() {
        return annotationRendering;
    }

    public String getPassword() {
        return password;
    }

    public ScrollHandle getScrollHandle() {
        return scrollHandle;
    }

    public boolean isAntialiasing() {
        return antialiasing;
    }

    public int getSpacing() {
        return spacing;
    }

    public boolean isAutoSpacing() {
        return autoSpacing;
    }

    public boolean isAutoReleasingWhenDetachedFromWindow() {
        return autoReleasingWhenDetachedFromWindow;
    }

    public FitPolicy getPageFitPolicy() {
        return pageFitPolicy;
    }

    public boolean isFitEachPage() {
        return fitEachPage;
    }

    public boolean isPageFling() {
        return pageFling;
    }

    public boolean isPageSnap() {
        return pageSnap;
    }

    public boolean isNightMode() {
        return nightMode;
    }

    public boolean isDisableLongpress() {
        return disableLongpress;
    }

    public long getDuration() {
        return duration;
    }

    /*public PDFView getPdfView() {
        return pdfView;
    }*/
}