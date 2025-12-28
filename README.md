# Model light management / Gestion de l'éclairage d'une maquette

[Cliquez ici pour la version française plus bas dans ce document](#france)

# <a id="english">English version</a>

English version is under construction...

Stay tuned!

# <a id="france">Version française</a>
## Présentation

Cette application permet de gérer un ensemble de LED adressables selon un scénario prédéterminé.

Elle a été initialement conçue pour éclairer une maquette complexe, comportant plusieurs dizaines de points d'éclairage.

Afin de réduire considérablement le câblage, on a utilisé des rubans de LED adressables (genre NéoPixels) insérés dans les bâtiments de la maquette, de sorte que les liaisons entre les différents bâtiments soient réalisées avec seulement 3 conducteurs : une alimentation (masse et +5V) et un fil de transport des commandes.

Noter que le très faible coût de ces rubans (quelques Euros pour 150 LED sur 5 mètres de ruban), on a décidé de "perdre" quelques LED pour assurer la liaison entre les étages, plutôt que de relier les rubans entre eux par des fils, supprimant de fait les risques de faux contacts.

Ces rubans se trouvent en 30, 60 et même 144 LED au mètre, ce qui permet une densité d'éclairage intéressante ... 

Il n'est d'ailleurs pas interdit d'en utiliser au dessus des maquettes pour simuler la lumière du jour dans une pièce sombre.

L'idée de base est de présenter aux spectateurs un cycle de simulation sur une durée réduite (par exemple, une journée complète en 7 minutes). Si la durée d'un cycle est trop longue, ils s'ennuieront et s'en
iront.

La gestion du temps est gérée par l'agenda, qui indique à quelle heure (simulée) on active ou on désactive les 3 types d'éléments suivants :

- fixe, où l'état de l'éclairage reste fixe pendant la durée spécifiée
  dans l'agenda. Par exemple, allumer la pièce à 19:00 et l'éteindre à
  22:15,
- flashs et clignotants qui flashent/clignotent à intervalle régulier.
  Par exemple allumé pour une seconde, éteint pour 0,5 secondes,
- cycles, utilisés pour les séquences d'allumage complexes. Par exemple
  les feux de circulation ou les chenillards.

Flash, clignotants et cycles peuvent être définis sur des durées constantes ou aléatoires dans certaines limites. Ces aléas sont utiles par exemple pour simuler des éclairs de soudage à l'arc. Ces durées sont spécifiées en millisecondes, ce qui permet des actions extrêmement courtes ou rapides.

Il est possible de régler l'intensité maximale de l'éclairage globalement (pour l'adapter à la luminosité de la pièce où la maquette est installée) et individuellement (pour réduire le niveau de lumière d'une pièce sur éclairée).

L'application fonctionne sur un ESP8266, qui peut soit se connecter sur un réseau WiFi existant, soit créer son propre réseau Wifi. Le paramétrage s'effectue au travers d'un serveur Web embarqué, qui
permet également de trouver le positionnement des LED et tester des couleurs, clignotants ou flashs en temps réel. Les messages sont affichés sur l'interface Web, et peuvent également être envoyés sur un serveur `syslog`.

L'application est autonome et sait fonctionner sans action extérieure ni connexion, en utilisant les derniers paramètres connus. Il suffit juste de la mettre l'ESP sous tension.

La suite de ce document détaille tout d'abord la structure et le contenu des fichiers de configuration, plus pratiques que l'interface Web pour charger de grandes quantités de données.

On verra ensuite l'utilisation de l'interface Web.

## Description des données utilisées

Comme vu précédemment, les données relatives aux lumières sont écrite dans un fichier.

Il est possible de charger plusieurs fichiers de configuration dans l'ESP, une liste déroulantes dans la page de paramètres permet de choisir celle à activer.

Le nom de ces fichiers est laissé à la discrétion de l'utilisateur, mais l'extension doit être `.txt`.

On a 6 types de données, correspondant à autant de concepts différents (et de sous parties dans le fichier de configuration) :

- Les pièces, qui associent des LED à une localisation,
- Les groupes, qui regroupent plusieurs pièces,
- Les couleurs, qui décrivent les couleurs utilisées,
- Les cycles, qui décrivent des suites cycliques d'allumage et d'extinction (genre feux tricolores),
- Les flashs et clignotants, qui font ce qu'on attend d'eux,
- L'agenda, qui gère les activations et désactivations des pièces, groupes, cycles et flashs/clignotants en fonction de l'heure de simulation.

Le format interne utilisé est de type CSV (valeurs séparées par des ";").

Chaque type de données est précédé d'une entête qui permet de s'y retrouver lorsqu'on édite les données.

Cette édition peut être faite avec un tableur (LibreOffice, OpenOffice, voire même Excel pour les plus téméraires ;-) ou à l'aide d'un simple éditeur de texte (genre NotePad++ ou similaire).

Chaque partie est séparée de la suivante par une ligne vide.

Il est possible d'ajouter des commentaires dans le fichier, histoire de s'y retrouver. Ces commentaires doivent figurer en fin de ligne, et débuter par le caractère  # . Noter que tout ce qui suit le commentaire sur la même ligne sera simplement ignoré.

Certaines zones peuvent ne pas être spécifiées. Dans ce cas, on utilisera des valeurs par défaut (généralement 0, mais pas systématiquement : par exemple, la luminosité est mise à 100 par défaut).

Les lignes vides sont ignorées.

Les différentes sous parties peuvent être définies dans le fichier dans un ordre quelconque, sous réserve qu'une partie ne fasse pas référence à une donnée non encore décrite.

Il est donc recommandé d'utiliser l'ordre `Pièces`, `Groupes`, `Couleurs`, `Flashs`, `Cycles` et `Agenda`.

L'entête des sous parties vides peut être supprimée.

Si on ne respecte pas cet ordre, on aura un message d'erreur indiquant qu'une pièce ou couleur n'existe pas.

Voici un exemple de fichier vide :
```
Pieces;Premiere lampe;Nombre de lampes;Luminosite # Entete des pièces

Groupes;Piece # Entete des groupes

Couleurs;Rouge;Vert;Bleu # Entete des couleurs
Noir;0;0;0

Cycles;Piece;Couleur;Attente;Attente max # Entete des cycles

Cycles;Piece;Couleur;Attente;Attente max

Date;Piece;Couleur;Luminosite # Entete de l'agenda
```
Pour information, seule la première zone de chaque entête est testé (et doit être identique a celles de cet exemple). Il est possible de modifier les autres zones  d'entête pour les faire correspondre à ses besoins.

Noter que l'encodage des accents varie selon les systèmes d'exploitation. Il est prudent de ne pas en utiliser dans les fichiers de configuration, leur restitution pouvant amener à des résultats amusants ;-)

