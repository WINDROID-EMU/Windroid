package com.windroid.emu.utils;

import android.app.ActivityManager;
import android.content.Context;
import android.util.Log;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static com.windroid.emu.core.ShellLoader.runCommand;
import static com.windroid.emu.core.ShellLoader.runCommandWithOutput;

public class AppKiller {
    private static final String TAG = "AppKiller";

    // Lista de processos essenciais do sistema que nunca devem ser fechados
    private static final Set<String> SYSTEM_PROCESSES = new HashSet<>(Arrays.asList(
            // Sistema Android core
            "system",
            "system_server",
            "android",
            "com.android.systemui",
            "com.android.settings",

            // Zygote (gerenciador de processos)
            "zygote",
            "zygote64",
            "usap32",
            "usap64",

            // Launchers comuns
            "com.android.launcher3",
            "com.google.android.apps.nexuslauncher",
            "com.android.launcher",
            "com.teslacoilsw.launcher",
            "com.microsoft.launcher",
            "org.lineageos.trebuchet",
            "com.google.android.apps.pixellauncher",
            "com.motorola.launcher3",

            // Serviços essenciais
            "com.android.phone",
            "com.android.bluetooth",
            "com.android.nfc",
            "com.android.shell",
            "com.android.keychain",
            "com.android.providers.downloads",

            // Input methods
            "com.android.inputmethod",
            "com.google.android.inputmethod.latin",

            // Conectividade
            "com.android.wifi",
            "com.android.connectivity",

            // Este próprio app
            "com.windroid.emu"));

    /**
     * Fecha aplicativos em segundo plano para liberar recursos - ABORDAGEM
     * AGRESSIVA.
     * Prioridade: Root > Shell Commands > ActivityManager API
     * 
     * @param context Contexto da aplicação
     */
    public static void killBackgroundApps(Context context) {
        Log.i(TAG, "=== INICIANDO FECHAMENTO AGRESSIVO DE APPS ===");

        int killedCount = 0;

        // Tenta abordagem super agressiva com root primeiro
        killedCount = killBackgroundAppsRoot(context);

        if (killedCount < 0) {
            Log.w(TAG, "Abordagem root falhou, tentando shell commands sem root...");
            killedCount = killBackgroundAppsAdvanced(context);
        }

        if (killedCount < 0) {
            Log.w(TAG, "Shell commands falharam, usando método básico do Android...");
            killedCount = killBackgroundAppsBasic(context);
        }

        Log.i(TAG, "=== FINALIZADO. Total de apps processados: " + killedCount + " ===");
    }

    /**
     * Abordagem SUPER AGRESSIVA: usa comandos root
     * 
     * @param context Contexto da aplicação
     * @return Número de apps processados, ou -1 se falhou
     */
    private static int killBackgroundAppsRoot(Context context) {
        Log.d(TAG, "Tentando abordagem ROOT...");

        // Verifica se tem root disponível
        String testRoot = runCommandWithOutput("su -c 'echo test' 2>/dev/null", false);
        if (testRoot == null || !testRoot.contains("test")) {
            Log.w(TAG, "Root não disponível");
            return -1;
        }

        Log.i(TAG, "✓ Root detectado! Usando modo SUPER AGRESSIVO");

        ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (activityManager == null) {
            return -1;
        }

        List<ActivityManager.RunningAppProcessInfo> runningProcesses = activityManager.getRunningAppProcesses();
        if (runningProcesses == null || runningProcesses.isEmpty()) {
            Log.w(TAG, "Nenhum processo em execução encontrado");
            return -1;
        }

        Log.d(TAG, "Total de processos encontrados: " + runningProcesses.size());

        int killedCount = 0;
        String currentPackage = context.getPackageName();

        for (ActivityManager.RunningAppProcessInfo processInfo : runningProcesses) {
            String processName = processInfo.processName;

            // Log de todos os processos para debug
            Log.v(TAG, "Processo encontrado: " + processName + " (importance: " + processInfo.importance + ")");

            // Pula processos do sistema e o próprio app
            if (isSystemProcess(processName) || processName.equals(currentPackage)) {
                Log.v(TAG, "  → IGNORADO (sistema ou próprio app)");
                continue;
            }

            // Extrai o nome do pacote
            String packageName = processName.split(":")[0];

            Log.d(TAG, "  → MATANDO: " + packageName);

            try {
                // Usa MÚLTIPLAS estratégias de kill com root para garantir
                runCommand("su -c 'am force-stop " + packageName + "'", false);
                runCommand("su -c 'am kill " + packageName + "'", false);
                runCommand("su -c 'killall " + packageName + "'", false);

                Log.i(TAG, "  ✓ MORTO (root): " + packageName);
                killedCount++;
            } catch (Exception e) {
                Log.e(TAG, "  ✗ Erro ao matar " + packageName + ": " + e.getMessage());
            }
        }

        // Kill adicional de processos comuns que consomem recursos
        killCommonResourceHogs();

        return killedCount;
    }

