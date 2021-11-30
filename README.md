# Vanilla Conquer: Nintendo DSi port

This is a Nintendo DSi port of Command & Conquer based on Vanilla Conquer.

Command & Conquer is a Real Strategy Game released in 1995 developed by Westwood Studios and its
trademark is currently owned by Eletronic Arts. Tiberian Dawn was made freeware
in 2007, and its sourcecode made public in 2020. Vanilla Conquer is a source port
with multi-platform support.

Currently only VanillaTD is supported but Red Alert support is also planned.

For playing it on your Nintendo DSi, you will need:
- A jailbreaked Nintendo DSi capable of running .nds ROMs in DSi mode through the memory card.
- A memory card with at least 1.4Gb of free space.
- Assets from DOS Command & Conquer versions: both GDI and Nod discs, with Covert Operations also supported but not necessary.

## Downloads

- [VanillaTD Nintendo DS ROM](https://github.com/giulianobelinassi/Vanilla-Conquer/releases/download/nds-v0.1/vanillatd.nds).
- [C&C DOS: GDI disc](https://bigdownloads.cnc-comm.com/cnc1/DOSCNC_GDI.zip).
- [C&C DOS: Nod disc](https://bigdownloads.cnc-comm.com/cnc1/DOSCNC_Nod.zip).
- [C&C Covert Operations disc](https://bigdownloads.cnc-comm.com/cnc1/CNC_Covertops.zip).

## Install

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

