# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

# Declares and names the project.

project("mycplus")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

#[[add_library( # Sets the name of the library.
        mycplus

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp)]]

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

#[[find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)]]

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

#[[target_link_libraries( # Specifies the target library.
        mycplus

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})]]




# Specify where JPEGTurbo libraries are located
set(Pdfium_DIR ${CMAKE_SOURCE_DIR}/pdfium)

# shared lib will also be tucked into APK and sent to target
# refer to app/build.gradle, jniLibs section for that purpose.
# ${ANDROID_ABI} is handy for our purpose here. Probably this ${ANDROID_ABI} is
# the most valuable thing of this sample, the rest are pretty much normal cmake

#[[add_library(pdfsdk SHARED IMPORTED)
set_target_properties(pdfsdk PROPERTIES IMPORTED_LOCATION
        ${Pdfium_DIR}/lib/${ANDROID_ABI}/libpdfium.so)]]


# shared lib
add_library(shared-lib1 SHARED IMPORTED)
set_target_properties(shared-lib1 PROPERTIES IMPORTED_LOCATION ${Pdfium_DIR}/lib/${ANDROID_ABI}/libpdfium.cr.so)

add_library(shared-lib2 SHARED IMPORTED)
set_target_properties(shared-lib2 PROPERTIES IMPORTED_LOCATION ${Pdfium_DIR}/lib/${ANDROID_ABI}/libabsl.cr.so)

add_library(shared-lib3 SHARED IMPORTED)
set_target_properties(shared-lib3 PROPERTIES IMPORTED_LOCATION ${Pdfium_DIR}/lib/${ANDROID_ABI}/libicuuc.cr.so)

add_library(shared-lib4 SHARED IMPORTED)
set_target_properties(shared-lib4 PROPERTIES IMPORTED_LOCATION ${Pdfium_DIR}/lib/${ANDROID_ABI}/libchrome_zlib.cr.so)

add_library(shared-lib5 SHARED IMPORTED)
set_target_properties(shared-lib5 PROPERTIES IMPORTED_LOCATION ${Pdfium_DIR}/lib/${ANDROID_ABI}/libc++_chrome.cr.so)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-warn-mismatch")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        pdfsdk_jni

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        pdfsdk_jni.cpp )

target_include_directories(pdfsdk_jni PRIVATE
        ${CMAKE_SOURCE_DIR}/utils
        ${Pdfium_DIR}/include
        ${Pdfium_DIR}/include/cpp )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        pdfsdk_jni
        shared-lib1
        shared-lib2
        shared-lib3
        shared-lib4
        shared-lib5
        jnigraphics
        android
        GLESv2
        log )