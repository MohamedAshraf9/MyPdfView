//
// Created by 夏森海 on 2018/6/20.
//

#include <jni.h>
#include <string>
#include <vector>
#include <cwchar>
#include <memory>
#include <fstream>
#include <map>

extern "C" {
#include <stdlib.h>
}

#include <android/log.h>

#define JNI_FUNC(retType, bindClass, name)  JNIEXPORT retType JNICALL Java_org_benjinus_pdfium_##bindClass##_##name
#define JNI_ARGS    JNIEnv *env, jobject thiz

#define LOG_TAG "PDFSDK"
#define LOGI(...)   __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)   __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...)   __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-command-line-argument"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"

extern "C" {
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <Mutex.h>
#include <public/fpdfview.h>
#include <public/fpdf_doc.h>
#include <public/fpdf_edit.h>
#include <public/fpdf_text.h>
#include <public/fpdf_annot.h>
#include <public/cpp/fpdf_scopers.h>
}

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/bitmap.h>
#include <public/fpdf_save.h>

using namespace android;

static Mutex sLibraryLock;
static int sLibraryReferenceCount = 0;

static void initLibraryIfNeed() {
    Mutex::Autolock lock(sLibraryLock);
    if (sLibraryReferenceCount == 0) {
        FPDF_InitLibrary();
        LOGI("PDFSDK Library Initialized!");
    }
    sLibraryReferenceCount++;
}

static void destroyLibraryIfNeed() {
    Mutex::Autolock lock(sLibraryLock);
    sLibraryReferenceCount--;
    if (sLibraryReferenceCount == 0) {
        FPDF_DestroyLibrary();
        LOGI("PDFSDK Instance Destroyed!");
    }
}

struct rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

class DocumentFile {

public:
    FPDF_DOCUMENT pdfDocument = NULL;

    DocumentFile() { initLibraryIfNeed(); }

    ~DocumentFile();
};

DocumentFile::~DocumentFile() {
    if (pdfDocument != NULL) {
        FPDF_CloseDocument(pdfDocument);
    }

    destroyLibraryIfNeed();
}

template<class string_type>
inline typename string_type::value_type *WriteInto(string_type *str, size_t length_with_null) {
    str->reserve(length_with_null);
    str->resize(length_with_null - 1);
    return &((*str)[0]);
}

inline long getFileSize(int fd) {
    struct stat file_state;

    if (fstat(fd, &file_state) >= 0) {
        return (long) (file_state.st_size);
    } else {
        LOGE("Error getting file size");
        return 0;
    }
}

static char *getErrorDescription(const long error) {
    char *description = NULL;
    switch (error) {
        case FPDF_ERR_SUCCESS:
            asprintf(&description, "No error.");
            break;
        case FPDF_ERR_FILE:
            asprintf(&description, "File not found or could not be opened.");
            break;
        case FPDF_ERR_FORMAT:
            asprintf(&description, "File not in PDF format or corrupted.");
            break;
        case FPDF_ERR_PASSWORD:
            asprintf(&description, "Incorrect password.");
            break;
        case FPDF_ERR_SECURITY:
            asprintf(&description, "Unsupported security scheme.");
            break;
        case FPDF_ERR_PAGE:
            asprintf(&description, "Page not found or content error.");
            break;
        default:
            asprintf(&description, "Unknown error.");
    }

    return description;
}

int jniThrowException(JNIEnv *env, const char *className, const char *message) {
    jclass exClass = env->FindClass(className);
    if (exClass == NULL) {
        LOGE("Unable to find exception class %s", className);
        return -1;
    }

    if (env->ThrowNew(exClass, message) != JNI_OK) {
        LOGE("Failed throwing '%s' '%s'", className, message);
        return -1;
    }

    return 0;
}

int jniThrowExceptionFmt(JNIEnv *env, const char *className, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msgBuf[512];
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    return jniThrowException(env, className, msgBuf);
    va_end(args);
}

jobject NewLong(JNIEnv *env, jlong value) {
    jclass cls = env->FindClass("java/lang/Long");
    jmethodID methodID = env->GetMethodID(cls, "<init>", "(J)V");
    return env->NewObject(cls, methodID, value);
}

jobject NewInteger(JNIEnv *env, jint value) {
    jclass cls = env->FindClass("java/lang/Integer");
    jmethodID methodID = env->GetMethodID(cls, "<init>", "(I)V");
    return env->NewObject(cls, methodID, value);
}

uint16_t rgb_to_565(unsigned char R8, unsigned char G8, unsigned char B8) {
    unsigned char R5 = (R8 * 249 + 1014) >> 11;
    unsigned char G6 = (G8 * 253 + 505) >> 10;
    unsigned char B5 = (B8 * 249 + 1014) >> 11;
    return (R5 << 11) | (G6 << 5) | (B5);
}

void rgbBitmapTo565(void *source, int sourceStride, void *dest, AndroidBitmapInfo *info) {
    rgb *srcLine;
    uint16_t *dstLine;
    int y, x;
    for (y = 0; y < info->height; y++) {
        srcLine = (rgb *) source;
        dstLine = (uint16_t *) dest;
        for (x = 0; x < info->width; x++) {
            rgb *r = &srcLine[x];
            dstLine[x] = rgb_to_565(r->red, r->green, r->blue);
        }
        source = (char *) source + sourceStride;
        dest = (char *) dest + info->stride;
    }
}

extern "C" {

static constexpr char kContentsKey[] = "Contents";

static int getBlock(void *param, unsigned long position, unsigned char *outBuffer,
                    unsigned long size) {
    const int fd = reinterpret_cast<intptr_t>(param);
    const int readCount = pread(fd, outBuffer, size, position);
    if (readCount < 0) {
        LOGE("Cannot read from file descriptor.");
        return 0;
    }
    return 1;
}


JNI_FUNC(jlong, PdfiumSDK, nativeOpenDocument)(JNI_ARGS, jint fd, jstring password) {

    size_t fileLength = (size_t) getFileSize(fd);
    if (fileLength <= 0) {
        jniThrowException(env, "java/io/IOException",
                          "File is empty");
        return -1;
    }

    DocumentFile *docFile = new DocumentFile();

    FPDF_FILEACCESS loader;
    loader.m_FileLen = fileLength;
    loader.m_Param = reinterpret_cast<void *>(intptr_t(fd));
    loader.m_GetBlock = &getBlock;

    const char *cpassword = NULL;
    if (password != NULL) {
        cpassword = env->GetStringUTFChars(password, NULL);
    }

    FPDF_DOCUMENT document = FPDF_LoadCustomDocument(&loader, cpassword);

    if (cpassword != NULL) {
        env->ReleaseStringUTFChars(password, cpassword);
    }

    if (!document) {
        delete docFile;

        const long errorNum = FPDF_GetLastError();
        if (errorNum == FPDF_ERR_PASSWORD) {
            jniThrowException(env, "org/benjinus/pdfium/PdfPasswordException",
                              "Password required or incorrect password.");
        } else {
            char *error = getErrorDescription(errorNum);
            jniThrowExceptionFmt(env, "java/io/IOException",
                                 "cannot create document: %s", error);

            free(error);
        }

        return -1;
    }

    docFile->pdfDocument = document;

    return reinterpret_cast<jlong>(docFile);
}

JNI_FUNC(jlong, PdfiumSDK, nativeOpenMemDocument)(JNI_ARGS, jbyteArray data, jstring password) {
    DocumentFile *docFile = new DocumentFile();

    const char *cpassword = NULL;
    if (password != NULL) {
        cpassword = env->GetStringUTFChars(password, NULL);
    }

    jbyte *cData = env->GetByteArrayElements(data, NULL);
    int size = (int) env->GetArrayLength(data);
    jbyte *cDataCopy = new jbyte[size];
    memcpy(cDataCopy, cData, size);
    FPDF_DOCUMENT document = FPDF_LoadMemDocument(reinterpret_cast<const void *>(cDataCopy),
                                                  size, cpassword);
    env->ReleaseByteArrayElements(data, cData, JNI_ABORT);

    if (cpassword != NULL) {
        env->ReleaseStringUTFChars(password, cpassword);
    }

    if (!document) {
        delete docFile;

        const long errorNum = FPDF_GetLastError();
        if (errorNum == FPDF_ERR_PASSWORD) {
            jniThrowException(env, "org/benjinus/pdfium/PdfPasswordException",
                              "Password required or incorrect password.");
        } else {
            char *error = getErrorDescription(errorNum);
            jniThrowExceptionFmt(env, "java/io/IOException",
                                 "cannot create document: %s", error);

            free(error);
        }

        return -1;
    }

    docFile->pdfDocument = document;

    return reinterpret_cast<jlong>(docFile);
}

JNI_FUNC(jint, PdfiumSDK, nativeGetPageCount)(JNI_ARGS, jlong documentPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(documentPtr);
    return (jint) FPDF_GetPageCount(doc->pdfDocument);
}

JNI_FUNC(void, PdfiumSDK, nativeCloseDocument)(JNI_ARGS, jlong documentPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(documentPtr);
    delete doc;
}

static jlong loadPageInternal(JNIEnv *env, DocumentFile *doc, int pageIndex) {
    try {
        if (doc == NULL) throw "Get page document null";

        FPDF_DOCUMENT pdfDoc = doc->pdfDocument;
        if (pdfDoc != NULL) {
            FPDF_PAGE page = FPDF_LoadPage(pdfDoc, pageIndex);
            if (page == NULL) {
                throw "Loaded page is null";
            }
            return reinterpret_cast<jlong>(page);
        } else {
            throw "Get page pdf document null";
        }

    } catch (const char *msg) {
        LOGE("%s", msg);

        jniThrowException(env, "java/lang/IllegalStateException",
                          "cannot load page");

        return -1;
    }
}

static void closePageInternal(jlong pagePtr) {
    FPDF_ClosePage(reinterpret_cast<FPDF_PAGE>(pagePtr));
}

JNI_FUNC(jlong, PdfiumSDK, nativeLoadPage)(JNI_ARGS, jlong docPtr, jint pageIndex) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    return loadPageInternal(env, doc, (int) pageIndex);
}
JNI_FUNC(jlongArray, PdfiumSDK, nativeLoadPages)(JNI_ARGS, jlong docPtr, jint fromIndex,
                                                 jint toIndex) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);

    if (toIndex < fromIndex) return NULL;
    jlong pages[toIndex - fromIndex + 1];

    int i;
    for (i = 0; i <= (toIndex - fromIndex); i++) {
        pages[i] = loadPageInternal(env, doc, (i + fromIndex));
    }

    jlongArray javaPages = env->NewLongArray((jsize) (toIndex - fromIndex + 1));
    env->SetLongArrayRegion(javaPages, 0, (jsize) (toIndex - fromIndex + 1), (const jlong *) pages);

    return javaPages;
}

