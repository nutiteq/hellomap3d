package com.nutiteq.utils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;

import com.nutiteq.log.Log;

public class MGMUtils {

  public static String[] split(final String s, final char c, final boolean dblquotes, final int max) {
    int j = 0;
    final Vector<String> vector = new Vector<String>();

    // add first max-1 components
    int num = 0;
    int i = 0;
    String ss = null;
    int k1;
    int k2;
    for (i = 0; num != max - 1; i = j + 1) {
      k1 = -1;
      k2 = -1;
      j = s.indexOf(c, i);
      if (dblquotes) {
        // should have k1=0
        k1 = s.indexOf('"', i);
        // quote found and before delimiter
        if (k1 >= 0 && k1 < j) {
          // next quote
          k2 = s.indexOf('"', k1 + 1);
          if (k2 >= 0) {
            // recompute next delimiter - should have j=k2+1
            j = s.indexOf(c, k2 + 1);
          }
        }
      }
      if (j >= 0) {
        if (dblquotes && k1 >= 0 && k2 >= 0) {
          ss = s.substring(k1 + 1, k2);
        } else {
          ss = s.substring(i, j);
        }
        vector.addElement(ss);
        num++;
      } else {
        if (dblquotes && k1 >= 0 && k2 >= 0) {
          ss = s.substring(k1 + 1, k2);
        } else {
          ss = s.substring(i);
        }
        vector.addElement(ss);
        num++;
        break;
      }
    }

    // add the max-th component
    k1 = -1;
    k2 = -1;
    if (max != 0 && j >= 0) {
      if (dblquotes) {
        k1 = s.indexOf('"', i);
        // quote found and before delimiter
        if (k1 >= 0) {
          // next quote
          k2 = s.indexOf('"', k1 + 1);
        }
      }
      if (dblquotes && k1 >= 0 && k2 >= 0) {
        ss = s.substring(k1 + 1, k2);
      } else {
        ss = s.substring(i);
      }
      vector.addElement(ss);
      num++;
    }

    // convert to array
    final String as[] = new String[num];
    vector.copyInto(as);

    // return the array
    return as;
  }

  public static byte[] readFully(final InputStream is) {
    ByteArrayOutputStream out = null;
    final byte[] buffer = new byte[1024];
    byte[] result;
    try {
      out = new ByteArrayOutputStream();
      int read;
      while ((read = is.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
      out.flush();
      result = out.toByteArray();
    } catch (final IOException e) {
      Log.error(MGMUtils.class.getName() + ": Failed to read the stream. " + e.getMessage());
      result = new byte[0];
    } finally {
      try {
        out.close();
      } catch (IOException e) {
        Log.error(MGMUtils.class.getName() + ": Failed to close the stream. " + e.getMessage());
      }
    }
    return result;
  }

  public static int skip(final InputStream is, final int n, final int bufferSize) throws IOException {
    int rd = 0;
    long ch = 0;
    while (rd < n && ch >= 0) {
      final long cn = (n - rd > bufferSize) ? bufferSize : (n - rd);
      ch = is.skip(cn);

      if (ch > 0) {
        rd += ch;
      }
    }
    return rd;
  }

}
