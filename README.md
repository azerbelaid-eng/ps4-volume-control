# PS4 Volume Control Plugin (`ps4_volume_control.prx`)

A **GoldHEN Plugin** for jailbroken PlayStation 4 consoles that enables **wireless volume control over HDMI** directly from your DualShock 4 controller.

Designed specifically for PC monitors and sound setups without remote controls.

---

## 🎮 Controller Shortcuts

Hold **L3** (press down the left analog stick) and press a D-Pad button:

| Combination | Action | On-Screen Display |
| :--- | :--- | :--- |
| **`L3 + D-Pad UP`** | Increase Volume (+5%) | `Volume: [||||||||..] 80%` |
| **`L3 + D-Pad DOWN`** | Decrease Volume (-5%) | `Volume: [||||||....] 60%` |
| **`L3 + D-Pad LEFT`** | Toggle Mute / Unmute | `Volume: [ MUTED ]` |
| **`L3 + D-Pad RIGHT`** | Reset Volume to 100% | `Volume: [||||||||||] 100%` |

> **Note:** The plugin automatically intercepts and suppresses the D-Pad buttons while holding `L3`, so you won't accidentally trigger in-game menus, weapon wheels, or potions while adjusting the volume.

---

## ⚙️ Initial Console Setup (Required Once)

### 1. Set Audio Output to Linear PCM
1. On your PS4, go to **Settings** > **Sound and Screen** > **Audio Output Settings**.
2. Select **Audio Format (Priority)** and set it to **Linear PCM**.
   *(Linear PCM outputs raw audio frames that allow software volume scaling).*

### 2. Set Monitor Physical Volume High Once
* Turn up the physical volume buttons on your PC monitor to the maximum comfortable volume you would ever want.
* From now on, you will use the controller to attenuate (lower) or restore the volume from your seat.

---

## 📦 How to Install on Your PS4

### Step 1: Copy the PRX Plugin to Your PS4
You can transfer files using **FTP** (via FileZilla/WinSCP) or using **PS4-Xplorer** from a USB drive:
1. Connect to your PS4 via FTP (port `1337` or `2121` depending on GoldHEN settings).
2. Navigate to:
   ```text
   /data/GoldHEN/plugins/
   ```
   *(If the `plugins` folder doesn't exist, create it).*
3. Copy **`ps4_volume_control.prx`** into that folder.

### Step 2: Configure `plugins.ini`
1. On your PS4, navigate to:
   ```text
   /data/GoldHEN/
   ```
2. Open or create the file named **`plugins.ini`**.
3. Add the following lines:
   ```ini
   [default]
   /data/GoldHEN/plugins/ps4_volume_control.prx=true
   ```
   *Setting it under `[default]` automatically loads the volume control inside all games.*

### Step 3: Enable Plugin Loader in GoldHEN
1. On your PS4 home screen, go to **Settings** > **GoldHEN**.
2. Go to **Plugin Settings**.
3. Ensure **Enable Plugin Loader** is checked (`ON`).
4. (Optional) In GoldHEN settings, check **Show Plugin Loading Notification** so you see a confirmation when launching a game.

---

## 🛠️ How to Build from Source

### Option A: Automatic Build via GitHub Actions (Easiest)
1. Push this folder to a GitHub repository.
2. The included `.github/workflows/build.yml` will automatically build the `.prx` in Ubuntu with the OpenOrbis container.
3. Download the compiled `ps4_volume_control.prx` from the **Actions** tab artifacts.

### Option B: Local Build (Linux / Docker)
If you have Docker installed:
```bash
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/openorbis/toolchain:latest make
```

---

## 💾 Volume Persistence

Your last chosen volume level is automatically saved to `/data/GoldHEN/volume.ini`. When you restart the game or reboot the console, your preferred volume level is preserved!