### Les pièces

Elle définissent les LED concernées par une pièce.

On indique le numéro de la première LED concernée, le nombre de LED dans la pièce, et l'intensité relative de la pièce. Si l'intensité n'est pas spécifiée, elle est mise à 100% par défaut.

Par exemple, voici la partie correspondant à la définition de 6 LED, utilisées pour définir 2 feux de signalisation :
```
Pieces;Premiere lampe;Nombre de lampes;Luminosite
Feu1Vert;1;1;100
Feu1Orange;2;1;100
Feu1Rouge;3;1;100
Feu2Vert;4;1;100
Feu2Orange;5;1;100
Feu2Rouge;6;1;100

```
Noter qu'une LED peut figurer dans plusieurs pièces.

### Les groupes

Ils regroupent des pièces sous un nom donné (pour définir par exemple un bâtiment). On peut aussi les utiliser pour regrouper des séries de LED non contiguës.

On indique, sous le même nom de groupe, chaque pièce concernée.

Par exemple:
```
Groupes;Piece
#Définit le groupe "Coin gauche" comme étant composé des pièces 1, 2, 8 et 9
Coin gauche:P1
Coin gauche:P2
Coin gauche:P8
Coin gauche:P9

```
Noter qu'une pièce peut faire partie de plusieurs groupes.

### Les couleurs

On indique, sous un nom donné, les niveaux (entre 0 et 255) de rouge, vert et bleu à affecter à la lampe.

La première couleur du fichier est utilisée pour « éteindre » les LED. Il est habile de la définir sur `Noir` (0,0,0).

Par exemple :
```
Couleurs;Rouge;Vert;Bleu
Noir;0;0;0
Vert;0;255;0
Orange;255;128;0
Rouge;255;0;0

```
### Les cycles

Ils permettent de programmer des suites d'allumages et d'extinctions dans un ordre quelconque, en spécifiant une durée différente entre chaque changement. Les feux de signalisation sont un bon exemple.

On indique le nom de la séquence, le nom de la pièce, la couleur à affecter, la durée d'attente après l'activation de la séquence, et un éventuel aléa.

