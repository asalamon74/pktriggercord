package info.melda.sala.pktriggercord;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.content.res.AssetManager;
import android.widget.TextView;
import java.io.FileOutputStream;
import java.io.*;
import java.util.List;
import java.util.ArrayList;

public class MainActivity extends Activity {
    private static final String TAG = "PkTriggerCord";
    private static final String CLI_HOME = "/data/local/";
    private static final String CLI_DIR = "pktriggercord";
    private static final int BUFF_LEN = 1024;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
	installCli();
	commandWrapper(false, CLI_HOME+CLI_DIR+"/pktriggercord-cli --status --debug --timeout 3");
    }

    private void appendText(String txt) {
	TextView text = (TextView) findViewById(R.id.text1);
	text.append(txt);
    }

    private void copyFile(InputStream in, OutputStream out) throws IOException {
	byte[] buffer = new byte[1024];
	int read;
	while((read = in.read(buffer)) != -1){
	    out.write(buffer, 0, read);
	}
    }

    private void createCliDir() throws Exception {
	final int myUid = android.os.Process.myUid();
	simpleSudoWrapper( new ArrayList<String>() {{
		    add("mkdir -p "+CLI_HOME+CLI_DIR);
		    add("chown "+myUid+" "+CLI_HOME+CLI_DIR);
		    add("chmod 700 "+CLI_HOME+CLI_DIR);
		}});
    }

    private void copyAsset(String fileName) throws Exception {
        AssetManager assetManager =  getAssets();
	InputStream in = assetManager.open(fileName);
	String fullFileName = CLI_HOME + CLI_DIR + "/" + fileName;
	appendText("Before rm\n");
	simpleSudoWrapper("rm -f "+fullFileName);
	appendText("After rm\n");
	OutputStream out = new FileOutputStream(fullFileName);
	appendText("Before copyFile\n");
        copyFile(in, out);
	appendText("After copyFile\n");
        in.close();
	//        in = null;
        out.flush();
        out.close();
	//        out = null;
	appendText("Before chmod\n");
	simpleSudoWrapper("chmod 4700 "+fullFileName);
	appendText("After chmod\n");
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


    private void commandWrapper(boolean asRoot,String command) {
	appendText("Executing as "+(asRoot ? "" : "non-") + "root");
	List<String> commands = new ArrayList<String>();
	if( asRoot ) {
	    commands.add("su");
	    commands.add("-c");
	}
	commands.add("system/bin/sh");

	try {
	    Process p = new ProcessBuilder()
	    	.command("su", "-c", "system/bin/sh")
	    	.redirectErrorStream(true).
	    	start();
	    DataOutputStream stdin = new DataOutputStream(p.getOutputStream());
	    stdin.writeBytes(command+"\n"); // \n executes the command
	    stdin.flush();
	    stdin.writeBytes("exit\n");
            stdin.flush();
	    int suProcessRetval = p.waitFor();
	    appendText("suRet:"+suProcessRetval+"\n");
	    InputStream stdout = p.getInputStream();
	    BufferedReader br = new BufferedReader(new InputStreamReader(stdout));
	    String line;
	    String outputStr="";
	    while (br.ready()) {
		outputStr += br.readLine();
		outputStr += "\n";
	    }	    
	    appendText("out:"+outputStr+"\n");
	} catch( Exception e ) {
	    Log.e( TAG, e.getMessage(), e );
	    appendText("Error "+e.getMessage());
	}
    }

    private void installCli() {
	try {
	    appendText("Before copyAsset\n");
	    createCliDir();
	    copyAsset("pktriggercord-cli");
	    appendText("After copyAsset\n");
	} catch( Exception e ) {
	    Log.e( TAG, e.getMessage(), e );
	}
    }
}
