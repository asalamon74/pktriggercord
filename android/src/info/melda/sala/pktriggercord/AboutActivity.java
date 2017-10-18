package info.melda.sala.pktriggercord;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
import android.util.Log;
import android.content.pm.PackageManager;
import android.text.method.LinkMovementMethod;

public class AboutActivity extends Activity {

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.about);

        TextView aboutTextView = (TextView) findViewById(R.id.abouttext);
        String appName = getResources().getString(R.string.app_name);
        String versionName="";
        try {
            versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch ( PackageManager.NameNotFoundException e ) {
            // should not happen
            Log.e(PkTriggerCord.TAG, "Cannot read version number", e);
        }
        String copyright = getResources().getString(R.string.copyright);
        String license = getResources().getString(R.string.license);
        String weburl = getResources().getString(R.string.weburl);
        aboutTextView.setText( appName+" "+versionName+"\n\n"+copyright+"\n\n"+license+"\n\n"+weburl );

        aboutTextView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
