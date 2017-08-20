package org.mamedev.mame;

import java.io.*;
import android.app.*;
import android.os.*;
import android.content.res.AssetManager;
import android.util.Log;
import org.libsdl.app.SDLActivity;
import android.view.*;
/**
    SDL Activity
*/
public class MAME extends SDLActivity {
    private static final String TAG = "MAME";
    // Setup
    @Override
    protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
	//	Log.v(TAG, "getExternalFilesDir:" + getExternalFilesDir(null));
		copyAssetAll("mame.ini");
		copyAssetAll("ui.ini");
		copyAssetAll("roms");
    }
	
	public void copyAssetAll(String srcPath) {
		AssetManager assetMgr = this.getAssets();
		String assets[] = null;
		try {
			String destPath = getExternalFilesDir(null) + File.separator + srcPath;
			assets = assetMgr.list(srcPath);
			if (assets.length == 0) {
				copyFile(srcPath, destPath);
			} else {
				File dir = new File(destPath);
				if (!dir.exists())
					dir.mkdir();
				for (String element : assets) {
					copyAssetAll(srcPath + File.separator + element);
				}
			}
		} 
		catch (IOException e) {
		   e.printStackTrace();
		}
	}
	public void copyFile(String srcFile, String destFile) {
		AssetManager assetMgr = this.getAssets();
	  
		InputStream is = null;
		OutputStream os = null;
		try {
			is = assetMgr.open(srcFile);
			if (new File(destFile).exists() == false)
			{
				os = new FileOutputStream(destFile);
		  
				byte[] buffer = new byte[1024];
				int read;
				while ((read = is.read(buffer)) != -1) {
					os.write(buffer, 0, read);
				}
				is.close();
				os.flush();
				os.close();
				Log.v(TAG, "copy from Asset:" + destFile);
			}
		} 
		catch (IOException e) {
			e.printStackTrace();
		}
	}
	
    //variable for counting two successive up-down events
   int clickCount = 0;
    //variable for storing the time of first click
   long startTime;
    //variable for calculating the total time
   long duration;
    //constant for defining the time duration between the click that can be considered as double-tap
   static final int MAX_DURATION = 500;

	View.OnTouchListener MyOnTouchListener = new View.OnTouchListener()
	{   
		// Touch events
		@Override
		public boolean onTouch(View v, MotionEvent event) {
			switch(event.getAction() & MotionEvent.ACTION_MASK)
			{
			case MotionEvent.ACTION_DOWN:
				startTime = System.currentTimeMillis();
				clickCount++;
				break;
			case MotionEvent.ACTION_UP:
				long time = System.currentTimeMillis() - startTime;
				duration=  duration + time;
				if(clickCount == 2)
				{
					if(duration<= MAX_DURATION)
					{
						//Toast.makeText(captureActivity.this, "double tap",Toast.LENGTH_LONG).show();
						SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_ENTER);
					}
					clickCount = 0;
					duration = 0;
					break;             
				}
			}
			return true; 		
		}
	};
}
