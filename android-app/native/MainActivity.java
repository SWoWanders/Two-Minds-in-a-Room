package io.github.swowanders.twominds;

import android.os.Bundle;
import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        registerPlugin(LlamaPlugin.class);
        registerPlugin(VoicePlugin.class);
        registerPlugin(FilesPlugin.class);
        super.onCreate(savedInstanceState);
    }
}