JNI_FUNC(void, PdfiumSDK, nativeClosePage)(JNI_ARGS, jlong pagePtr) { closePageInternal(pagePtr); }
JNI_FUNC(void, PdfiumSDK, nativeClosePages)(JNI_ARGS, jlongArray pagesPtr) {
    int length = (int) (env->GetArrayLength(pagesPtr));
    jlong *pages = env->GetLongArrayElements(pagesPtr, NULL);

    int i;
    for (i = 0; i < length; i++) { closePageInternal(pages[i]); }
}

JNI_FUNC(jint, PdfiumSDK, nativeGetPageWidthPixel)(JNI_ARGS, jlong pagePtr, jint dpi) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint) (FPDF_GetPageWidth(page) * dpi / 72);
}
JNI_FUNC(jint, PdfiumSDK, nativeGetPageHeightPixel)(JNI_ARGS, jlong pagePtr, jint dpi) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint) (FPDF_GetPageHeight(page) * dpi / 72);
}

JNI_FUNC(jint, PdfiumSDK, nativeGetPageWidthPoint)(JNI_ARGS, jlong pagePtr) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint) FPDF_GetPageWidth(page);
}
JNI_FUNC(jint, PdfiumSDK, nativeGetPageHeightPoint)(JNI_ARGS, jlong pagePtr) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint) FPDF_GetPageHeight(page);
}
JNI_FUNC(jobject, PdfiumSDK, nativeGetPageSizeByIndex)(JNI_ARGS, jlong docPtr, jint pageIndex,
                                                       jint dpi) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    if (doc == NULL) {
        LOGE("Document is null");

        jniThrowException(env, "java/lang/IllegalStateException",
                          "Document is null");
        return NULL;
    }

    double width, height;
    int result = FPDF_GetPageSizeByIndex(doc->pdfDocument, pageIndex, &width, &height);

    if (result == 0) {
        width = 0;
        height = 0;
    }

    jint widthInt = (jint) (width * dpi / 72);
    jint heightInt = (jint) (height * dpi / 72);

    jclass clazz = env->FindClass("com/megahed/pdfview/util/Size");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, widthInt, heightInt);
}

static void renderPageInternal(FPDF_PAGE page,
                               ANativeWindow_Buffer *windowBuffer,
                               int startX, int startY,
                               int canvasHorSize, int canvasVerSize,
                               int drawSizeHor, int drawSizeVer,
                               bool renderAnnot) {

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx(canvasHorSize, canvasVerSize,
                                                FPDFBitmap_BGRA,
                                                windowBuffer->bits,
                                                (int) (windowBuffer->stride) * 4);

    if (drawSizeHor < canvasHorSize || drawSizeVer < canvasVerSize) {
        FPDFBitmap_FillRect(pdfBitmap, 0, 0, canvasHorSize, canvasVerSize,
                            0x848484FF); //Gray
    }

    int flags = FPDF_REVERSE_BYTE_ORDER;

    if (renderAnnot) {
        flags |= FPDF_ANNOT;
    }

    FPDF_RenderPageBitmap(pdfBitmap, page,
                          startX, startY,
                          drawSizeHor, drawSizeVer,
                          0, flags);
}

JNI_FUNC(void, PdfiumSDK, nativeRenderPage)(JNI_ARGS, jlong pagePtr, jobject objSurface,
                                            jint dpi, jint startX, jint startY,
                                            jint drawSizeHor, jint drawSizeVer,
                                            jboolean renderAnnot) {
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, objSurface);
    if (nativeWindow == NULL) {
        LOGE("native window pointer null");
        return;
    }
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);

    if (page == NULL || nativeWindow == NULL) {
        LOGE("Render page pointers invalid");
        return;
    }

    if (ANativeWindow_getFormat(nativeWindow) != WINDOW_FORMAT_RGBA_8888) {
        LOGD("Set format to RGBA_8888");
        ANativeWindow_setBuffersGeometry(nativeWindow,
                                         ANativeWindow_getWidth(nativeWindow),
                                         ANativeWindow_getHeight(nativeWindow),
                                         WINDOW_FORMAT_RGBA_8888);
    }

    ANativeWindow_Buffer buffer;
    int ret;
    if ((ret = ANativeWindow_lock(nativeWindow, &buffer, NULL)) != 0) {
        LOGE("Locking native window failed: %s", strerror(ret * -1));
        return;
    }

    renderPageInternal(page, &buffer,
                       (int) startX, (int) startY,
                       buffer.width, buffer.height,
                       (int) drawSizeHor, (int) drawSizeVer,
                       (bool) renderAnnot);

    ANativeWindow_unlockAndPost(nativeWindow);
    ANativeWindow_release(nativeWindow);
}

JNI_FUNC(void, PdfiumSDK, nativeRenderPageBitmap)(JNI_ARGS, jlong pagePtr, jobject bitmap,
                                                  jint dpi, jint startX, jint startY,
                                                  jint drawSizeHor, jint drawSizeVer,
                                                  jboolean renderAnnot) {

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);

    if (page == NULL || bitmap == NULL) {
        LOGE("Render page pointers invalid");
        return;
    }

    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("Fetching bitmap info failed: %s", strerror(ret * -1));
        return;
    }

    int canvasHorSize = info.width;
    int canvasVerSize = info.height;

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
        info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        LOGE("Bitmap format must be RGBA_8888 or RGB_565");
        return;
    }

    void *addr;
    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &addr)) != 0) {
        LOGE("Locking bitmap failed: %s", strerror(ret * -1));
        return;
    }

    void *tmp;
    int format;
    int sourceStride;
    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        tmp = malloc(canvasVerSize * canvasHorSize * sizeof(rgb));
        sourceStride = canvasHorSize * sizeof(rgb);
        format = FPDFBitmap_BGR;
    } else {
        tmp = addr;
        sourceStride = info.stride;
        format = FPDFBitmap_BGRA;
    }

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx(canvasHorSize, canvasVerSize,
                                                format, tmp, sourceStride);

    if (drawSizeHor < canvasHorSize || drawSizeVer < canvasVerSize) {
        FPDFBitmap_FillRect(pdfBitmap, 0, 0, canvasHorSize, canvasVerSize,
                            0x848484FF); //Gray
    }

    int baseHorSize = (canvasHorSize < drawSizeHor) ? canvasHorSize : (int) drawSizeHor;
    int baseVerSize = (canvasVerSize < drawSizeVer) ? canvasVerSize : (int) drawSizeVer;
    int baseX = (startX < 0) ? 0 : (int) startX;
    int baseY = (startY < 0) ? 0 : (int) startY;
    int flags = FPDF_REVERSE_BYTE_ORDER;

    if (renderAnnot) {
        flags |= FPDF_ANNOT;
    }

    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        FPDFBitmap_FillRect(pdfBitmap, baseX, baseY, baseHorSize, baseVerSize,
                            0xFFFFFFFF); //White
    }

    FPDF_RenderPageBitmap(pdfBitmap, page,
                          startX, startY,
                          (int) drawSizeHor, (int) drawSizeVer,
                          0, flags);

    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        rgbBitmapTo565(tmp, sourceStride, addr, &info);
        free(tmp);
    }

    AndroidBitmap_unlockPixels(env, bitmap);
}

