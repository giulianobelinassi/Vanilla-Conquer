# Vanilla-Conquer: Nintendo DSi port

**This is a Nintendo DSi port of both Command & Conquer and Command & Conquer: Red Alert
based on Vanilla Conquer.**

Command & Conquer is a Real Strategy Game released in 1995 developed by Westwood Studios and its
trademark is currently owned by Eletronic Arts. Tiberian Dawn was made freeware
in 2007, and its sourcecode made public in 2020. Vanilla Conquer is a source port
with multi-platform support.

Command & Conquer: Red Alert is another game of its series released in 1996 and has
improved gameplay mechanics, AI and graphics.

For playing it on your **Nintendo DSi**, you will need:
- A jailbroken Nintendo DSi capable of running .nds ROMs in DSi mode through the memory card.
- A memory card with at least 1.4Gb of free space for Tiberian Dawn.
- A memory card with at least 2.0Gb of free space for Red Alert.
- Assets from DOS Command & Conquer versions: both GDI and Nod discs, with Covert Operations also supported but not necessary.
- Assets from Red Alert versions: Both Allied and Soviet discs, with Counterstrike and aftermath supported but not needed.

For playing it on your **Nintendo DS**, you will need
- A Flashcard.
- The DS Expansion Pak. Opera, GBA Movie Player, and EZ flash 3in1 combo are confirmed to work.
- A flashcard with a memory card with at least 1.4Gb of free space for Tiberian Dawn.
- Assets from DOS Command & Conquer versions: both GDI and Nod discs, with Covert Operations also supported but not necessary.
- **Red Alert is not supported in DS mode**.

## Downloads

