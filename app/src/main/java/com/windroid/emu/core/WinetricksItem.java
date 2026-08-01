package com.windroid.emu.core;

import android.content.Context;

public class WinetricksItem {
    private final String name;
    private final String description;
    private final String category;
    private final String simpleName;
    private final int iconResId;
    private boolean isSelected = false;
    private boolean isInstalled = false;

    public WinetricksItem(String name, String description, String category, String simpleName, int iconResId) {
        this.name = name;
        this.description = description;
        this.category = category;
        this.simpleName = simpleName;
        this.iconResId = iconResId;
    }

    public String getName() {
        return name;
    }

    public String getDescription() {
        return description;
    }

    public String getCategory() {
        return category;
    }

    public String getSimpleName() {
        return simpleName;
    }

    public int getIconResId() {
        return iconResId;
    }

    public boolean isSelected() {
        return isSelected;
    }

    public void setSelected(boolean selected) {
        isSelected = selected;
    }

    public boolean isInstalled() {
        return isInstalled;
    }

    public void setInstalled(boolean installed) {
        isInstalled = installed;
    }

    // Mapeamento de nomes simplificados e ícones
    public static String getSimpleNameForPackage(String name) {
        String lower = name.toLowerCase();
        
        // DLLs comuns
        if (lower.contains("vcrun")) return name.replaceAll("(?i)vcrun", "VC++ ");
        if (lower.contains("directx")) return "DirectX";
        if (lower.contains("dotnet")) return ".NET " + name.replaceAll("(?i)dotnet", "").replace("_", ".");
        if (lower.contains("msxml")) return "MSXML " + name.replaceAll("(?i)msxml", "");
        if (lower.contains("d3dx")) return "Direct3D " + name.replaceAll("(?i)d3dx", "");
        if (lower.contains("dxvk")) return "DXVK";
        if (lower.contains("vkd3d")) return "VKD3D";
        
        // Fontes
        if (lower.contains("corefonts")) return "Core Fonts";
        if (lower.contains("tahoma")) return "Tahoma Font";
        if (lower.contains("allfonts")) return "All Fonts";
        
        // Windows components
        if (lower.contains("ie6")) return "IE 6";
        if (lower.contains("ie7")) return "IE 7";
        if (lower.contains("ie8")) return "IE 8";
        if (lower.contains("msls31")) return "MS Line Services";
        if (lower.contains("riched20")) return "RichEdit 2.0";
        if (lower.contains("mdac")) return "MDAC";
        if (lower.contains("wmf")) return "Windows Media";
        
        // Outros
        if (lower.contains("physx")) return "PhysX";
        if (lower.contains("xact")) return "XACT";
        if (lower.contains("openal")) return "OpenAL";
        if (lower.contains("wininet")) return "WinInet";
        if (lower.contains("gdiplus")) return "GDI+";
        
        // Default: retorna o nome original formatado
        return name.replace("_", " ");
    }

    public static int getIconForPackage(String name, Context context) {
        String lower = name.toLowerCase();
        String pkgName = context.getPackageName();
        
        // Ícones baseados no tipo
        if (lower.contains("vcrun")) {
            return context.getResources().getIdentifier("ic_dll", "drawable", pkgName);
        }
        if (lower.contains("directx") || lower.contains("d3dx") || lower.contains("dxvk") || lower.contains("vkd3d")) {
            return context.getResources().getIdentifier("ic_gpu", "drawable", pkgName);
        }
        if (lower.contains("dotnet")) {
            return context.getResources().getIdentifier("ic_settings", "drawable", pkgName);
        }
        if (lower.contains("font") || lower.contains("tahoma")) {
            return context.getResources().getIdentifier("ic_edit", "drawable", pkgName);
        }
        if (lower.contains("ie") || lower.contains("wininet")) {
            return context.getResources().getIdentifier("ic_globe", "drawable", pkgName);
        }
        if (lower.contains("wine")) {
            return context.getResources().getIdentifier("ic_wine", "drawable", pkgName);
        }
        if (lower.contains("sound") || lower.contains("xact") || lower.contains("openal")) {
            return context.getResources().getIdentifier("ic_sound", "drawable", pkgName);
        }
        
        // Default
        return context.getResources().getIdentifier("ic_dll", "drawable", pkgName);
    }
}
