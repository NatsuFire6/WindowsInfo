# WindowsInfo

`WindowsInfoLib` est une bibliothèque C++ pour suivre les fenêtres visibles de
premier niveau de la session Windows courante. Elle peut énumérer des fenêtres
d'autres applications, mais ne peut pas déterminer leur état de sauvegarde ni
déclencher leur sauvegarde. Leur état est alors `Unknown`.

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
```

`CloseWindow` poste `WM_CLOSE` après validation ; la procédure de fenêtre reste
responsable de traiter ce message. Les fenêtres `Unknown` déclenchent aussi le
callback de confirmation ; la décision `Save` échoue si aucun callback de
sauvegarde n'a été enregistré pour cette fenêtre. `force = true` contourne la
confirmation et peut donc fermer une fenêtre d'une autre application.

`DiscoverOpenWindows` ajoute les fenêtres visibles manquantes sans remplacer
les callbacks ou états des fenêtres déjà enregistrées. Elle retire également
les fenêtres détruites de la liste. Appelez-la à nouveau pour actualiser la
découverte. Appelez `UnregisterWindow` lorsque la fenêtre est détruite. La
bibliothèque doit être utilisée depuis le thread UI de l'application.

## Compilation

La cible statique `WindowsInfoLib` est créée par CMake et liée à `user32`.