    /**
     * Mata processos comuns que consomem muita memória/CPU
     */
    private static void killCommonResourceHogs() {
        String[] resourceHogs = {
                "com.google.android.gms", // Google Play Services (pode consumir muita RAM)
                "com.google.android.gms.persistent",
                "com.android.vending", // Play Store
                "com.facebook.katana", // Facebook
                "com.facebook.orca", // Messenger
                "com.whatsapp", // WhatsApp
                "com.instagram.android", // Instagram
                "com.snapchat.android", // Snapchat
                "com.android.chrome", // Chrome
                "com.google.android.youtube", // YouTube
                "com.twitter.android" // Twitter
        };

        Log.d(TAG, "Matando processos conhecidos por consumir recursos...");

        for (String pkg : resourceHogs) {
            try {
                runCommand("su -c 'am force-stop " + pkg + "' 2>/dev/null", false);
                Log.d(TAG, "  ✓ Tentou matar: " + pkg);
            } catch (Exception ignored) {
                // Ignora se não estiver instalado
            }
        }
    }

    /**
     * Abordagem avançada: usa comandos shell 'am force-stop' sem root
     * 
     * @param context Contexto da aplicação
     * @return Número de apps processados, ou -1 se falhou
     */
    private static int killBackgroundAppsAdvanced(Context context) {
        Log.d(TAG, "Tentando abordagem SHELL (sem root)...");

        ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (activityManager == null) {
            return -1;
        }

        List<ActivityManager.RunningAppProcessInfo> runningProcesses = activityManager.getRunningAppProcesses();
        if (runningProcesses == null || runningProcesses.isEmpty()) {
            return -1;
        }

        int killedCount = 0;
        String currentPackage = context.getPackageName();

        for (ActivityManager.RunningAppProcessInfo processInfo : runningProcesses) {
            String processName = processInfo.processName;

            if (isSystemProcess(processName) || processName.equals(currentPackage)) {
                continue;
            }

            String packageName = processName.split(":")[0];

            try {
                runCommand("am force-stop " + packageName, false);
                runCommand("am kill " + packageName, false);
                Log.d(TAG, "Fechado (shell): " + packageName);
                killedCount++;
            } catch (Exception e) {
                Log.w(TAG, "Erro ao fechar " + processName + " (shell): " + e.getMessage());
                return -1;
            }
        }

        return killedCount;
    }

    /**
     * Abordagem básica: usa ActivityManager.killBackgroundProcesses()
     * 
     * @param context Contexto da aplicação
     * @return Número de apps processados
     */
    private static int killBackgroundAppsBasic(Context context) {
        Log.d(TAG, "Usando abordagem BÁSICA (ActivityManager)...");

        ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (activityManager == null) {
            Log.e(TAG, "ActivityManager não disponível");
            return 0;
        }

        List<ActivityManager.RunningAppProcessInfo> runningProcesses = activityManager.getRunningAppProcesses();
        if (runningProcesses == null || runningProcesses.isEmpty()) {
            Log.w(TAG, "Nenhum processo em execução encontrado");
            return 0;
        }

        int killedCount = 0;
        String currentPackage = context.getPackageName();

        for (ActivityManager.RunningAppProcessInfo processInfo : runningProcesses) {
            String processName = processInfo.processName;

            if (isSystemProcess(processName) || processName.equals(currentPackage)) {
                continue;
            }

            if (processInfo.importance >= ActivityManager.RunningAppProcessInfo.IMPORTANCE_BACKGROUND) {
                try {
                    activityManager.killBackgroundProcesses(processName);
                    Log.d(TAG, "Fechado (básico): " + processName);
                    killedCount++;
                } catch (Exception e) {
                    Log.w(TAG, "Erro ao fechar " + processName + ": " + e.getMessage());
                }
            }
        }

        return killedCount;
    }

    /**
     * Verifica se um processo é essencial do sistema
     * 
     * @param processName Nome do processo
     * @return true se for processo do sistema
     */
    private static boolean isSystemProcess(String processName) {
        if (SYSTEM_PROCESSES.contains(processName)) {
            return true;
        }

        // Verifica padrões de nomes de processos do sistema
        return processName.startsWith("system")
                || processName.startsWith("android")
                || processName.startsWith("com.android.system")
                || processName.contains("zygote");
    }
}