Par exemple, pour définir un cycle de feux tricolores, avec 2 feux (Feu1 sur un sens de circulation, Feu 2 sur le sens perpendiculaire), on procède de la façon suivante :
```
Cycles;Piece;Couleur;Attente;Attente max
Croisement;Feu2Rouge;Rouge #Allume le feu 2, en rouge, à 100% sans attente après l'allumage
Croisement;Feu1Vert;Vert;5000 #Allume le feu 1 en vert et attend 5 secondes
Croisement;Feu1Vert; #Éteint le feu 1 vert, sans attente après l'extinction
Croisement;Feu1Orange;Orange;1000 #Allume le feu 1 en orange et attend une seconde
Croisement;Feu1Orange #Éteint le feu 1 orange, sans attente après l'extinction
Croisement;Feu1Rouge;Rouge;2000 #Allume le feu 1 en rouge et attend 2 secondes
Croisement;Feu2Rouge; #Éteint le feu 2 rouge, sans attente après l'extinction
Croisement;Feu2Vert;Vert;5000 #Allume le feu 2 en vert et attend 5 secondes
Croisement;Feu2Vert #Éteint le feu 2 vert, sans attente après l'extinction
Croisement;Feu2Orange;Orange;1000 #Allume le feu 2 en orange et attend une seconde
Croisement;Feu2Orange #Éteint le feu 2 orange, sans attente après l'extinction
Croisement;Feu2Rouge;Rouge;2000 #Allume le feu 2 en rouge et attend 2 secondes
Croisement;Feu1Rouge #Éteint le feu 1 rouge, sans attente après l'extinction

```
Si la couleur n'est pas définie, on utilise la première couleur de la liste `Couleurs` (d'où l'intérêt de la définir à `Noir`. Si l'attente n'est pas définie, on utilise 0. Pareil pour `attente max`

Si "Attente max" est définie, le temps d'attente utilisé sera une valeur aléatoire comprise ente `Attente` et `Attente max`.

Ne pas oublier d'activer le cycle "Croisement" dans l'agenda (décrit plus loin), par un :
```
Date;Piece;Couleur;Luminosite
00:00;Croisement;1 #Active le cycle "Croisement" à 0h00

```
On peut regrouper ces données dans un fichier `Croisement.txt`, qui contiendrait :
```
Pieces;Premiere lampe;Nombre de lampes;Luminosite # Entete des pièces
Feu1Vert;1;1;100
Feu1Orange;2;1;100
Feu1Rouge;3;1;100
Feu2Vert;4;1;100
Feu2Orange;5;1;100
Feu2Rouge;6;1;100

Couleurs;Rouge;Vert;Bleu # Entete des couleurs
Noir;0;0;0
Vert;0;255;0
Orange;255;128;0
Rouge;255;0;0

Cycles;Piece;Couleur;Attente;Attente max # Entete des cycles
Cycles;Piece;Couleur;Attente;Attente max
Croisement;Feu2Rouge;Rouge #Allume le feu 2, en rouge, à 100% sans attente après l'allumage
Croisement;Feu1Vert;Vert;5000 #Allume le feu 1 en vert et attend 5 secondes
Croisement;Feu1Vert; #Éteint le feu 1 vert, sans attente après l'extinction
Croisement;Feu1Orange;Orange;1000 #Allume le feu 1 en orange et attend une seconde
Croisement;Feu1Orange #Éteint le feu 1 orange, sans attente après l'extinction
Croisement;Feu1Rouge;Rouge;2000 #Allume le feu 1 en rouge et attend 2 secondes
Croisement;Feu2Rouge; #Éteint le feu 2 rouge, sans attente après l'extinction
Croisement;Feu2Vert;Vert;5000 #Allume le feu 2 en vert et attend 5 secondes
Croisement;Feu2Vert #Éteint le feu 2 vert, sans attente après l'extinction
Croisement;Feu2Orange;Orange;1000 #Allume le feu 2 en orange et attend une seconde
Croisement;Feu2Orange #Éteint le feu 2 orange, sans attente après l'extinction
Croisement;Feu2Rouge;Rouge;2000 #Allume le feu 2 en rouge et attend 2 secondes
Croisement;Feu1Rouge #Éteint le feu 1 rouge, sans attente après l'extinction

Date;Piece;Couleur;Luminosite # Entete de l'agenda
00:00;Croisement;1 #Active le cycle "Croisement" à 0h00

```
Pour passer à une version à 2 feux sur chaque sens de circulation, on peut par exemple définir les feux 3 et 4 de la même façon que 1 et 2, définir des groupes pour chaque couleur et sens de circulation, puis d'utiliser ces groupes à la place des pièces.

### Les flashs et clignotants

Ils permettent de faire flasher de façon régulière ou non, les LED d'une pièce.

Noter qu'après le flash, la couleur des LED avant le flash est réutilisée.

On indique le nom du flash, les durées minimales (en millisecondes) d'allumage, d'extinction et de pause entre les séries, ainsi que le nombre de répétition de la séquence d'allumage/extinction. Il est également possible de définir des valeurs variables aléatoires en précisant des valeurs maximales en plus des valeurs minimales.

