package com.nutiteq.filepicker;

import java.io.FileFilter;

public interface FilePickerActivity {

    String getFileSelectMessage();

    FileFilter getFileFilter();

}
