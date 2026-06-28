# Flipper Zero Calc Invader (Digital Invader)

A recreation of the classic "Digital Invader" game from the iconic Casio MG-880 game calculator (released in 1980) for the Flipper Zero.

## Game Overview

In this shooting/puzzle game, numeric "invaders" march from the right side of the screen toward your defense line on the left. Your objective is to match the turret's number with the incoming invaders and fire a beam to destroy them.

## Controls

Use the Flipper Zero buttons to play:

- **`▲ (Up)` / `▼ (Down)` / `◀ (Left)` / `▶ (Right)` (D-Pad)**: Cycles the turret's aiming digit (**AIM**).
  - Pressing any D-pad direction cycles the digit: `0` → `1` → `2` → ... → `9` → `n (UFO)` → `0`.
- **`OK` Button**: Fires the beam (**FIRE**).
  - If the turret's digit matches an invader on the screen, the **leftmost matching invader** is destroyed.
- **`Back` Button**: Exits the game.

---

## Game Rules & Mechanics

### 1. Level Structure
- Each stage (pattern) spawns a total of **16 invaders** (including the UFO).
- Clear the stage by destroying all 16 invaders.
- Upon clearing a stage, your lives and ammo are fully restored.

### 2. Lives & Game Over
- You start with 3 lives, visually represented on the screen by custom segment states: `≡` (3 lives), `二` (2 lives), and `ー` (1 life).
- If an invader reaches the left side of the screen (the defense line), a life is lost.
- The game ends if all 3 lives are lost, or if you run out of ammo before clearing the stage.

### 3. Ammo Limit (BEAM)
- You have a maximum of **30 beams** per stage.
- Be careful not to waste shots, or you will face a Game Over due to running out of ammunition.

### 4. Scoring
Destroying invaders closer to your base (further to the left) yields higher scores:
- **Part 1**: Invaders spawn at the rightmost column (Digit 8).
  - Points: `10`, `20`, `30`, `40`, `50`, `60` from left to right.
- **Part 2**: Invaders spawn one column closer (Digit 7).
  - Points: `20`, `40`, `60`, `80`, `100` from left to right.

### 5. UFO (n) Bonus
- If the sum of the digits of the defeated invaders ends in `0` (a multiple of 10, e.g., 10, 20, 30...), the next spawned invader will be a UFO (`n`).
- Match the turret to `n` and shoot it to earn a flat **300 points** bonus!
- UFOs are not included in the sum calculation.

---

## How to Build & Install

This project uses the `uv` tool and `ufbt` (micro Flipper Build Tool) for building.

### 1. Setup Dependencies
If you have `uv` installed, create a virtual environment and install `ufbt`:
```bash
# Create virtual environment
uv venv

# Install ufbt
uv pip install ufbt
```

### 2. Compile the App
```bash
.venv/bin/ufbt
```
Once compiled successfully, the `calc_inv.fap` file will be generated in the `dist/` directory.

### 3. Transfer and Run on Flipper Zero
Connect your Flipper Zero to your PC via a USB data cable and run:
```bash
.venv/bin/ufbt launch
```
This will automatically build, transfer, and launch the application on your device.