Par exemple, une durée d'allumage minimale de 10 et maximale de 100 produira une valeur aléatoire entre 10 et 100 (inclus) à chaque activation.

Pour faire un flash bref toutes les secondes, utiliser :
```
Flashs;Piece;Couleur;On mini;On maxi;Off mini;Off maxi;Nb mini;Nb maxi;Pause mini;Pause maxi
Flash 1;P1;Blanc;1;0;0;0;1;0;1000;0

```

Pour 3 flashs rapides suivis d'une attente d'une seconde :
```
Flashs;Piece;Couleur;On mini;On maxi;Off mini;Off maxi;Nb mini;Nb maxi;Pause mini;Pause maxi
Flash 2;P1;Blanc;1;0;1;0;3;0;1000;0

```
Une simulation de flash de soudure à l'arc pourrait s'écrire :
```
Flashs;Piece;Couleur;On mini;On maxi;Off mini;Off maxi;Nb mini;Nb maxi;Pause mini;Pause maxi
Soudure;P1;Blanc;10;30;5;20;5;30;20;5000

```
On enverra un flash d'une durée comprise entre 10 et 30 ms, suivi d'un retour à la couleur initiale pendant 5 à 20 ms, qui sera répété entre 5 et 30 fois, avec une pause entre 20 ms et 5 secondes.

On pourra avantageusement tester d'autres valeurs avec l'interface Web.

### L'agenda

Il permet de spécifier les activations et désactivations de pièces, groupes, flashs ou cycles, à une heure simulée donnée.

Pour les pièces et groupes, on spécifie l'heure de simulation, le nom de la pièce ou du groupe, la couleur et l'intensité. Si la couleur n'est pas donnée, on utilisera la première couleur de la liste. Si l'intensité n'est pas donnée, on utilisera 100%.

Pour les cycles et les flashs, on spécifie l'heure de simulation, le nom du cycle ou du flash, et un indicateur d'activation (0 = désactivé, nombre non nul = activé). Si l'indicateur n'est pas spécifié, il est pris à zéro par défaut (soit désactivé).

Par exemple :
```
Date;Piece;Couleur;Luminosite
06:00;Flash 1 #Désactive le flash 1 à 6h (il est activé à 20h)
09:30;Soudure;1 #Active le flash "Soudure" à 9h30
10:00;Soudure #Désactive le flash "Soudure" à 10h
11:00;Soudure;1 #Active le flash "Soudure" à 11:00
12:00;Soudure #Désactive le flash "Soudure" à midi
18:30;P1;Blanc #Allume la pièce 1 en blanc à 18h30
18:45;P2;Blanc;80 #Allume la pièce 2 en blanc à 80% de luminosité à 18h45
19:00;P3;Jaune #Allume la pièce 3 à 19:00
20:00;P2;Blanc #Allume la pièce 2 en blanc à 100% de luminosité à 20:00
20:00;Flash 1;1 #Active le flash 1 à 20h
22:30;P3 #Éteint la pièce 3 à 22h30
23:00;P1 #Éteint la pièce 1 à 23h
23:00;P2 #Éteint la pièce 2 à 23h

```
## Le serveur Web embarqué

L'interaction entre l'utilisateur et l'ESP s'effectue au travers d'un serveur Web embarqué, à l'aide d'un navigateur Web, depuis un téléphone ou un ordinateur.

L'adresse à utiliser dépend du paramétrage fait par l'utilisateur.

Dans le cas le plus courant où le module n'est pas connecté à un réseau WiFi existant, il faut connecter le téléphone ou l'ordinateur au réseau WiFi du module (avec les valeurs par défaut, il se nomme `Eclairage_xxxxxx` où `xxxxxx` représente la fin de l'adresse MAC du module). On se connecte alors au serveur par `http://192.168.4.1/`

Si on a précisé un réseau WiFi (et que celui-ci a pu être joint au démarrage du module), on sera alors connecté à ce réseau, et c'est le routeur du réseau qui lui affectera son adresse IP.

Dans tous les cas, le nom du réseau et l'IP à utiliser sont affichés sur le port console (USB) de l'ESP au lancement.

### La page principale

La page principale `/` ou `/index.htm` du serveur Web embarqué ressemble à :

![](doc/MainPage.jpg)

Elle est composée de plusieurs parties :

#### L'entête

![](doc/Header.jpg)

On y trouve tout d'abord le nom du module sur la première ligne.

Le cadre `Simulation` regroupe les informations sur la ... simulation ce qui, je dois le reconnaître, n'est pas banal ;-)

On trouve de quoi saisir les heures et minutes de début et fin de simulation. En principe, on devrait avoir `00:00` et `23:59`, mais il est possible au travers de ces zones de réduire l'intervalle, soit pour ne présenter qu'une partie de la journée, soit pour affiner le déverminage d'un passage compliqué.

Une fois les heures de début/fin saisies, on doit indiquer la durée d'un cycle de simulation, en minutes.

Noter que le fait de réduire l'intervalle de simulation va sensiblement la ralentir.

A titre indicatif, des durées entre 5 et 10 minutes semblent raisonnables.

La ligne suivante permet de régler la luminosité globale des LED, afin de s'adapter à la lumière présente dans la pièce où la simulation se déroule (en principe, on éclaire moins dans le noir qu'en plein jour).

