package com.megahed.pdfview.search;

import android.graphics.Rect;

public interface SearchHandler {
    int getStartIndex();

    int getStopIndex();

    Rect[] getResults();

}
