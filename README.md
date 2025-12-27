# Lighting server for model/Eclairage pour maquette
 
[English version to come]
 
[Version française]
Cette application permet de gérer un ensemble de LED adressables selon un scénario prédéterminé.

Elle a été initialement conçue pour éclairer une maquette complexe, comportant plusieurs dizaines de points d'éclairage.

Afin de réduire considérablement le câblage, on a utilisé des rubans de LED adressables (genre NéoPixels) insérés dans les bâtiments de la maquette, de sorte que les liaisons entre les différents bâtiments soient réalisées avec seulement 3 conducteurs : une alimentation (masse et +5V) et un fil de transport des commandes.

Noter que le très faible coût de ces rubans (quelques Euros pour 150 LED sur 5 mètres de ruban), on a décidé de "perdre" quelques LED pour assurer la liaison entre les étages, plutôt que de relier les rubans entre eux par des fils, supprimant de fait les risques de faux contacts. Ces rubans se trouvent en 30, 60 et même 144 LED au mètre, ce qui permet une densité d’éclairage intéressante … Il n’est d’ailleurs pas interdit d’en utiliser au dessus des maquettes pour simuler la lumière du jour dans une pièce sombre.

L'idée de base est de présenter aux spectateurs un cycle de simulation sur une durée réduite (par exemple, une journée complète en 7 minutes). Si la durée d'un cycle est trop longue, ils s'ennuieront et s'en iront.

La gestion du temps est gérée par l'agenda, qui indique à quelle heure (simulée) on active ou on désactive les 3 types d'éléments suivants :
    • fixe, où l'état de l'éclairage reste fixe pendant la durée spécifiée dans l'agenda. Par exemple, allumer la pièce à 19:00 et l'éteindre à 22:15,
    • flashs et clignotants qui flashent/clignotent à intervalle régulier. Par exemple allumé pour une seconde, éteint pour 0,5 secondes,
    • cycles, utilisés pour les séquences d'allumage complexes. Par exemple les feux de circulation ou les chenillards.

Flash, clignotants et cycles peuvent être définis sur des durées constantes ou aléatoires dans certaines limites. Ces aléas sont utiles par exemple pour simuler des éclairs de soudage à l'arc. Ces durées sont spécifiées en millisecondes, ce qui permet des actions extrêmement courtes ou rapides.

Il est possible de régler l'intensité maximale de l'éclairage globalement (pour l'adapter à la luminosité de la pièce où la maquette est installée) et individuellement (pour réduire le niveau de lumière d'une pièce sur éclairée).

L'application fonctionne sur un ESP8266, qui peut soit se connecter sur un réseau WiFi existant, soit créer son propre réseau Wifi. Le paramétrage s'effectue au travers d'un serveur Web embarqué, qui permet également de trouver le positionnement des LED et tester des couleurs, clignotants ou flashs en temps réel. Les messages sont affichés sur l'interface Web, et peuvent également être envoyés sur un serveur syslog.

L'application est autonome et sait fonctionner sans action extérieure ni connexion, en utilisant les derniers paramètres connus. Il suffit juste de la mettre l'ESP sous tension.

Pour plus de détails, consulter le fichier Eclairage.odt dans le répertoire /doc.
