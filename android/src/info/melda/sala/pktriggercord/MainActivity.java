package info.melda.sala.pktriggercord;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.NumberPicker;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
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
import java.util.concurrent.Executors;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.ToneGenerator;
import android.media.AudioManager;

public class MainActivity extends Activity {
    private static final int BUFF_LEN = 1024;
    private static final String SERVER_IP = "localhost";
    private static final int SERVER_PORT = 8888;
    private static final String OUTDIR = "/storage/sdcard0/pktriggercord";
    private CliHandler cli;
    private Bitmap previewBitmap;
    private ProgressBar progressBar;
    private NumberPicker npf;
    private NumberPicker npd;
    private long nextRepeat;
    private int currentRuns;
    private int currentMaxRuns;
    private ToneGenerator toneG;
    private Timer updateTimer = new Timer();
    private Timer actionTimer = new Timer();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
	getWindow().requestFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.main);

	progressBar = (ProgressBar) findViewById(R.id.downloadprogress);
	progressBar.setVisibility( View.INVISIBLE );

	npf = (NumberPicker) findViewById(R.id.frames);
	npf.setMaxValue(99);
	npf.setMinValue(1);
	npf.setWrapSelectorWheel(false);
	npf.setOnValueChangedListener(new NumberPicker.OnValueChangeListener() {
		@Override
		public void onValueChange(NumberPicker picker, int oldVal, int newVal) {
		    npd.setVisibility( newVal == 1 ? View.INVISIBLE : View.VISIBLE);
		}
	    });

	npd = (NumberPicker) findViewById(R.id.delay);
	npd.setMaxValue(120);
	npd.setMinValue(3);
	npd.setWrapSelectorWheel(false);

	npd.setFormatter(new NumberPicker.Formatter() {
		@Override
		public String format(int i) {
		    return String.format("%d s", i);
		}
	});

	if( savedInstanceState != null ) {
	    previewBitmap = savedInstanceState.getParcelable("preview");
	    if( previewBitmap != null ) {
		ImageView preview = (ImageView) findViewById(R.id.preview);
		preview.setImageBitmap( previewBitmap );			
	    }
	    npf.setValue( savedInstanceState.getInt("frames"));
	    npd.setValue( savedInstanceState.getInt("delay"));	    
	}
	npd.setVisibility( npf.getValue() == 1 ? View.INVISIBLE : View.VISIBLE);

	final Button focusButton = (Button) findViewById(R.id.focus);
        focusButton.setOnClickListener(new View.OnClickListener() {
		public void onClick(View v) {
		    callAsynchronousTask(0, 1, 0, 1000, new CliParam("focus"));
		}
	    });
	final Button shutterButton = (Button) findViewById(R.id.shutter);
        shutterButton.setOnClickListener(new View.OnClickListener() {
		public void onClick(View v) {
		    actionTimer = callAsynchronousTask(0, npf.getValue(), 0, 1000*npd.getValue(), new CliParam("shutter"));
		}
	    });
	final Button scriptButton = (Button) findViewById(R.id.script);
        scriptButton.setOnClickListener(new View.OnClickListener() {
		public void onClick(View v) {
                    callAsynchronousTask(0, 1, 0, 1000, new CliParam("focus"));
		}
	    });


	File saveDir = new File(OUTDIR);
	if( !saveDir.exists() && !saveDir.mkdir() ) {
	    Log.e( PkTriggerCord.TAG, "Cannot create output directory" );
	}
	
	updateTimer = callAsynchronousTask(0, 0, 0, 200, (CliParam[])null);

	if( savedInstanceState != null ) {
	    currentRuns = savedInstanceState.getInt("currentRuns");
	    currentMaxRuns = savedInstanceState.getInt("currentMaxRuns");
	    nextRepeat = savedInstanceState.getLong("nextRepeat");

	    if( currentRuns < currentMaxRuns ) {
		long initialDelay = Math.max( 0, nextRepeat - SystemClock.elapsedRealtime());

		TextView timedown = (TextView) findViewById(R.id.timedown);     
		timedown.setVisibility ( View.VISIBLE );
		timedown.setText( String.format("%2d/%d\n ", currentRuns+1, currentMaxRuns));

		actionTimer = callAsynchronousTask( currentRuns, currentMaxRuns, initialDelay, 1000*npd.getValue(), new CliParam("shutter") );
	    }
	}

    }

    @Override
    public void onDestroy() {
	if( updateTimer != null ) {
	    updateTimer.cancel();
	    updateTimer.purge();
	}
	if( actionTimer != null ) {
	    actionTimer.cancel();
	    actionTimer.purge();
	}
	super.onDestroy();
    }

    protected void onSaveInstanceState (Bundle outState) {
	outState.putParcelable("preview", previewBitmap);
	outState.putInt("frames", npf.getValue());
	outState.putInt("delay", npd.getValue());
	outState.putLong("nextRepeat", nextRepeat);
	outState.putInt("currentRuns", currentRuns);
	outState.putInt("currentMaxRuns", currentMaxRuns);
    }

    private Timer callAsynchronousTask(final int cRuns, final int maxRuns, long initialDelay, final long period, final CliParam... param) {
       	final Handler handler = new Handler();
	final Timer timer = new Timer();
	TimerTask doAsynchronousTask = new TimerTask() {       
		private int runs=cRuns;
		@Override
		public void run() {
		    handler.post(new Runnable() {
			    public void run() {  
				if( maxRuns > 1 || param != null ) {
				    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				} else {
				    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
				TextView timedown = (TextView) findViewById(R.id.timedown);     
				try {
				    if( maxRuns > 1 ) {
					timedown.setText( String.format("%2d/%d\n ", runs+1, maxRuns));
					timedown.setVisibility ( View.VISIBLE );
					if( runs+1 != maxRuns ) {
					    nextRepeat = SystemClock.elapsedRealtime()+ period;
					}
					currentRuns = runs+1;
					currentMaxRuns = maxRuns;					
				    } else if( param != null ) {
					timedown.setVisibility ( View.INVISIBLE );
				    } else {
					long tillNextRepeat = nextRepeat - SystemClock.elapsedRealtime();
					timedown.setText( String.format("%2d/%d\n%2.1f", currentRuns, currentMaxRuns, Math.round(1.0 * tillNextRepeat / 100) / 10.0 ));
				    }

				    CliHandler cli = new CliHandler();
				    if( param != null ) {
					cli.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR, param);
				    } else {
					cli.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
				    }
				} catch (Exception e) {
				    appendText("Error:"+e.getMessage());
				}
				if( ++runs == maxRuns ) {
				    timer.cancel();
				    timedown.setVisibility ( View.INVISIBLE );				    
				}
			    }
			});
		}
	    };
	timer.schedule(doAsynchronousTask, initialDelay, period);
	//	timer.scheduleAtFixedRate(doAsynchronousTask, 0, 50);
	return timer;
    }

    private void appendText(String txt) {
	Log.v(PkTriggerCord.TAG, "appendText:"+txt);
	//	TextView text = (TextView) findViewById(R.id.text1);
	//	text.append(txt);
    }

    private static class CliParam {
	String command;
	
	public CliParam(String command) {
            this.command = command;
	}

        private static CliParam[] parseCommands(String commands) {
            String[] commandArray = commands.split(";");
            CliParam []params = new CliParam[commandArray.length];
            int index = 0;
            for( String str : commandArray ) {
                params[index++] = new CliParam(str);
            }
            return params;
        }

    }

    private void beep() {
	if( toneG == null ) {
	    toneG = new ToneGenerator(AudioManager.STREAM_ALARM, 100);
	} else {
	    toneG.release();
	    toneG = new ToneGenerator(AudioManager.STREAM_ALARM, 100);
	}
	toneG.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, 200);
	//	toneG.release();
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

	private boolean answerStatus(String answer) {
	    return answer.startsWith("0");
	}

	private class DownloadProgressCallback implements PkTriggerCord.ProgressCallback {

	    Map<String,Object> pmap = new HashMap<String, Object>();

	    public void progressUpdate(double progress) {
		pmap.put("progress", (int)(100*progress));
		publishProgress(pmap);
	    }
	}
       	
	protected String doInBackground(CliParam... params) {

	    /*	    if( params.length > 0 ) {
		beep();
		return "";
		}*/

	    long time1 = SystemClock.elapsedRealtime();
	    map = new HashMap<String,Object>();
	    Socket socket=null;
	    SimpleDateFormat logDf = new SimpleDateFormat("yyyyMMdd_HHmmss.SSS");
	    DownloadProgressCallback pCallback = new DownloadProgressCallback();
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
			Calendar c = Calendar.getInstance();
			String formattedDate = logDf.format(c.getTime());
			Log.i( PkTriggerCord.TAG, "send shutter: "+formattedDate);
			dos.writeBytes(param.command);
			answer=readLine();
			c = Calendar.getInstance();
			formattedDate = logDf.format(c.getTime());			
			Log.i( PkTriggerCord.TAG, "read answer: "+formattedDate);
			/*			if( "shutter".equals(param.command) ) {
			    Integer frames = (Integer)param.getValue("frames");
			    int frameNum = frames == null ? 1 : frames.intValue();
			    Integer delay = (Integer)param.getValue("delay");
			    int delaySec = delay == null ? 0 : delay.intValue();
			    if( frameNum > 1 ) {
				for( int fIndex=2; fIndex <= frameNum; ++fIndex ) {
				    SystemClock.sleep( delay * 1000 );
				    dos.writeBytes(param.command);
				    answer=readLine();
				}
			    }
			    }*/
		    }
		    return null;
		}
		Calendar c = Calendar.getInstance();
		String formattedDate = logDf.format(c.getTime());			
		Log.i( PkTriggerCord.TAG, "connect: "+formattedDate);
		dos.writeBytes("connect");
		answer=readLine();
		if( answer == null ) {
		    return "answer null";
		}
		if( answerStatus(answer)  ) {
		    dos.writeBytes("update_status");
		    answer=readLine();
		    if( answerStatus(answer) ) {
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
			    c = Calendar.getInstance();
			    SimpleDateFormat df = new SimpleDateFormat("yyyyMMdd_HHmmss");
			    formattedDate = df.format(c.getTime());
			    File outFile = new File(OUTDIR + "/pktriggercord_"+formattedDate+".dng");
			    os = new FileOutputStream(outFile);
			    PkTriggerCord.copyStream( is, os, jpegLength, pCallback );
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
		Log.i( PkTriggerCord.TAG, "time "+ (time2-time1) + " ms");
		return "time "+ (time2-time1) + " ms";
	    } catch( Exception e ) {
		return "Error:"+e+"\n";
	    } finally {
		if( socket != null && !socket.isClosed() ) {
		    try {
			if( !socket.isInputShutdown() ) {
			    socket.shutdownInput();
			}
			if( !socket.isOutputShutdown() ) {
			    socket.shutdownOutput();
			}
			socket.close();		
		    } catch( IOException e ) {
			Log.e( "Cannot close socket", e.getMessage(), e);
			//			return "Cannot close socket";
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
		    } else if( "progress".equals( entry.getKey() ) ) {
			int prog = (Integer)entry.getValue();
			progressBar.setProgress(prog);
			progressBar.setVisibility( prog>0 && prog<100 ? View.VISIBLE : View.INVISIBLE );
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