JNI_FUNC(jstring, PdfiumSDK, nativeGetDocumentMetaText)(JNI_ARGS, jlong docPtr, jstring tag) {
    const char *ctag = env->GetStringUTFChars(tag, NULL);
    if (ctag == NULL) {
        return env->NewStringUTF("");
    }
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);

    size_t bufferLen = FPDF_GetMetaText(doc->pdfDocument, ctag, NULL, 0);
    if (bufferLen <= 2) {
        return env->NewStringUTF("");
    }
    std::wstring text;
    FPDF_GetMetaText(doc->pdfDocument, ctag, WriteInto(&text, bufferLen + 1), bufferLen);
    env->ReleaseStringUTFChars(tag, ctag);
    return env->NewString((jchar *) text.c_str(), bufferLen / 2 - 1);
}

JNI_FUNC(jobject, PdfiumSDK, nativeGetFirstChildBookmark)(JNI_ARGS, jlong docPtr,
                                                          jobject bookmarkPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_BOOKMARK parent;
    if (bookmarkPtr == NULL) {
        parent = NULL;
    } else {
        jclass longClass = env->GetObjectClass(bookmarkPtr);
        jmethodID longValueMethod = env->GetMethodID(longClass, "longValue", "()J");

        jlong ptr = env->CallLongMethod(bookmarkPtr, longValueMethod);
        parent = reinterpret_cast<FPDF_BOOKMARK>(ptr);
    }
    FPDF_BOOKMARK bookmark = FPDFBookmark_GetFirstChild(doc->pdfDocument, parent);
    if (bookmark == NULL) {
        return NULL;
    }
    return NewLong(env, reinterpret_cast<jlong>(bookmark));
}

JNI_FUNC(jobject, PdfiumSDK, nativeGetSiblingBookmark)(JNI_ARGS, jlong docPtr, jlong bookmarkPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_BOOKMARK parent = reinterpret_cast<FPDF_BOOKMARK>(bookmarkPtr);
    FPDF_BOOKMARK bookmark = FPDFBookmark_GetNextSibling(doc->pdfDocument, parent);
    if (bookmark == NULL) {
        return NULL;
    }
    return NewLong(env, reinterpret_cast<jlong>(bookmark));
}

JNI_FUNC(jstring, PdfiumSDK, nativeGetBookmarkTitle)(JNI_ARGS, jlong bookmarkPtr) {
    FPDF_BOOKMARK bookmark = reinterpret_cast<FPDF_BOOKMARK>(bookmarkPtr);
    size_t bufferLen = FPDFBookmark_GetTitle(bookmark, NULL, 0);
    if (bufferLen <= 2) {
        return env->NewStringUTF("");
    }
    std::wstring title;
    FPDFBookmark_GetTitle(bookmark, WriteInto(&title, bufferLen + 1), bufferLen);
    return env->NewString((jchar *) title.c_str(), bufferLen / 2 - 1);
}

JNI_FUNC(jlong, PdfiumSDK, nativeGetBookmarkDestIndex)(JNI_ARGS, jlong docPtr, jlong bookmarkPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_BOOKMARK bookmark = reinterpret_cast<FPDF_BOOKMARK>(bookmarkPtr);

    FPDF_DEST dest = FPDFBookmark_GetDest(doc->pdfDocument, bookmark);
    if (dest == NULL) {
        return -1;
    }
    return (jlong) FPDFDest_GetDestPageIndex(doc->pdfDocument, dest);
}

JNI_FUNC(jlongArray, PdfiumSDK, nativeGetPageLinks)(JNI_ARGS, jlong pagePtr) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    int pos = 0;
    std::vector<jlong> links;
    FPDF_LINK link;
    while (FPDFLink_Enumerate(page, &pos, &link)) {
        links.push_back(reinterpret_cast<jlong>(link));
    }

    jlongArray result = env->NewLongArray(links.size());
    env->SetLongArrayRegion(result, 0, links.size(), &links[0]);
    return result;
}

JNI_FUNC(jobject, PdfiumSDK, nativeGetDestPageIndex)(JNI_ARGS, jlong docPtr, jlong linkPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_LINK link = reinterpret_cast<FPDF_LINK>(linkPtr);
    FPDF_DEST dest = FPDFLink_GetDest(doc->pdfDocument, link);
    if (dest == NULL) {
        return NULL;
    }
    unsigned long index = FPDFDest_GetDestPageIndex(doc->pdfDocument, dest);
    return NewInteger(env, (jint) index);
}

JNI_FUNC(jstring, PdfiumSDK, nativeGetLinkURI)(JNI_ARGS, jlong docPtr, jlong linkPtr) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_LINK link = reinterpret_cast<FPDF_LINK>(linkPtr);
    FPDF_ACTION action = FPDFLink_GetAction(link);
    if (action == NULL) {
        return NULL;
    }
    size_t bufferLen = FPDFAction_GetURIPath(doc->pdfDocument, action, NULL, 0);
    if (bufferLen <= 0) {
        return env->NewStringUTF("");
    }
    std::string uri;
    FPDFAction_GetURIPath(doc->pdfDocument, action, WriteInto(&uri, bufferLen), bufferLen);
    return env->NewStringUTF(uri.c_str());
}

JNI_FUNC(jobject, PdfiumSDK, nativeGetLinkRect)(JNI_ARGS, jlong linkPtr) {
    FPDF_LINK link = reinterpret_cast<FPDF_LINK>(linkPtr);
    FS_RECTF fsRectF;
    FPDF_BOOL result = FPDFLink_GetAnnotRect(link, &fsRectF);

    if (!result) {
        return NULL;
    }

    jclass clazz = env->FindClass("android/graphics/RectF");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(FFFF)V");
    return env->NewObject(clazz, constructorID, fsRectF.left, fsRectF.top, fsRectF.right,
                          fsRectF.bottom);
}

JNI_FUNC(jobject, PdfiumSDK, nativePageCoordinateToDevice)(JNI_ARGS, jlong pagePtr, jint startX,
                                                       jint startY, jint sizeX,
                                                       jint sizeY, jint rotate, jdouble pageX,
                                                       jdouble pageY) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    int deviceX, deviceY;

    FPDF_PageToDevice(page, startX, startY, sizeX, sizeY, rotate, pageX, pageY, &deviceX, &deviceY);

    jclass clazz = env->FindClass("android/graphics/Point");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, deviceX, deviceY);
}

JNI_FUNC(jobject, PdfiumSDK, nativeDeviceCoordinateToPage)(JNI_ARGS, jlong pagePtr, jint startX,
                                                       jint startY, jint sizeX,
                                                       jint sizeY, jint rotate, jint deviceX,
                                                       jint deviceY) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    double pageX, pageY;

    FPDF_DeviceToPage(page, startX, startY, sizeX, sizeY, rotate, deviceX, deviceY, &pageX, &pageY);

    jclass clazz = env->FindClass("android/graphics/PointF");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(FF)V");
    return env->NewObject(clazz, constructorID, pageX, pageY);
}

JNI_FUNC(jint, PdfiumSDK, nativeGetPageRotation)(JNI_ARGS, jlong pagePtr) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint) FPDFPage_GetRotation(page);
}


//////////////////////////////////////////
// Begin PDF TextPage api
//////////////////////////////////////////

static jlong loadTextPageInternal(JNIEnv *env, DocumentFile *doc, int textPageIndex) {
    try {
        if (doc == NULL) throw "Get page document null";

        FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(loadPageInternal(env, doc, textPageIndex));
        if (page != NULL) {
            FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
            if (textPage == NULL) {
                throw "Loaded text page is null";
            }
            return reinterpret_cast<jlong>(textPage);
        } else {
            throw "Load page null";
        }
    } catch (const char *msg) {
        LOGE("%s", msg);

        jniThrowException(env, "java/lang/IllegalStateException",
                          "cannot load text page");

        return -1;
    }
}

static void closeTextPageInternal(jlong textPagePtr) {
    FPDFText_ClosePage(reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr));
}

JNI_FUNC(jlong, PdfiumSDK, nativeLoadTextPage)(JNI_ARGS, jlong docPtr, jint pageIndex) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    return loadTextPageInternal(env, doc, (int) pageIndex);
}

JNI_FUNC(jlongArray, PdfiumSDK, nativeLoadTextPages)(JNI_ARGS, jlong docPtr, jint fromIndex,
                                                     jint toIndex) {
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);

    if (toIndex < fromIndex) return NULL;
    jlong pages[toIndex - fromIndex + 1];

    int i;
    for (i = 0; i <= (toIndex - fromIndex); i++) {
        pages[i] = loadTextPageInternal(env, doc, (i + fromIndex));
    }

    jlongArray javaPages = env->NewLongArray((jsize) (toIndex - fromIndex + 1));
    env->SetLongArrayRegion(javaPages, 0, (jsize) (toIndex - fromIndex + 1), (const jlong *) pages);

    return javaPages;
}

JNI_FUNC(void, PdfiumSDK, nativeCloseTextPage)(JNI_ARGS, jlong textPagePtr) {
    closeTextPageInternal(textPagePtr);
}

