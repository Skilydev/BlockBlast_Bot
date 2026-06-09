# Fichier gérant la connexion entre l'appareil executant le script et l'appareil android.

import subprocess
import re

device = ""

def choose_device():
    global device
    list_devices = []

    out = subprocess.check_output(["adb", "devices"], text=True)

    regExpDevice = r"([^\r\n\t]+)\t([^\s]+)"
    matches = re.findall(regExpDevice, out)

    print("List of devices attached")

    if matches:
        for i, m in enumerate(matches):

            if m[1] == "device":
                print(f"[{len(list_devices)}] ", end="")
                list_devices.append(m[0])
            else:
                print("[X] ", end="")
            
            print(f"{m[0]}\t{m[1]}")

        output = input("\nQuel appareil voulez vous utiliser [digit or \'none\'] ? ")
        if output == "none":
            print("Aucun appareil connecté\nMode simulation activé")
        else:
            id_device = int(output)
            device = list_devices[id_device]
            print("Connecté à : " + device)
    else:
        print("Aucun appareil utilisable atuellement...\n\n")
        output = input("Voulez vous continuer en mode simulation [Y/n] ? ")
        if output == "Y" or output == "y":
            print("Aucun appareil connecté\nMode simulation activé")
        else: exit()

def adb_cmd(cmd):
    if not device:
        print("[simulation] La commande ne peut pas être exécutée")
        return

    combined_cmd = ["adb", "-s", device] + cmd.split()

    return subprocess.check_output(combined_cmd)

def capture_screen(parentFolder):
    if not device:
        name = input("[simulation] Quel fichier local souhaitez vous ouvrir ? ")
        
        if name == "" or name == "\n" or name == "\r\n":
            name = "screen.png"

        return name

    else:
        name = input("Sous quel nom souhaitez vous enregistrer la capture ? ")

        if name == "" or name == "\n" or name == "\r\n":
            name = "screen.png"

        data = adb_cmd("exec-out screencap -p")

        if data:
            with open(parentFolder + name, "wb") as f:
                f.write(data)

        print("Image enregistrée dans : " + parentFolder + name)

    return name