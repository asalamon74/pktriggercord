package info.melda.sala.pktriggercord;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import java.io.*;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.TimerTask;
import java.util.Timer;
import android.os.Handler;
import java.net.Socket;
import java.net.InetSocketAddress;
import android.os.SystemClock;
import android.os.AsyncTask;

public class MainActivity extends Activity {
    private static final int BUFF_LEN = 1024;
    private static final String SERVER_IP = "localhost";
    private static final int SERVER_PORT = 8888;
    private CliHandler cli;
    private Timer timer;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
	callAsynchronousTask();
    }

    @Override
    public void onDestroy() {
	super.onDestroy();
	/*if( timer != null ) {
	    timer.cancel();
	    timer = null;
	}
		try {
	    CliHandler cli = new CliHandler();
	    cli.execute(true);
	} catch (Exception e) {
	    appendText("Error:"+e.getMessage());
	    }*/
    }
    
    private void callAsynchronousTask() {
	final Handler handler = new Handler();
	timer = new Timer();
	TimerTask doAsynchronousTask = new TimerTask() {       

		@Override
		public void run() {
		    handler.post(new Runnable() {
			    public void run() {       
				try {
				    CliHandler cli = new CliHandler();
				    cli.execute(false);
				} catch (Exception e) {
				    appendText("Error:"+e.getMessage());
				}
			    }
			});
		}
	    };
	timer.schedule(doAsynchronousTask, 0, 5000);
    }


    private void appendText(String txt) {
	Log.v(PkTriggerCord.TAG, "appendText:"+txt);
	TextView text = (TextView) findViewById(R.id.text1);
	text.append(txt);
    }

    private class CliHandler extends AsyncTask<Boolean,Map<String,String>,String> {
	Map<String,String> map;
	DataOutputStream dos;
	BufferedReader br;

	private void readStatus(String fieldName) throws IOException {
	    dos.writeBytes("get_"+fieldName);
	    String answer = br.readLine();
	    map.put(fieldName, answer.substring(2));
	}

	protected String doInBackground(Boolean... stopServer) {
	    map = new HashMap<String,String>();
	    Socket socket=null;
	    try {
		socket = new Socket();
		InetSocketAddress isa = new InetSocketAddress(SERVER_IP, SERVER_PORT);
		socket.connect(isa, 3000);

		byte[] buffer = new byte[2100];
    
		int bytesRead;
		InputStream inputStream = socket.getInputStream();
		br = new BufferedReader(new InputStreamReader(inputStream));
		OutputStream outputStream = socket.getOutputStream();
		dos = new DataOutputStream(outputStream);
		if( stopServer[0] ) {
		    dos.writeBytes("stopserver");
		    String answer=br.readLine();
		    return null;
		}
		dos.writeBytes("connect");
		String answer=br.readLine();
		if( answer == null ) {
		    return "answer null\n";
		}
		if( answer.startsWith("0") ) {
		    dos.writeBytes("update_status");
		    answer = br.readLine();
		    if( answer.startsWith("0") ) {
			readStatus("camera_name");
			readStatus("lens_name");
			readStatus("current_shutter_speed");
			readStatus("current_aperture");
			readStatus("current_iso");
			publishProgress(map);
		    } else {
			return "Cannot update status\n";
		    }
		} else {
		//		    map.put("error_text", answer);
		//    publishProgress(map);
		    return "No camera connected\n";
		}
		return null;
	    } catch( Exception e ) {
		return "Error:"+e+"\n";
	    } finally {
		if( socket != null ) {
		    try {
			socket.shutdownInput();
			socket.shutdownOutput();
			socket.close();		
		    } catch( IOException e ) {
			map.put("error_text", "Cannot close socket");
			publishProgress(map);
		    }
		}
	    }
	}

	protected void onProgressUpdate(Map<String,String>... progress) {
	    TextView cameraStatus = (TextView) findViewById(R.id.camerastatus);
	    TextView lensStatus = (TextView) findViewById(R.id.lensstatus);
	    TextView currentShutterSpeedText = (TextView) findViewById(R.id.currentshutterspeed);
	    TextView currentApertureText = (TextView) findViewById(R.id.currentaperture);
	    TextView currentIsoText = (TextView) findViewById(R.id.currentiso);
	    //	    TextView errorText = (TextView) findViewById(R.id.errortext);
	    for( Map<String,String> pr : progress ) {
		for (Map.Entry<String, String> entry : pr.entrySet()) {
		    appendText(entry.getKey() + "/" + entry.getValue()+"\n");
		    if( "camera_name".equals( entry.getKey() ) ) {
			cameraStatus.setText(entry.getValue());
		    } else if( "lens_name".equals( entry.getKey() ) ) {
			lensStatus.setText(entry.getValue());
		    } else if( "current_shutter_speed".equals( entry.getKey() ) ) {
			currentShutterSpeedText.setText(entry.getValue()+"s");
		    } else if( "current_aperture".equals( entry.getKey() ) ) {
			currentApertureText.setText("f/"+entry.getValue()); 
		    } else if( "current_iso".equals( entry.getKey() ) ) {
			currentIsoText.setText("ISO "+entry.getValue()); 
			//		    } else if( "error_text".equals( entry.getKey() ) ) {
			//			errorText.setText("Error: "+entry.getValue());
		    }
		}
	    }
	}

	protected void onPostExecute(String result) {
	    TextView errorText = (TextView) findViewById(R.id.errortext);
	    if( result != null ) {
		appendText(result);
		errorText.setText(result);
	    } else {
		errorText.setText("");
	    }		
	}
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
	    //	    Process p = new ProcessBuilder()
	    //	    	.command("su", "-c", "system/bin/sh")
	    //	    	.redirectErrorStream(true).
	    //	    	start();
	    Process p = new ProcessBuilder()
	    	.command(commands)
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
	    Log.e( PkTriggerCord.TAG, e.getMessage(), e );
	    appendText("Error "+e.getMessage());
	}
    }

}
