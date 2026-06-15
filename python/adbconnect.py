# Fichier gérant la connexion entre l'appareil executant le script et l'appareil android.

import subprocess
import re

device = ""

def adb_cmd(cmd):
    if not device:
        print("[adbconnect/simulation] La commande ne peut pas être exécutée")
        return

    combined_cmd = ["adb", "-s", device] + cmd.split()

    return subprocess.check_output(combined_cmd)

def choose_device():
    global device
    list_devices = []

    out = subprocess.check_output(["adb", "devices"], text=True)
    pattern = r"([^\r\n\t]+)\t([^\s]+)"

    print("[adbconnect] List of devices attached")

    matches = re.findall(pattern, out)

    if matches:
        for i, m in enumerate(matches):

            if m[1] == "device":
                print(f"[adbconnect] [{len(list_devices)}] ", end="")
                list_devices.append(m[0])
            else:
                print("[adbconnect] [X] ", end="")
            
            print(f"{m[0]}\t{m[1]}")

        output = input("\n[adbconnect] Quel appareil voulez vous utiliser [digit or \'none\'] ? ")
        if output == "none":
            print("[adbconnect] Aucun appareil connecté\n[adbconnect] Mode simulation activé")
        else:
            id_device = int(output)
            device = list_devices[id_device]
            print("[adbconnect] Connecté à : " + device)
    else:
        print("[adbconnect] Aucun appareil utilisable atuellement...\n")
        output = input("[adbconnect] Voulez vous continuer en mode simulation [Y/n] ? ")
        if output == "Y" or output == "y":
            print("[adbconnect] Aucun appareil connecté\n[adbconnect] Mode simulation activé")
        else: exit()

def capture_screen(parentFolder):
    if not device:
        name = input("[adbconnect/simulation] Quel fichier local souhaitez vous ouvrir ? ")
        
        if name == "" or name == "\n" or name == "\r\n":
            name = "screen.png"

        return name

    else:
        name = input("[adbconnect] Sous quel nom souhaitez vous enregistrer la capture ? ")

        if name == "" or name == "\n" or name == "\r\n":
            name = "screen.png"

        data = adb_cmd("exec-out screencap -p")

        if data:
            with open(parentFolder + name, "wb") as f:
                f.write(data)

        print("[adbconnect] Image enregistrée dans : " + parentFolder + name)

    return name

def click(x, y):
    if not device:
        print(f"[adbconnect/simulation] clic x:{x} y:{y}")
        return

    else:
        cmd = "shell input tap " + str(x) + " " + str(y)
        adb_cmd(cmd)
        print(f"[adbconnect] clic x:{x} y:{y}")
        return