JNI_FUNC(void, PdfiumSDK, nativeCloseTextPages)(JNI_ARGS, jlongArray textPagesPtr) {
    int length = (int) (env->GetArrayLength(textPagesPtr));
    jlong *textPages = env->GetLongArrayElements(textPagesPtr, NULL);

    int i;
    for (i = 0; i < length; i++) { closeTextPageInternal(textPages[i]); }
}

JNI_FUNC(jint, PdfiumSDK, nativeTextCountChars)(JNI_ARGS, jlong textPagePtr) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_CountChars(textPage);// FPDF_TEXTPAGE
}

JNI_FUNC(jint, PdfiumSDK, nativeTextGetUnicode)(JNI_ARGS, jlong textPagePtr, jint index) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_GetUnicode(textPage, (int) index);
}

JNI_FUNC(jdoubleArray, PdfiumSDK, nativeTextGetCharBox)(JNI_ARGS, jlong textPagePtr, jint index) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jdoubleArray result = env->NewDoubleArray(4);
    if (result == NULL) {
        return NULL;
    }
    double fill[4];
    FPDFText_GetCharBox(textPage, (int) index, &fill[0], &fill[1], &fill[2], &fill[3]);
    env->SetDoubleArrayRegion(result, 0, 4, (jdouble *) fill);
    return result;
}

JNI_FUNC(jint, PdfiumSDK, nativeTextGetCharIndexAtPos)(JNI_ARGS, jlong textPagePtr, jdouble x,
                                                       jdouble y, jdouble xTolerance,
                                                       jdouble yTolerance) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_GetCharIndexAtPos(textPage, (double) x, (double) y, (double) xTolerance,
                                             (double) yTolerance);
}

JNI_FUNC(jint, PdfiumSDK, nativeTextGetText)(JNI_ARGS, jlong textPagePtr, jint start_index,
                                             jint count, jshortArray result) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jboolean isCopy = 0;
    unsigned short *arr = (unsigned short *) env->GetShortArrayElements(result, &isCopy);
    jint output = (jint) FPDFText_GetText(textPage, (int) start_index, (int) count, arr);
    if (isCopy) {
        env->SetShortArrayRegion(result, 0, output, (jshort *) arr);
        env->ReleaseShortArrayElements(result, (jshort *) arr, JNI_ABORT);
    }
    return output;
}

JNI_FUNC(jint, PdfiumSDK, nativeTextCountRects)(JNI_ARGS, jlong textPagePtr, jint start_index,
                                                jint count) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_CountRects(textPage, (int) start_index, (int) count);
}

JNI_FUNC(jdoubleArray, PdfiumSDK, nativeTextGetRect)(JNI_ARGS, jlong textPagePtr,
                                                     jint rect_index) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jdoubleArray result = env->NewDoubleArray(4);
    if (result == NULL) {
        return NULL;
    }
    double fill[4];
    FPDFText_GetRect(textPage, (int) rect_index, &fill[0], &fill[1], &fill[2], &fill[3]);
    env->SetDoubleArrayRegion(result, 0, 4, (jdouble *) fill);
    return result;
}


JNI_FUNC(jint, PdfiumSDK, nativeTextGetBoundedTextLength)(JNI_ARGS, jlong textPagePtr,
                                                          jdouble left,
                                                          jdouble top, jdouble right,
                                                          jdouble bottom) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);

    return (jint) FPDFText_GetBoundedText(textPage, (double) left, (double) top,
                                          (double) right, (double) bottom, NULL, 0);
}

JNI_FUNC(jint, PdfiumSDK, nativeTextGetBoundedText)(JNI_ARGS, jlong textPagePtr, jdouble left,
                                                    jdouble top, jdouble right, jdouble bottom,
                                                    jshortArray arr) {
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jboolean isCopy = 0;
    unsigned short *buffer = NULL;
    int bufLen = 0;
    if (arr != NULL) {
        buffer = (unsigned short *) env->GetShortArrayElements(arr, &isCopy);
        bufLen = env->GetArrayLength(arr);
    }
    jint output = (jint) FPDFText_GetBoundedText(textPage, (double) left, (double) top,
                                                 (double) right, (double) bottom, buffer, bufLen);
    if (isCopy) {
        env->SetShortArrayRegion(arr, 0, output, (jshort *) buffer);
        env->ReleaseShortArrayElements(arr, (jshort *) buffer, JNI_ABORT);
    }
    return output;
}

//////////////////////////////////////////
// Begin PDF SDK Search
//////////////////////////////////////////

unsigned short *convertWideString(JNIEnv *env, jstring query) {

    std::wstring value;
    const jchar *raw = env->GetStringChars(query, 0);
    jsize len = env->GetStringLength(query);
    value.assign(raw, raw + len);
    env->ReleaseStringChars(query, raw);

    size_t length = sizeof(uint16_t) * (value.length() + 1);
    unsigned short *result = static_cast<unsigned short *>(malloc(length));
    char *ptr = reinterpret_cast<char *>(result);
    size_t i = 0;
    for (wchar_t w : value) {
        ptr[i++] = w & 0xff;
        ptr[i++] = (w >> 8) & 0xff;
    }
    ptr[i++] = 0;
    ptr[i] = 0;

    return result;
}

JNI_FUNC(jlong, PdfiumSDK, nativeSearchStart)(JNI_ARGS, jlong textPagePtr, jstring query,
                                              jboolean matchCase, jboolean matchWholeWord) {
    // convert jstring to UTF-16LE encoded wide strings
    unsigned short *pQuery = convertWideString(env, query);
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    FPDF_SCHHANDLE search;
    unsigned long flags = 0;

    if (matchCase) {
        flags = FPDF_MATCHCASE;
    }
    if (matchWholeWord) {
        flags = flags | FPDF_MATCHWHOLEWORD;
    }

    search = FPDFText_FindStart(textPage, pQuery, flags, 0);
    return reinterpret_cast<jlong>(search);
}

JNI_FUNC(void, PdfiumSDK, nativeSearchStop)(JNI_ARGS, jlong searchHandlePtr) {
    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    FPDFText_FindClose(search);
}

