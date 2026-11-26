package io.github.swowanders.twominds;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Base64;

import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;

/** Saves a file the app made (a story, an export, a recording) into the phone's Downloads folder.
 *  Android's WebView ignores web-style downloads, so the page hands the bytes to this instead. */
@CapacitorPlugin(name = "Files")
public class FilesPlugin extends Plugin {
    @PluginMethod
    public void save(PluginCall call) {
        String name = call.getString("name", "pip-and-moss.txt").replaceAll("[^A-Za-z0-9._ '()-]", "_");
        String mime = call.getString("mime", "application/octet-stream");
        String data = call.getString("data", "");
        new Thread(() -> {
            try {
                byte[] bytes = Base64.decode(data, Base64.DEFAULT);
                String where;
                if (Build.VERSION.SDK_INT >= 29) {
                    ContentResolver cr = getContext().getContentResolver();
                    ContentValues v = new ContentValues();
                    v.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
                    v.put(MediaStore.MediaColumns.MIME_TYPE, mime);
                    v.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/Pip and Moss");
                    Uri uri = cr.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                    if (uri == null) throw new Exception("the phone wouldn't make the file");
                    try (OutputStream os = cr.openOutputStream(uri)) { os.write(bytes); }
                    where = "Downloads/Pip and Moss/" + name;
                } else {
                    File dir = new File(getContext().getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS), "Pip and Moss");
                    if (!dir.exists() && !dir.mkdirs()) throw new Exception("couldn't make the folder");
                    File f = new File(dir, name);
                    try (FileOutputStream os = new FileOutputStream(f)) { os.write(bytes); }
                    where = f.getAbsolutePath();
                }
                JSObject r = new JSObject(); r.put("path", where); call.resolve(r);
            } catch (Exception e) { call.reject("couldn't save: " + e.getMessage()); }
        }).start();
    }
}
