package org.transterpreter.occPlug.process;
/*
 * ReaderConduitTest2.java
 * part of the occPlug plugin for the jEdit text editor
 * Copyright (C) 2004-2007 Christian L. Jacobsen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

import java.io.IOException;
import java.io.InputStream;



//}}}

//{{{ Class: ReaderConduitTest2
/**
 * This class is a thread which when given a BufferedReader (probably
 * obtained from say a Process.getErrorStream() call) and a SimpleWriter
 * which is for example using a DocumentWriter object, and is started will
 * take all input from the BufferedReader untill it closes, and write it
 * using the SimpleWriter
 */
public class ReaderConduitTest2 extends Thread
{
	private SimpleWriter outgoing;
	private InputStream incomming;
	public Exception e = null;
	
	public ReaderConduitTest2(InputStream incomming, SimpleWriter outgoing)
	{
		this.incomming = incomming;
		this.outgoing = outgoing;
	}
	
	public void run()
	{
		final byte[] buf = new byte[256];
		int result;
		
		try
		{
			result = incomming.read(buf);
			while(result != -1)
			{
				//Log.log(Log.DEBUG, this, (new Date().toString()) + " " + incomming.available());
				outgoing.write(new String(buf, 0, result));
				result = incomming.read(buf);
			}
		}
		catch(IOException e)
		{
			this.e = e;
		}
		
		try
		{
			incomming.close();
		}
		catch(IOException e)
		{
			this.e = e;
		}			
	}
}