On démarre et arrête la simulation par les boutons du même nom.

L'heure simulée est indiquée entre ces deux boutons. Elle est mise à jour en temps réel lorsque la simulation est active.

#### Le cadre de test des LED

![](doc/TestLed.jpg)

L'intérieur de ce cadre n'est visible que si on coche la case `Test LED`.

Comme on pourrait s'en douter, il permet de tester les LED (et les flashs/clignotants).

Il permet d'allumer (ou éteindre en allumant en `Noir`) une série contiguë de LED (pratique pour identifier les LED d'une pièce) dans une couleur donnée (pratique pour calibrer les couleurs en fonction du ruban ou de la couleur des murs d'une pièce) avec une luminosité donnée (pratique pour calibrer des pièces avec plusieurs LED).

Il est possible de fixer le numéro de la `Première LED` à utiliser (la première LED du ruban est numérotée 1), le `Nombre de LED` à utiliser à partir de la première LED, et la `Luminosité` à affecter à ces LED.

Si la case `Envoi automatique` est cochée, les actions seront effectuées dès la modification d'une des zones. Si ce n'est pas le cas, il faudra cliquer sur `Envoyer` pour envoyer les commandes aux LED.

Si la case `Éteindre les LED avant l'envoi` est cochée, on éteindra l'ensemble des LED du ruban avant d'appliquer les modifications demandées. Dans le cas contraire, les LED non concernées par la modification resteront dans leur dernier état.

La couleur se règle :

- soit en indiquant les valeurs (entre 0 et 255) de `Niveau de rouge`, `Niveau de Vert` et `Niveau de bleu` dans les cases correspondantes.

- soit en cliquant sur la longue case entre les numéros de LED et les niveaux. Dans ce cas, l'affichage de la fenêtre de réglage des couleurs dépend du système utilisé pour interagir avec le serveur Web. Voici ce qu'une machine sous Windows (beurk ;-) montre :

  ![](doc/Colors.jpg)
  
  On peut soit cliquer sur une couleur déjà définie dans
  la partie gauche, soit utiliser la souris dans le cadre droit pour
  sélectionner sa couleur, en utilisant le curseur à l'extrême droite
  pour régler le niveau de saturation, du blanc (en haut) au noir (en
  bas) en passant par la couleur maximale (au milieu).

Noter que si la case `Envoi automatique` est cochée, les effets sur les LED seront immédiats, ce qui évite des allers/retours entre le choix de la couleur et le bouton `Envoyer`.

Les 2 lignes suivantes permettent de définir les paramètres d'un flash/clignotant.

On y trouve les zones `Temps allumé`, `Temps éteint`, `Répétitions` et `Temps pause` post-fixées par `min` sur la première ligne et `max` sur la seconde.

Lorsque les valeurs `max` sont définies, les valeurs `min` et `max` définissent l'intervalle aléatoire à utiliser. Si elles ne le sont pas (ou sont inférieures à la valeur `min`), la valeur `min` correspond à la valeur fixe à utiliser (pas d'aléa).

La case `Flash des lumières` doit être cochée pour que les LED clignotent (sinon, elles resteront fixes).

Noter que si la case `Envoi automatique` est cochée, les effets sur les LED seront immédiats, chaque modification réinitialisant le cycle de flash/clignotement.

#### Le cadre de mise à jour de la configuration

![](doc/Upload.jpg)

Comme le cadre `Test des LED`, ce cadre n'est visible que si sa case est cochée.

Il est utilisé pour charger un fichier de configuration décrit plus haut.

Il est possible d'indiquer le fichier à utiliser :

- soit en cliquant sur le bouton `Parcourir`, puis en sélectionnant le fichier dans la boîte de dialogue qui va s'ouvrir,
- soit en faisant glisser/lâcher (drag & drop) le fichier sur la case `Déposer le fichier de configuration ici`.

Le fichier sera alors chargé dans l'ESP, et son analyse lancée.

