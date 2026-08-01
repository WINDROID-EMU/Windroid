package com.windroid.emu.core;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class WinetricksCache {
    private static final String CACHE_FILE_NAME = "winetricks_packages_cache.txt";
    private static final String PREFS_NAME = "winetricks_cache_prefs";
    private static final String KEY_CACHE_TIMESTAMP = "cache_timestamp";
    private static final long CACHE_VALIDITY_DAYS = 7; // Cache válido por 7 dias
    
    private static List<WinetricksItem> cachedItems = null;
    
    public static boolean hasValidCache(Context context) {
        if (cachedItems != null && !cachedItems.isEmpty()) {
            return true;
        }
        
        File cacheFile = new File(context.getCacheDir(), CACHE_FILE_NAME);
        if (!cacheFile.exists()) {
            return false;
        }
        
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        long timestamp = prefs.getLong(KEY_CACHE_TIMESTAMP, 0);
        long now = System.currentTimeMillis();
        long validityMillis = CACHE_VALIDITY_DAYS * 24 * 60 * 60 * 1000;
        
        return (now - timestamp) < validityMillis;
    }
    
    public static List<WinetricksItem> loadFromCache(Context context) {
        if (cachedItems != null && !cachedItems.isEmpty()) {
            return new ArrayList<>(cachedItems);
        }
        
        List<WinetricksItem> items = new ArrayList<>();
        File cacheFile = new File(context.getCacheDir(), CACHE_FILE_NAME);
        
        if (!cacheFile.exists()) {
            return items;
        }
        
        try (BufferedReader reader = new BufferedReader(new FileReader(cacheFile))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String[] parts = line.split("\\|", 5);
                if (parts.length >= 5) {
                    items.add(new WinetricksItem(parts[0], parts[2], parts[1], parts[3], Integer.parseInt(parts[4])));
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        
        cachedItems = new ArrayList<>(items);
        return items;
    }
    
    public static void saveToCache(Context context, List<WinetricksItem> items) {
        cachedItems = new ArrayList<>(items);
        
        File cacheFile = new File(context.getCacheDir(), CACHE_FILE_NAME);
        try (FileWriter writer = new FileWriter(cacheFile)) {
            for (WinetricksItem item : items) {
                writer.write(item.getName() + "|" + item.getCategory() + "|" + item.getDescription() + "|" + item.getSimpleName() + "|" + item.getIconResId() + "\n");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putLong(KEY_CACHE_TIMESTAMP, System.currentTimeMillis()).apply();
    }
    
    public static void clearCache(Context context) {
        cachedItems = null;
        File cacheFile = new File(context.getCacheDir(), CACHE_FILE_NAME);
        if (cacheFile.exists()) {
            cacheFile.delete();
        }
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().remove(KEY_CACHE_TIMESTAMP).apply();
    }
}
