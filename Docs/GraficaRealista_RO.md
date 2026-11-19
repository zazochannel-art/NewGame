# Grafică realistă: cum adaugi arta reală (5–15 minute)

Din cod nu se poate face fotorealism, pentru că e nevoie de texturi foto, modele 3D și animații.
Unreal Engine are însă pachete gratuite incluse în instalare, iar jocul le folosește automat dacă le găsește.
Dacă lipsesc, jocul rămâne cu grafica construită din cod, fără erori.

## 1. Starter Content (texturi foto, stâncă, tufiș, valuri), obligatoriu pentru realism
1. Deschide `LostIsland.uproject` în editor.
2. Jos, **Content Drawer**, apoi butonul **+ Add**, apoi **Add Feature or Content Pack…**
3. Tab-ul **Content**, apoi **Starter Content**, apoi **Add to Project**.

Rezultat: pământ cu iarbă, mușchi, nisip și stâncă texturate (în loc de culori plate), stânci și tufișuri 3D reale, valuri care se mișcă pe mare, lemn adevărat pe construcții, plută și cufăr, metal periat și ruginit pe epava avionului.

> **UE 5.8:** dacă Starter Content nu apare în listă, înseamnă că nu e instalat cu motorul (instalarea standard
> nu-l mai include; `Templates\TemplateResources\High\StarterContent` nu există). În Epic Games Launcher:
> **Library**, săgeata de lângă **5.8**, **Options**, bifează **Starter Content**, **Apply**. Apoi pașii de mai sus.

## 2. Third Person (omul animat)
1. Tot din **Add Feature or Content Pack…**, tab-ul **Blueprint**, apoi **Third Person**, apoi **Add to Project**.

Fără editor (copiere manuală, UE 5.8): conținutul e în `<UE_5.8>\Templates\TemplateResources\High\Characters\Content`
(nu în `TP_ThirdPersonBP\Content\Characters`). Copiază folderul `Mannequins` de acolo în `Content\Characters\`,
ca să rezulte `Content\Characters\Mannequins\Meshes\SKM_Quinn_Simple.uasset` și
`Content\Characters\Mannequins\Anims\Unarmed\ABP_Unarmed.uasset`.

## Test vizual automat
`UnrealEditor.exe LostIsland.uproject -game -LIScreenshotTour` (sau comanda `LIScreenshotTour 1` în consolă):
zi, plajă, pădure, epavă, apus. Pentru fiecare: FPS mediu în log (`LIScreenshotTour:`) și o captură
`LITour_<nume>.png` în `Saved/Screenshots/`. Jocul se închide la final.

Rezultat: supraviețuitorul devine manechinul UE5 (Quinn), cu animații reale de mers, alergare și sărituri.

> Pachetul adaugă și harta lui de exemplu. Jocul pornește în continuare pe insula noastră; nu schimba harta de start.

## 3. Copaci reali (opțional, recomandat)
Pachetele de mai sus nu au copaci. Copaci fotorealiști gratuiți găsești pe **Fab** (fab.com), de exemplu
colecțiile Megascans de copaci și vegetație pentru Unreal Engine. Fab e integrat și direct în editor:
Window, apoi Fab.
1. Adaugă în proiect 2–4 copaci (de exemplu pini sau palmieri) și 1–2 tipuri de iarbă.
2. Click dreapta pe fiecare Static Mesh, apoi **Copy Reference**.
3. În `Config/DefaultGame.ini` adaugă (cu căile tale):
```
[/Script/LostIsland.LIVisualAssets]
+TreeMeshes=/Game/Fab/.../SM_Pine_01.SM_Pine_01
+TreeMeshes=/Game/Fab/.../SM_Pine_02.SM_Pine_02
+GrassMeshes=/Game/Fab/.../SM_Grass_01.SM_Grass_01
```
4. Repornește jocul (Play).

## Verificare
În **Output Log** caută:
- `Textured terrain material built.`: texturile foto merg.
- `Optional visual asset not in the project: ...`: lipsește un pachet sau o cale e greșită.

## Bine de știut
- Pachetele **nu se urcă pe GitHub** (sunt mari; sunt în `.gitignore`). Pe fiecare calculator, inclusiv al
  prietenului din co-op, se adaugă din editor, la fel ca mai sus. Cine nu le are vede grafica din cod.
- Totul funcționează doar când joci din editor (Play). Un joc exportat (packaged) va avea nevoie de
  materiale salvate ca asset-uri; asta e un pas separat, mai târziu.
- Prima pornire după adăugare compilează shaderele, iar câteva secunde obiectele pot apărea gri.
