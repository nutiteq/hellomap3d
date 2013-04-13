package com.nutiteq.advancedmap.mapquest;

import android.content.SearchRecentSuggestionsProvider;

/**
 * To create a search suggestions provider using the built-in recent queries mode, 
 * simply extend SearchRecentSuggestionsProvider as shown here, and configure with
 * a unique authority and the mode you with to use.  For more information, see
 * {@link android.content.SearchRecentSuggestionsProvider}.
 */
public class SearchSuggestionProvider extends SearchRecentSuggestionsProvider {
    
    /**
     * This is the provider authority identifier.  The same string must appear in your
     * Manifest file, and any time you instantiate a 
     * {@link android.provider.SearchRecentSuggestions} helper class. 
     */
    final static String AUTHORITY = "com.nutiteq.osm";
    /**
     * These flags determine the operating mode of the suggestions provider.  This value should 
     * not change from run to run, because when it does change, your suggestions database may 
     * be wiped.
     */
    final static int MODE = DATABASE_MODE_QUERIES;
    
    /**
     * The main job of the constructor is to call {@link #setupSuggestions(String, int)} with the
     * appropriate configuration values.
     */
    public SearchSuggestionProvider() {
        super();
        setupSuggestions(AUTHORITY, MODE);
    }
}
