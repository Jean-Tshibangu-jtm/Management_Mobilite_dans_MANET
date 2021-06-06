# Management_Mobilite_dans_MANET
# Management de la mobilité dans MANET
# Abstract
  Aujourd’hui, avec le succès des communications sans fil, il devient commode d’avoir accès  à un réseau en tous lieux et à n'importe quand sans faire appel physiquement aux différents équipements communicants  à une  infrastructure.  Les nœuds (ordinateurs portables, smartphones, etc) peuvent analyser les différents canaux radio dans l’objet de pouvoir s’associer à un réseau sans fil disponible (station de base, point d’accès, etc.). Un avantage incontestable de ses technologies sans fil est l’amabilité d’être mobile  tout en restant connecté. Nonobstant, la mobilité est une tâche difficile à manager car elle doit être abordée à différentes couches pour être transparente aux utilisateurs. 

   Dans le réseau Mobile Ad hoc Network(MANET), les protocoles de routage se servent des métriques pour choisir les  routes optimales. Les métriques ont les facultés de refléter la qualité de la liaison sans fil et favoriser à manager la mobilité. Par ailleurs, un retard considérable entre l’estimation des métriques et leur inclusion dans le processus de routage rend cette approche non rentable.  
Ce projet se focalise  à la proposition de nouvelles méthodes de calcul des métriques de routage pour gérer la problématique de la mobilité dans les réseaux MANET. Il est évident pour les nouvelles métriques de refléter la qualité du lien et être sensibles à la mobilité en même temps. Nous avons considéré les métriques classiques, en particulier ETX (Expected Transmission Count) et ETT (Expected Transmission Time). Nous interjetons  de nouvelles méthodes pour anticiper les valeurs de ces métriques en utilisant des algorithmes de prédiction. Nous employons une optique Cross Layer, qui admette l’emploi conjoint de l’information à partir des couches 1, 2 et 3.

   L’approbation de nouvelles méthodes de calcul des métriques de routage exige une évaluation
par le truchement de d’un réel banc d’essai. Les nouvelles métriques de routage mise en œuvre par le banc d’essai ont été comparer avec les performances des métriques classiques. Les performances trouvées avec les métriques anticipées nous permettent d’affirmer en toute confiance que l’approche considérée est donc efficace et permet de bien gérer la mobilité. Ses performances sont donc très logiquement liées à l’efficacité de la méthode de prédiction.

Mots clés : Ad Hoc, anticipation, ETX, métrique, mobilité, réseau sans fil.

# Outils
# NS-2, C++,C