- [Vanilla-Conquer Nintendo DSi ROMs](https://github.com/giulianobelinassi/Vanilla-Conquer/releases/download/nds-v0.3/NDS_Vanilla-Conquer_0.3.zip).
- [C&C DOS: GDI disc](https://bigdownloads.cnc-comm.com/cnc1/DOSCNC_GDI.zip).
- [C&C DOS: Nod disc](https://bigdownloads.cnc-comm.com/cnc1/DOSCNC_Nod.zip).
- [C&C Covert Operations disc](https://bigdownloads.cnc-comm.com/cnc1/CNC_Covertops.zip).
- [C&C RA: Allies disc](https://bigdownloads.cnc-comm.com/ra/RA_Allies.zip)
- [C&C RA: Soviet disc](https://bigdownloads.cnc-comm.com/ra/RA_Soviet.zip)
- [C&C RA: Counterstrike disc](https://bigdownloads.cnc-comm.com/ra/RA_Counterstrike.zip)
- [C&C RA: Aftermath disc](https://bigdownloads.cnc-comm.com/ra/RA_Aftermath.zip)

## Install Tiberian Dawn

For you to play VanillaTD on your NDSi, you must:

- Download both [GDI](https://bigdownloads.cnc-comm.com/cnc1/DOSCNC_GDI.zip) and [Nod](https://bigdownloads.cnc-comm.com/cnc1/DOSCNC_Nod.zip) DOS C&C discs.
- Create `/vanilla-conquer/vanillatd/` path on the root of your flashcard.
- Extract the content of both GDI and Nod discs into the folders as follows:
```
vanilla-conquer/
└── vanillatd/
    ├── aud.mix
    ├── conquer.mix
    ├── desert.mix
    ├── gdi
    │   ├── general.mix
    │   ├── movies.mix
    │   └── scores.mix
    ├── local.mix
    ├── nod
    │   ├── general.mix
    │   ├── movies.mix
    │   └── scores.mix
    ├── speech.mix
    ├── sounds.mix
    ├── temperat.mix
    ├── transit.mix
    └── winter.mix

3 directories, 15 files
```
Files outside the `gdi` and `nod` folders can be retrieved from either disc, but only the GDI files are tested.

- Double check if you extracted all files. `speech.mix` is easy to miss.
- Move the `vanillatd.nds` rom on the root of your memory card.
- Play.

Now, if you want to play the covert operations extension:
- Download the [Covert Operations](https://bigdownloads.cnc-comm.com/cnc1/CNC_Covertops.zip) disc image.
- Extract `sc-000.mix`, `sc-001.mix`, and `local.mix` into the `vanilla-conquer/vanillatd` folder.
- Extract `general.mix`, `movies.mix` and `scores.mix` into `covertops` folder.
- The final folder will look like this (from a clean install):

```
vanilla-conquer/
└── vanillatd
    ├── aud.mix
    ├── conquer.mix
    ├── covertops
    │   ├── general.mix
    │   ├── movies.mix
    │   └── scores.mix
    ├── desert.mix
    ├── gdi
    │   ├── general.mix
    │   ├── movies.mix
    │   └── scores.mix
    ├── local.mix
    ├── nod
    │   ├── general.mix
    │   ├── movies.mix
    │   └── scores.mix
    ├── sc-000.mix
    ├── sc-001.mix
    ├── speech.mix
    ├── sounds.mix
    ├── temperat.mix
    ├── transit.mix
    └── winter.mix
```

## Install Red Alert

### Retail Game

Download both [Allies](https://bigdownloads.cnc-comm.com/ra/RA_Allies.zip) and
[Soviet](https://bigdownloads.cnc-comm.com/ra/RA_Soviet.zip) discs.

On the root of your SD card, create the folder `/vanilla-conquer/vanillara/`.
Then create two more directories: `allied` and `soviet`.

From the allies disc, copy the following files:
 - `INSTALL/REDALERT.MIX` into `/vanilla-conquer/vanillara/`
 - `MAIN.MIX` into `/vanilla-conquer/vanillara/allied/`

From the soviet disc, copy:
 - `MAIN.MIX` into `/vanilla-conquer/vanillara/soviet/`.

Finally, copy the ROM `vanillara.nds` into the root of your SD card and you
are done! Just launch the game!

The final working tree should look like this:

```
vanilla-conquer/
└── vanillara
    ├── REDALERT.MIX
    ├── allied
    │   └── MAIN.MIX
    └── soviet
        └── MAIN.MIX
```

### Installing Counterstrike

**WARNING**: Installing Counterstrike will degrade game's performance, as more things
has to be loaded on memory thus leaving less space for the shapes cache (ingame sprites).
Only install the extension if you intend to play it.

Download the [Counterstrike](https://bigdownloads.cnc-comm.com/ra/RA_Counterstrike.zip) disc.

Create a new folder named `counterstrike` inside `vanillara`. Then copy from the Counterstrike disc:
 - `MAIN.MIX` into `/vanilla-conquer/vanillara/counterstrike/`.
 - `EXPAND.MIX` (inside SETUP/INSTALL/CSTRIKE.RTP package) into `/vanilla-conquer/vanillara/`.

On some systems it may be easier to install the game on DOSBox to retrieve `EXPAND.MIX`.

The final working tree should look like this:
```
vanilla-conquer/
└── vanillara
    ├── REDALERT.MIX
    ├── EXPAND.MIX
    ├── allied
    │   └── MAIN.MIX
    ├── soviet
    │   └── MAIN.MIX
    └── counterstrike
        └── MAIN.MIX
```

### Installing Aftermath

**WARNING**: Installing Aftermath will degrade game's performance even more, as it will require
to disable the shapes cache to load the game. Only install the extension if you intend to play it.

Download the [Aftermath](https://bigdownloads.cnc-comm.com/ra/RA_Aftermath.zip) disc.

Create a new folder named `aftermath` inside `vanillara`. Then copy from the Aftermath disc:
 - `MAIN.MIX` into `/vanilla-conquer/vanillara/aftermath/`.
 - `EXPAND2.MIX` (inside SETUP/INSTALL/PATCH.RTP package) into `/vanilla-conquer/vanillara/`.

The final working tree should look like this:
```
vanilla-conquer/
└── vanillara
    ├── REDALERT.MIX
    ├── EXPAND.MIX
    ├── allied
    │   └── MAIN.MIX
    ├── soviet
    │   └── MAIN.MIX
    ├── counterstrike
    │   └── MAIN.MIX
    └── aftermath
        └── MAIN.MIX
```

## Controls

Currently controls are assigned in the following manner, but it may be changed
in the future as better controls are developed. This one tries to concentrate
the main functions on the left hand, while the right hand is used to command
units.

- Touchscreen: Command unit (Mouse LCLICK)
- DPAD: Scroll screen.
- L: Cancel selection/action (Mouse RCLICK).
- B: Force attack (CTRL).
- A: Force trample (ALT).
- X: Area Guard (G).
- Y: Scatter Units (X).
- R: Move viewport to Construction Yard (H).
- L + DPAD: Select Team #.
- B + L + DPAD: Assign Team #.

## Bugs

Report bugs on the issues page in github.

