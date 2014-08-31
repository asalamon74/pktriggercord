package info.melda.sala.pktriggercord;

import android.util.Log;
import android.app.Application;
import android.content.res.AssetManager;
import java.io.*;
import java.util.ArrayList;
import java.util.List;
import android.os.SystemClock;

public class PkTriggerCord extends Application {
    public static final String TAG = "PkTriggerCord";
    private static final String CLI_HOME = "/data/local/";
    private static final String CLI_DIR = "pktriggercord";
    private Process p;
    
    @Override
    public void onCreate() {
	super.onCreate();
	Log.v( TAG, "Before installCli");
	installCli();
	Log.v( TAG, "After installCli");
	startCli();
    }

    private void startCli() {
	try {
	    p = new ProcessBuilder()
		.command("su", "-c", CLI_HOME+CLI_DIR+"/pktriggercord-cli --servermode")
		.start();
	    SystemClock.sleep(1000);
	} catch( Exception e ) {
	    Log.e( TAG, e.getMessage(), e);
	}
    }

    private void installCli() {
	try {
	    createCliDir();
	    copyAsset("pktriggercord-cli");
	} catch( Exception e ) {
	    Log.e( TAG, e.getMessage(), e );
	}
    }

    private void copyFile(InputStream in, OutputStream out) throws IOException {
	byte[] buffer = new byte[1024];
	int read;
	while((read = in.read(buffer)) != -1){
	    out.write(buffer, 0, read);
	}
    }

    private void simpleSudoWrapper(String command) throws Exception {
	Process p = Runtime.getRuntime().exec(new String[]{"su", "-c", command});
	p.waitFor();
	p.destroy();
    }

    private void simpleSudoWrapper(List<String> commands) throws Exception {
	String appendedCommands="";
	String sep = "";
	for( String command : commands ) {
	    appendedCommands += sep + command;
	    sep = " && ";
	}
	simpleSudoWrapper( appendedCommands );
    }

    private void createCliDir() throws Exception {
	final int myUid = android.os.Process.myUid();
	simpleSudoWrapper( new ArrayList<String>() {{
		    add("mkdir -p "+CLI_HOME+CLI_DIR);
		    add("chown "+myUid+" "+CLI_HOME+CLI_DIR);
		    add("chmod 777 "+CLI_HOME+CLI_DIR);
		}});
    }

    private void copyAsset(String fileName) throws Exception {
        AssetManager assetManager =  getAssets();
	InputStream in = assetManager.open(fileName);
	String fullFileName = CLI_HOME + CLI_DIR + "/" + fileName;
	//	appendText("Before rm\n");
	simpleSudoWrapper("rm -f "+fullFileName);
	//	appendText("After rm\n");
	OutputStream out = new FileOutputStream(fullFileName);
	//	appendText("Before copyFile\n");
        copyFile(in, out);

	//	appendText("After copyFile\n");
        in.close();
	//        in = null;
        out.flush();
        out.close();
	//        out = null;
	//	appendText("Before chmod\n");
	//	simpleSudoWrapper("chown root "+fullFileName);
	simpleSudoWrapper("chmod 4777 "+fullFileName);
	//	appendText("After chmod\n");
    }

}