Les éventuelles erreurs seront affichées dans le cadre de trace, l'état final sera repris dans le cadre d'état.

#### Le cadre d'état

![](/doc/Status.jpg)

Ce cadre permet :

- D'afficher la page de paramètres en cliquant sur le bouton `Paramètres` (voir ci-dessous),
- D'afficher la page de l'éditeur de fichiers en cliquant sur le bouton `Éditeur` (voir ci-dessous),
- D'afficher une ligne indiquant l'état du WiFi (point d'accès local ou WiFi existant) et l'adresse IP de l'ESP,
- D'afficher une ligne indiquant la mémoire disponible, et la taille du plus gros bloc (déverminage),
- D'afficher une ligne indiquant le résultat du dernier chargement des fichiers paramètres (<Nom du fichier de configuration> chargé si pas d'erreur, message d'erreur sinon).

#### Le cadre de trace

![](doc/Log.jpg)

Ce cadre contient la liste des traces émises par l'ESP, la dernière ligne affichée en haut de la liste.

Des explications sur les différents messages figurent au chapitre `Les traces générées par le système`.

### La page de paramétrage du système

La page de paramétrage `/setup` du serveur Web embarqué ressemble à :

![](doc/Parameters.jpg)

La première ligne contient est un sélecteur `Langue` qui indique ... la langue utilisée pour l'affichage des pages de ce navigateur (bien vu, non ?).

Si le navigateur supporte le Français, on l'utilisera. Sinon, on basculera automatiquement en Anglais.

Noter que la langue du serveur est définie au moment de la compilation.

Le sélecteur `Configuration` permet de choisir le fichier de configuration à utiliser. La liste déroulante montre les différents fichiers disponibles sur l'ESP.

La ligne suivante permet de définir les caractéristiques du ruban utilisé :

- `Nombre de LED` utilisées,
- `Pinoche des LED` sur laquelle le fil de commande du ruban est connecté à l'ESP. Attention, la valeur donnée ici est le numéro de GPIO (différent du numéro `Dn` gravé sur le circuit). Pour info, sur un ESP8266, on a : D0 = GPIO16, D1 = GPIO5, D2 = GPIO4, D3 = GPIO0, D4 = GPIO2, D5 = GPIO14, D6 = GPIO12, D7 = GPIO13, D8 = GPIO15). Par exemple, si le ruban est connecté sur le repère D2, c'est 4 (le numéro de GPIO correspondant) qu'il faut indiquer,
- `Type de LED` (RGB , RBG, GRB, GBR, BRG, BGR). Pour déterminer le type de ruban utilisé, utiliser le cadre test LED, et régler la couleur à (255, 0, 0). Si rien ne s'allume (c'est que `Envoi automatique` n'est pas coché), cliquer sur `Envoyer`. Repérer la couleur de la première LED. Si elle est rouge, utiliser `R` comme premier caractère, verte `G`, bleue `B`. Régler ensuite la couleur à (0, 255, 0) et repérer le second caractère de la même façon. Choisir alors dans la liste le type qui commence par ces 2 caractères,
- `Fréquence des LED`, qui peut être soit 400 kHz ou 800 kHz. Si aucune LED ne s'allume après un clic sur `Envoyer`, changer la valeur.

Un message indiquera que le changement ne sera pris en compte qu'après redémarrage de l'ESP.

Le cadre suivant contient les informations sur les paramètre réseau de l'ESP :

- `SSID Wifi` indique le nom du réseau WiFi existant sur lequel connecter le module. S'il n'est pas défini, ou ne peut être localisé durant les 10 premières secondes de vie de l'ESP, on basculera sur un point d'accès WiFi crée par l'ESP,
- `Mot de passe` indique le mot de passe associé au SSID WiFi précédent, s'il en possède un,
- `Mot de passe point d'accès` indique le mot de passe associé au point d'accès généré par l'ESP (qui ne sera pas protégé si laissé vide),
- `Nom du module` précise le nom réseau associé au module (pour le distinguer des autres modules le cas échéant).

Le cadre suivant contient les informations dans le cas où on souhaiterait envoyer les traces à un serveur de log de type `syslog` :

- `Serveur syslog` indique le nom ou l'adresse IP du serveur (vide, on n'utilisera pas de serveur `syslog`),
- `Port syslog` précise le numéro de port à utiliser (par défaut 514).

Viennent ensuite des indicateurs utilisés pour contrôler les types de message de trace à afficher/envoyer. Ils sont tous utilisés pour le déverminage :

