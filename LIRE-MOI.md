# MEtAL — Six heures chez vous

Prototype pour Unreal Engine 5.7.4. Les robots et les objets sont des formes provisoires : ils pourront etre remplaces par les modeles definitifs.

Le robot de la porte est maintenant Cor, modele 3D construit d'apres le dessin fourni : tete triangulaire, corps mince, bras courbe a pointe, longues jambes asymetriques et yeux ambre. Les robots des fenetres gardent leurs modeles actuels.

## Jouer

Double-cliquer sur **JOUER.bat**, puis appuyer sur **Echap** pour commencer. Pour ouvrir l'editeur, ouvrir **MEtAL.uproject**, puis cliquer sur **Play**. La chambre est construite au lancement du jeu : la carte Entry parait vide avant Play.

## Commandes

| Action | Commande |
| --- | --- |
| Marcher | ZQSD ou WASD |
| Regarder | Souris |
| Poser/reprendre la toupie, ouvrir/fermer une fenetre | Viser l'objet de pres et appuyer sur E |
| Activer le detecteur et augmenter sa puissance | Clic gauche |
| Revenir au niveau 1 | Double-clic gauche |
| Allumer/eteindre le detecteur | Clic droit |
| Lampe torche | F |
| Pause / reprendre | Echap |
| Choisir une option du menu | Souris + clic gauche (Reprendre, Nouvelle partie, Quitter) |
| Reprendre apres une mort | R |
| Nouvelle partie | N dans la pause ou apres la victoire |

## Regles de cette version

- Six heures de six minutes, avec une courte periode de courant entre chaque heure.
- Les robots accelerent a chaque heure : x1, x1,20, x1,45, x1,70, x2 et x2,35. Ils approchent et reviennent plus vite ; les coups sur la porte accelerent aussi. Le delai de huit secondes aux fenetres reste le meme.
- Checkpoint sauvegarde au debut de chaque panne. Quitter puis relancer reprend cette heure.
- La batterie se recharge quand le courant revient. La porte conserve ses degats.
- La boussole indique le robot detectable le plus proche. Elle traverse les murs, ignore le premier stade d'attaque et respecte une portee limitee.
- Quatre puissances : plus de portee et moins d'attente, mais davantage de consommation.
- La toupie sur le socle blanc protege la porte et consomme la batterie. Il faut la reprendre pour detecter.
- Un robot devant la porte recule apres quelques secondes de protection. Sans protection, ses coups abiment la porte.
- Deux autres robots approchent des fenetres. Un avertissement laisse huit secondes pour fermer quand ils arrivent. Fermer interrompt l'attaque ; rouvrir devant un robot donne un nouveau delai de huit secondes. Ils peuvent attendre, puis repartent. Ils ne rouvrent pas les volets.
- Deux fenetres ouvertes : l'oxygene remonte. Une seule : il baisse lentement. Aucune : il baisse vite.
- La ventilation electrique restaure l'oxygene pendant le retour du courant (reglage provisoire).
- La lune donne un conseil lie a la cause de la mort.
- Si un robot entre par une fenetre, il se jette sur le joueur en environ 0,2 seconde. La saisie et la ruee durent 0,4 seconde, puis viennent la chute et le coup de pied. Le son fourni « the-screamer-shared-between-mallie-and-jenny.mp3 » demarre au debut de l'attaque, a sa vitesse originale. Coupure au noir a l'impact, puis la lune apparait et R permet de reprendre.

Les vitesses, la consommation, les durees d'attente et les distances sont des valeurs de depart a ajuster apres essais. La lampe sert a regarder dehors ; le robot specialise qui reagit a la lumiere attend sa conception definitive.

## Organisation

- `Source/MEtAL/MetalPrototype.cpp` : chambre, joueur, robots, regles et interface.
- `Source/MEtAL/MetalPrototype.h` : declarations.
- `Config/` : moteur, commandes et projet.
- `Saved/` : sauvegardes et journaux locaux generes par Unreal.

La version actuelle construit le decor en C++ au lancement. Pour editer le decor en deplacant des objets dans Unreal, il faudra ensuite convertir la chambre en carte et exposer les reglages dans des Blueprints.
