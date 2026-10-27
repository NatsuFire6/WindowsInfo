# WindowsInfo

`WindowsInfoLib` est une bibliothèque C++ pour suivre les fenêtres visibles de
premier niveau de la session Windows courante. Pour les fenêtres d'autres
applications, elle ne peut pas confirmer l'état de sauvegarde; leur état est
alors `Unknown`. Elle peut leur envoyer Ctrl+S, sans pouvoir vérifier le
résultat.

## Utilisation

```cpp
#include "window_save_manager.h"

wininfo::WindowSaveManager windows(
    [](HWND window) {
        // Afficher ici la boîte de dialogue Enregistrer / Ignorer / Annuler.
        return wininfo::CloseDecision::Save;
    });

windows.RegisterWindow(mainWindow, [&document] {
    return document.Save(); // true seulement si l'enregistrement a réussi
});

windows.SetUnsaved(mainWindow);
windows.SetUnsaved(mainWindow, false); // l'application confirme que tout est enregistré

const auto state = windows.GetState(mainWindow);
if (state == wininfo::WindowSaveState::Unsaved) {
    windows.SaveWindow(mainWindow);
}

windows.CloseWindow(mainWindow);       // demande confirmation si non sauvegardée
windows.CloseWindow(mainWindow, true); // force la fermeture sans vérification

const bool allSaved = windows.AreAllWindowsSaved();
const auto unsavedWindows = windows.GetUnsavedWindows();

// Ajoute à la liste toutes les fenêtres de premier niveau actuellement visibles.
if (windows.DiscoverOpenWindows()) {
    const auto unknownWindows = windows.GetUnknownWindows();
}

// À appeler périodiquement depuis le thread UI, par exemple depuis un timer.
windows.UpdateForegroundWindowState();

// Vérifie le chemin fourni et recherche les fenêtres dont le titre contient
// exactement le nom et l'extension du fichier.
windows.SaveFile(L"C:\\Documents\\rapport.docx");
```

`CloseWindow` poste `WM_CLOSE` après validation ; la procédure de fenêtre reste
responsable de traiter ce message. Les fenêtres `Unknown` déclenchent aussi le
callback de confirmation. Pour une fenêtre externe, la décision `Save` envoie
Ctrl+S mais ne ferme pas la fenêtre, car la bibliothèque ne peut pas confirmer
que l'enregistrement a réussi. `force = true` contourne la confirmation et
peut donc fermer une fenêtre d'une autre application.

`DiscoverOpenWindows` ajoute les fenêtres visibles manquantes sans remplacer
les callbacks ou états des fenêtres déjà enregistrées. Elle retire également
les fenêtres détruites de la liste. Appelez-la à nouveau pour actualiser la
découverte. Appelez `UnregisterWindow` lorsque la fenêtre est détruite. La
bibliothèque doit être utilisée depuis le thread UI de l'application.

`UpdateForegroundWindowState` suit la fenêtre de premier plan au fil des appels.
Après une minute continue, son état repasse à `Unknown`, sauf si une fenêtre
enregistrée dans la bibliothèque a explicitement indiqué qu'elle était
`Saved`. Il faut donc appeler cette méthode périodiquement depuis un timer.
Pour une fenêtre `Unknown` sans callback de sauvegarde, `SaveWindow` active la
fenêtre puis simule Ctrl+S. L'envoi du raccourci ne prouve pas que le fichier a
été enregistré : l'état reste `Unknown` jusqu'à ce que l'application appelle
`SetUnsaved(window, false)`.

`FindWindowsForFile` et `SaveFile` ne parcourent aucun disque : ils vérifient
le chemin transmis par l'appelant, puis recherchent les fenêtres visibles dont
le titre contient le nom complet du fichier (nom et extension). `SaveFile`
essaie de sauvegarder toutes les fenêtres correspondantes et échoue si aucune
ne correspond ou si l'une des sauvegardes échoue. Pour les fenêtres inconnues,
cela envoie Ctrl+S; pour les fenêtres enregistrées avec un callback, cela
appelle ce callback.

## Compilation

La cible statique `WindowsInfoLib` est créée par CMake et liée à `user32`.
