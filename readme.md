# BlockBlast Bot
Ce bot est capable de jouer de manière autonome et intéligente au jeu BlockBlast disponible sur Android !  
## Prérequis :
BlockBlast sur Android :  
<a href="https://play.google.com/store/apps/details?id=com.votre.package" target="_blank">
    <img src="https://play.google.com/intl/en_us/badges/static/images/badges/fr_badge_web_generic.png" alt="Disponible sur Google Play" width="180">
</a>  
[ADB (Android Debug Bridge)](https://developer.android.com/tools/releases/platform-tools?hl=fr)
## Installation :
Le Bot est disponible en deux versions :
 - Python
 - C++

#### Pour les deux version :
Après avoir téléchargé ADB, il faut ajouter le chemin de l'executable dans les variables d'environnement systèmes (PATH).  
Ensuite, activez les paramètres pour développeur Android et le déboguage USB.  
  
Connéctez le téléphone Android en executant dans le terminal :  
```adb pair IP:PORT``` (un code vous serra demandé), puis  
```adb connect IP:PORT```  


Une fois le téléphone connecté, le bot peut être lancé ou compilé et lancé selon le language choisi.
ATTENTION : Assurez vous que le dossier où seront stockée les images (PARENT_IMG_FOLDER dans main.py) est créé AVANT l'execution.