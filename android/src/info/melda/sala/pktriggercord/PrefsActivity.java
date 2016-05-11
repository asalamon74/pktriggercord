package info.melda.sala.pktriggercord;

import android.os.Bundle;
import android.preference.PreferenceActivity;

/**
 *
 * @author salamon
 */
public class PrefsActivity extends PreferenceActivity {

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        addPreferencesFromResource(R.xml.prefs);
    }
}
