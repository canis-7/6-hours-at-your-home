# MEtAL — Six heures chez vous

Un prototype de jeu d'horreur et de survie en première personne, créé avec Unreal Engine 5.7.4.

Jack travaille pour MEtAL sur un détecteur expérimental. Après une panne de courant chez lui, des robots approchent de sa chambre. Il doit gérer sa batterie, ses fenêtres et ses défenses pour survivre pendant six heures. Chaque heure dure six minutes.

## Ouvrir le projet

1. Cloner ou télécharger ce dépôt.
2. Installer Unreal Engine 5.7 et Visual Studio 2022 avec les outils de développement de jeux en C++ et le Windows SDK.
3. Ouvrir `MEtAL.uproject`. Accepter la compilation des modules si Unreal la propose.
4. Cliquer sur **Play** dans l'éditeur.

La chambre et les personnages sont construits en C++ au lancement : la carte paraît vide avant Play. Les fichiers de compilation ne sont pas inclus dans le dépôt.

Après une première compilation, `JOUER.bat` lance directement le jeu si Unreal est installé à son emplacement Windows habituel.

## Commandes

| Action | Commande |
| --- | --- |
| Se déplacer | ZQSD / WASD |
| Regarder | Souris |
| Interagir, ouvrir, prendre ou poser | E |
| Lampe | F |
| Se cacher face au placard | H |
| Carnet | J |
| Détecteur | Clic droit |
| Augmenter la puissance | Clic gauche |
| Revenir à la première puissance | Double-clic gauche |
| Pause | Échap |
| Réessayer après une mort | R |

Les menus et le carnet se naviguent aussi à la souris.

## Survie

- Le détecteur indique le robot proche le plus proche. Une puissance plus élevée augmente la portée et accélère la détection, en consommant davantage de batterie.
- Poser le détecteur sur le socle protège la porte, mais empêche de détecter. Les dégâts de la porte persistent entre les heures.
- Les fenêtres fermées arrêtent les robots, mais réduisent la ventilation. Un robot arrivé à une fenêtre laisse huit secondes pour la fermer.
- Le Rampeur arrive dans le conduit à partir de la deuxième heure. Le marchepied permet de l'observer. Se cacher dans le placard avant sa descente, puis attendre son départ, permet de lui échapper.
- Le robot du placard est actif à partir de la troisième heure. Ouvrir les portes et éclairer son visage le fait reculer.
- Le carnet débloque les informations au fil des rencontres. Ses découvertes sont conservées entre les parties.
- La progression est sauvegardée au début de chaque heure. Le retour du courant recharge la batterie et restaure l'oxygène.

## Organisation

- `Source/MEtAL/` : jeu, personnages, décor, menus et carnet.
- `Config/` : configuration Unreal et commandes.
- `Content/Audio/` : son du screamer importé dans Unreal.
- `SourceAudio/` et `Tools/` : source et outil d'import du son.
- `LIRE-MOI.md` : notes initiales du prototype.

Les modèles et animations sont provisoires. Le projet est en développement.
