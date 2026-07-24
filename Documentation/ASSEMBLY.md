# FootSense — Assembly Guide

Step-by-step physical build instructions for the FootSense smart insole.  
Read the full [Wiring Guide](WIRING.md) before starting — this guide assumes you already know which pin goes where and focuses on the physical assembly sequence.

---

## What You Need

### Components
| Item | Qty |
|---|---|
| ESP32 DevKit V1 (38-pin, DOIT) | 1 |
| MPU6050 module | 1 |
| 2.2" flex sensor (2-pin) | 3 |
| 50mm piezoelectric disc | 2 |
| 10kΩ resistor | 2 |
| 33kΩ resistor | 3 |
| 1MΩ resistor | 2 |
| Perfboard ("zero PCB") | 1 |
| Li-ion cell (18650 or similar) | 1 |
| HW-373 (TP4056 charging + protection module) | 1 |
| Boost converter module (3.7V → 5V) | 1 |
| Gel insole | 2 |
| Foam padding sheet | 1 |
| Cheap shoe | 1 pair |
| Hookup wire (assorted colours) | — |

### Tools
| Tool | Purpose |
|---|---|
| Soldering iron | All PCB connections |
| Solder | — |
| Multimeter | Continuity checks at each stage |
| Wire stripper | Preparing hookup wire |
| Hot glue gun | Mounting sensors to sole |
| Scissors or craft knife | Cutting foam and insole layers |
| Tape (masking or electrical) | Temporary labelling during build |

---

## Stage 1 — Test Everything on a Breadboard First

**Do not skip this stage.** Debugging a bad solder joint on a completed insole is significantly harder than catching a wiring mistake on a breadboard.

### 1.1 — Test the MPU6050

1. Wire MPU6050 to the ESP32 on a breadboard per the [Wiring Guide](WIRING.md).
2. Add 10kΩ pull-up resistors on SDA and SCL.
3. Upload `mpu6050_esp32.ino` from the `/test-sketches` folder.
4. Open Serial Monitor at 115200 baud.
5. **Pass criteria:** sensor initialises without error, pitch and roll values change sensibly when you tilt the board.

### 1.2 — Test each flex sensor individually

1. Wire one flex sensor as a voltage divider (33kΩ, into GPIO34).
2. Upload `flex_sensor_esp32.ino`.
3. Keep the sensor flat during startup calibration.
4. Bend the sensor — deviation value should rise cleanly from 0.
5. Repeat for the other two sensors on GPIO35 and GPIO32.
6. **Pass criteria:** each sensor reads near-zero at rest and responds smoothly to bending, with no unexplained cross-talk between channels.

### 1.3 — Test each piezo disc individually

1. Wire one piezo with its 1MΩ damping resistor in parallel, into GPIO25.
2. Upload `piezo_esp32.ino`.
3. Do not touch the piezo during baseline measurement.
4. Tap it at a realistic pressure — not a hard finger tap, but the kind of pressure it would actually experience through the sole of a shoe.
5. Note the **deviation value** during real taps — you will need this to set the threshold in the final firmware.
6. Repeat for the second piezo on GPIO26.
7. **Pass criteria:** both piezos show a stable baseline and a clear spike on tap. Record the peak deviation value for each.

### 1.4 — Combined breadboard test with dashboard

1. Wire all sensors together on the breadboard.
2. Fill in your WiFi SSID and password in `breadboard_wifi_test.ino` and upload it.
3. Open Serial Monitor — note the IP address printed after WiFi connects.
4. Open `insole_dashboard.html` in a browser on the same WiFi network, enter the IP, click Connect.
5. **Pass criteria:** dashboard shows Live status, foot diagram responds to flex sensor bending, piezo impact counter increases on tap, pitch and roll update when you tilt the board.

Only move to Stage 2 once all four of these pass cleanly.

---

## Stage 2 — Prepare the Sole Stack

### 2.1 — Plan your sensor positions

Before cutting or gluing anything, lay your three flex sensors and two piezo discs on top of the gel insole and physically confirm they fit where you intend:

- **Flex 1 (medial arch):** inside edge of the arch, roughly halfway between heel and ball of foot
- **Flex 2 (rear arch):** where the arch meets the heel on the medial side
- **Flex 3 (anterior arch):** where the arch transitions into the ball of the foot
- **Piezo 1 (heel):** centre of the heel pad
- **Piezo 2 (forefoot):** ball of the foot, roughly under the second/third toe

Mark each position lightly with a pen before committing to any glue.

