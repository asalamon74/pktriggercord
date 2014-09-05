package info.melda.sala.pktriggercord;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.NumberPicker;
import android.view.View;
import android.view.Window;
import java.io.*;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.TimerTask;
import java.util.Timer;
import java.util.Calendar;
import java.text.SimpleDateFormat;
import android.os.Handler;
import android.os.Environment;
import java.net.Socket;
import java.net.InetSocketAddress;
import android.os.SystemClock;
import android.os.AsyncTask;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

public class MainActivity extends Activity {
    private static final int BUFF_LEN = 1024;
    private static final String SERVER_IP = "localhost";
    private static final int SERVER_PORT = 8888;
    private static final String OUTDIR = "/storage/sdcard0/pktriggercord";
    private CliHandler cli;
    private Timer timer;
    private Bitmap previewBitmap;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
	getWindow().requestFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.main);

	if( savedInstanceState != null ) {
	    previewBitmap = savedInstanceState.getParcelable("preview");
	    if( previewBitmap != null ) {
		ImageView preview = (ImageView) findViewById(R.id.preview);
		preview.setImageBitmap( previewBitmap );			
	    }
	}
	//	View textScroller = findViewById(R.id.scrolltext);
	//	textScroller.setVisibility( View.GONE );

	final Button focusButton = (Button) findViewById(R.id.focus);
        focusButton.setOnClickListener(new View.OnClickListener() {
		public void onClick(View v) {
		    CliHandler cli = new CliHandler();
		    cli.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR, new CliParam("focus"));
		}
	    });
	final Button shutterButton = (Button) findViewById(R.id.shutter);
        shutterButton.setOnClickListener(new View.OnClickListener() {
		public void onClick(View v) {
		    CliHandler cli = new CliHandler();
		    cli.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR, new CliParam("shutter"));
		}
	    });

	final NumberPicker npf = (NumberPicker) findViewById(R.id.frames);
	final NumberPicker npd = (NumberPicker) findViewById(R.id.delay);
	npd.setVisibility( View.INVISIBLE );
	npf.setMaxValue(9);
	npf.setMinValue(1);
	npf.setWrapSelectorWheel(false);
	npf.setOnValueChangedListener(new NumberPicker.OnValueChangeListener() {
		@Override
		public void onValueChange(NumberPicker picker, int oldVal, int newVal) {
		    npd.setVisibility( newVal == 1 ? View.INVISIBLE : View.VISIBLE);
		}
	    });


	npd.setMaxValue(60);
	npd.setMinValue(5);
	npd.setWrapSelectorWheel(false);


	File saveDir = new File(OUTDIR);
	if( !saveDir.mkdir() ) {
	    Log.e( PkTriggerCord.TAG, "Cannot create output directory" );
	}
	callAsynchronousTask();
    }

    @Override
    public void onDestroy() {
	timer.cancel();
	super.onDestroy();
    }

    protected void onSaveInstanceState (Bundle outState) {
	outState.putParcelable("preview", previewBitmap);
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
				    cli.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
				} catch (Exception e) {
				    appendText("Error:"+e.getMessage());
				}
			    }
			});
		}
	    };
	timer.schedule(doAsynchronousTask, 0, 1000);
	//	timer.scheduleAtFixedRate(doAsynchronousTask, 0, 50);
    }

    private void appendText(String txt) {
	Log.v(PkTriggerCord.TAG, "appendText:"+txt);
	//	TextView text = (TextView) findViewById(R.id.text1);
	//	text.append(txt);
    }

    private class CliParam {
	String command;
	Map commandParams;
	
	public CliParam(String command, Map commandParams) {
	    this.command = command;
	    this.commandParams = commandParams;
	}

	public CliParam(String command) {
	    this(command, null);
	}
    }

    private class CliHandler extends AsyncTask<CliParam,Map<String,Object>,String> {
	Map<String,Object> map;
	DataOutputStream dos;
	InputStream is;

	private String readLine() throws IOException {
	    byte []buffer = new byte[2100];
	    int bindex=0;
	    int intBuf=is.read();
	    while( intBuf != 10 && intBuf != -1 ) {
		buffer[bindex++] = (byte)intBuf;
		intBuf=is.read();
	    }
	    return new String( buffer, 0, bindex );
	}

	private void readStatus(String fieldName) throws IOException {
	    dos.writeBytes("get_"+fieldName);
	    String answer = readLine();
	    map.put(fieldName, answer.substring(2));
	}

	private int getIntParam(String str) {
	    String[] separated = str.split(" ");
	    return Integer.parseInt(separated[1]);
	}
       
	protected String doInBackground(CliParam... params) {
	    long time1 = SystemClock.elapsedRealtime();
	    map = new HashMap<String,Object>();
	    Socket socket=null;
	    try {
		socket = new Socket();
		InetSocketAddress isa = new InetSocketAddress(SERVER_IP, SERVER_PORT);
		socket.connect(isa, 3000);

		byte[] buffer = new byte[2100];
    
		int bytesRead;
		is = socket.getInputStream();
		OutputStream outputStream = socket.getOutputStream();
		dos = new DataOutputStream(outputStream);
		String answer;
		int jpegBytes;
		if( params.length > 0 ) {
		    for( CliParam param : params ) {
			dos.writeBytes(param.command);
			answer=readLine();
		    }
		    return null;
		}
		dos.writeBytes("connect");
		answer=readLine();
		if( answer == null ) {
		    return "answer null";
		}
		if( answer.startsWith("0") ) {
		    dos.writeBytes("update_status");
		    answer=readLine();
		    if( answer.startsWith("0") ) {
			readStatus("camera_name");
			readStatus("lens_name");
			readStatus("current_shutter_speed");
			readStatus("current_aperture");
			readStatus("current_iso");
			readStatus("bufmask");
			if( !"0".equals(map.get("bufmask")) ) {
			    // assuming that the first buffer contains the image
			    // TODO: fix
			    dos.writeBytes("get_preview_buffer");
			    answer = readLine();
			    map.put("answer", answer);
			    int jpegLength = getIntParam(answer);

			    ByteArrayOutputStream bos = new ByteArrayOutputStream(jpegLength);
			    PkTriggerCord.copyStream( is, bos, jpegLength );
			    Bitmap bm = BitmapFactory.decodeByteArray(bos.toByteArray(), 0, jpegLength);
			    map.put("jpegbytes",jpegLength);
			    map.put("preview",bm);
			    publishProgress(map);
			    dos.writeBytes("get_buffer");
			    answer = readLine();
			    map.put("answer", answer);
			    jpegLength = getIntParam(answer);
			    OutputStream os;
			    Calendar c = Calendar.getInstance();
			    SimpleDateFormat df = new SimpleDateFormat("yyyyMMdd_HHmmss");
			    String formattedDate = df.format(c.getTime());
			    File outFile = new File(OUTDIR + "/pktriggercord_"+formattedDate+".dng");
			    os = new FileOutputStream(outFile);
			    PkTriggerCord.copyStream( is, os, jpegLength );
			    os.close();
			    map.put("jpegbytes",jpegLength);
			    publishProgress(map);

			    dos.writeBytes("delete_buffer");
			    answer = readLine();
			}
			publishProgress(map);
		    } else {
			return "Cannot update status";
		    }
		} else {
		    return "No camera connected";
		}
		long time2 = SystemClock.elapsedRealtime();
		return "time "+ (time2-time1) + " ms";
	    } catch( Exception e ) {
		return "Error:"+e+"\n";
	    } finally {
		if( socket != null ) {
		    try {
			socket.shutdownInput();
			socket.shutdownOutput();
			socket.close();		
		    } catch( IOException e ) {
			return "Cannot close socket";
		    }
		}
	    }
	}

	protected void onProgressUpdate(Map<String,Object>... progress) {
	    TextView cameraStatus = (TextView) findViewById(R.id.camerastatus);
	    TextView lensStatus = (TextView) findViewById(R.id.lensstatus);
	    TextView currentShutterSpeedText = (TextView) findViewById(R.id.currentshutterspeed);
	    TextView currentApertureText = (TextView) findViewById(R.id.currentaperture);
	    TextView currentIsoText = (TextView) findViewById(R.id.currentiso);
	    for( Map<String,Object> pr : progress ) {
		for (Map.Entry<String, Object> entry : pr.entrySet()) {
		    appendText(entry.getKey() + "/" + entry.getValue()+"\n");
		    if( "camera_name".equals( entry.getKey() ) ) {
			cameraStatus.setText((String)entry.getValue());
		    } else if( "lens_name".equals( entry.getKey() ) ) {
			lensStatus.setText((String)entry.getValue());
		    } else if( "current_shutter_speed".equals( entry.getKey() ) ) {
			currentShutterSpeedText.setText(entry.getValue()+"s");
		    } else if( "current_aperture".equals( entry.getKey() ) ) {
			currentApertureText.setText("f/"+entry.getValue()); 
		    } else if( "current_iso".equals( entry.getKey() ) ) {
			currentIsoText.setText("ISO "+entry.getValue()); 
		    } else if( "preview".equals( entry.getKey() ) ) {
			ImageView preview = (ImageView) findViewById(R.id.preview);
			previewBitmap = (Bitmap)entry.getValue();
			preview.setImageBitmap( (Bitmap)entry.getValue());
			//			preview.setAdjustViewBounds(true);        
			//			preview.setScaleType(ImageView.ScaleType.FIT_CENTER);
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