- `Affichage des entrées de routines` est utilisé afficher le nom des principales routines utilisées par le module,
- `Affichage des messages de déverminage` fait ce qu'on pense qu'il va faire,
- `Affichage des messages verbeux` ajoute plus de messages à la trace,
- `Affichage des traces Java` affiche des messages de trace des modules JavaScript,
- `Envoi des traces à Syslog` permet d'activer l'envoi des traces au serveur `syslog`.

Le bouton `Relancer` permet de ... relancer le module (encore bien vu!). Il est utile lorsqu'on modifie un paramètre qui nécessite un redémarrage (comme tout ce qui touche aux caractéristiques du ruban ou aux réglages du WiFi).

La dernière partie contient la trace de trace déjà décrit plus haut.

### La page de gestion des fichiers embarqués

La page de paramétrage `/edit` du serveur Web embarqué ressemble à :

![](doc/Edit.jpg)

Elle permet de lister les fichiers présents sur l'ESP dans la partie gauche, de les modifier, supprimer, télécharger, d'en créer de nouveau vides ou à partir d'un fichier présent sur l'ordinateur ou le portable utilisé.

A n'utiliser que sur demande.

## Les traces générées par le système

Le système génère plus ou moins (selon les indicateurs définis dans les paramètres précédemment exposés) de traces.

Ces traces proviennent de deux sources : l'ESP et le navigateur.

### Les traces de l'ESP

Ce sont les traces émises par l'ESP lui même, pour indiquer son état et reporter les erreurs qu'il pourrait détecter. On trouve :

#### Les traces de l'analyse des fichiers de paramétrage

Comme déjà expliqué, l'ESP analyse les fichiers de configuration pendant son lancement, et après chaque téléchargement de fichier.

