# Ghid de compilare: LOST ISLAND (Windows)

## 1. Ce trebuie instalat (o singură dată)
1. **Epic Games Launcher**, apoi tab-ul Unreal Engine → Library → instalează **Unreal Engine 5.5**.
2. **Visual Studio 2022 Community**. În instalator bifează:
   - **Game development with C++** (cu „Unreal Engine installer” și „Windows 10/11 SDK”);
   - **Desktop development with C++**;
   - la *Individual components*: **.NET Framework 4.8 SDK** (sau mai nou).
3. **Git for Windows**.

## 2. Descarcă proiectul
```
git clone https://github.com/zazochannel-art/NewGame.git
cd NewGame
git checkout claude/lost-island-prototype-hcf1rv
```

## 3. Generează și compilează
1. Click dreapta pe `LostIsland.uproject` → **Generate Visual Studio project files**.
   - Dacă opțiunea lipsește: dublu-click pe `.uproject` și răspunde **Yes** la „rebuild modules”.
2. Deschide `LostIsland.sln`.
3. Sus, alege **Development Editor** și **Win64**.
4. **Build → Build Solution** (Ctrl+Shift+B).

## 4. Dacă apar erori
- Deschide fereastra **Output** (View → Output) și copiază **primele 20–30 de linii cu `error`**.
  Primele erori sunt cele importante; restul sunt adesea doar consecințe ale lor.
- Sau trimite fișierul `Saved/Logs/LostIsland.log`, ori log-ul din `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`.
- Lipește-le în chat și le repar.

## 5. Dacă se compilează
1. Deschide `LostIsland.uproject` și apasă **Play**.
2. Spune-mi ce vezi și urmează listele de teste din `Docs/DevelopmentPlan.md` (secțiunile 4–4i).
3. Probleme cunoscute care se rezolvă din setări:
   - Vezi doar apă și cazi prin insulă: setează `bFlipWinding = true` (explicat în `Docs/Systems/Island.md`).
   - Consola (pentru comenzile `LI...`): tasta **`** sau **~**.
