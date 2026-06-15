# BlockBlast Bot
Ce bot est capable de jouer de manière autonome et intéligente au jeu BlockBlast disponible sur Android !  
## Prérequis :
BlockBlast sur Android :  
<a href="https://play.google.com/store/apps/details?id=com.block.juggle" target="_blank">
    <img src="https://play.google.com/intl/en_us/badges/static/images/badges/fr_badge_web_generic.png" alt="Disponible sur Google Play" width="180">
</a>  
[ADB (Android Debug Bridge)](https://developer.android.com/tools/releases/platform-tools)  
## Installation : 
Le Bot est disponible en deux versions :
 - Python
 - C++

### Pour les deux version :
Après avoir téléchargé ADB, il faut ajouter le chemin de l'executable dans les variables d'environnement systèmes (PATH).  
Ensuite, activez les paramètres pour développeur Android et [activez le déboguage USB](https://developer.android.com/tools/adb#Enabling).  
  
### C++
Téléchargez la librairie stb_image.h [ici](https://github.com/nothings/stb/blob/master/stb_image.h).  
Placez les fichiers stb_image.h, main.cpp, adbconnect.cpp et adbconnect.h dans le même dossier.  
Vous pouvez compiler et lancer le programme en exécutant ```g++ -O2 main.cpp adbconnect.cpp -o main.exe```.  
  
**ATTENTION** : Assurez vous que le dossier où seront stockée les images (PARENT_IMG_FOLDER dans main.cpp) est créé **AVANT** l'execution.

### Python
Téléchargez la librairie Pilow (PIL) [ici](https://pypi.org/project/pillow/).  
Placez les fichiers main.py et adbconnect.py dans le même dossier.  
Vous pouvez lancer le script en exécutant ```python main.py```.  

**ATTENTION** : Assurez vous que le dossier où seront stockée les images (PARENT_IMG_FOLDER dans main.cpp) est créé **AVANT** l'execution.