Il est possible qu'il détecte des incohérences pendant cette analyse. Il les signalera pas un message donnant des informations sur le problème. L'affichage d'une erreur provoquera l'arrêt de l'analyse, et empêchera l'activation de la simulation (mais laissera la possibilité d'utiliser le test des LED, et le chargement de nouveaux fichiers).

Dans ce qui suit les parties variables sont précisées entre `< >`. Par exemple, `<numéro de ligne>` sera remplacé par le numéro de ligne du fichier concerné.

On trouvera :

- `Ne peut ouvrir <nom de fichier>` : problème lors de l'ouverture du fichier,
- `Entête <entête du fichier> incorrecte dans <nom du fichier>` : la première ligne du fichier ne semble pas être une entête correcte. L'entête est testée sur sa première zone, qui doit être `Agenda`, `Pieces`, `Couleurs`, `Cycles`, `Groupes` ou `Flashs`,
- `Fichier <nom du fichier> manquant` : le fichier n'a pas pu être trouvé,
- `Zones <type de zone> déjà définie avant <nom de fichier>` : un fichier a déjà défini le type de zone avant qu'on lise le fichier courant. Dit autrement, <type de zone> est défini deux fois dans le fichier,
- `Nb zones (<nombre de zones>) incorrect ligne <numéro de ligne> de <nom de fichier>` : le nombre de zones de la ligne est incorrect (soit inférieur au nombre minimum de zones demandées, soit supérieur au nombre maximum de zones définies),
- `Valeur <valeur donnée> incorrecte, zone <numéro de zone>, ligne <numéro de ligne> de <nom de fichier>` : la valeur donnée pour la zone n'est pas celle attendue,
- `Valeur <valeur donnée> hors limite, zone <numéro de zone>, ligne <numéro de ligne> de <nom de fichier>` : la valeur donnée est hors des limites imposées,
- `Pièce <nom de la pièce> inconnue, ligne <numéro de ligne> de <nom de fichier>` : le nom de pièce (voire de groupe, de cycle ou de flash) n'existe pas dans les définitions précédentes,
- `Heure <heure donnée> incorrecte, zone <numéro de zone>, ligne <numéro de ligne> de <nom de fichier>` : l'heure spécifiée est incorrecte,
- `LED de fin <numéro de LED> incorrecte, ligne <numéro de ligne> de <nom de fichier>` : le numéro de LED de fin (calculé à partir du numéro de LED de début et du nombre de LED) est supérieur au nombre de LED du ruban,
- `Erreur <numéro de l'erreur> inconnue, fichier <nom de fichier>, ligne <numéro de ligne>, entier <valeur entière>, chaîne <chaîne de caractères>` : le programme ne sait pas décoder une erreur. L'ensemble des paramètres envoyés à la procédure est affiché. A faire remonter au développeur.

#### Les traces de l'exécution de la simulation

Le programme envoie certains messages pour informer l'utilisateur de son avancement.

Les principaux sont :

- `Agenda <numéro de ligne de l'agenda>, maintenant <heure de simulation>` : informe de l'activation de l'agenda à une heure de simulation données (permet de suivre où on en est dans la simulation, et quelle(s) ligne(s) va(vont) être exécutée(s),
- `On passe de <heure de fin> à <heure de début>` : indique la fin d'un cycle de simulation,
- `LED <première LED> à <dernière LED> mises à (<rouge>, <vert>, <bleu>) pour <durée> ms (S<numéro de séquence>P<numéro de pièce>C<numéro de cycle>)` : trace le changement de LED lié a l'activation d'une séquence de cycle,
- `LED <première LED> à <dernière LED> mises à (<rouge>, <vert>, <bleu>) (F<numéro du flash>P%<numéro de pièce>)` : trace le changement de LED lié à l'allumage d'un flash/clignotant,
- `LED <première LED> à <dernière LED> mises à <valeur de la couleur> (F<numéro du flash>P%<numéro de pièce>)` : trace le changement de LED lié à l'extinction d'un flash/clignotant,
- `LED <première LED> à <dernière LED> mises à (<rouge>, <vert>, <bleu>) (A<ligne de l'agenda>P<numéro de pièce>C<numéro de couleur>)` : trace le changement de LED lié à l'activation d'une ligne de l'agenda,
- `LED <première LED> à <dernière LED> mises à (<rouge>, <vert>, <bleu>) (A<ligne de l'agenda>G<numéro de groupe>P<numéro de pièce>C<numéro de couleur>)` : trace le changement de LED lié à l'activation d'un groupe,
- `LED <première LED> à <dernière LED> mises à (<rouge>, <vert>, <bleu>), flash <indicateur flash actif> (Utilisateur)` : indique un test de LED initié par l'utilisateur,
- `Agenda <numéro de ligne de l'agenda>, cycle <numéro de cycle> actif` (ou inactif) : trace l'activation (ou la désactivation) d'un cycle,
- `Agenda <numéro de ligne de l'agenda>, flash <numéro de flash> actif` (ou inactif) : trace l'activation (ou la désactivation) d'un flash,
- `Luminosité <luminosité globale>` : la luminosité globale a été modifiée,
- `Tout éteint` : on éteint l'ensemble des LED,
- `Cycle <numéro de cycle> séquence <numéro de séquence> inconnue, ignorée` : le programme ne trouve pas une séquence donnée. A faire   remonter au développeur !
- `Flag <valeur du flag> ligne <numéro de ligne> de l'agenda inconnu` : indique un problème dans les indicateurs d'une ligne d'agenda (et une probable corruption de données). A faire remonter au développeur !

#### Les autres traces

D'autres messages peuvent être envoyés. Il est possible de râler auprès du développeur s'ils ne sont pas assez clairs ;-)

### Les traces du navigateur

En plus des messages générés par le serveur, certaines erreurs générés par le navigateur seront également affichées dans la zone de trace des erreurs.

En particulier, les traces Java si elles sont activées, et les erreurs graves lorsqu'elles se produisent dans le navigateur.

## Les requêtes supportées par le serveur Web

Le serveur Web embarqué répond aux URL suivantes :

- `/` : affiche la page d'accueil,
- `/status` : retourne l'état du programme sous forme JSON,
- `/setup` : affiche la page de paramètres,
- `/settings` : retourne la paramètres au format JSON,
- `/configs` : retourne la liste des fichiers de configuration présents sur l'ESP,
- `/debug` : retourne les variables internes pour déverminer,
- `/log` : retourne les dernières lignes du log mémorisé,
- `/edit` : gère et édite le système de fichier,
- `/tables` : retourne le contenu de l'ensemble des tables du programme,
- `/rest/restart` : redémarre l'ESP,
- `/command/enable/enter` : arme l'indicateur `Affichage des entrées de routines`,
- `/command/enable/debug` : arme l'indicateur `Affichage des messages de déverminage`,
- `/command/enable/verbose` : arme l'indicateur `Affichage des messages verbeux`,
- `/command/enable/java` : arme l'indicateur Affichage des traces Java,
- `/command/enable/syslog` : arme l'indicateur `Envoi des traces à syslog`,
- `/command/disable/enter` : désarme l'indicateur `Affichage des entrées de routines`,
- `/command/disable/debug` : désarme l'indicateur `Affichage des messages de déverminage`,
- `/command/disable/verbose` : désarme l'indicateur `Affichage des messages verbeux`,
- `/command/disable/java` : désarme l'indicateur `Affichage des traces Java`,
- `/command/disable/syslog` : désarme l'indicateur `Envoi des traces à syslog`,
- `/languages` : Retourne la liste des langues supportées,
- `/changed` : change la valeur d'une variable (utilisation interne),
- `/upload` : charge un fichier (utilisation interne).
