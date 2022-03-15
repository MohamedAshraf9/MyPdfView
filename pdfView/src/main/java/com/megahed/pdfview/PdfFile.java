/**
 * Copyright 2017 Bartosz Schiller
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.megahed.pdfview;

import android.graphics.Bitmap;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import android.util.SparseBooleanArray;

import com.megahed.pdfview.enms.FitPolicy;
import com.megahed.pdfview.exception.PageRenderingException;
import com.megahed.pdfview.search.TextSearchContext;
import com.megahed.pdfview.util.PageSizeCalculator;
import com.megahed.pdfview.util.Size;
import com.megahed.pdfview.util.SizeF;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

public class PdfFile {

  private static final Object lock = new Object();
  private PdfiumSDK pdfiumCore;
  private int pagesCount = 0;
  /** Original page sizes */
  private List<Size> originalPageSizes = new ArrayList<>();
  /** Scaled page sizes */
  private List<SizeF> pageSizes = new ArrayList<>();
  /** Opened pages with indicator whether opening was successful */
  private SparseBooleanArray openedPages = new SparseBooleanArray();
  /** Page with maximum width */
  private Size originalMaxWidthPageSize = new Size(0, 0);
  /** Page with maximum height */
  private Size originalMaxHeightPageSize = new Size(0, 0);
  /** Scaled page with maximum height */
  private SizeF maxHeightPageSize = new SizeF(0, 0);
  /** Scaled page with maximum width */
  private SizeF maxWidthPageSize = new SizeF(0, 0);
  /** True if dualPageMode is on */
  private boolean showTwoPages;

  /** true if show cover is on * */
  private boolean showCover;

  /** True if scrolling is vertical, else it's horizontal */
  private boolean isVertical;
  /** Fixed spacing between pages in pixels */
  private int spacingPx;
  /** Calculate spacing automatically so each page fits on it's own in the center of the view */
  private boolean autoSpacing;
  /** Calculated offsets for pages */
  private List<Float> pageOffsets = new ArrayList<>();
  /** Calculated auto spacing for pages */
  private List<Float> pageSpacing = new ArrayList<>();
  /** Calculated document length (width or height, depending on swipe mode) */
  private float documentLength = 0;

  private final FitPolicy pageFitPolicy;
  /**
   * True if every page should fit separately according to the FitPolicy, else the largest page fits
   * and other pages scale relatively
   */
  private final boolean fitEachPage;

  private final boolean isLandscape;
  /** The pages the user want to display in order (ex: 0, 2, 2, 8, 8, 1, 1, 1) */
  private int[] originalUserPages;

  PdfFile(PdfiumSDK pdfiumCore, FitPolicy pageFitPolicy, Size viewSize, int[] originalUserPages, boolean showTwoPages,
          boolean showCover, boolean isVertical, int spacing, boolean autoSpacing, boolean fitEachPage, boolean isLandscape) {
    this.showTwoPages = showTwoPages;
    this.showCover = showCover;
    this.pdfiumCore = pdfiumCore;
    this.pageFitPolicy = pageFitPolicy;
    this.originalUserPages = originalUserPages;
    this.isVertical = isVertical;
    this.spacingPx = spacing;
    this.autoSpacing = autoSpacing;
    this.fitEachPage = fitEachPage;
    this.isLandscape = isLandscape;
    setup(viewSize);
  }

  private void setup(Size viewSize) {
    if (originalUserPages != null) {
      pagesCount = originalUserPages.length;
    } else {
      pagesCount = pdfiumCore.getPageCount();
    }

    for (int i = 0; i < pagesCount; i++) {
      Size pageSize = pdfiumCore.getPageSize(documentPage(i));
      if (pageSize.getWidth() > originalMaxWidthPageSize.getWidth()) {
        originalMaxWidthPageSize = pageSize;
      }
      if (pageSize.getHeight() > originalMaxHeightPageSize.getHeight()) {
        originalMaxHeightPageSize = pageSize;
      }
      originalPageSizes.add(pageSize);
    }

    recalculatePageSizes(viewSize);
  }

  /**
   * Call after view size change to recalculate page sizes, offsets and document length
   *
   * @param viewSize new size of changed view
   */
  public void recalculatePageSizes(Size viewSize) {
    pageSizes.clear();
    PageSizeCalculator calculator =
        new PageSizeCalculator(
            pageFitPolicy,
            originalMaxWidthPageSize,
            originalMaxHeightPageSize,
            viewSize,
            fitEachPage);
    maxWidthPageSize = calculator.getOptimalMaxWidthPageSize();
    maxHeightPageSize = calculator.getOptimalMaxHeightPageSize();
    for (Size size : originalPageSizes) {
      //pageSizes.add(calculator.calculate(size, showTwoPages, isLandscape, originalPageSizes.indexOf(size)));
      pageSizes.add(calculator.calculate(size));
    }
    if (autoSpacing || showTwoPages) {
      prepareAutoSpacing(viewSize);
    }
    prepareDocLen();
    preparePagesOffset();
  }

  public int getPagesCount() {
    return pagesCount;
  }

  public SizeF getPageSize(int pageIndex) {
    int docPage = documentPage(pageIndex);
    if (docPage < 0) {
      return new SizeF(0, 0);
    }
    return pageSizes.get(pageIndex);
  }

  public SizeF getScaledPageSize(int pageIndex, float zoom) {
    SizeF size = getPageSize(pageIndex);
    return new SizeF(size.getWidth() * zoom, size.getHeight() * zoom);
  }

  /**
   * get page size with biggest dimension (width in vertical mode and height in horizontal mode)
   *
   * @return size of page
   */
  public SizeF getMaxPageSize() {
    return isVertical ? maxWidthPageSize : maxHeightPageSize;
  }

  public float getMaxPageWidth() {
    return getMaxPageSize().getWidth();
  }

  public float getMaxPageHeight() {
    return getMaxPageSize().getHeight();
  }

  private void prepareAutoSpacing(Size viewSize) {
    pageSpacing.clear();
    SizeF pageSize;
    float spacing;
    for (int i = 0; i < getPagesCount(); i++) {
      pageSize = pageSizes.get(i);
      spacing =
          Math.max(
              0,
              isVertical
                  ? viewSize.getHeight() - pageSize.getHeight()
                  : viewSize.getWidth() - pageSize.getWidth());
      if (i < getPagesCount() - 1) {
        spacing += spacingPx;
      }
      if (showTwoPages && showCover && (i == 0 || i == getPagesCount() - 1)) {
        pageSpacing.add(spacing);
      } else if (showTwoPages && i == 0) {
        pageSpacing.add(spacing / 1.5f);
      } else if (showTwoPages){
        if(showCover && i % 2 != 0){
          pageSpacing.add(spacing / 1.5f);
        }else {
          pageSpacing.add(1.5f);
        }
      } else {
        pageSpacing.add(spacing);
      }
    }
  }

  private void prepareDocLen() {
    float length = 0;
    for (int i = 0; i < getPagesCount(); i++) {
      SizeF pageSize = pageSizes.get(i);
      length += isVertical ? pageSize.getHeight() : pageSize.getWidth();
      if (autoSpacing || showTwoPages) {
        length += pageSpacing.get(i);
      } else if (i < getPagesCount() - 1) {
        length += spacingPx;
      }
    }
    documentLength = length;
  }

  private void preparePagesOffset() {
    pageOffsets.clear();
    float offset = 0;
    for (int i = 0; i < getPagesCount(); i++) {
      SizeF pageSize = pageSizes.get(i);
      float size = isVertical ? pageSize.getHeight() : pageSize.getWidth();
      if (autoSpacing || showTwoPages) {
        offset += pageSpacing.get(i) / 2f;
        if (i == 0 && showCover) {
          offset -= spacingPx / 2f;
        } else if (i == getPagesCount() - 1 && showCover) {
          offset += spacingPx / 2f;
        }
        pageOffsets.add(offset);
        if((!showCover && i == 0) || (showCover && i%2 != 0)) {
          offset += size;
        }else {
          offset += size + pageSpacing.get(i) / 2f;
        }

      } else {
        pageOffsets.add(offset);
        offset += size + spacingPx;
      }
    }
  }

  public float getDocLen(float zoom) {
    return documentLength * zoom;
  }

  /** Get the page's height if swiping vertical, or width if swiping horizontal. */
  public float getPageLength(int pageIndex, float zoom) {
    SizeF size = getPageSize(pageIndex);
    SizeF size2 = getPageSize(pageIndex+1);
    return (isVertical ? size.getHeight() : size.getWidth()) * zoom;
  }

  public float getPageSpacing(int pageIndex, float zoom) {
    float spacing = autoSpacing ? pageSpacing.get(pageIndex) : spacingPx;
    return spacing * zoom;
  }

  /** Get primary page offset, that is Y for vertical scroll and X for horizontal scroll */
  public float getPageOffset(int pageIndex, float zoom) {
    int docPage = documentPage(pageIndex);
    if (docPage < 0) {
      return 0;
    }
    return pageOffsets.get(pageIndex) * zoom;
  }

  /** Get secondary page offset, that is X for vertical scroll and Y for horizontal scroll */
  public float getSecondaryPageOffset(int pageIndex, float zoom) {
    SizeF pageSize = getPageSize(pageIndex);
    if (isVertical) {
      float maxWidth = getMaxPageWidth();
      return zoom * (maxWidth - pageSize.getWidth()) / 2; // x
    } else {
      float maxHeight = getMaxPageHeight();
      return zoom * (maxHeight - pageSize.getHeight()) / 2; // y
    }
  }

  public int getPageAtOffset(float offset, float zoom) {
    int currentPage = 0;
    for (int i = 0; i < getPagesCount(); i++) {
      float off = pageOffsets.get(i) * zoom - getPageSpacing(i, zoom) / 2f;
      if (off >= offset) {
        break;
      }
      currentPage++;
    }
    return --currentPage >= 0 ? currentPage : 0;
  }

  public boolean openPage(int pageIndex) throws PageRenderingException {
    int docPage = documentPage(pageIndex);
    if (docPage < 0) {
      return false;
    }

    synchronized (lock) {
      if (openedPages.indexOfKey(docPage) < 0) {
        try {
          pdfiumCore.openPage(docPage);
          openedPages.put(docPage, true);
          return true;
        } catch (Exception e) {
          openedPages.put(docPage, false);
          throw new PageRenderingException(pageIndex, e);
        }
      }
      return false;
    }
  }

  public boolean pageHasError(int pageIndex) {
    int docPage = documentPage(pageIndex);
    return !openedPages.get(docPage, false);
  }

  public void renderPageBitmap(
      Bitmap bitmap, int pageIndex, Rect bounds, boolean annotationRendering) {
    int docPage = documentPage(pageIndex);
    pdfiumCore.renderPageBitmap(
        bitmap,
        docPage,
        bounds.left,
        bounds.top,
        bounds.width(),
        bounds.height(),
        annotationRendering);
  }

  public Meta getMetaData() {
    if (pdfiumCore.getDocumentMeta() == null) {
      return null;
    }
    else return pdfiumCore.getDocumentMeta();
  }

  public List<Bookmark> getBookmarks() {
    if (pdfiumCore.getTableOfContents() == null) {
      return new ArrayList<>();
    }
    else return pdfiumCore.getTableOfContents();
  }

  public List<Link> getPageLinks(int pageIndex) {
    int docPage = documentPage(pageIndex);
    return pdfiumCore.getPageLinks( docPage);
  }

  public RectF mapRectToDevice(int pageIndex, int startX, int startY, int sizeX, int sizeY,
                               RectF rect) {
    int docPage = documentPage(pageIndex);
    return pdfiumCore.mapPageCoordinateToDevice( docPage, startX, startY, sizeX, sizeY, 0, rect);
  }

  public void dispose() {
    if (pdfiumCore != null) {
      pdfiumCore.closeDocument();
    }
    originalUserPages = null;
  }

  /**
   * Given the UserPage number, this method restrict it to be sure it's an existing page. It takes
   * care of using the user defined pages if any.
   *
   * @param userPage A page number.
   * @return A restricted valid page number (example : -2 => 0)
   */
  public int determineValidPageNumberFrom(int userPage) {
    if (userPage <= 0) {
      return 0;
    }
    if (originalUserPages != null) {
      if (userPage >= originalUserPages.length) {
        return originalUserPages.length - 1;
      }
    } else {
      if (userPage >= getPagesCount()) {
        return getPagesCount() - 1;
      }
    }
    return userPage;
  }

  public int documentPage(int userPage) {
    int documentPage = userPage;
    if (originalUserPages != null) {
      if (userPage < 0 || userPage >= originalUserPages.length) {
        return -1;
      } else {
        documentPage = originalUserPages[userPage];
      }
    }

    if (documentPage < 0 || userPage >= getPagesCount()) {
      return -1;
    }

    return documentPage;
  }



  //from here


  public String getPageText(int pageIndex){

    return pdfiumCore.extractCharacters(pageIndex,0,getCharNum(pageIndex));
  }
  public int getCharNum(int pageIndex){
    return pdfiumCore.countCharactersOnPage(pageIndex);
  }

  public boolean loadText(int pageIndex) {
    synchronized (lock) {
      if (pdfiumCore != null) {
        boolean shouldClose = !pageHasError(pageIndex);

        tid= pdfiumCore.prepareTextInfo(pageIndex);

        return shouldClose;
      }
      return false;
    }

  }



  long tid;
  String allText;



  public TextSearchContext search(int pin, String string, boolean matchCase, boolean matchWholeWord){

   return pdfiumCore.newPageSearch(pin,string,matchCase,matchWholeWord);
  }




  static class AnnotShape {
    int index;
    RectF box;
    QuadShape[] attachPts;

    public AnnotShape(RectF rect, int i) {
      box = rect;
      index=i;
    }
  }

  static class QuadShape {
    PointF p1=new PointF();
    PointF p2=new PointF();
    PointF p3=new PointF();
    PointF p4=new PointF();
  }

  boolean IsPointInMatrix(PointF p, PointF p1, PointF p2, PointF p3, PointF p4) {
    return GetCross(p1, p2, p) * GetCross(p3, p4, p) >= 0 && GetCross(p2, p3, p) * GetCross(p4, p1, p) >= 0;
  }

  float GetCross(PointF p, PointF p1, PointF p2) {
    return (p2.x - p1.x) * (p.y - p1.y) - (p.x - p1.x) * (p2.y - p1.y);
  }







}