JNI_FUNC(jboolean, PdfiumSDK, nativeSearchNext)(JNI_ARGS, jlong searchHandlePtr) {
    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    FPDF_BOOL result = FPDFText_FindNext(search);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNI_FUNC(jboolean, PdfiumSDK, nativeSearchPrev)(JNI_ARGS, jlong searchHandlePtr) {
    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    FPDF_BOOL result = FPDFText_FindPrev(search);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNI_FUNC(jint, PdfiumSDK, nativeGetCharIndexOfSearchResult)(JNI_ARGS, jlong searchHandlePtr) {
    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    return FPDFText_GetSchResultIndex(search);
}

JNI_FUNC(jint, PdfiumSDK, nativeCountSearchResult)(JNI_ARGS, jlong searchHandlePtr) {
    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    return FPDFText_GetSchCount(search);
}

//////////////////////////////////////////
// Begin PDF Annotation api
//////////////////////////////////////////
JNI_FUNC(jlong, PdfiumSDK, nativeAddTextAnnotation)(JNI_ARGS, jlong docPtr, int page_index,
                                                    jstring text_,
                                                    jintArray color_, jintArray bound_) {

    FPDF_PAGE page;
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    int pagePtr = loadPageInternal(env, doc, page_index);
    if (pagePtr == -1) {
        return -1;
    } else {
        page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    }

    // Get the bound array
    jint *bounds = env->GetIntArrayElements(bound_, NULL);
    int boundsLen = (int) (env->GetArrayLength(bound_));
    if (boundsLen != 4) {
        return -1;
    }

    // Set the annotation rectangle.
    FS_RECTF rect;
    rect.left = bounds[0];
    rect.top = bounds[1];
    rect.right = bounds[2];
    rect.bottom = bounds[3];

    // Get the text color
    unsigned int R, G, B, A;
    jint *colors = env->GetIntArrayElements(color_, NULL);
    int colorsLen = (int) (env->GetArrayLength(color_));
    if (colorsLen == 4) {
        R = colors[0];
        G = colors[1];
        B = colors[2];
        A = colors[3];
    } else {
        R = 51u;
        G = 102u;
        B = 153u;
        A = 204u;
    }

    // Add a text annotation to the page.
    FPDF_ANNOTATION annot = FPDFPage_CreateAnnot(page, FPDF_ANNOT_TEXT);

    // set the rectangle of the annotation
    FPDFAnnot_SetRect(annot, &rect);
    env->ReleaseIntArrayElements(bound_, bounds, 0);

    // Set the color of the annotation.
    FPDFAnnot_SetColor(annot, FPDFANNOT_COLORTYPE_Color, R, G, B, A);
    env->ReleaseIntArrayElements(color_, colors, 0);

    // Set the content of the annotation.
    unsigned short *kContents = convertWideString(env, text_);
    FPDFAnnot_SetStringValue(annot, kContentsKey, kContents);

    // save page
    FPDF_DOCUMENT pdfDoc = doc->pdfDocument;
    if (!FPDF_SaveAsCopy(pdfDoc, NULL, FPDF_INCREMENTAL)) {
        return -1;
    }

    // close page
    closePageInternal(pagePtr);

    // reload page
    pagePtr = loadPageInternal(env, doc, page_index);

    jclass clazz = env->FindClass("com/megahed/mycplus/PdfiumSDK");
    jmethodID callback = env->GetMethodID(clazz, "onAnnotationAdded",
                                          "(Ljava/lang/Integer;)Ljava/lang/Long;");
    env->CallObjectMethod(thiz, callback, page_index, pagePtr);

    return reinterpret_cast<jlong>(annot);
}

}//extern C

#pragma clang diagnostic pop
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeOpenDocument(JNIEnv *env, jobject thiz, jint fd,
                                                      jstring password) {
    // TODO: implement nativeOpenDocument()


    size_t fileLength = (size_t) getFileSize(fd);
    if (fileLength <= 0) {
        jniThrowException(env, "java/io/IOException",
                          "File is empty");
        return -1;
    }

    DocumentFile *docFile = new DocumentFile();

    FPDF_FILEACCESS loader;
    loader.m_FileLen = fileLength;
    loader.m_Param = reinterpret_cast<void *>(intptr_t(fd));
    loader.m_GetBlock = &getBlock;

    const char *cpassword = NULL;
    if (password != NULL) {
        cpassword = env->GetStringUTFChars(password, NULL);
    }

    FPDF_DOCUMENT document = FPDF_LoadCustomDocument(&loader, cpassword);

    if (cpassword != NULL) {
        env->ReleaseStringUTFChars(password, cpassword);
    }

    if (!document) {
        delete docFile;

        const long errorNum = FPDF_GetLastError();
        if (errorNum == FPDF_ERR_PASSWORD) {
            jniThrowException(env, "org/benjinus/pdfium/PdfPasswordException",
                              "Password required or incorrect password.");
        } else {
            char *error = getErrorDescription(errorNum);
            jniThrowExceptionFmt(env, "java/io/IOException",
                                 "cannot create document: %s", error);

            free(error);
        }

        return -1;
    }

    docFile->pdfDocument = document;

    return reinterpret_cast<jlong>(docFile);

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeOpenMemDocument(JNIEnv *env, jobject thiz, jbyteArray data,
                                                         jstring password) {
    // TODO: implement nativeOpenMemDocument()

    DocumentFile *docFile = new DocumentFile();

    const char *cpassword = NULL;
    if (password != NULL) {
        cpassword = env->GetStringUTFChars(password, NULL);
    }

    jbyte *cData = env->GetByteArrayElements(data, NULL);
    int size = (int) env->GetArrayLength(data);
    jbyte *cDataCopy = new jbyte[size];
    memcpy(cDataCopy, cData, size);
    FPDF_DOCUMENT document = FPDF_LoadMemDocument(reinterpret_cast<const void *>(cDataCopy),
                                                  size, cpassword);
    env->ReleaseByteArrayElements(data, cData, JNI_ABORT);

    if (cpassword != NULL) {
        env->ReleaseStringUTFChars(password, cpassword);
    }

    if (!document) {
        delete docFile;

        const long errorNum = FPDF_GetLastError();
        if (errorNum == FPDF_ERR_PASSWORD) {
            jniThrowException(env, "org/benjinus/pdfium/PdfPasswordException",
                              "Password required or incorrect password.");
        } else {
            char *error = getErrorDescription(errorNum);
            jniThrowExceptionFmt(env, "java/io/IOException",
                                 "cannot create document: %s", error);

            free(error);
        }

        return -1;
    }

    docFile->pdfDocument = document;

    return reinterpret_cast<jlong>(docFile);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeCloseDocument(JNIEnv *env, jobject thiz, jlong doc_ptr) {
    // TODO: implement nativeCloseDocument()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(doc_ptr);
    delete doc;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageCount(JNIEnv *env, jobject thiz, jlong doc_ptr) {
    // TODO: implement nativeGetPageCount()
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(doc_ptr);
    return (jint) FPDF_GetPageCount(doc->pdfDocument);
}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeLoadPage(JNIEnv *env, jobject thiz, jlong doc_ptr,
                                                  jint page_index) {
    // TODO: implement nativeLoadPage()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(doc_ptr);
    return loadPageInternal(env, doc, (int) page_index);

}
extern "C"
JNIEXPORT jlongArray JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeLoadPages(JNIEnv *env, jobject thiz, jlong doc_ptr,
                                                   jint from_index, jint to_index) {
    // TODO: implement nativeLoadPages()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(doc_ptr);

    if (to_index < from_index) return NULL;
    jlong pages[to_index - from_index + 1];

    int i;
    for (i = 0; i <= (to_index - from_index); i++) {
        pages[i] = loadPageInternal(env, doc, (i + from_index));
    }

    jlongArray javaPages = env->NewLongArray((jsize) (to_index - from_index + 1));
    env->SetLongArrayRegion(javaPages, 0, (jsize) (to_index - from_index + 1), (const jlong *) pages);

    return javaPages;


}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeClosePage(JNIEnv *env, jobject thiz, jlong page_ptr) {
    // TODO: implement nativeClosePage()
    closePageInternal(page_ptr);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeClosePages(JNIEnv *env, jobject thiz,
                                                    jlongArray pages_ptr) {
    // TODO: implement nativeClosePages()

    int length = (int) (env->GetArrayLength(pages_ptr));
    jlong *pages = env->GetLongArrayElements(pages_ptr, NULL);

    int i;
    for (i = 0; i < length; i++) { closePageInternal(pages[i]); }

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageWidthPixel(JNIEnv *env, jobject thiz,
                                                           jlong page_ptr, jint dpi) {
    // TODO: implement nativeGetPageWidthPixel()
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(page_ptr);
    return (jint) (FPDF_GetPageWidth(page) * dpi / 72);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageHeightPixel(JNIEnv *env, jobject thiz,
                                                            jlong page_ptr, jint dpi) {
    // TODO: implement nativeGetPageHeightPixel()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(page_ptr);
    return (jint) (FPDF_GetPageHeight(page) * dpi / 72);
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageWidthPoint(JNIEnv *env, jobject thiz,
                                                           jlong page_ptr) {
    // TODO: implement nativeGetPageWidthPoint()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(page_ptr);
    return (jint) FPDF_GetPageWidth(page);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageHeightPoint(JNIEnv *env, jobject thiz,
                                                            jlong page_ptr) {
    // TODO: implement nativeGetPageHeightPoint()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(page_ptr);
    return (jint) FPDF_GetPageHeight(page);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageRotation(JNIEnv *env, jobject thiz,
                                                         jlong page_ptr) {
    // TODO: implement nativeGetPageRotation()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(page_ptr);
    return (jint) FPDFPage_GetRotation(page);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeRenderPage(JNIEnv *env, jobject thiz, jlong pagePtr,
                                                    jobject surface, jint dpi, jint startX,
                                                    jint startY, jint drawSizeHor,
                                                    jint drawSizeVer, jboolean renderAnnot) {
    // TODO: implement nativeRenderPage()

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == NULL) {
        LOGE("native window pointer null");
        return;
    }
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);

    if (page == NULL || nativeWindow == NULL) {
        LOGE("Render page pointers invalid");
        return;
    }

    if (ANativeWindow_getFormat(nativeWindow) != WINDOW_FORMAT_RGBA_8888) {
        LOGD("Set format to RGBA_8888");
        ANativeWindow_setBuffersGeometry(nativeWindow,
                                         ANativeWindow_getWidth(nativeWindow),
                                         ANativeWindow_getHeight(nativeWindow),
                                         WINDOW_FORMAT_RGBA_8888);
    }

    ANativeWindow_Buffer buffer;
    int ret;
    if ((ret = ANativeWindow_lock(nativeWindow, &buffer, NULL)) != 0) {
        LOGE("Locking native window failed: %s", strerror(ret * -1));
        return;
    }

    renderPageInternal(page, &buffer,
                       (int) startX, (int) startY,
                       buffer.width, buffer.height,
                       (int) drawSizeHor, (int) drawSizeVer,
                       (bool) renderAnnot);

    ANativeWindow_unlockAndPost(nativeWindow);
    ANativeWindow_release(nativeWindow);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeRenderPageBitmap(JNIEnv *env, jobject thiz, jlong pagePtr,
                                                          jobject bitmap, jint dpi, jint startX,
                                                          jint startY, jint drawSizeHor,
                                                          jint drawSizeVer,
                                                          jboolean renderAnnot) {
    // TODO: implement nativeRenderPageBitmap()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);

    if (page == NULL || bitmap == NULL) {
        LOGE("Render page pointers invalid");
        return;
    }

    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("Fetching bitmap info failed: %s", strerror(ret * -1));
        return;
    }

    int canvasHorSize = info.width;
    int canvasVerSize = info.height;

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
        info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        LOGE("Bitmap format must be RGBA_8888 or RGB_565");
        return;
    }

    void *addr;
    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &addr)) != 0) {
        LOGE("Locking bitmap failed: %s", strerror(ret * -1));
        return;
    }

    void *tmp;
    int format;
    int sourceStride;
    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        tmp = malloc(canvasVerSize * canvasHorSize * sizeof(rgb));
        sourceStride = canvasHorSize * sizeof(rgb);
        format = FPDFBitmap_BGR;
    } else {
        tmp = addr;
        sourceStride = info.stride;
        format = FPDFBitmap_BGRA;
    }

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx(canvasHorSize, canvasVerSize,
                                                format, tmp, sourceStride);

    if (drawSizeHor < canvasHorSize || drawSizeVer < canvasVerSize) {
        FPDFBitmap_FillRect(pdfBitmap, 0, 0, canvasHorSize, canvasVerSize,
                            0x848484FF); //Gray
    }

    int baseHorSize = (canvasHorSize < drawSizeHor) ? canvasHorSize : (int) drawSizeHor;
    int baseVerSize = (canvasVerSize < drawSizeVer) ? canvasVerSize : (int) drawSizeVer;
    int baseX = (startX < 0) ? 0 : (int) startX;
    int baseY = (startY < 0) ? 0 : (int) startY;
    int flags = FPDF_REVERSE_BYTE_ORDER;

    if (renderAnnot) {
        flags |= FPDF_ANNOT;
    }

    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        FPDFBitmap_FillRect(pdfBitmap, baseX, baseY, baseHorSize, baseVerSize,
                            0xFFFFFFFF); //White
    }

    FPDF_RenderPageBitmap(pdfBitmap, page,
                          startX, startY,
                          (int) drawSizeHor, (int) drawSizeVer,
                          0, flags);

    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        rgbBitmapTo565(tmp, sourceStride, addr, &info);
        free(tmp);
    }

    AndroidBitmap_unlockPixels(env, bitmap);

}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetDocumentMetaText(JNIEnv *env, jobject thiz,
                                                             jlong docPtr, jstring tag) {
    // TODO: implement nativeGetDocumentMetaText()

    const char *ctag = env->GetStringUTFChars(tag, NULL);
    if (ctag == NULL) {
        return env->NewStringUTF("");
    }
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);

    size_t bufferLen = FPDF_GetMetaText(doc->pdfDocument, ctag, NULL, 0);
    if (bufferLen <= 2) {
        return env->NewStringUTF("");
    }
    std::wstring text;
    FPDF_GetMetaText(doc->pdfDocument, ctag, WriteInto(&text, bufferLen + 1), bufferLen);
    env->ReleaseStringUTFChars(tag, ctag);
    return env->NewString((jchar *) text.c_str(), bufferLen / 2 - 1);

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetFirstChildBookmark(JNIEnv *env, jobject thiz,
                                                               jlong docPtr,
                                                               jobject bookmarkPtr) {
    // TODO: implement nativeGetFirstChildBookmark()
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_BOOKMARK parent;
    if (bookmarkPtr == NULL) {
        parent = NULL;
    } else {
        jclass longClass = env->GetObjectClass(bookmarkPtr);
        jmethodID longValueMethod = env->GetMethodID(longClass, "longValue", "()J");

        jlong ptr = env->CallLongMethod(bookmarkPtr, longValueMethod);
        parent = reinterpret_cast<FPDF_BOOKMARK>(ptr);
    }
    FPDF_BOOKMARK bookmark = FPDFBookmark_GetFirstChild(doc->pdfDocument, parent);
    if (bookmark == NULL) {
        return NULL;
    }
    return NewLong(env, reinterpret_cast<jlong>(bookmark));

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetSiblingBookmark(JNIEnv *env, jobject thiz,
                                                            jlong docPtr, jlong bookmarkPtr) {
    // TODO: implement nativeGetSiblingBookmark()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_BOOKMARK parent = reinterpret_cast<FPDF_BOOKMARK>(bookmarkPtr);
    FPDF_BOOKMARK bookmark = FPDFBookmark_GetNextSibling(doc->pdfDocument, parent);
    if (bookmark == NULL) {
        return NULL;
    }
    return NewLong(env, reinterpret_cast<jlong>(bookmark));

}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetBookmarkTitle(JNIEnv *env, jobject thiz,
                                                          jlong bookmarkPtr) {
    // TODO: implement nativeGetBookmarkTitle()

    FPDF_BOOKMARK bookmark = reinterpret_cast<FPDF_BOOKMARK>(bookmarkPtr);
    size_t bufferLen = FPDFBookmark_GetTitle(bookmark, NULL, 0);
    if (bufferLen <= 2) {
        return env->NewStringUTF("");
    }
    std::wstring title;
    FPDFBookmark_GetTitle(bookmark, WriteInto(&title, bufferLen + 1), bufferLen);
    return env->NewString((jchar *) title.c_str(), bufferLen / 2 - 1);

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetBookmarkDestIndex(JNIEnv *env, jobject thiz,
                                                              jlong docPtr, jlong bookmarkPtr) {
    // TODO: implement nativeGetBookmarkDestIndex()
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_BOOKMARK bookmark = reinterpret_cast<FPDF_BOOKMARK>(bookmarkPtr);

    FPDF_DEST dest = FPDFBookmark_GetDest(doc->pdfDocument, bookmark);
    if (dest == NULL) {
        return -1;
    }
    return (jlong) FPDFDest_GetDestPageIndex(doc->pdfDocument, dest);

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageSizeByIndex(JNIEnv *env, jobject thiz,
                                                            jlong docPtr, jint pageIndex,
                                                            jint dpi) {
    // TODO: implement nativeGetPageSizeByIndex()
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    if (doc == NULL) {
        LOGE("Document is null");

        jniThrowException(env, "java/lang/IllegalStateException",
                          "Document is null");
        return NULL;
    }

    double width, height;
    int result = FPDF_GetPageSizeByIndex(doc->pdfDocument, pageIndex, &width, &height);

    if (result == 0) {
        width = 0;
        height = 0;
    }

    jint widthInt = (jint) (width * dpi / 72);
    jint heightInt = (jint) (height * dpi / 72);

    jclass clazz = env->FindClass("com/megahed/mycplus/util/Size");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, widthInt, heightInt);

}
extern "C"
JNIEXPORT jlongArray JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetPageLinks(JNIEnv *env, jobject thiz, jlong pagePtr) {
    // TODO: implement nativeGetPageLinks()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    int pos = 0;
    std::vector<jlong> links;
    FPDF_LINK link;
    while (FPDFLink_Enumerate(page, &pos, &link)) {
        links.push_back(reinterpret_cast<jlong>(link));
    }

    jlongArray result = env->NewLongArray(links.size());
    env->SetLongArrayRegion(result, 0, links.size(), &links[0]);
    return result;

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetDestPageIndex(JNIEnv *env, jobject thiz, jlong docPtr,
                                                          jlong linkPtr) {
    // TODO: implement nativeGetDestPageIndex()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_LINK link = reinterpret_cast<FPDF_LINK>(linkPtr);
    FPDF_DEST dest = FPDFLink_GetDest(doc->pdfDocument, link);
    if (dest == NULL) {
        return NULL;
    }
    unsigned long index = FPDFDest_GetDestPageIndex(doc->pdfDocument, dest);
    return NewInteger(env, (jint) index);

}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetLinkURI(JNIEnv *env, jobject thiz, jlong docPtr,
                                                    jlong linkPtr) {
    // TODO: implement nativeGetLinkURI()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    FPDF_LINK link = reinterpret_cast<FPDF_LINK>(linkPtr);
    FPDF_ACTION action = FPDFLink_GetAction(link);
    if (action == NULL) {
        return NULL;
    }
    size_t bufferLen = FPDFAction_GetURIPath(doc->pdfDocument, action, NULL, 0);
    if (bufferLen <= 0) {
        return env->NewStringUTF("");
    }
    std::string uri;
    FPDFAction_GetURIPath(doc->pdfDocument, action, WriteInto(&uri, bufferLen), bufferLen);
    return env->NewStringUTF(uri.c_str());

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetLinkRect(JNIEnv *env, jobject thiz, jlong linkPtr) {
    // TODO: implement nativeGetLinkRect()

    FPDF_LINK link = reinterpret_cast<FPDF_LINK>(linkPtr);
    FS_RECTF fsRectF;
    FPDF_BOOL result = FPDFLink_GetAnnotRect(link, &fsRectF);

    if (!result) {
        return NULL;
    }

    jclass clazz = env->FindClass("android/graphics/RectF");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(FFFF)V");
    return env->NewObject(clazz, constructorID, fsRectF.left, fsRectF.top, fsRectF.right,
                          fsRectF.bottom);

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativePageCoordinateToDevice(JNIEnv *env, jobject thiz,
                                                                jlong pagePtr, jint startX,
                                                                jint startY, jint sizeX,
                                                                jint sizeY, jint rotate,
                                                                jdouble pageX, jdouble pageY) {
    // TODO: implement nativePageCoordinateToDevice()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    int deviceX, deviceY;

    FPDF_PageToDevice(page, startX, startY, sizeX, sizeY, rotate, pageX, pageY, &deviceX, &deviceY);

    jclass clazz = env->FindClass("android/graphics/Point");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, deviceX, deviceY);

}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeDeviceCoordinateToPage(JNIEnv *env, jobject thiz,
                                                                jlong pagePtr, jint startX,
                                                                jint startY, jint sizeX,
                                                                jint sizeY, jint rotate,
                                                                jint deviceX, jint deviceY) {
    // TODO: implement nativeDeviceCoordinateToPage()

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    double pageX, pageY;

    FPDF_DeviceToPage(page, startX, startY, sizeX, sizeY, rotate, deviceX, deviceY, &pageX, &pageY);

    jclass clazz = env->FindClass("android/graphics/PointF");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(FF)V");
    return env->NewObject(clazz, constructorID, pageX, pageY);

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeLoadTextPage(JNIEnv *env, jobject thiz, jlong docPtr,
                                                      jint pageIndex) {
    // TODO: implement nativeLoadTextPage()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    return loadTextPageInternal(env, doc, (int) pageIndex);

}
extern "C"
JNIEXPORT jlongArray JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeLoadTextPages(JNIEnv *env, jobject thiz, jlong docPtr,
                                                       jint fromIndex, jint toIndex) {
    // TODO: implement nativeLoadTextPages()

    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);

    if (toIndex < fromIndex) return NULL;
    jlong pages[toIndex - fromIndex + 1];

    int i;
    for (i = 0; i <= (toIndex - fromIndex); i++) {
        pages[i] = loadTextPageInternal(env, doc, (i + fromIndex));
    }

    jlongArray javaPages = env->NewLongArray((jsize) (toIndex - fromIndex + 1));
    env->SetLongArrayRegion(javaPages, 0, (jsize) (toIndex - fromIndex + 1), (const jlong *) pages);

    return javaPages;

}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeCloseTextPage(JNIEnv *env, jobject thiz, jlong page_ptr) {
    // TODO: implement nativeCloseTextPage()
    closeTextPageInternal(page_ptr);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeCloseTextPages(JNIEnv *env, jobject thiz,
                                                        jlongArray textPagesPtr) {
    // TODO: implement nativeCloseTextPages()

    int length = (int) (env->GetArrayLength(textPagesPtr));
    jlong *textPages = env->GetLongArrayElements(textPagesPtr, NULL);

    int i;
    for (i = 0; i < length; i++) { closeTextPageInternal(textPages[i]); }

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextCountChars(JNIEnv *env, jobject thiz,
                                                        jlong textPagePtr) {
    // TODO: implement nativeTextCountChars()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_CountChars(textPage);// FPDF_TEXTPAGE

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetText(JNIEnv *env, jobject thiz, jlong text_page_ptr,
                                                     jint start_index, jint count,
                                                     jshortArray result) {
    // TODO: implement nativeTextGetText()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(text_page_ptr);
    jboolean isCopy = 0;
    unsigned short *arr = (unsigned short *) env->GetShortArrayElements(result, &isCopy);
    jint output = (jint) FPDFText_GetText(textPage, (int) start_index, (int) count, arr);
    if (isCopy) {
        env->SetShortArrayRegion(result, 0, output, (jshort *) arr);
        env->ReleaseShortArrayElements(result, (jshort *) arr, JNI_ABORT);
    }
    return output;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetUnicode(JNIEnv *env, jobject thiz,
                                                        jlong textPagePtr, jint index) {
    // TODO: implement nativeTextGetUnicode()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_GetUnicode(textPage, (int) index);

}
extern "C"
JNIEXPORT jdoubleArray JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetCharBox(JNIEnv *env, jobject thiz,
                                                        jlong textPagePtr, jint index) {
    // TODO: implement nativeTextGetCharBox()
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jdoubleArray result = env->NewDoubleArray(4);
    if (result == NULL) {
        return NULL;
    }
    double fill[4];
    FPDFText_GetCharBox(textPage, (int) index, &fill[0], &fill[1], &fill[2], &fill[3]);
    env->SetDoubleArrayRegion(result, 0, 4, (jdouble *) fill);
    return result;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetCharIndexAtPos(JNIEnv *env, jobject thiz,
                                                               jlong textPagePtr, jdouble x,
                                                               jdouble y, jdouble xTolerance,
                                                               jdouble yTolerance) {
    // TODO: implement nativeTextGetCharIndexAtPos()
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_GetCharIndexAtPos(textPage, (double) x, (double) y, (double) xTolerance,
                                             (double) yTolerance);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextCountRects(JNIEnv *env, jobject thiz,
                                                        jlong textPagePtr, jint start_index,
                                                        jint count) {
    // TODO: implement nativeTextCountRects()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    return (jint) FPDFText_CountRects(textPage, (int) start_index, (int) count);

}
extern "C"
JNIEXPORT jdoubleArray JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetRect(JNIEnv *env, jobject thiz, jlong textPagePtr,
                                                     jint rect_index) {
    // TODO: implement nativeTextGetRect()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jdoubleArray result = env->NewDoubleArray(4);
    if (result == NULL) {
        return NULL;
    }
    double fill[4];
    FPDFText_GetRect(textPage, (int) rect_index, &fill[0], &fill[1], &fill[2], &fill[3]);
    env->SetDoubleArrayRegion(result, 0, 4, (jdouble *) fill);
    return result;


}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetBoundedTextLength(JNIEnv *env, jobject thiz,
                                                                  jlong textPagePtr, jdouble left,
                                                                  jdouble top, jdouble right,
                                                                  jdouble bottom) {
    // TODO: implement nativeTextGetBoundedTextLength()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);

    return (jint) FPDFText_GetBoundedText(textPage, (double) left, (double) top,
                                          (double) right, (double) bottom, NULL, 0);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeTextGetBoundedText(JNIEnv *env, jobject thiz,
                                                            jlong textPagePtr, jdouble left,
                                                            jdouble top, jdouble right,
                                                            jdouble bottom, jshortArray arr) {
    // TODO: implement nativeTextGetBoundedText()

    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    jboolean isCopy = 0;
    unsigned short *buffer = NULL;
    int bufLen = 0;
    if (arr != NULL) {
        buffer = (unsigned short *) env->GetShortArrayElements(arr, &isCopy);
        bufLen = env->GetArrayLength(arr);
    }
    jint output = (jint) FPDFText_GetBoundedText(textPage, (double) left, (double) top,
                                                 (double) right, (double) bottom, buffer, bufLen);
    if (isCopy) {
        env->SetShortArrayRegion(arr, 0, output, (jshort *) buffer);
        env->ReleaseShortArrayElements(arr, (jshort *) buffer, JNI_ABORT);
    }
    return output;

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeSearchStart(JNIEnv *env, jobject thiz, jlong textPagePtr,
                                                     jstring query, jboolean matchCase,
                                                     jboolean matchWholeWord) {
    // TODO: implement nativeSearchStart()

    // convert jstring to UTF-16LE encoded wide strings
    unsigned short *pQuery = convertWideString(env, query);
    FPDF_TEXTPAGE textPage = reinterpret_cast<FPDF_TEXTPAGE>(textPagePtr);
    FPDF_SCHHANDLE search;
    unsigned long flags = 0;

    if (matchCase) {
        flags = FPDF_MATCHCASE;
    }
    if (matchWholeWord) {
        flags = flags | FPDF_MATCHWHOLEWORD;
    }

    search = FPDFText_FindStart(textPage, pQuery, flags, 0);
    return reinterpret_cast<jlong>(search);


}
extern "C"
JNIEXPORT void JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeSearchStop(JNIEnv *env, jobject thiz,
                                                    jlong searchHandlePtr) {
    // TODO: implement nativeSearchStop()

    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    FPDFText_FindClose(search);

}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeSearchNext(JNIEnv *env, jobject thiz,
                                                    jlong searchHandlePtr) {
    // TODO: implement nativeSearchNext()

    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    FPDF_BOOL result = FPDFText_FindNext(search);
    return result ? JNI_TRUE : JNI_FALSE;

}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeSearchPrev(JNIEnv *env, jobject thiz,
                                                    jlong searchHandlePtr) {
    // TODO: implement nativeSearchPrev()

    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    FPDF_BOOL result = FPDFText_FindPrev(search);
    return result ? JNI_TRUE : JNI_FALSE;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetCharIndexOfSearchResult(JNIEnv *env, jobject thiz,
                                                                    jlong searchHandlePtr) {
    // TODO: implement nativeGetCharIndexOfSearchResult()

    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    return FPDFText_GetSchResultIndex(search);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeCountSearchResult(JNIEnv *env, jobject thiz,
                                                           jlong searchHandlePtr) {
    // TODO: implement nativeCountSearchResult()

    FPDF_SCHHANDLE search = reinterpret_cast<FPDF_SCHHANDLE>(searchHandlePtr);
    return FPDFText_GetSchCount(search);

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeAddTextAnnotation(JNIEnv *env, jobject thiz, jlong docPtr,
                                                           jint page_index, jstring text_,
                                                           jintArray color_, jintArray bound_) {
    // TODO: implement nativeAddTextAnnotation()

    FPDF_PAGE page;
    DocumentFile *doc = reinterpret_cast<DocumentFile *>(docPtr);
    int pagePtr = loadPageInternal(env, doc, page_index);
    if (pagePtr == -1) {
        return -1;
    } else {
        page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    }

    // Get the bound array
    jint *bounds = env->GetIntArrayElements(bound_, NULL);
    int boundsLen = (int) (env->GetArrayLength(bound_));
    if (boundsLen != 4) {
        return -1;
    }

    // Set the annotation rectangle.
    FS_RECTF rect;
    rect.left = bounds[0];
    rect.top = bounds[1];
    rect.right = bounds[2];
    rect.bottom = bounds[3];

    // Get the text color
    unsigned int R, G, B, A;
    jint *colors = env->GetIntArrayElements(color_, NULL);
    int colorsLen = (int) (env->GetArrayLength(color_));
    if (colorsLen == 4) {
        R = colors[0];
        G = colors[1];
        B = colors[2];
        A = colors[3];
    } else {
        R = 51u;
        G = 102u;
        B = 153u;
        A = 204u;
    }

    // Add a text annotation to the page.
    FPDF_ANNOTATION annot = FPDFPage_CreateAnnot(page, FPDF_ANNOT_TEXT);

    // set the rectangle of the annotation
    FPDFAnnot_SetRect(annot, &rect);
    env->ReleaseIntArrayElements(bound_, bounds, 0);

    // Set the color of the annotation.
    FPDFAnnot_SetColor(annot, FPDFANNOT_COLORTYPE_Color, R, G, B, A);
    env->ReleaseIntArrayElements(color_, colors, 0);

    // Set the content of the annotation.
    unsigned short *kContents = convertWideString(env, text_);
    FPDFAnnot_SetStringValue(annot, kContentsKey, kContents);

    // save page
    FPDF_DOCUMENT pdfDoc = doc->pdfDocument;
    if (!FPDF_SaveAsCopy(pdfDoc, NULL, FPDF_INCREMENTAL)) {
        return -1;
    }

    // close page
    closePageInternal(pagePtr);

    // reload page
    pagePtr = loadPageInternal(env, doc, page_index);

    jclass clazz = env->FindClass("com/megahed/mycplus/PdfiumSDK");
    jmethodID callback = env->GetMethodID(clazz, "onAnnotationAdded",
                                          "(Ljava/lang/Integer;)Ljava/lang/Long;");
    env->CallObjectMethod(thiz, callback, page_index, pagePtr);

    return reinterpret_cast<jlong>(annot);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetCharIndexAtCoord(JNIEnv *env, jobject thiz,
                                                             jlong page_ptr, jdouble width,
                                                             jdouble height, jlong text_ptr,
                                                             jdouble pos_x, jdouble pos_y,
                                                             jdouble tol_x, jdouble tol_y) {
    // TODO: implement nativeGetCharIndexAtCoord()

    double px, py;
    FPDF_DeviceToPage((FPDF_PAGE)page_ptr, 0, 0, width, height, 0, pos_x, pos_y, &px, &py);
    return FPDFText_GetCharIndexAtPos((FPDF_TEXTPAGE)text_ptr, px, py, tol_x, tol_y);

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetLinkAtCoord(JNIEnv *env, jobject thiz, jlong page_ptr,
                                                        jdouble width, jdouble height,
                                                        jdouble pos_x, jdouble pos_y) {
    // TODO: implement nativeGetLinkAtCoord()

    double px, py;
    FPDF_DeviceToPage((FPDF_PAGE)page_ptr, 0, 0, width, height, 0, pos_x, pos_y, &px, &py);
    return (jlong)FPDFLink_GetLinkAtPoint((FPDF_PAGE)page_ptr, px, py);

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeCountAndGetRects(JNIEnv *env, jobject thiz, jlong pagePtr,
                                                          jint offsetY, jint offsetX, jint width,
                                                          jint height, jobject arr, jlong textPtr,
                                                          jint st, jint ed) {
    // TODO: implement nativeCountAndGetRects()

    jclass arrList = env->FindClass("java/util/ArrayList");
    jmethodID arrList_add = env->GetMethodID(arrList,"add","(Ljava/lang/Object;)Z");
    jmethodID arrList_get = env->GetMethodID(arrList,"get","(I)Ljava/lang/Object;");
    jmethodID arrList_size = env->GetMethodID(arrList,"size","()I");
    jmethodID arrList_enssurecap = env->GetMethodID(arrList,"ensureCapacity","(I)V");

    jclass rectF = env->FindClass("android/graphics/RectF");
    jmethodID rectF_ = env->GetMethodID(rectF, "<init>", "(FFFF)V");
    jmethodID rectF_set = env->GetMethodID(rectF, "set", "(FFFF)V");


    int rectCount = FPDFText_CountRects((FPDF_TEXTPAGE)textPtr, (int)st, (int)ed);
    env->CallVoidMethod( arr, arrList_enssurecap, rectCount);
    double left, top, right, bottom;
    int arraySize = env->CallIntMethod(arr, arrList_size);
    int deviceX, deviceY;
    for(int i=0;i<rectCount;i++) {
        if(FPDFText_GetRect((FPDF_TEXTPAGE)textPtr, i, &left, &top, &right, &bottom)) {
            FPDF_PageToDevice((FPDF_PAGE)pagePtr, 0, 0, width, height, 0, left, top, &deviceX, &deviceY);
            int width = right-left;
            int height = top-bottom;
            left=deviceX+offsetX;
            top=deviceY+offsetY;
            right=left+width;
            bottom=top+height;
            if(i>=arraySize) {
                env->CallBooleanMethod(arr
                        , arrList_add
                        , env->NewObject(rectF, rectF_, left, top, right, bottom) );
            } else {
                jobject rI = env->CallObjectMethod(arr, arrList_get, i);
                env->CallVoidMethod(rI, rectF_set, left, top, right, bottom);
            }
        }
    }
    return rectCount;



}

extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetCharPos(JNIEnv *env, jobject thiz, jlong pagePtr,
                                                    jint offsetY, jint offsetX, jint width,
                                                    jint height, jobject pt, jlong textPtr, jint idx,
                                                    jboolean loose) {
    // TODO: implement nativeGetCharPos()

    //jclass point = env->FindClass("android/graphics/PointF");
    //jmethodID point_set = env->GetMethodID(point,"set","(FF)V");
    jclass rectF = env->FindClass("android/graphics/RectF");
    jmethodID rectF_ = env->GetMethodID(rectF, "<init>", "(FFFF)V");
    jmethodID rectF_set = env->GetMethodID(rectF, "set", "(FFFF)V");
    double left, top, right, bottom;
    if(loose) {
        FS_RECTF res={0};
        if(!FPDFText_GetLooseCharBox((FPDF_TEXTPAGE)textPtr, idx, &res)) {
            return false;
        }
        left=res.left;
        top=res.top;
        right=res.right;
        bottom=res.bottom;
    } else {
        if(!FPDFText_GetCharBox((FPDF_TEXTPAGE)textPtr, idx, &left, &right, &bottom, &top)) {
            return false;
        }
    }
    int deviceX, deviceY;
    FPDF_PageToDevice((FPDF_PAGE)pagePtr, 0, 0, width, height, 0, left, top, &deviceX, &deviceY);
    width = right-left;
    height = top-bottom;
    left=deviceX+offsetX;
    top=deviceY+offsetY;
    right=left+width;
    bottom=top+height;
    //env->CallVoidMethod(pt, rectF_set, left, top);
    env->CallVoidMethod(pt, rectF_set,((float)left), ((float)top), ((float)right), ((float)bottom));


}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeGetMixedLooseCharPos(JNIEnv *env, jobject thiz,
                                                              jlong pagePtr, jint offsetY,
                                                              jint offsetX, jint width,
                                                              jint height, jobject pt, jlong textPtr,
                                                              jint idx, jboolean loose) {
    // TODO: implement nativeGetMixedLooseCharPos()

    jclass rectF = env->FindClass("android/graphics/RectF");
    jmethodID rectF_ = env->GetMethodID(rectF, "<init>", "(FFFF)V");
    jmethodID rectF_set = env->GetMethodID(rectF, "set", "(FFFF)V");
    double left, top, right, bottom;
    if(!FPDFText_GetCharBox((FPDF_TEXTPAGE)textPtr, idx, &left, &right, &bottom, &top)) {
        return false;
    }
    FS_RECTF res={0};
    if(!FPDFText_GetLooseCharBox((FPDF_TEXTPAGE)textPtr, idx, &res)) {
        return false;
    }
    top=fmax(res.top, top);
    bottom=fmin(res.bottom, bottom);
    left=fmin(res.left, left);
    right=fmax(res.right, right);
    int deviceX, deviceY;
    FPDF_PageToDevice((FPDF_PAGE)pagePtr, 0, 0, width, height, 0, left, top, &deviceX, &deviceY);
    width = right-left;
    height = top-bottom;
    top=deviceY+offsetY;
    left=deviceX+offsetX;
    right=left+width;
    bottom=top+height;
    env->CallVoidMethod(pt, rectF_set,((float)left), ((float)top), ((float)right), ((float)bottom));
    return true;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_megahed_pdfview_PdfiumSDK_nativeCountAnnot(JNIEnv *env, jobject thiz, jlong pagePtr) {
    // TODO: implement nativeCountAnnot()
    return FPDFPage_GetAnnotCount((FPDF_PAGE)pagePtr);
}