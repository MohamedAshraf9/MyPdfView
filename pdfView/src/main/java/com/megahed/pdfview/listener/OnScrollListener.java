package com.megahed.pdfview.listener;

public interface OnScrollListener {

    /**
     * Called on every move while scrolling
     *
     * @param x for horizontal offset
     * @param y for vertical offset
     */
    void onScroll(float x, float y);
}