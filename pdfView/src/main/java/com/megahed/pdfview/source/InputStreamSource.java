/*
 * Copyright (C) 2016 Bartosz Schiller.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.megahed.pdfview.source;

import android.content.Context;
import android.widget.Toast;

import androidx.core.content.ContextCompat;

import com.megahed.pdfview.PdfiumSDK;
import com.megahed.pdfview.util.Util;

import java.io.IOException;
import java.io.InputStream;

public class InputStreamSource implements DocumentSource {

    private InputStream inputStream;
    private String s;

    public InputStreamSource() {
    }

    public InputStreamSource(InputStream inputStream, String s) {
        this.inputStream = inputStream;
        this.s = s;
    }

    @Override
    public void createDocument(Context context, PdfiumSDK core, String password) throws IOException {
         core.newDocument(Util.toByteArray(inputStream), password);
         s=core.getS();
    }

    public String getS() {
        return s;
    }
}