### 2.2 — Cut the foam layer

Cut the foam sheet to the shape of your insole. This layer sits directly over the sensors and protects your foot from feeling the hard edges of the perfboard and sensor housings.

Cut small recesses or thin spots in the foam wherever a sensor will sit — just enough to let the sensor make good contact with the insole above it without the foam compressing so much that it damps the signal entirely. For the piezo discs especially, too thick a foam layer above them will reduce the voltage spike they produce on impact.

### 2.3 — Prepare the gel insole

The gel insole is the layer your foot actually contacts. It sits on top of everything. No modifications needed to the gel insole itself — it just needs to be clean and dry before any glue goes near it.

---

## Stage 3 — Solder the Perfboard

Build in this specific order. Soldering the power bus and ESP32 first gives you stable reference points; adding sensors one at a time lets you test as you go rather than discovering a problem only after everything is soldered.

### 3.1 — Power bus strips

1. Run a continuous 3V3 strip along one long edge of the perfboard.
2. Run a continuous GND strip along the other long edge.
3. Continuity-check both strips end to end with a multimeter before placing anything else.

### 3.2 — ESP32 DevKit

1. Solder the ESP32 into the centre of the perfboard.
2. Run short jumper wires from its 3V3 pin to the 3V3 bus strip, and from its GND pin to the GND bus strip.
3. Confirm continuity from the ESP32's 3V3 pin to the far end of the bus strip.

### 3.3 — MPU6050 + pull-up resistors

1. Solder the MPU6050 module.
2. Run wires: VCC → 3V3 bus, GND → GND bus, SDA → GPIO21, SCL → GPIO22.
3. Solder the two 10kΩ pull-up resistors: one from the 3V3 bus to the SDA wire, one from the 3V3 bus to the SCL wire. Place them physically close to the MPU6050, not spread across the board.
4. **Test now:** upload `mpu6050_esp32.ino` and confirm it still initialises correctly before continuing.

### 3.4 — Flex sensor voltage dividers

For each flex sensor, solder the divider circuit directly on the perfboard before the wires go anywhere near the shoe:

1. Solder one leg of the 33kΩ resistor to a pad.
2. Connect the other leg of the resistor to GND bus.
3. The top of the resistor (junction point) connects to the ESP32 analog pin and will also connect to the flex sensor's second lead — solder a short wire from this junction to the correct GPIO now.
4. Leave the flex sensor itself on a flying lead (long enough to reach its mounting position in the sole) — do not solder the flex sensor directly onto the perfboard.
5. Repeat for all three flex sensors on GPIO34, GPIO35, GPIO32.
6. **Test now:** upload `flex_sensor_esp32.ino` and bend each sensor by hand to confirm all three channels respond before continuing.

### 3.5 — Piezo damping resistors

1. Solder one leg of each 1MΩ resistor to a pad.
2. Connect the other leg to GND bus.
3. Run a wire from the top of each resistor (junction point) to the correct GPIO (GPIO25 for heel, GPIO26 for forefoot).
4. Leave the piezo disc on a flying lead — do not solder it directly onto the perfboard.
5. **Test now:** tap each piezo by hand and confirm both channels register impacts in Serial Monitor before continuing.

### 3.6 — Power chain

1. Solder the HW-373 module's B+ and B− pads to wires that will connect to the battery.
2. Connect HW-373 OUT+ and OUT− to the boost converter's input.
3. Connect the boost converter's 5V output to the ESP32's VIN pin.
4. Connect the boost converter's GND output to the GND bus.
5. **Do not connect the battery yet.**

### 3.7 — Full board test before closing up

With all components soldered and sensors still on flying leads:

1. Connect the battery.
2. Upload `smart_insole_final.ino` (with your WiFi credentials filled in).
3. Open Serial Monitor — confirm MPU6050 calibrates, flex baselines print sensibly, piezo baselines print sensibly, and an IP address appears.
4. Open the dashboard, connect, and confirm all channels respond.
5. **This is your last easy chance to fix anything.** Only continue to Stage 4 once this passes completely.

---

## Stage 4 — Mount Sensors in the Shoe

### 4.1 — Route wires out of the shoe

Decide where the perfboard will sit — the recommended position is on the outside of the shoe, low on the heel counter, so the electronics stay dry and accessible. Before gluing anything inside the sole:

1. Mark where the sensor wires will exit the shoe.
2. Use a craft knife to cut a small channel through the side of the shoe's upper or sole wall — just big enough for the wire bundle to pass through.
3. Thread all the flying leads through this channel so you know the wire length is correct before any glue goes on.

