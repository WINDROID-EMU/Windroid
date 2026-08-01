import os
import re

dir_path = "app/src/main/java/com/windroid/emu"

# Function to process files
def process_file(file_path):
    with open(file_path, "r") as f:
        content = f.read()

    original_content = content

    # Replace .sendBroadcast(new Intent(ACTION)) with .sendBroadcast(new Intent(ACTION).setPackage("com.windroid.emu"))
    content = re.sub(r'sendBroadcast\(\s*new\s+Intent\((.*?)\)\s*\)', r'sendBroadcast(new Intent(\1).setPackage("com.windroid.emu"))', content)
    
    # Check for variables like activity.sendBroadcast(runWineIntent);
    # In AdapterGame.java
    content = re.sub(r'activity\.sendBroadcast\(runWineIntent\);', r'runWineIntent.setPackage("com.windroid.emu");\n            activity.sendBroadcast(runWineIntent);', content)
    
    # In AdapterFiles.java, AdapterSettings.java
    content = re.sub(r'context\.sendBroadcast\(intent\);', r'intent.setPackage("com.windroid.emu");\n                context.sendBroadcast(intent);', content)

    # In MainActivity.java
    content = re.sub(r'sendBroadcast\(createWinePrefixIntent\);', r'createWinePrefixIntent.setPackage("com.windroid.emu");\n        sendBroadcast(createWinePrefixIntent);', content)
    content = re.sub(r'sendBroadcast\(runWineIntent\);', r'runWineIntent.setPackage("com.windroid.emu");\n            sendBroadcast(runWineIntent);', content)

    # In CmdEntryPoint.java
    content = re.sub(r'ctx\.sendBroadcast\(intent\);', r'intent.setPackage("com.windroid.emu");\n            ctx.sendBroadcast(intent);', content)
    
    # In EditGamePreferencesFragment
    content = re.sub(r'requireContext\(\)\.sendBroadcast\(runWineIntent\);', r'runWineIntent.setPackage("com.windroid.emu");\n                    requireContext().sendBroadcast(runWineIntent);', content)

    if content != original_content:
        with open(file_path, "w") as f:
            f.write(content)
        print(f"Updated {file_path}")

for root, _, files in os.walk(dir_path):
    for file in files:
        if file.endswith(".java"):
            process_file(os.path.join(root, file))

