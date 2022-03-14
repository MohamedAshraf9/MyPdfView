package com.megahed.pdfview.listener;

public interface OnZoomChangeListener {
 /**
 * Called when the user use zoom action in page
 *
 * @param zoom      the new zoom
 */
    void onZoomChanged(double zoom);
}