### 4.2 — Hot-glue the sensors

**Work one sensor at a time.** Hot glue sets fast and repositioning is painful.

**Flex sensors:**
1. Apply a small amount of hot glue to the marked position on the sole.
2. Press the flex sensor flat into position immediately.
3. Hold for 15–20 seconds.
4. The flex sensor's marked/printed side should face inward (toward the insole layers above it), not toward the shoe sole below — bending the marked side outward can damage the sensor over time.
5. Leave a small loop of slack wire near the sensor's base — the sole flexes during walking and a wire pulled taut will eventually break at the solder joint.

**Piezo discs:**
1. Apply hot glue to the marked heel and forefoot positions.
2. Press the piezo disc flat — the brass-coloured side faces down toward the shoe sole, the ceramic/white side faces up toward your foot.
3. Keep the 1MΩ resistor physically close to the piezo disc, not routed all the way back to the perfboard — a short high-impedance loop picks up less noise.
4. Leave a small wire loop for strain relief, same as the flex sensors.

### 4.3 — Route and secure wires inside the shoe

1. Run all wires along the inside wall of the shoe, not across the sole where they'd be walked on.
2. Tack them down with small spots of hot glue every 3–4 cm so they can't flap or get pinched when the shoe flexes.
3. Where the wire bundle exits through the channel in the shoe wall, apply hot glue around the hole to act as a strain relief grommet.

### 4.4 — Mount the perfboard

1. Secure the perfboard to the outside of the heel counter using hot glue, a 3D-printed clip, or hook-and-loop (Velcro) if you want it to be removable.
2. Ensure no solder joints or component legs can contact the shoe's exterior and short against anything metallic.
3. The battery and HW-373 module can sit inside or alongside the enclosure — keep them accessible so the battery can be removed for charging.

---

## Stage 5 — Final Sole Stack Assembly

Layer order from bottom (touching shoe sole) to top (touching foot):

| Layer | Component |
|---|---|
| 1 (bottom) | Original shoe insole or bare shoe sole |
| 2 | Sensors (flex + piezo), already hot-glued into position |
| 3 | Foam padding — cut to insole shape, with thin spots over each sensor |
| 4 (top) | Gel insole — the surface your foot actually contacts |

Press the gel insole down gently over the foam. If it doesn't sit flat, the foam recesses over the sensors may need to be deepened slightly.

---

## Stage 6 — Power-On and Final Validation

1. Connect the battery.
2. Wait for the calibration sequence to complete — keep the shoe flat and still during the first 2 seconds.
3. Open the dashboard and confirm Live status.
4. Walk around with the shoe on for at least 60 seconds.
5. Check each of the following:

| Check | Expected result |
|---|---|
| Arch zone dots on dashboard | All three change colour/size as your foot loads the arch |
| Step count | Increments with each step — not double-counting |
| Heel impacts | Increments on heel-strike |
| Forefoot impacts | Increments on forefoot contact |
| Average impact angle | A non-zero value that seems plausible for your gait |
| Get diagnosis | Produces a sensible written summary after 60+ seconds of walking |

6. If any sensor appears dead or stuck at zero, see the Troubleshooting section in [WIRING.md](WIRING.md).

---

## Calibration Notes

### Flex sensor deadzone
The firmware applies a deadzone (`FLEX_DEADZONE = 15` raw ADC counts) so sensors read a clean 0 at rest rather than jittering. If you still see small nonzero values at rest, increase this value in `smart_insole_final.ino`. If the sensors feel unresponsive and need a firm bend before registering, decrease it.

### Piezo thresholds
Each piezo has its own independent threshold (`PIEZO_IMPACT_THRESHOLD[2]`). The values in firmware are starting estimates only. Run the isolated piezo test sketches from `/test-sketches`, tap at realistic walking pressure, read the peak deviation value, and set each threshold to roughly half that value. Thresholds that worked on a breadboard may need adjusting once the piezo is glued into the sole, since the mounting material changes how much force reaches the disc.

### Step count sensitivity
Step detection uses a peak-detection algorithm with a high threshold of 1.35g and a low threshold of 0.85g. If steps are being missed or double-counted during normal walking, adjust `STEP_THRESHOLD_HIGH` and `STEP_THRESHOLD_LOW` in `smart_insole_final.ino`. Walking tends to produce lower acceleration peaks than running; if the insole is being used primarily for running, the thresholds can be raised slightly.

---

